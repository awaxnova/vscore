import { BleTransport } from './ble.js';
import { WifiTransport } from './wifi.js';

export class Transport {
  constructor(log) {
    this.log = log || console;
    this.ble = new BleTransport(this.log);
    this.wifi = new WifiTransport(this.log);
    this.active = null;
    this.onMessage = () => {};
    this.onStatus = () => {};
  }

  async connectBle() {
    this.active = this.ble;
    this._wire(this.ble);
    await this.ble.connect();
    await this.ble.send({ type: 'hello', from: 'pwa' });
  }

  async connectWifi(baseUrl) {
    this.active = this.wifi;
    this._wire(this.wifi);
    await this.wifi.connect(baseUrl);
    await this.wifi.send({ type: 'hello', from: 'pwa' });
  }

  async send(obj) {
    if (!this.active) throw new Error('No transport');
    return this.active.send(obj);
  }

  _wire(t) {
    t.onMessage = (msg) => this.onMessage?.(msg);
    t.onStatus = (s) => this.onStatus?.(s);
  }

  status() {
    if (!this.active) return { kind: 'idle' };
    return this.active.status();
  }
}
