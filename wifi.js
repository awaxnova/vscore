// Wi‑Fi fallback transport: simple JSON POSTs to the ESP32 HTTP server.
// Requires CORS enabled on the ESP32 for your GitHub Pages origin or '*'.

export class WifiTransport {
  constructor(log=console) {
    this.log = log;
    this.baseUrl = null;
    this.onMessage = () => {};
    this.onStatus = () => {};
    this._pollTimer = null;
  }

  async connect(baseUrl) {
    if (!/^https?:\/\//i.test(baseUrl)) {
      baseUrl = 'http://' + baseUrl;
    }
    this.baseUrl = baseUrl.replace(/\/+$/, '');
    this.onStatus({ kind: 'connecting', transport: 'wifi', baseUrl: this.baseUrl });
    // quick ping
    await this._fetch('/api/v1/ping', { method: 'GET' });
    this._subscribe();
    this.onStatus({ kind: 'connected', transport: 'wifi', baseUrl: this.baseUrl });
  }

  status() {
    return { kind: this.baseUrl ? 'connected' : 'disconnected', transport: 'wifi', baseUrl: this.baseUrl };
  }

  async send(obj) {
    if (!this.baseUrl) throw new Error('Not connected (Wi‑Fi)');
    await this._fetch('/api/v1/scoreboard', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(obj),
    });
  }

  _subscribe() {
    // Try SSE first
    const sseUrl = this.baseUrl + '/api/v1/events';
    try {
      const es = new EventSource(sseUrl);
      es.onmessage = (ev) => {
        try { this.onMessage(JSON.parse(ev.data)); }
        catch { this.onMessage({ type: 'text', data: ev.data }); }
      };
      es.onerror = () => {
        es.close();
        this._beginPolling();
      };
    } catch {
      this._beginPolling();
    }
  }

  _beginPolling() {
    const poll = async () => {
      try {
        const res = await fetch(this.baseUrl + '/api/v1/state', { cache: 'no-store' });
        if (res.ok) {
          const obj = await res.json();
          this.onMessage(obj);
        }
      } catch {}
      this._pollTimer = setTimeout(poll, 1000);
    };
    poll();
  }

  async _fetch(path, init) {
    const res = await fetch(this.baseUrl + path, {
      ...init,
      mode: 'cors',
      cache: 'no-store',
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return res;
  }
}
