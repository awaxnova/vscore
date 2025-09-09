// Web Bluetooth transport using a Nordic UART-style custom service.
// Replace UUIDs if you use different ones on the ESP32.
const SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const TX_CHAR_UUID = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'; // notify (ESP32 -> browser)
const RX_CHAR_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'; // write without response (browser -> ESP32)

export class BleTransport {
  constructor(log=console) {
    this.log = log;
    this.device = null;
    this.server = null;
    this.tx = null;
    this.rx = null;
    this.onMessage = () => {};
    this.onStatus = () => {};
  }

  async connect() {
    if (!navigator.bluetooth) {
      throw new Error('Web Bluetooth not supported on this browser.');
    }
    this.onStatus({ kind: 'connecting', transport: 'ble' });
    this.device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [SERVICE_UUID] }],
      optionalServices: [SERVICE_UUID]
    });
    this.device.addEventListener('gattserverdisconnected', () => {
      this.onStatus({ kind: 'disconnected', transport: 'ble' });
    });
    this.server = await this.device.gatt.connect();
    const svc = await this.server.getPrimaryService(SERVICE_UUID);
    this.tx = await svc.getCharacteristic(TX_CHAR_UUID);
    this.rx = await svc.getCharacteristic(RX_CHAR_UUID);
    await this.tx.startNotifications();
    this.tx.addEventListener('characteristicvaluechanged', (e) => {
      const v = e.target.value;
      const str = new TextDecoder().decode(v);
      try { this.onMessage(JSON.parse(str)); }
      catch { this.onMessage({ type: 'text', data: str }); }
    });
    this.onStatus({ kind: 'connected', transport: 'ble', name: this.device.name || 'BLE device' });
  }

  status() {
    const connected = !!(this.device && this.device.gatt && this.device.gatt.connected);
    return { kind: connected ? 'connected' : 'disconnected', transport: 'ble', name: this.device?.name };
  }

  async send(obj) {
    if (!this.rx) throw new Error('Not connected');
    const payload = new TextEncoder().encode(JSON.stringify(obj));
    // Split into 180B chunks to be safe with MTU limits.
    const chunkSize = 180;
    for (let i = 0; i < payload.byteLength; i += chunkSize) {
      const slice = payload.slice(i, i + chunkSize);
      await this.rx.writeValueWithoutResponse(slice);
    }
  }
}
