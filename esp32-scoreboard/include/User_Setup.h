// Local TFT_eSPI configuration for the 3.2" ESP32 display
// NOTE: Verify controller and pin mapping from your board's docs:
//   https://www.lcdwiki.com/3.2inch_ESP32-32E_Display
// If your controller/pins differ, adjust below accordingly.

// ---- Display driver ----
// Common for many 320x240 3.2" modules is ILI9341. If your page says
// ST7789, ST7735, or other, change the driver define accordingly.
#define ILI9341_DRIVER

// ---- Display resolution ----
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ---- SPI pins (per ESP32-32E LCD) ----
// LCD uses SPI on IO14/13/12 (shared with touch)
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_SCLK 14

// ---- Control pins (verify against board docs) ----
// From board docs:
// IO15 = LCD CS (low-active), IO2 = D/C, EN = reset (shared with ESP32 EN)
#define TFT_CS   15   // Chip select control pin
#define TFT_DC    2   // Data/Command control pin
#define TFT_RST  -1   // Reset tied to EN; no GPIO reset

// Optional backlight control (comment out if not used)
// IO27 controls LCD backlight (high = on)
#define TFT_BL   27
#define TFT_BACKLIGHT_ON HIGH

// ---- SPI performance ----
#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY 20000000

// ---- Touch (XPT2046 on this board) ----
// IO33 = touch CS (low-active), IO36 = touch IRQ (low when pressed)
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define SPI_TOUCH_FREQUENCY  2500000

// ---- Font / features (optional trims) ----
#define SMOOTH_FONT

// Rotation is handled in code (display.h) via setRotation().
// If your display is mirrored or upside down, adjust setRotation() in
// include/display.h accordingly (0..3).
