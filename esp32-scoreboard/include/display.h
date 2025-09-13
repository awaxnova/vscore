#pragma once
#include <Arduino.h>

// Very lightweight rendering hook. By default we just log to Serial.
// If you want on-device graphics, define USE_TFT_ESPI in platformio.ini
// and provide a configured TFT_eSPI setup for your ST7789 display.

struct ScoreboardState {
  String ta = "Team A";
  String tb = "Team B";
  String ca = "#42a5f5";
  String cb = "#ef5350";
  int a = 0;
  int b = 0;
  char sv = 'A'; // 'A' or 'B'
  int set = 1;
  int ma = 0;
  int mb = 0;
  int bo = 3;
  bool ble = false; // BLE connected status
  // Extended styling and rotations
  String abg = "#0c1220"; // team A panel background
  String bbg = "#0c1220"; // team B panel background
  String ra[6];
  String rb[6];
  uint8_t rsa = 0; // current server slot 0..5
  uint8_t rsb = 0; // current server slot 0..5
  struct LogEntry { String reason; String scorer; uint32_t ts = 0; };
  LogEntry la[4]; uint8_t laCount = 0; // last 4 for Team A
  LogEntry lb[4]; uint8_t lbCount = 0; // last 4 for Team B
};

class DisplayRenderer {
public:
  virtual ~DisplayRenderer() {}
  virtual void begin() {}
  virtual void loop() {}
  virtual void render(const ScoreboardState& s) {
    Serial.printf("[DISPLAY] %s(%d) %s(%d) • serve:%c • set:%d • match:%d-%d • best:%d\n",
      s.ta.c_str(), s.a, s.tb.c_str(), s.b, s.sv, s.set, s.ma, s.mb, s.bo);
  }
};

#ifdef USE_TFT_ESPI
  #include <TFT_eSPI.h>
  #include <WiFi.h>
  #include <qrcode.h>
  // Convert "#RRGGBB" or "RRGGBB" to RGB565. Falls back to white on parse error.
  inline uint16_t hexTo565(const String& hex) {
    auto isHex = [](char c){ return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'); };
    int i = 0;
    if (hex.length() >= 1 && (hex[0]=='#')) i = 1;
    if (hex.length() - i < 6) return 0xFFFF;
    char buf[7];
    for (int k=0; k<6; ++k) {
      char c = hex[i+k];
      if (!isHex(c)) return 0xFFFF;
      buf[k] = c;
    }
    buf[6] = 0;
    uint32_t rgb = strtoul(buf, nullptr, 16);
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = (rgb) & 0xFF;
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }
  class TftRenderer : public DisplayRenderer {
    TFT_eSPI tft;
    bool showQR = false;
    bool touchPrev = false;
    uint32_t lastToggleMs = 0;
    ScoreboardState lastState;
  public:
    void begin() override {
      Serial.println("[DISPLAY] Using TFT_eSPI renderer");
#ifdef TFT_BL
      pinMode(TFT_BL, OUTPUT);
#ifdef TFT_BACKLIGHT_ON
      digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#else
      digitalWrite(TFT_BL, HIGH);
#endif
#endif
      tft.init();
      tft.setRotation(1); // adjust as needed
      // Quick test pattern to verify panel output
      tft.fillScreen(TFT_RED);   delay(150);
      tft.fillScreen(TFT_GREEN); delay(150);
      tft.fillScreen(TFT_BLUE);  delay(150);
      tft.fillScreen(TFT_BLACK);
      // Use GLCD font (1) so setTextSize scaling works
      tft.setTextFont(1);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setCursor(6, 6);
      tft.print("TFT init OK");
      delay(500);
    }
    void loop() override {
      uint16_t tx, ty;
      bool pressed = tft.getTouch(&tx, &ty);
      uint32_t now = millis();
      if (pressed && !touchPrev && (now - lastToggleMs > 400)) {
        showQR = !showQR;
        lastToggleMs = now;
        if (showQR) drawQR(lastState); else drawScoreboard(lastState);
      }
      touchPrev = pressed;
    }

    void render(const ScoreboardState& s) override {
      Serial.println("[DISPLAY] render() called");
      lastState = s;
      if (showQR) drawQR(lastState); else drawScoreboard(lastState);
    }

  private:
    void drawScoreboard(const ScoreboardState& s) {
      tft.fillScreen(TFT_BLACK);
      tft.setTextFont(1);
      uint16_t colA = hexTo565(s.ca);
      uint16_t colB = hexTo565(s.cb);
      uint16_t bgA = hexTo565(s.abg);
      uint16_t bgB = hexTo565(s.bbg);
      const int W = tft.width();
      const int H = tft.height();
      const int pad = 10;
      const int colW = (W - pad * 3) / 2;
      const int colAX = pad;
      const int colBX = pad * 2 + colW;
      const int topY = 8;
      // Panel backgrounds
      tft.fillRoundRect(colAX - 2, 2, colW + 4, H - 30, 8, bgA);
      tft.fillRoundRect(colBX - 2, 2, colW + 4, H - 30, 8, bgB);
      auto calcWidth = [](const String& txt, int size){ return (int)(txt.length() * 6 * size); };
      tft.setTextSize(2);
      tft.setTextColor(TFT_WHITE, bgA);
      int nameH = 8 * 2;
      int aNameW = calcWidth(s.ta, 2);
      int aNameX = colAX + (colW - aNameW) / 2;
      tft.setCursor(aNameX, topY);
      tft.print(s.ta);
      int bNameW = calcWidth(s.tb, 2);
      int bNameX = colBX + (colW - bNameW) / 2;
      tft.setTextColor(TFT_WHITE, bgB);
      tft.setCursor(bNameX, topY);
      tft.print(s.tb);
      int siY = topY + nameH / 2;
      tft.fillCircle((s.sv=='A'||s.sv=='a') ? (colAX+4) : (colBX+4), siY, 3, TFT_YELLOW);
      const int scoreSize = 5;
      const int scoreY = topY + nameH + 8;
      tft.setTextSize(scoreSize);
      String aScore = String(s.a < 100 ? (s.a < 10 ? "0" : "") : "") + String(s.a);
      int aScoreW = calcWidth(aScore, scoreSize);
      int aScoreX = colAX + (colW - aScoreW) / 2;
      tft.setTextColor(colA, bgA);
      tft.setCursor(aScoreX, scoreY);
      tft.print(aScore);
      String bScore = String(s.b < 100 ? (s.b < 10 ? "0" : "") : "") + String(s.b);
      int bScoreW = calcWidth(bScore, scoreSize);
      int bScoreX = colBX + (colW - bScoreW) / 2;
      tft.setTextColor(colB, bgB);
      tft.setCursor(bScoreX, scoreY);
      tft.print(bScore);
      tft.setTextSize(2);
      tft.setTextColor(TFT_WHITE, bgA);
      int statsY = scoreY + (8 * scoreSize) + 6;
      String at = String("Set ") + String(s.set) + String("  W:") + String(s.ma);
      int atW = calcWidth(at, 2);
      int atX = colAX + (colW - atW) / 2;
      tft.setCursor(atX, statsY);
      tft.print(at);
      tft.setTextColor(TFT_WHITE, bgB);
      String bt = String("Set ") + String(s.set) + String("  W:") + String(s.mb);
      int btW = calcWidth(bt, 2);
      int btX = colBX + (colW - btW) / 2;
      tft.setCursor(btX, statsY);
      tft.print(bt);

      // Serving indicators (small triangle at top of serving column)
      uint16_t serveCol = TFT_YELLOW;
      int triY = topY + nameH + 6;
      if (s.sv == 'A') {
        tft.fillTriangle(colAX + 6, triY, colAX + 26, triY, colAX + 16, triY + 12, serveCol);
      } else {
        tft.fillTriangle(colBX + colW - 26, triY, colBX + colW - 6, triY, colBX + colW - 16, triY + 12, serveCol);
      }

      // Rotations: draw 2x3 grid per team, inner divider as the net.
      // Left team: position #1 at lower-left; count upward counter-clockwise (around perimeter).
      // Right team: position #1 at upper-right; count clockwise (around perimeter).
      const int gapX = 6;
      const int gapY = 6;
      const int cw = (colW - 20 - gapX) / 2; // two columns
      const int ch = 24;                      // three rows
      auto drawRotation = [&](int x, int y, const String rs[6], uint8_t cur, uint16_t bg, bool isLeft){
        tft.setTextSize(2);
        tft.setTextColor(TFT_WHITE, bg);
        // Mapping from rotation index (0..5) to (col,row)
        // Left side (isLeft=true): 1(lower-left)->2(mid-left)->3(top-left)->4(top-right)->5(mid-right)->6(lower-right)
        // Right side (isLeft=false): 1(upper-right)->2(mid-right)->3(lower-right)->4(lower-left)->5(mid-left)->6(upper-left)
        auto mapIndex = [&](int i, int &c, int &r){
          if (isLeft) {
            switch (i) { // CCW from bottom-left
              case 0: c=0; r=2; break; // #1
              case 1: c=0; r=1; break; // #2
              case 2: c=0; r=0; break; // #3
              case 3: c=1; r=0; break; // #4
              case 4: c=1; r=1; break; // #5
              default: c=1; r=2; break; // #6
            }
          } else {
            switch (i) { // CW from top-right
              case 0: c=1; r=0; break; // #1
              case 1: c=1; r=1; break; // #2
              case 2: c=1; r=2; break; // #3
              case 3: c=0; r=2; break; // #4
              case 4: c=0; r=1; break; // #5
              default: c=0; r=0; break; // #6
            }
          }
        };
        for (int i=0;i<6;i++) {
          int c, r; mapIndex(i, c, r);
          int gx = x + c * (cw + gapX);
          int gy = y + r * (ch + gapY);
          uint16_t frame = (i == (int)cur) ? TFT_YELLOW : TFT_DARKGREY;
          tft.drawRoundRect(gx, gy, cw, ch, 4, frame);
          String txt = rs[i];
          int tw = calcWidth(txt, 2);
          int tx = gx + (cw - tw) / 2;
          int ty = gy + 4;
          tft.setCursor(tx, ty);
          tft.print(txt);
        }
      };
      int rotTop = statsY + 26;
      drawRotation(colAX + 10, rotTop, s.ra, s.rsa, bgA, true);
      drawRotation(colBX + 10, rotTop, s.rb, s.rsb, bgB, false);

      // Draw the net between teams across the rotation area
      {
        int netX = colAX + colW + ( (pad) / 2 );
        int netY1 = rotTop - 4;
        int netY2 = rotTop + (3 * ch + 2 * gapY) + 8;
        for (int dy = netY1; dy <= netY2; dy += 4) {
          tft.drawFastVLine(netX, dy, 2, TFT_LIGHTGREY);
        }
      }

      // Logs: last 4 entries for each team, small font, near bottom of column
      auto drawLogs = [&](int x, int y, const ScoreboardState::LogEntry* items, uint8_t n, uint16_t bg){
        tft.setTextSize(1);
        tft.setTextColor(TFT_LIGHTGREY, bg);
        int lineH = 10;
        // Draw from oldest to newest among the kept entries
        for (int i=0; i<n; i++) {
          const auto &e = items[i];
          // format HH:MM from ts (ms)
          uint32_t sec = (e.ts / 1000UL) % 86400UL;
          uint32_t hh = sec / 3600UL;
          uint32_t mm = (sec % 3600UL) / 60UL;
          char when[6];
          snprintf(when, sizeof(when), "%02lu:%02lu", (unsigned long)hh, (unsigned long)mm);
          String line = String(when) + String(" ") + e.reason;
          if (e.scorer.length()) line += String(" #") + e.scorer;
          tft.setCursor(x, y + i*lineH);
          tft.print(line);
        }
      };

      int logsTop = H - 60; // leave space for footer
      drawLogs(colAX + 8, logsTop, s.la, s.laCount, bgA);
      drawLogs(colBX + 8, logsTop, s.lb, s.lbCount, bgB);
      String footer = String("Match ") + String(s.ma) + String("-") + String(s.mb) + String("  Bo") + String(s.bo);
      int fSize = 2;
      int fW = calcWidth(footer, fSize);
      int fX = (W - fW) / 2;
      int fY = H - (8 * fSize) - 6;
      tft.setTextSize(fSize);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setCursor(fX, fY);
      tft.print(footer);
    }

    void drawQrAt(int x, int y, int maxSize, const String& payload) {
      // Pick a conservative QR version to ensure payload fits
      const uint8_t version = 6; // 41x41 modules
      // Allocate a buffer large enough for up to version 10 (57x57 -> ~406 bytes)
      static uint8_t qrData[600];
      QRCode qr;
      qrcode_initText(&qr, qrData, version, ECC_MEDIUM, payload.c_str());
      int size = qr.size;
      int scale = max(1, maxSize / (size + 4)); // quiet zone of 2 modules each side
      int qW = (size + 4) * scale;
      tft.fillRect(x, y, qW, qW, TFT_WHITE);
      int ox = x + 2 * scale;
      int oy = y + 2 * scale;
      for (int iy = 0; iy < size; iy++) {
        for (int ix = 0; ix < size; ix++) {
          if (qrcode_getModule(&qr, ix, iy)) {
            tft.fillRect(ox + ix * scale, oy + iy * scale, scale, scale, TFT_BLACK);
          }
        }
      }
    }

    void drawQR(const ScoreboardState& s) {
      tft.fillScreen(TFT_BLACK);
      tft.setTextFont(1);
      tft.setTextSize(2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      const int W = tft.width();
      const int H = tft.height();
      const int pad = 10;
      const int titleY = 6;
      tft.setCursor(pad, titleY);
      tft.print("Connect");

      // Build controller URL with safe fallback when GH_PAGES_ORIGIN is "*"
      String origin = String(GH_PAGES_ORIGIN);
      if (origin == "*") origin = "https://awaxnova.github.io"; // fallback default
      String controller = origin + String("/vscore/");
      String wifi = String("WIFI:T:WPA;S:") + String(SOFTAP_SSID) + String(";P:") + String(SOFTAP_PASS) + String(";;");

      if (s.ble) {
        int maxQ = min(W, H) - 2 * pad - 20;
        int qX = (W - maxQ) / 2;
        int qY = (H - maxQ) / 2;
        drawQrAt(qX, qY, maxQ, wifi);
        tft.setTextSize(2);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setCursor(pad, H - 28);
        tft.print("Join WiFi: ");
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.printf("%s / %s", SOFTAP_SSID, SOFTAP_PASS);
      } else {
        const int colW = (W - pad * 3) / 2;
        const int colAX = pad;
        const int colBX = pad * 2 + colW;
        const int qSize = colW;
        const int qTop = 30;
        drawQrAt(colAX, qTop, qSize, controller);
        tft.setTextSize(2);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setCursor(colAX, qTop - 22);
        tft.print("Controller (BLE)");
        drawQrAt(colBX, qTop, qSize, wifi);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setCursor(colBX, qTop - 22);
        tft.print("Join WiFi");
        // Credentials + IP footer
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(pad, H - 46);
        tft.printf("SSID:%s  PASS:%s", SOFTAP_SSID, SOFTAP_PASS);
        tft.setCursor(pad, H - 26);
        IPAddress ap = WiFi.softAPIP();
        IPAddress sta = WiFi.localIP();
        tft.printf("AP:%s  STA:%s", ap.toString().c_str(), sta.toString().c_str());
      }
      // Hint
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
      tft.setTextSize(1);
      tft.setCursor(pad, H - 10);
      tft.print("Tap to return");
    }
  };
#endif

inline DisplayRenderer* makeRenderer() {
#ifdef USE_TFT_ESPI
  static TftRenderer r;
  return &r;
#else
  static DisplayRenderer r;
  return &r;
#endif
}
