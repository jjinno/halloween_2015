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

// Fan Control
#define MOSFET_PIN                3

// Microphone / SPL Meter
#define SPARKFUN_VOL              A4
#define SPARKFUN_GATE             A5

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

void setPixelColor_1(uint16_t x, uint32_t color) {
  pixel.setPixelColor(x, color);
}

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
#define MODE_NONE           0
#define MODE_COLOR_PICKER   1
#define MODE_CONTROLLER     2
#define MODE_ENVELOPE       3
#define MODE_VRAINBOW       4
#define MODE_HRAINBOW       5
#define MODE_SNAKE          6
#define MODE_DJ             7
#define MODE_STATIC         8
#define MODE_DRWHO          9

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

struct DpadButton {
  int  value;
  bool pressed;
};
DpadButton g_Button;
struct DpadController {
  struct DpadButton b1;
  struct DpadButton b2;
  struct DpadButton b3;
  struct DpadButton b4;

  struct DpadButton up;
  struct DpadButton down;
  struct DpadButton left;
  struct DpadButton right;
};
DpadController g_Controller;

void parseButtons(uint8_t *buffer, uint8_t len) {
  if (len < 5) {
    Serial.print("Dont know button command: ");
    Serial.print("[");
    Serial.print(len);
    Serial.print("] = ");
    for (int i=0; i<len; i++) Serial.print(buffer[i]);
    Serial.println();
    return;
  }

  // First 2 bytes are: !B

  g_Button.value = (buffer[2] - 48);
  g_Button.pressed = (buffer[3] == '1') ? true : false;

  switch (g_Button.value) {
    case 1: g_Controller.b1.pressed = g_Button.pressed; break;
    case 2: g_Controller.b2.pressed = g_Button.pressed; break;
    case 3: g_Controller.b3.pressed = g_Button.pressed; break;
    case 4: g_Controller.b4.pressed = g_Button.pressed; break;
    case 5: g_Controller.up.pressed = g_Button.pressed; break;
    case 6: g_Controller.down.pressed = g_Button.pressed; break;
    case 7: g_Controller.left.pressed = g_Button.pressed; break;
    case 8: g_Controller.right.pressed = g_Button.pressed; break;
  }

  Serial.print("BUTTON: (");
  Serial.print(g_Button.value);
  Serial.print(",");
  Serial.print((g_Button.pressed) ? "isDown" : "isUp");
  Serial.println(")");
}

struct VolumeMeter {
  uint8_t volume;
};
VolumeMeter g_VolumeMeter;
void readVolume() {
  g_VolumeMeter.volume = analogRead(SPARKFUN_VOL);
}

struct FanState {
  bool isOn;
};
FanState g_FanState;
void toggleFan() {
  if (g_FanState.isOn == true) {
    g_FanState.isOn = false;
    digitalWrite(MOSFET_PIN, LOW);
  }
  else {
    g_FanState.isOn = true;
    digitalWrite(MOSFET_PIN, HIGH);
  }
}

/*===========================================================================
 * Basic Functions
 *===========================================================================*/

void drawColor() {
  for (int i=0; i<NUMPIXELS; i++)
    setPixelColor_2(i, pixel.Color(g_Color.r, g_Color.g, g_Color.b));
  pixel.show();
}

struct Tardis {
  uint8_t col;
  uint8_t delay;
};
Tardis s_Tardis; 
void drawTardis()
{
  uint8_t r,c, cb, ca, cbb, caa;

// Generic sleep-until routine...
  s_gTimer.now = millis();
  if (s_gTimer.now - s_gTimer.last >= s_gTimer.sleep) {
    s_gTimer.last = s_gTimer.now;
  }
  else return;
  
  s_Tardis.col++;
  if (s_Tardis.col == 16) s_Tardis.col = 0;

  clearPanel(false);
  for (r=0; r<16; r++) {
    for (cbb=0; cbb<16; cbb++) {
      cb = (cbb+1)%16;
      c  = (cb+1)%16;
      ca = (c+1)%16;
      caa =(ca+1)%16;
      
      if (c == s_Tardis.col) {
        setPixelColor_2( panel.led[cbb][r], pixel.Color(0,0,10) );
        setPixelColor_2( panel.led[cb][r], pixel.Color(0,0,20) );
        setPixelColor_2( panel.led[c][r], pixel.Color(0,0,50) );
        setPixelColor_2( panel.led[ca][r], pixel.Color(0,0,100) );
        setPixelColor_2( panel.led[caa][r], pixel.Color(0,0,200) );
      }
      
    }
  }
  pixel.show();
  delay(s_Tardis.delay);
}

void drawVolume(uint8_t column, uint8_t width)
{
  int c,r,k;
  uint8_t v;
  uint32_t color;

  // cant see anything if you dont clear screen first...
  clearPanel(true);

  v = (g_VolumeMeter.volume / 16);
  Serial.print("VOLUME: ");
  Serial.println(v);
  
  for (r=15; r>=0; r--) {
    for (c=0; c<16; c++) {
      if ((c >= column) && (c < column+width)) {
        k=16-r;
        
        // volume is 0-255, but needs to be converted to 0-15
        if (k <= v) {
          
          if (k <= 10)      color=pixel.Color(0,100,0);
          else if (k <= 14) color=pixel.Color(100,50,0);
          else              color=pixel.Color(100,0,0); 
          
          setPixelColor_2( panel.led[c][r], color);
        }
      }
    }
  }
  pixel.show();
}

void drawStatic()
{
  int i, j;

  // Generic sleep-until routine...
  s_gTimer.now = millis();
  if (s_gTimer.now - s_gTimer.last >= s_gTimer.sleep) {
    s_gTimer.last = s_gTimer.now;
  }
  else return;
  
  for (i=0; i<16; i++) {
    for (j=0; j<16; j++) {
      switch (random(15)) {
        case 0:
          g_Color.r = random(200);
          g_Color.g = random(200);
          g_Color.b = random(200);
          break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
          g_Color.r = 0;
          g_Color.g = 0;
          g_Color.b = 0;
          break;
        default:
          g_Color.r = random(25);
          g_Color.g = random(25);
          g_Color.b = random(25);
          break;
      }
      setPixelColor_2(panel.led[i][j], pixel.Color(g_Color.r, g_Color.g, g_Color.b));
    }
  }
  pixel.show();
}

/*===========================================================================
 * Struct-based Functions
 *===========================================================================*/
uint32_t RainbowWheel(byte WheelPos)
{
  if (WheelPos < 85) {
   return pixel.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if (WheelPos < 170) {
   WheelPos -= 85;
   return pixel.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170; 
   return pixel.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

struct PanelRainbow {
  int offset;
};
struct PanelRainbow s_rainbow;

void drawRainbowPanel(bool vertical)
{
  uint8_t r,c;
  int n=0;
  
  // Generic sleep-until routine...
  s_gTimer.now = millis();
  if (s_gTimer.now - s_gTimer.last >= s_gTimer.sleep) {
    s_gTimer.last = s_gTimer.now;
  }
  else return;

  // Set all of one row to the same color...
  for (r=0; r<16; r++) {
    for (c=0; c<16; c++) {
      if (vertical)
        setPixelColor_2(panel.led[r][c], RainbowWheel(n+s_rainbow.offset));
      else
        setPixelColor_2(panel.led[c][r], RainbowWheel(n+s_rainbow.offset));
    }
    n+=16;
  }

  // Make sure we keep track of where we are globally...
  s_rainbow.offset += 16;
  if (s_rainbow.offset == 256) s_rainbow.offset=0;

  // Show the results
  pixel.show();
}

/*===========================================================================
 * DJ Modes
 *===========================================================================*/
void drawFX_fizzle()
{
  int c=255, t=50;
  int i=0, j=0, k=0;

  // Generic sleep-until routine...
  s_gTimer.now = millis();
  if (s_gTimer.now - s_gTimer.last >= 100) {
    s_gTimer.last = s_gTimer.now;
  }
  else return;

  Serial.println(__FUNCTION__);
  
  while (c > 50) {
    for (i=0; i<(WIDTH * HEIGHT); i++) {
      setPixelColor_2(i, pixel.Color(c,c,c)); 
    }
    pixel.show();
    c = c / 2;
    delay(20);
  }

  while (j < ((WIDTH * HEIGHT)/4) ) {
    for (i=0; i<(WIDTH * HEIGHT); i++) {
      setPixelColor_2(i, pixel.Color(c,c,c));
    }
    for (i=0; i<j; i++) {
      for (k=0; k<20; k++) {
        setPixelColor_2(random(WIDTH * HEIGHT), pixel.Color(0,0,0));
      }
    }
    pixel.show();
    delay(t);
    t=t/2;
    j++;
  }
  
  clearPanel(true);
}

void drawFX_bars(int n)
{
  int x=0, y=0;
  uint32_t c = pixel.Color(random(100), random(100), random(100));

  // Generic sleep-until routine...
  s_gTimer.now = millis();
  if (s_gTimer.now - s_gTimer.last >= 100) {
    s_gTimer.last = s_gTimer.now;
  }
  else return;

  Serial.println(__FUNCTION__);

  if (n < 0 || n >= 4) n = random(4);
  for (y=0; y<16; y++) {
    for (x=0; x<16; x++) {
      switch (n) {
        case 0:
          if ((x/4)%2 == 1)
            setPixelColor_2(panel.led[x][y], c);
          else
            setPixelColor_2(panel.led[x][y], 0);
          break;
          
        case 1:
          if ((x/4)%2 == 0)
            setPixelColor_2(panel.led[x][y], c);
          else
            setPixelColor_2(panel.led[x][y], 0);
          break;

        case 2:
          if ((y/4)%2 == 1)
            setPixelColor_2(panel.led[x][y], c);
          else
            setPixelColor_2(panel.led[x][y], 0);
          break;

        case 3:
          if ((y/4)%2 == 0)
            setPixelColor_2(panel.led[x][y], c);
          else
            setPixelColor_2(panel.led[x][y], 0);
          break;
      }
    }
  }
  
  pixel.show();
}

void drawFX_linesHorizontal()
{
  uint8_t y,x;
  uint32_t c = pixel.Color(random(100), random(100), random(100));

  // Generic sleep-until routine...
  s_gTimer.now = millis();
  if (s_gTimer.now - s_gTimer.last >= 100) {
    s_gTimer.last = s_gTimer.now;
  }
  else return;

  Serial.println(__FUNCTION__);

  for (int i=0; i<16; i++) {
    for (y=0; y<16; y++) {
      for (x=0; x<=i; x++) {
        if (y & 0x01) setPixelColor_2(panel.led[16-x][y], c);
        else          setPixelColor_2(panel.led[x][y], c);
      }
    }
    pixel.show();
    delay(5);
  }

  for (int i=0; i<16; i++) {
    for (y=0; y<16; y++) {
      for (x=0; x<=i; x++) {
        if (y & 0x01) setPixelColor_2(panel.led[16-x][y], 0);
        else          setPixelColor_2(panel.led[x][y], 0);
      }
    }
    pixel.show();
    delay(5);
  }
  
  clearPanel(true);
}

void drawFX_linesVertical()
{
  uint8_t y,x;
  uint32_t c = pixel.Color(random(100), random(100), random(100));

  // Generic sleep-until routine...
  s_gTimer.now = millis();
  if (s_gTimer.now - s_gTimer.last >= 100) {
    s_gTimer.last = s_gTimer.now;
  }
  else return;

  Serial.println(__FUNCTION__);

  for (int i=0; i<16; i++) {
    for (x=0; x<16; x++) {
      for (y=0; y<=i; y++) {
        if (x & 0x01) setPixelColor_2(panel.led[x][16-y], c);
        else          setPixelColor_2(panel.led[x][y], c);
      }
    }
    pixel.show();
    delay(5);
  }

  for (int i=0; i<16; i++) {
    for (x=0; x<16; x++) {
      for (y=0; y<=i; y++) {
        if (x & 0x01) setPixelColor_2(panel.led[x][16-y], 0);
        else          setPixelColor_2(panel.led[x][y], 0);
      }
    }
    pixel.show();
    delay(5);
  }
  
  clearPanel(true);
}

void drawFX_checkers(uint8_t x, uint8_t y)
{
  uint8_t i,j;
  uint32_t c = pixel.Color(random(100), random(100), random(100));

  Serial.println(__FUNCTION__);

  // Generic sleep-until routine...
  s_gTimer.now = millis();
  if (s_gTimer.now - s_gTimer.last >= 100) {
    s_gTimer.last = s_gTimer.now;
  }
  else return;

  if (x > 1) x = random(2);
  if (y > 1) y = random(2);

  for (i=0; i<16; i++) {
    for (j=0; j<16; j++) {
      if ( ((i/4)%2) == x) {
        if ( ((j/4)%2) == y)
          setPixelColor_2(panel.led[i][j], c);
        else
          setPixelColor_2(panel.led[i][j], 0);
      }
      else {
        if ( ((j/4)%2) != y)
          setPixelColor_2(panel.led[i][j], c);
        else
          setPixelColor_2(panel.led[i][j], 0);
      }
    }
  }
  
  pixel.show();
}

struct CurrentLightshow {
  uint8_t v1;
  uint8_t v2;
  uint8_t v3;
  uint8_t v4;
  uint8_t v5;
  uint8_t v6;
  uint8_t v7;
  uint8_t v8;
};
CurrentLightshow g_CurrentLightshow;
void drawLightshow()
{
  if (g_Controller.up.pressed == true) {
    g_CurrentLightshow.v5++;
    if (g_CurrentLightshow.v5 > 1) g_CurrentLightshow.v5 = 0;
    if (g_CurrentLightshow.v5 == 0) drawFX_bars(0);
    if (g_CurrentLightshow.v5 == 1) drawFX_bars(1);
  }

  if (g_Controller.right.pressed == true) {
    g_CurrentLightshow.v8++;
    if (g_CurrentLightshow.v8 > 1) g_CurrentLightshow.v8 = 0;
    if (g_CurrentLightshow.v8 == 0) drawFX_bars(2);
    if (g_CurrentLightshow.v8 == 1) drawFX_bars(3);
  }

  if (g_Controller.left.pressed == true) {
//    g_CurrentLightshow.v7++;
    drawFX_linesHorizontal();
  }

  if (g_Controller.down.pressed == true) {
//    g_CurrentLightshow.v6++;
    drawFX_linesVertical();
  }

  if (g_Controller.b1.pressed == true) {
//    g_CurrentLightshow.v1++;
    drawFX_checkers(0,1);
  }

  if (g_Controller.b2.pressed == true) {
//    g_CurrentLightshow.v2++;
    drawFX_checkers(0,0);
  }

  if (g_Controller.b3.pressed == true) {
//    g_CurrentLightshow.v3++;
    drawFX_fizzle();
  }

  if (g_Controller.b4.pressed == true) {
//    g_CurrentLightshow.v4++;
    drawStatic();
  }
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
      g_Mode = MODE_NONE;
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
#define compare(x,y) (strncmp((char *)x, (char *)y, strlen(x) ))
#define commandIs(x) (strncmp((char *)x, (char *)buffer, strlen(x) ) == 0)
void rxCallback(uint8_t *buffer, uint8_t len)
{
  if (len > 1 && buffer[0] == '!') {
    if (buffer[1] == 'C') {
      Serial.println("[MODE] change to COLOR_PICKER");
       g_Mode = MODE_COLOR_PICKER;
       parseColor(buffer, len);
    }
    else
    if (buffer[1] == 'B') {
      parseButtons(buffer, len);
      if (g_Mode != MODE_SNAKE && \
          g_Mode != MODE_DJ) {
        Serial.println("[MODE] FORCE change to NONE (temporary)");
        g_Mode = MODE_NONE;
      }
      
      if (g_Mode == MODE_NONE) {
        Serial.print("VALUE: ");
        Serial.print(g_Button.value);
        Serial.print("PRESSED: ");
        Serial.print(g_Button.pressed);
        
        if (g_Button.value == 1 && g_Button.pressed == true) {
          Serial.println("[MODE] change to SNAKE");
          g_Mode = MODE_SNAKE;
        }
        else
        if (g_Button.value == 2 && g_Button.pressed == true) {
          Serial.println("[MODE] change to DJ");
          g_Mode = MODE_DJ;
        }
      }
    }
    // more to come...
  }
  
  else
  if (len > 0) {
    // generic UART commands...
    Serial.print("[recv] ");
    for (int i=0; i<len; i++) Serial.print(buffer[i]);
    Serial.println();

    if (compare("mic", buffer) == 0) {
      Serial.println("[MODE] change to ENVELOPE");
      g_Mode = MODE_ENVELOPE;
    }
    else 
    if (compare("off", buffer) == 0) {
      Serial.println("[MODE] change to NONE");
      clearPanel(true);
      g_Mode = MODE_NONE;
    }
    else
    if (compare("fan", buffer) == 0) {
      toggleFan();
    }
    else
    if (commandIs("vbow")) {
      g_Mode = MODE_VRAINBOW;
    }
    else
    if (commandIs("hbow")) {
      g_Mode = MODE_HRAINBOW;
    }
    else 
    if (commandIs("static")) {
      g_Mode = MODE_STATIC;
    }
    else 
    if (commandIs("who")) {
      g_Mode = MODE_DRWHO;
    }
    else
    if (commandIs("+")) {
      if ((s_gTimer.sleep - 10) > 10) s_gTimer.sleep -= 10;
      else s_gTimer.sleep == 10;
    }
    else
    if (commandIs("=")) {
      if ((s_gTimer.sleep + 10) < 200) s_gTimer.sleep += 10;
      else s_gTimer.sleep == 200;
    }
    else if (commandIs("a")) drawFX_bars(0);
    else if (commandIs("b")) drawFX_bars(1);
    else if (commandIs("c")) drawFX_bars(2);
    else if (commandIs("d")) drawFX_bars(3);
    else if (commandIs("e")) drawFX_linesHorizontal();
    else if (commandIs("f")) drawFX_linesVertical();
    else if (commandIs("g")) drawFX_fizzle();
    else if (commandIs("h")) drawFX_checkers(0,0);
    else if (commandIs("i")) drawFX_checkers(0,1);
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

  // Fan Control Stuff
  pinMode(MOSFET_PIN, OUTPUT);

  // Microphone Input
  pinMode(SPARKFUN_VOL, INPUT);
  pinMode(SPARKFUN_GATE, INPUT);
  
  // Seed the random number generator
  srandom(analogRead(A3));

  // Timer
  s_gTimer.last = 0;
  s_gTimer.now = 0;
  s_gTimer.sleep = 100;

  debugPixel(0,0,200);
  
}

/*===========================================================================
 * Main Loop
 *===========================================================================*/
void loop()
{
  // Things that have to happen every loop...
  readVolume();
  uart.pollACI();

  // Things that DONT happen every loop...
  switch (g_Mode) {
    case MODE_COLOR_PICKER:
      drawColor();
      break;
    case MODE_ENVELOPE:
      drawVolume(0,16);
      break;
    case MODE_VRAINBOW:
      drawRainbowPanel(true);
      break;
    case MODE_HRAINBOW:
      drawRainbowPanel(false);
      break;
    case MODE_DJ:
      drawLightshow();
      break;
    case MODE_STATIC:
      drawStatic();
      break;
    case MODE_DRWHO:
      drawTardis();
      break;
    default:
      break;  
  }
}
