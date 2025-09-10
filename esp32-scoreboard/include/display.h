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
      // Header row: team names (white)
      tft.setCursor(6, 6);
      tft.setTextFont(1);
      tft.setTextSize(2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.printf("%s  %s\n", s.ta.c_str(), s.tb.c_str());

      // Team scores in team colors
      uint16_t colA = hexTo565(s.ca);
      uint16_t colB = hexTo565(s.cb);
      int margin = 20;
      int digitW = 6 * 4; // GLCD font width (6) * size (4)
      int scoreWidth = digitW * 2; // two digits
      int leftX = 6;
      int rightX = tft.width() - margin - scoreWidth;
      int scoreY = 40;

      tft.setTextSize(4);
      tft.setTextColor(colA, TFT_BLACK);
      tft.setCursor(leftX, scoreY);
      tft.printf("%02d", s.a);

      tft.setTextColor(colB, TFT_BLACK);
      tft.setCursor(rightX, scoreY);
      tft.printf("%02d", s.b);

      // Footer details (white)
      tft.setTextSize(2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setCursor(6, 110);
      tft.printf("Serve: %c  Set: %d  Match:%d-%d  Bo%d",
        s.sv, s.set, s.ma, s.mb, s.bo);
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
