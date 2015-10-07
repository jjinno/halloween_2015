#include "Adafruit_WS2801.h"
#include <SPI.h>

#define NUMPIXELS    256
#define PIN_DATA     17
#define PIN_CLOCK    16

Adafruit_WS2801 strip = Adafruit_WS2801(NUMPIXELS, PIN_DATA, PIN_CLOCK);

void setup() {
  strip.begin();
  strip.show();
}

void loop() {
  colorWipe(strip.Color(51, 0, 0), 10);
  colorWipe(strip.Color(0, 51, 0), 10);
  colorWipe(strip.Color(0, 0, 51), 10);
  
  rainbow(10);
  rainbowCycle(10);
}

void rainbow(uint8_t wait) {
  int i, j;

  for (j=0; j < 256; j++) {     // 3 cycles of all 256 colors in the wheel
    for (i=0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel( (i + j) % 255));
    }  
    strip.show();   // write all the pixels out
    delay(wait);
  }
}

void rainbowCycle(uint8_t wait) {
  int i, j;
  
  for (j=0; j < 256 * 5; j++) {
    for (i=0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel( ((i * 256 / strip.numPixels()) + j) % 256) );
    }  
    strip.show();
    delay(wait);
  }
}

void colorWipe(uint32_t c, uint8_t wait) {
  int i;
  
  for (i=0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      strip.show();
      delay(wait);
  }
}

//Input a value 0 to 255 to get a color value.
//The colours are a transition r - g -b - back to r
uint32_t Wheel(byte WheelPos)
{
  if (WheelPos < 85) {
   return strip.Color((WheelPos * 3)/5, (255 - WheelPos * 3)/5, 0);
  } else if (WheelPos < 170) {
   WheelPos -= 85;
   return strip.Color((255 - WheelPos * 3)/5, 0, (WheelPos * 3)/5);
  } else {
   WheelPos -= 170; 
   return strip.Color(0, (WheelPos * 3)/5, (255 - WheelPos * 3)/5);
  }
}
