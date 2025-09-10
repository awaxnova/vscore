PAGES: https://awaxnova.github.io/vscore/

DEVICE: https://www.amazon.com/dp/B0DRFZ2FP9?smid=A1PKC2PUMNR8VD&ref_=chk_typ_imgToDp&th=1
DATASHEET: https://www.lcdwiki.com/3.2inch_ESP32-32E_Display
MANUFACTURER: https://hosyond.com/

# Scoreboard PWA (BLE + Wi‑Fi fallback)

A portable controller UI that talks to an ESP32‑based scoreboard display using **Web Bluetooth (BLE GATT)** when available, and falls back to **HTTP (Wi‑Fi SoftAP or LAN)** when not.

Host this on **GitHub Pages** (HTTPS). The service worker caches the app so it can run offline when you switch to the ESP32 SoftAP network.

---

## GATT Layout (JSON over Nordic UART‑style)

**Service (Custom / NUS‑style):** `6e400001-b5a3-f393-e0a9-e50e24dcca9e`  
**TX Characteristic (ESP32 ➜ Browser, notify):** `6e400003-b5a3-f393-e0a9-e50e24dcca9e`  
**RX Characteristic (Browser ➜ ESP32, write w/o response):** `6e400002-b5a3-f393-e0a9-e50e24dcca9e`

- Encoding: UTF‑8 JSON, max ~180 bytes per chunk (the browser will chunk writes; keep messages small).
- Message envelope:
```jsonc
{ "type": "hello" | "state" | "ack" | "error", "data": { ... } }
```

### Messages (examples)

**Browser ➜ ESP32**

- `hello` — optional handshake
```json
{ "type": "hello", "from": "pwa" }
```

- `state` — scoreboard update (send on any UI change)
```json
{
  "type": "state",
  "data": {
    "ta": "Team A",      // team A name (<= 20 chars)
    "tb": "Team B",      // team B name
    "ca": "#42a5f5",     // team A color (hex)
    "cb": "#ef5350",     // team B color (hex)
    "a": 12, "b": 13,    // scores 0..99
    "sv": "A",           // serving: "A" | "B"
    "set": 1,            // current set # (1-based)
    "ma": 0, "mb": 0,    // match wins A/B
    "bo": 3              // best of: 3 or 5
  }
}
```

**ESP32 ➜ Browser**

- `state` — device pushes current state (on connect and whenever it changes)
```json
{ "type": "state", "data": { /* same fields as above */ } }
```

- `ack` — acknowledge last command
```json
{ "type": "ack", "data": { "ok": true } }
```

- `error` — any error details
```json
{ "type": "error", "data": { "code": "bad_field", "msg": "score out of range" } }
```

> **Tip:** You can also offer a standard **Device Information Service (0x180A)** for `Model Number` and `Firmware Revision`, but it’s optional here since we do JSON handshakes.

---

## Wi‑Fi Fallback (HTTP JSON API)

When BLE is not available (e.g., iOS Safari), the PWA can talk to an ESP32 HTTP server. Enter your base URL (e.g., `http://192.168.4.1` in SoftAP mode) and press **Use Wi‑Fi**.

**Enable CORS** on the ESP32 for your GitHub Pages origin (or `*` while developing).

### Endpoints (sketch)

- `GET /api/v1/ping` → `200 OK` (health check)
- `POST /api/v1/scoreboard` (JSON body = same `state` envelope as BLE)
- `GET /api/v1/state` → returns latest state JSON
- `GET /api/v1/events` → **SSE stream** of state updates (optional; app falls back to polling `/state`)

### CORS headers (example)

```
Access-Control-Allow-Origin: https://<your-gh-username>.github.io
Access-Control-Allow-Methods: GET, POST, OPTIONS
Access-Control-Allow-Headers: Content-Type
```

---

## Build & Deploy (GitHub Pages)

1. Push these files into a repository root (or `/docs`).
2. In **Settings → Pages**, select `main` branch (`/root` or `/docs`).  
3. Visit your Pages URL. Click **Install** (Add to Home Screen) to cache offline.
4. For **SoftAP** use: connect the client device to the ESP32 Wi‑Fi, then open the PWA (it will load from cache), and enter `http://192.168.4.1` as the base URL.

---

## ESP32 Implementation Hints

- **BLE:** Use NimBLE or ESP-IDF/Arduino BLE, create the service/characteristics above; on RX, parse JSON; on any change, `notify` the TX characteristic with the new `state` JSON.
- **HTTP:** Use `ESPAsyncWebServer` (or IDF HTTP server). Implement the endpoints and set CORS. For SSE, use `AsyncEventSource` at `/api/v1/events` and `events.send(json, "state")` when state changes.
- **Display Update:** On receiving `state`, update names, colors, scores, serve dot, set/match markers on the TFT.

---

## Security Considerations

- BLE pairing/bonding optional; for events, simple proximity controls usually suffice. Consider a **PIN gate** in JSON messages.
- For HTTP, consider a short **Bearer token** (e.g., `Authorization: Bearer <token>`). The PWA can store it in localStorage.

---

## License

MIT (do what you like, no warranty).
