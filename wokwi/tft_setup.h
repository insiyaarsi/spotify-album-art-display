#ifndef TFT_SETUP_H
#define TFT_SETUP_H

#define USER_SETUP_INFO "Wokwi ILI9341"
#define USER_SETUP_LOADED

#define ILI9341_DRIVER

// Standard Wokwi ESP32 + ILI9341 wiring (matches diagram.json).
// RST is not simulated on wokwi-ili9341; use -1.
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4

#define SPI_FREQUENCY 40000000

#endif
