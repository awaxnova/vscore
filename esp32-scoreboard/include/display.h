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
    }
    void render(const ScoreboardState& s) override {
      Serial.println("[DISPLAY] render() called");
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(6, 6);
      tft.setTextFont(1);
      tft.setTextSize(2);
      tft.printf("%s  %s\n", s.ta.c_str(), s.tb.c_str());
      tft.setTextSize(4);
      tft.setCursor(6, 40);
      tft.printf("%02d        %02d\n", s.a, s.b);
      tft.setTextSize(2);
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
