/*********************************************************************
 This is an exampMle for our nRF51822 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

#include <Arduino.h>
#include <SPI.h>
#if not defined (_VARIANT_ARDUINO_DUE_X_) && not defined (_VARIANT_ARDUINO_ZERO_)
    #include <SoftwareSerial.h>
#endif

#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"

#include "BluefruitConfig.h"

#define NUMPIXELS                   256

//#ifdef USE_NEOPIXELS
//  #include <Adafruit_NeoPixel.h>
//  #define LED_PIN                   17
//  Adafruit_NeoPixel pixel = Adafruit_NeoPixel(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
//
//#elseif USE_WS2801
  #include "Adafruit_WS2801.h"
  #include "SPI.h"
  #define DATA_PIN                  15
  #define CLOCK_PIN                 16
  Adafruit_WS2801 pixel = Adafruit_WS2801(NUMPIXELS, DATA_PIN, CLOCK_PIN);
//#else
//  #error *** WARNING *** Must define USE_NEOPIXELS or USE_WS2801
//#endif

#define FAN_PIN                     9

#define FACTORYRESET_ENABLE         1
#define MINIMUM_FIRMWARE_VERSION    "0.6.6"
#define MODE_LED_BEHAVIOUR          "MODE"
/*=========================================================================*/

/* ...hardware SPI, using SCK/MOSI/MISO hardware SPI pins and then user selected CS/IRQ/RST */
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

/*===========================================================================
 * Generic Panel Implementation
 *===========================================================================*/
struct Panel {
  uint8_t led[16][16] = {
    {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15},
    {31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16},
    {32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47},
    {63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48},
//                           XXX XXX XXX
//                           XXX XXX XXX
    {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79},
    {95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80},
    {96, 97, 98, 99, 100,101,102,103,104,105,106,107,108,109,110,111},
    {127,126,125,124,123,122,121,120,119,118,117,116,115,114,113,112},
//               XXX XXX XXX
//               XXX XXX XXX
    {128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143},
    {159,158,157,156,155,154,153,152,151,150,149,148,147,146,145,144},
    {160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175},
    {191,190,189,188,187,186,185,184,183,182,181,180,179,178,177,176},
//                           XXX XXX XXX
//                           XXX XXX XXX
    {192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207},
    {223,222,221,220,219,218,217,216,215,214,213,212,211,210,209,208},
    {224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239},
    {255,254,253,252,251,250,249,248,247,246,245,244,243,242,241,240}
  };
};
struct Panel panel;

struct MissingPixels {
  uint8_t leds[18] = {
    55, 56, 57,    70, 71, 72,
    122,123,124,   131,132,133,
    183,184,185,   198,199,200
  };
};
struct MissingPixels missing;

//void setPixelColor_1(uint16_t x, uint32_t color) {
//  uint8_t i;
//  uint8_t n=0; // the number of missing pixels we have passed...
//
//  // count the number of missing pixels up to this point...
//  for (i=0; i<sizeof(missing.leds); i++) {
//    if (missing.leds[i] > x) break;
//    n++;
//  }
//
//  // and subtract them from the actual pixel value...
//  pixel.setPixelColor(x-n, color);
//}

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

/*===========================================================================
 * Generic Helper Functions
 *===========================================================================*/
void error(const __FlashStringHelper *err) {
  Serial.println(err);
  while (1);
}

int parseint(char *buffer) 
{
  float f = ((float *)buffer)[0];
  return (int)f;
}

void clearPanel(bool doShow)
{
  for (uint16_t i=0; i<NUMPIXELS; i++)
    pixel.setPixelColor(i, pixel.Color(0,0,0));
  if (doShow) pixel.show();
}

/*===========================================================================
 * Fan Control (pin 21; aka: FAN_PIN)
 *===========================================================================*/
struct FanControl {
  bool prev;
  bool fan;
};
FanControl s_fanControl;
 
void fanWriteState()
{
  if (s_fanControl.fan != s_fanControl.prev) {

  Serial.print("fan=");
  Serial.print(s_fanControl.fan);
  Serial.print(", prev=");
  Serial.println(s_fanControl.prev);
    
    if (s_fanControl.fan == true) digitalWrite(FAN_PIN, HIGH);
    else digitalWrite(FAN_PIN, LOW);
    s_fanControl.prev = s_fanControl.fan;
  }
}

void fanToggle()
{
  if (s_fanControl.fan == true) {
    Serial.println("<< FAN TOGGLE: OFF >>");
    s_fanControl.fan = false;
  }
  else {
    Serial.println("<< FAN TOGGLE: ON >>");
    s_fanControl.fan = true;
  }
}

void fanResetAll()
{
  s_fanControl.fan = false;
  s_fanControl.prev = false;
}

/*===========================================================================
 * SPL Sensor (10g, 4y, 2r)
 *===========================================================================*/
void drawVolume(uint8_t column, uint8_t width, uint8_t vol)
{
  int c,r,k;
  uint32_t color;

//  Serial.print("volume=");
//  Serial.println(vol);
  
  for (r=15; r>=0; r--) {
    for (c=0; c<16; c++) {      
      if ((c >= column) && (c < column+width)) {
        k=16-r;
        if (k <= vol) {
          
          if (k <= 10)      color=pixel.Color(0,100,0);
          else if (k <= 14) color=pixel.Color(100,50,0);
          else              color=pixel.Color(100,0,0); 
          
//          pixel.setPixelColor( panel.led[c][r], color);
          setPixelColor_2( panel.led[c][r], color);
        }
      }
    }
  }
  pixel.show();
}

/*===========================================================================
 * Tardis (blue chaser line)
 *===========================================================================*/
struct Tardis {
  uint8_t col;
  uint8_t delay;
};
Tardis s_Tardis; 
void drawTardis()
{
  uint8_t r,c, cb, ca, cbb, caa;
  
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
//        pixel.setPixelColor( panel.led[cbb][r], pixel.Color(0,0,10) );
//        pixel.setPixelColor( panel.led[cb][r], pixel.Color(0,0,20) );
//        pixel.setPixelColor( panel.led[c][r], pixel.Color(0,0,50) );
//        pixel.setPixelColor( panel.led[ca][r], pixel.Color(0,0,100) );
//        pixel.setPixelColor( panel.led[caa][r], pixel.Color(0,0,200) );
        setPixelColor_2( panel.led[cbb][r], pixel.Color(0,0,10) );
        setPixelColor_2( panel.led[cb][r], pixel.Color(0,0,20) );
        setPixelColor_2( panel.led[c][r], pixel.Color(0,0,30) );
        setPixelColor_2( panel.led[ca][r], pixel.Color(0,0,40) );
        setPixelColor_2( panel.led[caa][r], pixel.Color(0,0,50) );
      }
      
    }
  }
  pixel.show();
  delay(s_Tardis.delay);
}

/*===========================================================================
 * GLOW STICK ROUTINE (CRACK & SHAKE) 
 *===========================================================================*/
struct Glowstick {
  bool initialized;
  uint16_t g[16][16];
};
Glowstick s_Glowstick;
uint8_t max_a;
#define GREEN(x) pixel.Color(x > 0 ? 50 : 0,x,0)
void glowMore(uint8_t howMuch)
{
  uint8_t r,c,n,x;
  uint32_t color = pixel.Color(0,25,0);
  
  if (! s_Glowstick.initialized) {
    clearPanel(false);
    for (r=0; r<16; r++) {
      for (c=0; c<16; c++) {
        s_Glowstick.g[c][r] = 0;
      }
    }
    
    r = random(16); c = random(16);
    s_Glowstick.g[c][r] = 25;
    
//    pixel.setPixelColor(panel.led[c][r], GREEN(s_Glowstick.g[c][r]));
    setPixelColor_2(panel.led[c][r], GREEN(s_Glowstick.g[c][r]));
    pixel.show();

    s_Glowstick.initialized = true;
    return;
  }

  for (x=0; x<howMuch/3; x++) {
    for (r=0; r<16; r++) {
      for (c=0; c<16; c++) {
        if (s_Glowstick.g[c][r] != 0) {
          s_Glowstick.g[c][r] = s_Glowstick.g[c][r] * 2;
          if (s_Glowstick.g[c][r] > 255) s_Glowstick.g[c][r] = 255;
          
          n = random(4); // number of new LEDs to light up?
          switch (n) {
            case 3: // N
              if (r>0) {
                if (s_Glowstick.g[c][r-1] == 0) {
                  s_Glowstick.g[c][r-1] = 25;
                  x++; if (x >= howMuch/3) break;
                }
                else s_Glowstick.g[c][r-1] = s_Glowstick.g[c][r-1] * 2;
              }
            case 2: // E
              if (c<15) {
                if (s_Glowstick.g[c+1][r] == 0) {
                  s_Glowstick.g[c+1][r] = 25;
                  x++; if (x >= howMuch/3) break;
                }
                else s_Glowstick.g[c+1][r] = s_Glowstick.g[c+1][r] * 2;
              }
            case 1: // S
              if (r<15) {
                if (s_Glowstick.g[c][r+1] == 0) {
                  s_Glowstick.g[c][r+1] = 25;
                  x++; if (x >= howMuch/3) break;
                }
                else s_Glowstick.g[c][r+1] = s_Glowstick.g[c][r+1] * 2;
              }
            case 0: // W
              if (c>0) {
                if (s_Glowstick.g[c-1][r] == 0) {
                  s_Glowstick.g[c-1][r] = 25;
                  x++; if (x >= howMuch/3) break;
                }
                else s_Glowstick.g[c-1][r] = s_Glowstick.g[c-1][r] * 2;
              }
              break;
          }
        }
      }
    }
  }

  for (r=0; r<16; r++) {
    for (c=0; c<16; c++) {
//      pixel.setPixelColor(panel.led[c][r], GREEN(s_Glowstick.g[c][r]));
      setPixelColor_2(panel.led[c][r], GREEN(s_Glowstick.g[c][r]));
    }
  }
  pixel.show();
}
void resetGlowstick()
{
  s_Glowstick.initialized = false;
}

/*===========================================================================
 * Timer
 *===========================================================================*/
typedef struct Timer {
  uint32_t last;
  uint32_t sleep;
  uint32_t now;
} Timer;
Timer s_gTimer;
/*===========================================================================
 * BLE Controller
 *===========================================================================*/
#define CTRL_UP      1
#define CTRL_DOWN    2
#define CTRL_LEFT    3
#define CTRL_RIGHT   4

#define CTRL_1       5
#define CTRL_2       6
#define CTRL_3       7
#define CTRL_4       8

struct BLEController {
  uint8_t previous;
  uint8_t button;
  bool    pressed;
};
BLEController Controller;

/*===========================================================================
 * Snake
 *===========================================================================*/
#define SNAKE_APPLE  -1
#define SNAKE_EMPTY  0

struct SnakeBoard {
  uint8_t  direction;
  uint8_t  difficulty;
  bool     needsApple;
  bool     initialized;
  uint8_t  state;
  bool     hit; // hit yourself?
  uint8_t  died;

  Timer    t;
  
  uint8_t  x;
  uint8_t  y;
  
  int cell[16][16] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
  };
  
};
SnakeBoard s_SnakeBoard;

void redPanel()
{
  // used when dying... full-red
  int i;
  for (i=0; i<265; i++)
//    pixel.setPixelColor(i, pixel.Color(100,0,0));
    setPixelColor_2(i, pixel.Color(100,0,0));
  pixel.show();
}

bool deathFrameWasShown()
{
  int i;
  
  if (s_SnakeBoard.died > 0) {
    s_SnakeBoard.t.sleep = 350;
    s_SnakeBoard.difficulty = 0;
    
    s_SnakeBoard.died++;
    Serial.print("... DIED=");
    Serial.println(s_SnakeBoard.died);
    if ((s_SnakeBoard.died % 2) == 0) {
      redPanel(); delay(20);
    }
    else {
      clearPanel(true); delay(20);
    }

    // no more dying... restart the game
    if (s_SnakeBoard.died > 10) {
      s_SnakeBoard.initialized = false;
      return false;
    }

    return true;
  }

  return false;
}

void initializeGameIfNeeded()
{
  int i, j;
  if (!s_SnakeBoard.initialized) {
    Serial.println("initializing game...");

    // clear the entire board
    for (i=0; i<16; i++) {
      for (j=0; j<16; j++) {
        s_SnakeBoard.cell[i][j] = 0;
      }
    }

    // now set up default values
    s_SnakeBoard.t.sleep = 350;
    s_SnakeBoard.state = 0;
    s_SnakeBoard.difficulty = 0;
    s_SnakeBoard.needsApple = true;
    s_SnakeBoard.died = 0;
    s_SnakeBoard.hit = false;
    s_SnakeBoard.x = random(16); // location of the head
    s_SnakeBoard.y = random(16); //

    // Set the value (length) in the head
    s_SnakeBoard.cell[s_SnakeBoard.x][s_SnakeBoard.y] = 1;
    s_SnakeBoard.initialized = true;
  }
}

void placeAppleIfNeeded()
{
  int x, y;
  bool found = false;
  
  if (s_SnakeBoard.needsApple) {
    
pickAnApple:
    x = random(16); y = random(16);
    if (s_SnakeBoard.cell[x][y] == 0) s_SnakeBoard.cell[x][y] = -1;
    else goto pickAnApple;

    s_SnakeBoard.needsApple = false;

    // increase difficulty?
    s_SnakeBoard.difficulty++;
  }
  
  for (x=0; x<16; x++) {
    for (y=0; y<16; y++) {
      if (found == true) {
        if (s_SnakeBoard.cell[x][y] == -1)
          s_SnakeBoard.cell[x][y] = 0; // too many apples?
      }
      
      if (s_SnakeBoard.cell[x][y] == -1) found = true;
    }
  }
}

void debugDrawBoard()
{
  int i, j;
  for (i=0; i<16; i++) {
    for (j=0; j<16; j++) {
      switch (s_SnakeBoard.cell[j][i]) {
        case -1: Serial.print("A"); break;
        case 0:  Serial.print("."); break;
        default: Serial.print("S"); break;
      }
    }
    Serial.print("\n");
  }
  Serial.print("\n");
}

void stepSnakeBoard()
{
  uint8_t i,j;
  int head, next, x, y;
  bool foundSnake = false;
  
  // figure out where the next step will take us:
  //   this is done by finding the head, the direction, and apple-checking...
  head = s_SnakeBoard.cell[s_SnakeBoard.x][s_SnakeBoard.y];

  switch (Controller.button) {
    case CTRL_UP:
      // boundary checking... N & S
      if ((s_SnakeBoard.y - 1) < 0) goto dieReturn;
      x = s_SnakeBoard.x; y = s_SnakeBoard.y - 1;      
      break;
    case CTRL_DOWN:
      // boundary checking... N & S
      if ((s_SnakeBoard.y + 1) > 15) goto dieReturn;
      x = s_SnakeBoard.x; y = s_SnakeBoard.y + 1;
      break;
    case CTRL_RIGHT:
      // boundary checking... E & W (infinite scroll)
      x = s_SnakeBoard.x + 1; y = s_SnakeBoard.y;
      if (x > 15) x = 0;
      break;
    case CTRL_LEFT:
      // boundary checking... E & W (infinite scroll)
      x = s_SnakeBoard.x - 1; y = s_SnakeBoard.y;
      if (x < 0) x = 15;
      break;
  }

  // If the cell we are about to go to (next) is an apple, then we need to
  // "grow" by one cell...
  next = s_SnakeBoard.cell[x][y];
  if (next == SNAKE_APPLE) {
    Serial.print("we ate an apple...");
    s_SnakeBoard.cell[x][y] = 0; // EAT IT!!!
    s_SnakeBoard.needsApple = true;
    head++; // makes the "length" longer...
    Serial.print("making head=");
    Serial.println(head);
  }

  // What if we hit ourselves with the move?
  else if (next != SNAKE_EMPTY) {
    Serial.println("we hit ourselves");
    s_SnakeBoard.hit = true; // OUCH! We hit ourselves...
  }

  // at this point, x & y contain the new head location, but we need to move
  // the rest of the cells up one... we do that by decrementing... cause the
  // tail (1) then becomes empty (0)... fancy huh?
  for (i=0; i<16; i++) {
    for (j=0; j<16; j++) {
      if (s_SnakeBoard.cell[i][j] > 0) {
        s_SnakeBoard.cell[i][j]--;
        foundSnake = true;
      }
    }
  }

  // if we couldnt find the snake, we must have died?
  if (!foundSnake) goto dieReturn;

  // but once we have "moved" the snake, we need to put its head back in
  // the newly calculated position... but if we have hit ourselves, make it
  // look like we shrink back to nothing and die...
  if (!s_SnakeBoard.hit) {
    s_SnakeBoard.cell[x][y] = head;
    s_SnakeBoard.x = x;
    s_SnakeBoard.y = y;

    s_SnakeBoard.state = 0; // ready to accept a new direction...
  }
  return;

dieReturn:
  Serial.println("we ran into a wall AND/OR died");
  s_SnakeBoard.died = 1;
  return;
}

void drawSnake()
{
  uint8_t i, j;

//  Serial.println("drawSnake() entry");
  s_SnakeBoard.t.now = millis();
  if(s_SnakeBoard.t.now - s_SnakeBoard.t.last >= \
          (s_SnakeBoard.t.sleep - (10 * s_SnakeBoard.difficulty))) {
    s_SnakeBoard.t.last = s_SnakeBoard.t.now;
  }
  else return;

  if (deathFrameWasShown()) return;
  initializeGameIfNeeded();
  placeAppleIfNeeded();
  stepSnakeBoard();

  // Make the snake board turn into pixel output...
  for (i=0; i<16; i++) {
    for (j=0; j<16; j++) {
      switch (s_SnakeBoard.cell[i][j]) {
        // draw apple
        case -1:
//          pixel.setPixelColor(panel.led[i][j], pixel.Color(100,0,0));
          setPixelColor_2(panel.led[i][j], pixel.Color(100,0,0));
          break;
          
        // draw "space"
        case 0:
//          pixel.setPixelColor(panel.led[i][j], pixel.Color(0,0,0));
          setPixelColor_2(panel.led[i][j], pixel.Color(0,0,0));
          break;
          
        // draw snake
        default:
          if (s_SnakeBoard.hit)
//            pixel.setPixelColor(panel.led[i][j], pixel.Color(0,0,100));
            setPixelColor_2(panel.led[i][j], pixel.Color(0,0,100));
          else
//            pixel.setPixelColor(panel.led[i][j], pixel.Color(100,100,0));
            setPixelColor_2(panel.led[i][j], pixel.Color(100,100,0));
          break;
      }
    }
  }
  pixel.show();
}

/*===========================================================================
 * Test
 *===========================================================================*/
struct TestState {
  int led;
  int state;
};
struct TestState s_testState;

void drawTest()
{
  // Generic sleep-until routine...
  s_gTimer.now = millis();
  if (s_gTimer.now - s_gTimer.last >= 20) {
    s_gTimer.last = s_gTimer.now;
  }
  else return;

  s_testState.led++;
  if (s_testState.led > NUMPIXELS) {
    s_testState.led = 0;
    s_testState.state++;
    if (s_testState.state > 6) s_testState.state = 0;
  }
  
  switch (s_testState.state) {
    case 0:
      pixel.setPixelColor(s_testState.led, pixel.Color(100,0,0));
      pixel.show();
      break;
    case 2:
      pixel.setPixelColor(s_testState.led, pixel.Color(100,0,0));
      pixel.show();
      break;
    case 4:
      pixel.setPixelColor(s_testState.led, pixel.Color(100,0,0));
      pixel.show();
      break;
    default:
      pixel.setPixelColor(s_testState.led, pixel.Color(0,0,0));
      pixel.show();
      break;
  }
}

/*===========================================================================
 * Static?
 *===========================================================================*/
void drawStatic()
{
  int i, j, c;

  // Generic sleep-until routine...
  s_gTimer.now = millis();
  if (s_gTimer.now - s_gTimer.last >= 100) {
    s_gTimer.last = s_gTimer.now;
  }
  else return;
  
  for (i=0; i<16; i++) {
    for (j=0; j<16; j++) {
      switch (random(15)) {
        case 0:
          c = random(100);
          break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
          c = 0;
          break;
        default:
          c = random(25);
          break;
      }
//      pixel.setPixelColor(panel.led[i][j], pixel.Color(c,c,c));
      setPixelColor_2(panel.led[i][j], pixel.Color(c,c,c));
    }
  }
  pixel.show();
}

/*===========================================================================
 * DJ Lighting Effects
 *===========================================================================*/

struct FX_checkers {
  uint8_t prev_x;
  uint8_t prev_y;
};
struct FX_checkers s_FX_checkers;

void drawFX_checkers1(uint8_t x, uint8_t y)
{
  uint8_t i,j;
  uint32_t c = pixel.Color(random(100), random(100), random(100));

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

  s_FX_checkers.prev_x = x;
  s_FX_checkers.prev_y = y;
  pixel.show();
}

void drawFX_checkers2(uint8_t x, uint8_t y)
{
  uint8_t i,j;
  uint32_t c = pixel.Color(random(100), random(100), random(100));

  if (x > 1) x = random(2);
  if (y > 1) y = random(2);

  for (i=0; i<16; i++) {
    for (j=0; j<16; j++) {
      if ( ((i/4)%2) == x) {
        if ( ((j/4)%2) == y)
          setPixelColor_2(panel.led[j][i], c);
        else
          setPixelColor_2(panel.led[j][i], 0);
      }
      else {
        if ( ((j/4)%2) != y)
          setPixelColor_2(panel.led[j][i], c);
        else
          setPixelColor_2(panel.led[j][i], 0);
      }
    }
  }

  s_FX_checkers.prev_x = x;
  s_FX_checkers.prev_y = y;
  pixel.show();
}

void drawLightingEffect()
{
  uint8_t x, y;
  
  switch (Controller.button) {
    case CTRL_UP:
repickUp:
      x = random(2); y = random(2);
      if (x == s_FX_checkers.prev_x && y == s_FX_checkers.prev_y)
        goto repickUp;
      drawFX_checkers1(x, y);
      break;
      
    case CTRL_DOWN:
repickDown:
      x = random(2); y = random(2);
      if (x == s_FX_checkers.prev_x && y == s_FX_checkers.prev_y)
        goto repickDown;
      drawFX_checkers1(x, y);
      break;
  }
}

/*===========================================================================
 * Rainbow
 *===========================================================================*/

uint32_t Wheel(byte WheelPos)
{
  if (WheelPos < 85) {
   return pixel.Color((WheelPos * 3)/5, (255 - WheelPos * 3)/5, 0);
  } else if (WheelPos < 170) {
   WheelPos -= 85;
   return pixel.Color((255 - WheelPos * 3)/5, 0, (WheelPos * 3)/5);
  } else {
   WheelPos -= 170; 
   return pixel.Color(0, (WheelPos * 3)/5, (255 - WheelPos * 3)/5);
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
  if (s_gTimer.now - s_gTimer.last >= 10) {
    s_gTimer.last = s_gTimer.now;
  }
  else return;

  // Set all of one row to the same color...
  for (r=0; r<16; r++) {
    for (c=0; c<16; c++) {
      if (vertical)
        setPixelColor_2(panel.led[r][c], Wheel(n+s_rainbow.offset));
      else
        setPixelColor_2(panel.led[c][r], Wheel(n+s_rainbow.offset));
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
 * Setup
 *===========================================================================*/
void setup(void)
{
  
  Serial.begin(115200);
  Serial.println(F("Adafruit Bluefruit Command Mode Example"));
  Serial.println(F("---------------------------------------"));

  Serial.print(F("Initializing the NeoPixel object"));
  pixel.begin();
  pixel.setPixelColor(0, pixel.Color(200,0,0));
  pixel.show(); delay(500);

  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if ( !ble.begin(VERBOSE_MODE) ) {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );
  pixel.setPixelColor(0, pixel.Color(200,0,200));
  pixel.show(); delay(500);

  if ( FACTORYRESET_ENABLE ) {
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ) error(F("Couldn't factory reset"));
  }
  pixel.setPixelColor(0, pixel.Color(0,200,0));
  pixel.show(); delay(500);

  /* Disable command echo from Bluefruit */
  ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();

  Serial.println("Setting TX power level:");
  ble.println("AT+BLEPOWERLEVEL=4");

  pixel.setPixelColor(0, pixel.Color(200,200,0));
  pixel.show(); delay(500);

  Serial.println(F("Please use Adafruit Bluefruit LE app to connect in UART mode"));
  Serial.println(F("Then Enter characters to send to Bluefruit"));
  Serial.println();

  ble.verbose(false);  // debug info is a little annoying after this point!

  pixel.setPixelColor(0, pixel.Color(0,0,200));
  pixel.show(); delay(500);

  /* Wait for connection */
  while (! ble.isConnected()) delay(500);

  pixel.setPixelColor(0, pixel.Color(0,200,200));
  pixel.show(); delay(500);

  // LED Activity command is only supported from 0.6.6
  if ( ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION) ) {
    // Change Mode LED Activity
    Serial.println(F("******************************"));
    Serial.println(F("Change LED activity to " MODE_LED_BEHAVIOUR));
    ble.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
    Serial.println(F("******************************"));
  }

  pixel.setPixelColor(0, pixel.Color(200,200,200));
  pixel.show(); delay(500);

  // Setup Snake Game
  s_SnakeBoard.initialized = false;
  s_SnakeBoard.difficulty = 0;

  // Setup the controller defaults
  Controller.previous = 0;
  Controller.button = 0;
  Controller.pressed = false;

  // Seed the random number generator
  srandom(analogRead(A3));

  // Setup mic pin
  pinMode(A4, INPUT);
  pinMode(A5, INPUT);

  // Setup Fans...
  pinMode(FAN_PIN, OUTPUT);
  fanResetAll();
}

/*===========================================================================
 * Main Loop Modes
 *===========================================================================*/

#define MODE_DEFAULT   0
#define MODE_CLEAR     1
#define MODE_ENVELOPE  2
#define MODE_GATE      3
#define MODE_DRWHO     4
#define MODE_SNAKE     5
#define MODE_STATIC    6
#define MODE_VRAINBOW  7
#define MODE_HRAINBOW  8
#define MODE_DJ        9
#define MODE_TEST      10
#define MODE_STARTUP   9999

/*===========================================================================
 * Main Loop
 *===========================================================================*/
int mode = MODE_STARTUP;
void loop(void)
{
  uint8_t v=0;
  uint8_t r=0, g=0, b=0;
  uint16_t i=0;

  // Fan state is more important than BLE...
  //___________________________________________________________________________
  fanWriteState();

  // LED turns blue when BLE disconnected
  //___________________________________________________________________________
  if (! ble.isConnected()) {
    for (v=0; v<20; v++) {
      delay(100);
      if ((v/5)%2==0) {
        pixel.setPixelColor(0, pixel.Color(0,0,200)); pixel.show();
      }
      else {
        pixel.setPixelColor(0, pixel.Color(0,0,0)); pixel.show();
      }
      if (ble.isConnected())
        goto momentaryDisconnect;
    }

    clearPanel(true);
    pixel.setPixelColor(0, pixel.Color(0,0,200)); pixel.show();
    while (! ble.isConnected()) delay(500);
    
    s_Glowstick.initialized = false;

    s_SnakeBoard.died = false;
    s_SnakeBoard.initialized = false;
    
    mode = MODE_DEFAULT;
  }

momentaryDisconnect:

  // Handle "modes" (set by mode setters)
  //___________________________________________________________________________
  switch (mode) {
    case MODE_STARTUP:
      for (v=0; v<10; v++) {
        pixel.setPixelColor(0,0); pixel.show(); delay(100);
        pixel.setPixelColor(0,pixel.Color(100,100,100)); pixel.show(); delay(100);
      }
      mode = MODE_CLEAR;
      break;
    case MODE_CLEAR:
      clearPanel(true);
      mode = MODE_DEFAULT;
      break;
    case MODE_ENVELOPE:
      // volume
      v = analogRead(A4);
      Serial.print("!VE");
      Serial.println(v);
      delay(50);

      clearPanel(true);
      drawVolume(0,16,v/16);
      break;
    case MODE_GATE:
      // gate
      v = digitalRead(19);
      Serial.print("!VG");
      Serial.println(v);
      delay(50);

      clearPanel(true);
      drawVolume(0,16,v*16);
      break;
    case MODE_DRWHO:
      drawTardis();
      break;
    case MODE_SNAKE:
      drawSnake();
      break;
    case MODE_DJ:
      drawLightingEffect();
      break;
    case MODE_STATIC:
      drawStatic();
      break;
    case MODE_VRAINBOW:
      drawRainbowPanel(true);
      break;
    case MODE_HRAINBOW:
      drawRainbowPanel(false);
      break;
    case MODE_TEST:
      drawTest();
      break;
    default: // MODE_DEFAULT
      // not in a "mode"... so lets continue
      break;
  }
  
  // Check for incoming characters from Bluefruit
//  Serial.println("[BLE READ]");
  ble.println("AT+BLEUARTRX");
  ble.readline();
  if (strcmp(ble.buffer, "OK") == 0) {
    // no data
    return;
  }

//  Serial.print(F("  [Recv] '"));
//  Serial.print(ble.buffer);
//  Serial.println("'");

  // Modifiers
  //___________________________________________________________________________
  if (strcmp(ble.buffer, "+") == 0) {
    if (mode == MODE_DRWHO && s_Tardis.delay > 10) s_Tardis.delay -= 10;
  }
  if (strcmp(ble.buffer, "-") == 0) {
    if (mode == MODE_DRWHO && s_Tardis.delay < 200) s_Tardis.delay += 10;
  }

  // Mode setters
  //___________________________________________________________________________
  if (strcmp(ble.buffer, "mic") == 0)    mode = MODE_ENVELOPE;
  
  if (strcmp(ble.buffer, "back") == 0)   mode = MODE_DEFAULT;
  
  if (strcmp(ble.buffer, "off") == 0)    mode = MODE_CLEAR;
  
  if (strcmp(ble.buffer, "gate") == 0)   mode = MODE_GATE;
  
  if (strcmp(ble.buffer, "static") == 0) mode = MODE_STATIC;
  
  if (strcmp(ble.buffer, "vbow") == 0)   mode = MODE_VRAINBOW;
  if (strcmp(ble.buffer, "vrain") == 0)  mode = MODE_VRAINBOW;
  
  if (strcmp(ble.buffer, "hbow") == 0)   mode = MODE_HRAINBOW;
  if (strcmp(ble.buffer, "hrain") == 0)  mode = MODE_HRAINBOW;

  if (strcmp(ble.buffer, "fan") == 0) fanToggle();

  if (strcmp(ble.buffer, "test") == 0)   mode = MODE_TEST;
  
  if (strcmp(ble.buffer, "who") == 0)  {
    s_Tardis.delay = 30;
    mode = MODE_DRWHO;
  }
  

  // Accelerometer
  //___________________________________________________________________________
  if (ble.buffer[1] == 'A') {
    int a,b,c;
    a = abs(parseint(ble.buffer+2));
    if (a > 255 || a < -255) a = 0;
    
    b = abs(parseint(ble.buffer+6));
    if (b > 255 || b < -255) b = 0;
    
    c = abs(parseint(ble.buffer+10));
    if (c > 255 || c < -255) c = 0;

    v=((uint8_t)a+(uint8_t)b+(uint8_t)c);
    if (v > max_a) max_a = v;

    glowMore(v);
  }
  
  // Color Picker
  //___________________________________________________________________________
  else if (ble.buffer[1] == 'C') {
    r = ble.buffer[2];
    g = ble.buffer[3];
    b = ble.buffer[4];

    for(i=0; i<NUMPIXELS; i++) pixel.setPixelColor(i, pixel.Color(r,g,b));
    pixel.show(); // This sends the updated pixel color to the hardware.
    mode = MODE_DEFAULT;

    s_Glowstick.initialized = false;
  }

  // Control Pad (D-Pad)
  //___________________________________________________________________________
  else if (ble.buffer[1] == 'B') {
    
    switch (ble.buffer[3]) {
      case '1':
        Controller.previous = Controller.button;
        Controller.pressed = true;
        break;
      case '0':
        Controller.pressed = false;
        break;
    }

    switch (ble.buffer[2]) {
      case '1':
        Controller.button = CTRL_1;

        // If we are not in a mode, then start a game...
        if (mode == MODE_DEFAULT) mode = MODE_SNAKE;
        break;
      case '2':
        Controller.button = CTRL_2;

        // If we are not in a mode, then start DJ mode...
        if (mode == MODE_DEFAULT) mode = MODE_DJ;
        break;
      case '3':
        Controller.button = CTRL_3;
        break;
      case '4':
        Controller.button = CTRL_4;
        break;
      case '5':
        Controller.button = CTRL_UP;
        break;
      case '6':
        Controller.button = CTRL_DOWN;
        break;
      case '7':
        Controller.button = CTRL_LEFT;
        break;
      case '8':
        Controller.button = CTRL_RIGHT;
        break;
    }

      Serial.print(">>> ");
      Serial.println(ble.buffer);
  }
  
  // Some data was found, its in the buffer
  //___________________________________________________________________________
  else {
    Serial.print(F("[Recv] "));
    Serial.println(ble.buffer); 
  }
  ble.waitForOK();
}
