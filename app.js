import { Transport } from './transport.js';

const $ = (sel) => document.querySelector(sel);
const logEl = $('#log');
const transportDot = $('#transportDot');
const transportText = $('#transportText');
const buildInfo = $('#buildInfo');
buildInfo.textContent = new Date().toISOString();

const state = {
  a: 0,
  b: 0,
  sv: 'A', // serving team: 'A' or 'B'
  set: 1,
  ma: 0,
  mb: 0,
  bo: 3,
  ta: 'Team A',
  tb: 'Team B',
  ca: '#42a5f5', // text color / accent
  cb: '#ef5350',
  abg: '#0c1220', // team A panel background
  bbg: '#0c1220', // team B panel background
  ra: ['', '', '', '', '', ''], // rotation A (6 slots)
  rb: ['', '', '', '', '', ''], // rotation B (6 slots)
  rsa: 0, // current server index A (0-5)
  rsb: 0, // current server index B (0-5)
  la: [], // logs for A: {reason, scorer, ts}
  lb: []  // logs for B
};

function render() {
  $('#scoreA').textContent = state.a;
  $('#scoreB').textContent = state.b;
  $('#teamAName').value = state.ta;
  $('#teamBName').value = state.tb;
  $('#teamAColor').value = state.ca;
  $('#teamBColor').value = state.cb;
  const bgA = $('#teamABgColor');
  const bgB = $('#teamBBgColor');
  if (bgA) bgA.value = state.abg;
  if (bgB) bgB.value = state.bbg;
  $('#setNumber').value = state.set;
  $('#matchA').value = state.ma;
  $('#matchB').value = state.mb;
  $('#bestOf').value = state.bo;
  $('#serveA').checked = state.sv === 'A';
  $('#serveB').checked = state.sv === 'B';

  // rotation fields
  for (let i = 0; i < 6; i++) {
    const ai = document.getElementById(`rotationA${i + 1}`);
    const bi = document.getElementById(`rotationB${i + 1}`);
    if (ai) ai.value = state.ra[i] ?? '';
    if (bi) bi.value = state.rb[i] ?? '';
  }

  // panel backgrounds (cosmetic)
  const aPanel = document.getElementById('teamASection');
  const bPanel = document.getElementById('teamBSection');
  if (aPanel) aPanel.style.background = state.abg;
  if (bPanel) bPanel.style.background = state.bbg;
  if (aPanel) aPanel.style.borderRadius = '12px';
  if (bPanel) bPanel.style.borderRadius = '12px';
  if (aPanel) aPanel.style.padding = '12px';
  if (bPanel) bPanel.style.padding = '12px';

  renderLogsSafe();
}
render();

function log(...args) {
  const line = args.map(a => typeof a === 'string' ? a : JSON.stringify(a)).join(' ');
  logEl.textContent += line + '\n';
  logEl.scrollTop = logEl.scrollHeight;
  console.log(...args);
}

function statusBadge(s) {
  if (!s) return;
  if (s.kind === 'connected') {
    transportDot.className = 'status-dot dot-ok';
  } else if (s.kind === 'connecting') {
    transportDot.className = 'status-dot dot-busy';
  } else {
    transportDot.className = 'status-dot dot-bad';
  }
  const label = s.transport === 'ble' ? (s.name || 'Bluetooth') : (s.baseUrl || 'Wi‑Fi');
  transportText.textContent = `${s.kind} via ${label}`;
}

const t = new Transport({ log });
t.onMessage = (msg) => {
  log('<=', msg);
  if (msg.type === 'state') {
    // Merge state from device, preserving client-only fields when absent
    Object.assign(state, msg.data);
    render();
  } else if (msg.type === 'ack') {
    // ignore
  }
};
t.onStatus = (s) => statusBadge(s);

$('#connectBle').addEventListener('click', async () => {
  try {
    await t.connectBle();
    log('BLE connected');
  } catch (e) {
    log('BLE error:', e.message || e);
    alert(e.message || e);
  }
});

$('#connectWifi').addEventListener('click', async () => {
  const base = $('#wifiBaseUrl').value || 'http://192.168.4.1';
  try {
    await t.connectWifi(base);
    log('Wi‑Fi connected to', base);
  } catch (e) {
    log('Wi‑Fi error:', e.message || e);
    alert(e.message || e);
  }
});

$('#swapBtn').addEventListener('click', () => {
  [state.ta, state.tb] = [state.tb, state.ta];
  [state.ca, state.cb] = [state.cb, state.ca];
  [state.abg, state.bbg] = [state.bbg, state.abg];
  [state.ra, state.rb] = [state.rb, state.ra];
  [state.rsa, state.rsb] = [state.rsb, state.rsa];
  [state.la, state.lb] = [state.lb, state.la];
  render();
  sendState();
});

$('#teamAName').addEventListener('input', (e) => { state.ta = e.target.value; sendState(); });
$('#teamBName').addEventListener('input', (e) => { state.tb = e.target.value; sendState(); });
$('#teamAColor').addEventListener('input', (e) => { state.ca = e.target.value; sendState(); });
$('#teamBColor').addEventListener('input', (e) => { state.cb = e.target.value; sendState(); });
const teamABgEl = document.getElementById('teamABgColor');
const teamBBgEl = document.getElementById('teamBBgColor');
if (teamABgEl) teamABgEl.addEventListener('input', (e) => { state.abg = e.target.value; render(); sendState(); });
if (teamBBgEl) teamBBgEl.addEventListener('input', (e) => { state.bbg = e.target.value; render(); sendState(); });

// Rotation inputs
for (let i = 0; i < 6; i++) {
  const ai = document.getElementById(`rotationA${i + 1}`);
  const bi = document.getElementById(`rotationB${i + 1}`);
  if (ai) ai.addEventListener('input', (e) => { state.ra[i] = e.target.value; sendState(); });
  if (bi) bi.addEventListener('input', (e) => { state.rb[i] = e.target.value; sendState(); });
}

$('#plusA').addEventListener('click', () => { addPoint('A', 'Manual +1'); });
$('#minusA').addEventListener('click', () => { removePoint('A', 'Manual -1'); });
$('#plusB').addEventListener('click', () => { addPoint('B', 'Manual +1'); });
$('#minusB').addEventListener('click', () => { removePoint('B', 'Manual -1'); });

$('#serveA').addEventListener('change', () => { state.sv = 'A'; sendState(); });
$('#serveB').addEventListener('change', () => { state.sv = 'B'; sendState(); });

// Team-specific actions
const sideoutA = document.getElementById('sideoutA');
const sideoutB = document.getElementById('sideoutB');
const servedScoredA = document.getElementById('servedScoredA');
const servedScoredB = document.getElementById('servedScoredB');
const voidPrevA = document.getElementById('voidPrevA');
const voidPrevB = document.getElementById('voidPrevB');
const wonSetA = document.getElementById('wonSetA');
const wonSetB = document.getElementById('wonSetB');
const clearSetA = document.getElementById('clearSetA');
const clearSetB = document.getElementById('clearSetB');

if (sideoutA) sideoutA.addEventListener('click', () => doSideOut('A'));
if (sideoutB) sideoutB.addEventListener('click', () => doSideOut('B'));
if (servedScoredA) servedScoredA.addEventListener('click', () => doServedAndScored('A'));
if (servedScoredB) servedScoredB.addEventListener('click', () => doServedAndScored('B'));
if (voidPrevA) voidPrevA.addEventListener('click', () => removePoint('A', 'Voided previous point'));
if (voidPrevB) voidPrevB.addEventListener('click', () => removePoint('B', 'Voided previous point'));
if (wonSetA) wonSetA.addEventListener('click', () => doWonSet('A'));
if (wonSetB) wonSetB.addEventListener('click', () => doWonSet('B'));
if (clearSetA) clearSetA.addEventListener('click', () => doClearSet());
if (clearSetB) clearSetB.addEventListener('click', () => doClearSet());

$('#setNumber').addEventListener('input', (e) => { state.set = parseInt(e.target.value || '1', 10); sendState(); });
$('#matchA').addEventListener('input', (e) => { state.ma = parseInt(e.target.value || '0', 10); sendState(); });
$('#matchB').addEventListener('input', (e) => { state.mb = parseInt(e.target.value || '0', 10); sendState(); });
$('#bestOf').addEventListener('change', (e) => { state.bo = parseInt(e.target.value, 10); sendState(); });

$('#endSet').addEventListener('click', () => {
  if (state.a > state.b) state.ma++; else if (state.b > state.a) state.mb++;
  state.a = 0; state.b = 0; state.set++;
  render(); sendState();
});
$('#undo').addEventListener('click', () => {
  // Simple logical undo demo (in practice, keep a stack)
  log('Undo not implemented: keep a stack if needed.');
});

$('#clearLog').addEventListener('click', () => logEl.textContent = '');

async function sendState() {
  const msg = { type: 'state', data: state };
  log('=>', msg);
  try {
    await t.send(msg);
  } catch (e) {
    log('send error:', e.message || e);
  }
}

// Safer DOM-based log rendering (no innerHTML)
function renderLogsSafe() {
  const ulA = document.getElementById('reasonsA');
  const ulB = document.getElementById('reasonsB');
  if (ulA) renderList(ulA, state.la);
  if (ulB) renderList(ulB, state.lb);
}

function renderList(ul, arr) {
  ul.textContent = '';
  for (const e of arr) {
    const t = new Date(e.ts).toLocaleTimeString();
    const who = e.scorer ? ` - #${e.scorer}` : '';
    const li = document.createElement('li');
    li.textContent = `${e.reason}${who} - ${t}`;
    ul.appendChild(li);
  }
}
// Helpers for logs and scoring
function renderLogs() {
  const ulA = document.getElementById('reasonsA');
  const ulB = document.getElementById('reasonsB');
  if (ulA) ulA.innerHTML = state.la.map(entryToLi).join('');
  if (ulB) ulB.innerHTML = state.lb.map(entryToLi).join('');
}

function entryToLi(e) {
  const t = new Date(e.ts).toLocaleTimeString();
  const who = e.scorer ? ` — #${e.scorer}` : '';
  return `<li>${escapeHtml(e.reason)}${who} — ${t}</li>`;
}

function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, (c) => ({ '&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;','\'':'&#39;' }[c]));
}

function addLog(team, reason, scorer) {
  const entry = { reason, scorer: scorer || '', ts: Date.now() };
  if (team === 'A') state.la.push(entry); else state.lb.push(entry);
  renderLogs();
}

function currentServer(team) {
  if (team === 'A') return state.ra[state.rsa] || '';
  return state.rb[state.rsb] || '';
}

function addPoint(team, reason, scorer) {
  if (team === 'A') state.a++; else state.b++;
  addLog(team, reason, scorer);
  render();
  sendState();
}

function removePoint(team, reason) {
  if (team === 'A') state.a = Math.max(0, state.a - 1); else state.b = Math.max(0, state.b - 1);
  addLog(team, reason || 'Point removed');
  render();
  sendState();
}

function doSideOut(team) {
  // Receiving team gains serve and rotates one position
  state.sv = team;
  if (team === 'A') state.rsa = (state.rsa + 1) % 6; else state.rsb = (state.rsb + 1) % 6;
  addLog(team, 'Side out: gained serve');
  render();
  sendState();
}

function doServedAndScored(team) {
  const srv = currentServer(team);
  addPoint(team, 'Served and scored', srv);
  // same server continues on point;
}

function doWonSet(team) {
  if (team === 'A') state.ma++; else state.mb++;
  addLog(team, 'Won set');
  state.a = 0; state.b = 0; state.set++;
  render();
  sendState();
}

function doClearSet() {
  state.a = 0; state.b = 0;
  addLog('A', 'Set cleared');
  addLog('B', 'Set cleared');
  render();
  sendState();
}

// SW status (cosmetic)
if (navigator.serviceWorker) {
  navigator.serviceWorker.ready.then(() => {
    document.getElementById('swStatusDot').className = 'status-dot dot-ok';
    document.getElementById('swStatusText').textContent = 'ready';
  }).catch(() => {
    document.getElementById('swStatusDot').className = 'status-dot dot-bad';
    document.getElementById('swStatusText').textContent = 'off';
  });
}
