/*********************************************************************
This is an example for our nRF8001 Bluetooth Low Energy Breakout

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/products/1697

Adafruit invests time and resources providing this open source code, 
please support Adafruit and open-source hardware by purchasing 
products from Adafruit!

Written by Kevin Townsend/KTOWN  for Adafruit Industries.
MIT license, check LICENSE for more information
All text above, and the splash screen below must be included in any redistribution
*********************************************************************/

// This version uses call-backs on the event and RX so there's no data handling in the main loop!

#include <SPI.h>
#include "Adafruit_BLE_UART.h"

// Bluetooth Module
#define ADAFRUITBLE_SCK           13
#define ADAFRUITBLE_MISO          12
#define ADAFRUITBLE_MOSI          11
#define ADAFRUITBLE_REQ           10
#define ADAFRUITBLE_RDY           2
#define ADAFRUITBLE_RST           20
#define ADAFRUITBLE_ACT           -1 // unused

Adafruit_BLE_UART uart = Adafruit_BLE_UART(ADAFRUITBLE_REQ, ADAFRUITBLE_RDY, ADAFRUITBLE_RST);

// WS2801 LEDs
#define NUMPIXELS                 256
#define WIDTH                     16
#define HEIGHT                    16

#include "Adafruit_WS2801.h"
#include "SPI.h"
#define DATA_PIN                  17 
#define CLOCK_PIN                 16

Adafruit_WS2801 pixel = Adafruit_WS2801(NUMPIXELS, DATA_PIN, CLOCK_PIN);

/*===========================================================================
 * Generic Panel Implementation
 *===========================================================================*/
struct Panel {
  uint8_t led[16][16] = {
    {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15},
    {31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16},
    {32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47},
//                           XXX XXX XXX
//                           XXX XXX XXX
    {63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48},
    {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79},
    {95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80},
    {96, 97, 98, 99, 100,101,102,103,104,105,106,107,108,109,110,111},
    {127,126,125,124,123,122,121,120,119,118,117,116,115,114,113,112},
    {128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143},
    {159,158,157,156,155,154,153,152,151,150,149,148,147,146,145,144},
    {160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175},
//                           XXX XXX XXX
//                           XXX XXX XXX
    {191,190,189,188,187,186,185,184,183,182,181,180,179,178,177,176},
    {192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207},
    {223,222,221,220,219,218,217,216,215,214,213,212,211,210,209,208},
    {224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239},
//               XXX XXX XXX
//               XXX XXX XXX
    {255,254,253,252,251,250,249,248,247,246,245,244,243,242,241,240}
  };
};
struct Panel panel;

struct MissingPixels {
  uint8_t leds[18] = {
      38, 39, 40,    55, 56, 57,
      166,167,168,   183,184,185,
      227,228,229,   250,251,252
  };
};
struct MissingPixels missing;

void setPixelColor_2(uint16_t x, uint32_t color) {
  uint8_t i;
  uint8_t n=0; // the number of missing pixels we have passed...

  // dont set anything unless you are a real LED...
  for (i=0; i<sizeof(missing.leds); i++) {
    if (missing.leds[i] == x) return;
  }

  // if you get here, you are not missing... so now we need to find you...
  for (i=0; i<sizeof(missing.leds); i++) {
    if (missing.leds[i] > x) break;
    n++;
  }

  // and subtract them from the actual pixel value...
  pixel.setPixelColor(x-n, color);
}

void fastDebugPixel(uint8_t r, uint8_t g, uint8_t b)
{
  setPixelColor_2(0, pixel.Color(r,g,b));
  pixel.show(); delay(100);
}

void debugPixel(uint8_t r, uint8_t g, uint8_t b)
{
  setPixelColor_2(0, pixel.Color(r,g,b));
  pixel.show(); delay(500);
}

void clearPanel(bool doShow)
{
  for (uint16_t i=0; i<NUMPIXELS; i++)
    setPixelColor_2(i, pixel.Color(0,0,0));
  if (doShow) pixel.show();
}

/*===========================================================================
 * Global Struct Helpers & Parsers
 *===========================================================================*/
#define M_NONE           0
#define M_COLOR_PICKER   1

int g_Mode = 9999;
 
typedef struct Timer {
  uint32_t last;
  uint32_t sleep;
  uint32_t now;
} Timer;
Timer s_gTimer;

struct ColorPicker {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t w;
};
ColorPicker g_Color;
void parseColor(uint8_t *buffer, uint8_t len)
{
  if (len < 6) {
    Serial.print("Dont know color command: ");
    for (int i=0; i<len; i++) Serial.print(buffer[i]);
    Serial.println();
    return;
  }

  // First 2 bytes are: !C

  g_Color.r = buffer[2];
  g_Color.g = buffer[3];
  g_Color.b = buffer[4];

  Serial.print("COLOR: (");
  Serial.print(g_Color.r);
  Serial.print(",");
  Serial.print(g_Color.g);
  Serial.print(",");
  Serial.print(g_Color.b);
  Serial.println(")");
}

/*===========================================================================
 * Basic Functions
 *===========================================================================*/

void drawColor() {
  for (int i=0; i<NUMPIXELS; i++)
    setPixelColor_2(i, pixel.Color(g_Color.r, g_Color.g, g_Color.b));
  pixel.show();
}

/*===========================================================================
 * ACI Callback
 *===========================================================================*/
void aciCallback(aci_evt_opcode_t event)
{
  switch(event)
  {
    case ACI_EVT_DEVICE_STARTED:
      debugPixel(0,0,200);
      Serial.println(F("Advertising started"));
      break;
    case ACI_EVT_CONNECTED:
      for (int i=0; i<10; i++) {
        if (i%2 == 0) fastDebugPixel(0,0,0);
        else fastDebugPixel(200,200,200);
      }
      Serial.println(F("Connected!"));
      break;
    case ACI_EVT_DISCONNECTED:
      g_Mode = M_NONE;
      debugPixel(200,0,0);
      Serial.println(F("Disconnected or advertising timed out"));
      break;
    default:
      break;
  }
}

/*===========================================================================
 * RX Callback
 *===========================================================================*/
void rxCallback(uint8_t *buffer, uint8_t len)
{
  if (len > 1 && buffer[0] == '!') {
    if (buffer[1] == 'C') {
       g_Mode = M_COLOR_PICKER;
       parseColor(buffer, len);
    }
    // more to come...
  }
  
  else
  if (len > 0) {
    // generic UART commands...
    Serial.print("[recv] ");
    for (int i=0; i<len; i++) Serial.print(buffer[i]);
    Serial.println();
  }
}

/*===========================================================================
 * Setup
 *===========================================================================*/
void setup(void)
{ 
  Serial.begin(9600);
//  while(!Serial); // Leonardo/Micro should wait for serial init
  Serial.println(F("Adafruit Bluefruit Low Energy nRF8001 Callback Echo demo"));

  // Initialize LEDs
  Serial.println(F("Initializing the NeoPixel object"));
  pixel.begin();

  debugPixel(200,0,0);

  // BLE stuff
  uart.setRXcallback(rxCallback);
  uart.setACIcallback(aciCallback);
  uart.setDeviceName("COSTUME"); /* 7 characters max! */
  uart.begin();

  debugPixel(200,0,200);

  // Seed the random number generator
  srandom(analogRead(A3));

  debugPixel(0,0,200);
}

/*===========================================================================
 * Main Loop
 *===========================================================================*/
void loop()
{
  // Things that have to happen every loop...
  uart.pollACI();

  switch (g_Mode) {
    case M_COLOR_PICKER:
      drawColor();
      break;
    default:
      break;  
  }
}
