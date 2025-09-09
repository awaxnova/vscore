import { Transport } from './transport.js';

const $ = (sel) => document.querySelector(sel);
const logEl = $('#log');
const transportDot = $('#transportDot');
const transportText = $('#transportText');
const buildInfo = $('#buildInfo');
buildInfo.textContent = new Date().toISOString();

const state = {
  a: 0, b: 0, sv: 'A', set: 1, ma: 0, mb: 0, bo: 3,
  ta: 'Team A', tb: 'Team B', ca: '#42a5f5', cb: '#ef5350'
};

function render() {
  $('#scoreA').textContent = state.a;
  $('#scoreB').textContent = state.b;
  $('#teamAName').value = state.ta;
  $('#teamBName').value = state.tb;
  $('#teamAColor').value = state.ca;
  $('#teamBColor').value = state.cb;
  $('#setNumber').value = state.set;
  $('#matchA').value = state.ma;
  $('#matchB').value = state.mb;
  $('#bestOf').value = state.bo;
  $('#serveA').checked = state.sv === 'A';
  $('#serveB').checked = state.sv === 'B';
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
  render();
  sendState();
});

$('#teamAName').addEventListener('input', (e) => { state.ta = e.target.value; sendState(); });
$('#teamBName').addEventListener('input', (e) => { state.tb = e.target.value; sendState(); });
$('#teamAColor').addEventListener('input', (e) => { state.ca = e.target.value; sendState(); });
$('#teamBColor').addEventListener('input', (e) => { state.cb = e.target.value; sendState(); });

$('#plusA').addEventListener('click', () => { state.a++; render(); sendState(); });
$('#minusA').addEventListener('click', () => { state.a = Math.max(0, state.a - 1); render(); sendState(); });
$('#plusB').addEventListener('click', () => { state.b++; render(); sendState(); });
$('#minusB').addEventListener('click', () => { state.b = Math.max(0, state.b - 1); render(); sendState(); });

$('#serveA').addEventListener('change', () => { state.sv = 'A'; sendState(); });
$('#serveB').addEventListener('change', () => { state.sv = 'B'; sendState(); });

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
