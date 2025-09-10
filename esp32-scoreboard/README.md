# CONTROLLER PAGE: 
  https://awaxnova.github.io/vscore/
# DEVICE: 
  https://www.amazon.com/dp/B0DRFZ2FP9?smid=A1PKC2PUMNR8VD&ref_=chk_typ_imgToDp&th=1
# DATASHEET: 
  https://www.lcdwiki.com/3.2inch_ESP32-32E_Display
# MANUFACTURER: 
  https://hosyond.com/

# ESP32 Scoreboard (BLE + HTTP)

Matches the PWA protocol (JSON over BLE UART-style and HTTP).

## Features
- **BLE GATT** (Nordic UART-style UUIDs) with JSON messages
- **HTTP API** with CORS, plus **SSE** (`/api/v1/events`) for live updates
- **SoftAP** enabled by default (`ESP32-SCOREBOARD` / `volley123`)
- **Display renderer stub** (Serial). Optional **TFT_eSPI** support via `-D USE_TFT_ESPI`

## Build (PlatformIO)
1. Install PlatformIO.
2. Open this folder and `pio run -t upload -e esp32dev` (or use the VSCode button).

### Libraries (auto-installed via `lib_deps`)
- NimBLE-Arduino
- ESP Async WebServer + AsyncTCP
- ArduinoJson

## Configure
- Change SoftAP SSID/pass in `platformio.ini` (`SOFTAP_SSID`, `SOFTAP_PASS`).
- Restrict CORS: set `GH_PAGES_ORIGIN` to your real Pages origin, e.g.
  ```ini
  build_flags = -D GH_PAGES_ORIGIN="\"https://yourname.github.io\""
  ```

## HTTP Endpoints
- `GET /api/v1/ping` => 200 `"pong"`
- `POST /api/v1/scoreboard` => apply state JSON; returns `{"type":"ack","data":{"ok":true}}`
- `GET /api/v1/state` => current state JSON
- `GET /api/v1/events` => SSE stream of state updates (`event: state`)

## BLE UUIDs
- Service: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- RX (write): `6e400002-b5a3-f393-e0a9-e50e24dcca9e`
- TX (notify): `6e400003-b5a3-f393-e0a9-e50e24dcca9e`

## TFT (optional)
TFT support is enabled via **TFT_eSPI**.

- Enabled: `-D USE_TFT_ESPI` and TFT_eSPI dependency are configured in `platformio.ini`.
- Local config: edit `include/User_Setup.h` with the correct controller and pin map for your ESP32-32E 3.2" board. Defaults assume ILI9341 240x320 and VSPI pins.
- If the screen is rotated, tweak `setRotation()` inside `include/display.h`.
- If your board has a backlight pin, adjust `TFT_BL` in `include/User_Setup.h`.

## Curl test
```bash
curl -X POST http://192.168.4.1/api/v1/scoreboard \
  -H "Content-Type: application/json" \
  -d '{"type":"state","data":{"ta":"Hawks","tb":"Eagles","a":5,"b":7,"sv":"B","set":1,"ma":0,"mb":0,"bo":3}}'

curl http://192.168.4.1/api/v1/state
```

## Notes
- BLE writes may be chunked; the code accumulates until valid JSON parses.
- JSON size kept small; MTU set to 247 to help notifications.


## Future Features TODO
- Add a security feature for making updates.  Maybe a code that accompanys the administration or connections, but also a whitelist of admin devices.
- Add colors for teams to the display
- Fix formatting for the stats
- Add a scrolling stat window showing more info: rotation, serve order, points per server, etc...
- Add a power switch inline with the battery using 1.2mm jst connectors.
- Add a team background color, which would be a backdrop upon which to display their score and other info.
- Show info on how to connect, like a QR code upon startup to get the phone connected using a randomized credential, 
  and then upon request (tap screen or command), a QR code that gives someone the wifi info, so they can maintain the game state/stats over wifi as well.
  Screen tap cycles through the QR codes available, including a way to reset credentials to a new QR code for BLE or WIFI.
- Handle a click on the screen to flip the screen to a different render handler... let's have this alternate handler show the connection QR codes for the BLE and the WIFI... or only WIFI if BLE is connected already.
- Handle a click on a QR code to update credentials and boot all connections.
- Hold score to clear
- Allow for set/win manipulation... for sets won, show a circle of the appropriate color fg/bg between the columns.
- When the serve moves, add an opponent point before moving it.
- When the serve stays, mark a server point and leave it in place.
- For a redo, indicate it in the log with a double thumbs up button - allow for point adjustment and strikethrough the last server's stat.
