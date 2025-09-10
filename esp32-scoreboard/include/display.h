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
};

class DisplayRenderer {
public:
  virtual ~DisplayRenderer() {}
  virtual void begin() {}
  virtual void render(const ScoreboardState& s) {
    Serial.printf("[DISPLAY] %s(%d) %s(%d) • serve:%c • set:%d • match:%d-%d • best:%d\n",
      s.ta.c_str(), s.a, s.tb.c_str(), s.b, s.sv, s.set, s.ma, s.mb, s.bo);
  }
};

#ifdef USE_TFT_ESPI
  #include <TFT_eSPI.h>
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
    
    void render(const ScoreboardState& s) override {
      Serial.println("[DISPLAY] render() called");
      tft.fillScreen(TFT_BLACK);

      // Common
      tft.setTextFont(1);

      // Colors
      uint16_t colA = hexTo565(s.ca);
      uint16_t colB = hexTo565(s.cb);

      // Layout metrics for a clean two-column grid
      const int W = tft.width();
      const int H = tft.height();
      const int pad = 10;            // outer + gutter padding
      const int colW = (W - pad * 3) / 2; // two columns + 3 paddings
      const int colAX = pad;
      const int colBX = pad * 2 + colW;
      const int topY = 8;

      auto calcWidth = [](const String& txt, int size){ return (int)(txt.length() * 6 * size); };

      // Team names (centered in each column)
      tft.setTextSize(2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      int nameH = 8 * 2; // GLCD height (8) * size
      // A name
      int aNameW = calcWidth(s.ta, 2);
      int aNameX = colAX + (colW - aNameW) / 2;
      tft.setCursor(aNameX, topY);
      tft.print(s.ta);
      // B name
      int bNameW = calcWidth(s.tb, 2);
      int bNameX = colBX + (colW - bNameW) / 2;
      tft.setCursor(bNameX, topY);
      tft.print(s.tb);

      // Serve indicator: small yellow dot near serving team name
      int siY = topY + nameH / 2;
      if (s.sv == 'A' || s.sv == 'a') {
        tft.fillCircle(colAX + 4, siY, 3, TFT_YELLOW);
      } else {
        tft.fillCircle(colBX + 4, siY, 3, TFT_YELLOW);
      }

      // Scores (large, centered per column, in team colors)
      const int scoreSize = 5; // bigger scores
      const int scoreY = topY + nameH + 8; // spacing below names
      tft.setTextSize(scoreSize);
      // A score
      String aScore = String(s.a < 100 ? (s.a < 10 ? "0" : "") : "") + String(s.a);
      int aScoreW = calcWidth(aScore, scoreSize);
      int aScoreX = colAX + (colW - aScoreW) / 2;
      tft.setTextColor(colA, TFT_BLACK);
      tft.setCursor(aScoreX, scoreY);
      tft.print(aScore);
      // B score
      String bScore = String(s.b < 100 ? (s.b < 10 ? "0" : "") : "") + String(s.b);
      int bScoreW = calcWidth(bScore, scoreSize);
      int bScoreX = colBX + (colW - bScoreW) / 2;
      tft.setTextColor(colB, TFT_BLACK);
      tft.setCursor(bScoreX, scoreY);
      tft.print(bScore);

      // Row of per-team small stats under scores (optional: set wins)
      tft.setTextSize(2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      int statsY = scoreY + (8 * scoreSize) + 6; // below large digits
      // A small stat: sets won (ma)
      {
        String at = String("Set ") + String(s.set) + String("  W:") + String(s.ma);
        int atW = calcWidth(at, 2);
        int atX = colAX + (colW - atW) / 2;
        tft.setCursor(atX, statsY);
        tft.print(at);
      }
      // B small stat: sets won (mb)
      {
        String bt = String("Set ") + String(s.set) + String("  W:") + String(s.mb);
        int btW = calcWidth(bt, 2);
        int btX = colBX + (colW - btW) / 2;
        tft.setCursor(btX, statsY);
        tft.print(bt);
      }

      // Footer centered summary
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
