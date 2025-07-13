/*

 MegaTinyCore => ATtiny1616 @ 16MHz (internal)

*/

#include <Arduino.h>
#include <Bounce2.h> 
#include <FastLED.h> // re: below, see https://github.com/FastLED/FastLED/issues/1754
volatile unsigned long timer_millis = 0;
void update_millis() {
    static unsigned long last_micros = 0;
    unsigned long current_micros = micros();
    if (current_micros - last_micros >= 1000) {
        timer_millis++;
        last_micros = current_micros;
    }
}

#define BTN_1_PIN 3 // megaTinyCore # for PA7
#define BTN_2_PIN 8 // megaTinyCore # for PB1
#define DATA_PIN 1 // megaTinyCore # for PA5
#define NUM_LEDS 20
#define BRIGHTNESS_OUTER 15
#define BRIGHTNESS_INNER_FRONT 20
#define BRIGHTNESS_INNER_BACK 25
#define AMINATION_FPS 120

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

// Use segments to simplify control of outer/acrylic front/back
CRGB leds_raw[NUM_LEDS];
CRGBSet leds(leds_raw, NUM_LEDS);
CRGBSet leds_outer(leds(0, 15)); 
CRGBSet leds_inner_front(leds(16, 17)); 
CRGBSet leds_inner_back(leds(18, 19));  

Bounce button_1 = Bounce(); 
Bounce button_2 = Bounce(); 

void setup() {
  FastLED.addLeds<WS2812,DATA_PIN,GRB>(leds, NUM_LEDS);

  pinMode(BTN_1_PIN,INPUT_PULLUP);
  button_1.attach(BTN_1_PIN);
  button_1.interval(100); // interval in ms
  
  pinMode(BTN_2_PIN,INPUT_PULLUP);
  button_2.attach(BTN_2_PIN);
  button_2.interval(100); // interval in ms
}

// native SetBrightness() does not accept a CRGBSet
void setSegBrightness(CRGBSet& segment, uint8_t brightness) {
  for (uint16_t i = 0; i < segment.len; i++) {
    segment[i].r = scale8(segment[i].r, brightness);
    segment[i].g = scale8(segment[i].g, brightness);
    segment[i].b = scale8(segment[i].b, brightness);
  }
}

/* 
 Patterns
 */
uint8_t outterCurrentPattern = 0; // Index number of which pattern is current
uint8_t outterHuePosition = 0; // rotating "base color" used by many of the patterns
int outterLEDPosition = 0; // marker for point-based animations
uint8_t innerCurrentPattern = 0; // Index number of which pattern is current
uint8_t innerHuePosition = 0; // rotating "base color" used by many of the patterns

// rainbow for use inouterRainbow(), heavily tweaked to ok on the tomorrowland pendant outer ring
DEFINE_GRADIENT_PALETTE(rainbowLoopAgroGamma) {
      0, 255,   0,   0,   // Red 
     21, 255,  60,   0,   // Red-Orange 
     42, 220, 128,   0,   // Yellow 
     63,  80, 255,   0,   // Yellow-Green 
     85,   0, 255,   0,   // Green 
    106,   0, 255, 128,   // Green-Cyan
    127,   0, 220, 255,   // Cyan 
    148,   0,  80, 255,   // Cyan-Blue
    169,   0,   0, 255,   // Blue 
    190,  20,   0, 255,   // Blue-Violet
    211, 128,   0, 120,   // Magenta 
    232, 255,   0,  20,   // Magenta-Red
    255, 255,   0,   0    // Red (back to zero)
};

CRGBPalette16 myRainbowPalette = rainbowLoopAgroGamma;

// Rainbow around the perimeter
void outerRainbow() {
  //fill_rainbow( leds_outer, leds_outer.len, outterHuePosition, 16 ); // I did not like this rainbow 
  for (int i = 0; i < leds_outer.len; i++) {
    uint8_t colorIndex = (outterHuePosition + (i * (256 / leds_outer.len))) % 256;
    leds_outer[i] = ColorFromPalette(myRainbowPalette, colorIndex, 110, LINEARBLEND);
  }
  setSegBrightness(leds_outer, BRIGHTNESS_OUTER);
  EVERY_N_MILLISECONDS( 20 ) { outterHuePosition++; } 
}

#define PULSE_DECAY 0.35f // Fade by % each step
uint8_t pulseHeadBrightness = BRIGHTNESS_OUTER + 40; // brightness for this pattern
// Blue around the perimeter
void outerBluePulse() {
  // set outer_led to have a blue
  fill_solid(leds_outer, leds_outer.len, CRGB(0,0,1));
  // set the head
  leds_outer[outterLEDPosition] = CRGB(0, 0, pulseHeadBrightness);
  // set the opposing head
  int oppositePos = (outterLEDPosition + leds_outer.len / 2) % leds_outer.len;
  leds_outer[oppositePos] = CRGB(0, 0, pulseHeadBrightness);
  // move the head 
  EVERY_N_MILLISECONDS(150) { outterLEDPosition = (outterLEDPosition + 1) % leds_outer.len; }
  // dim the tail
  EVERY_N_MILLISECONDS(50) {
    for (int i = 0; i < leds_outer.len; i++) {
      leds_outer[i].r = (uint8_t)(leds_outer[i].r * PULSE_DECAY);
      leds_outer[i].g = (uint8_t)(leds_outer[i].g * PULSE_DECAY);
      leds_outer[i].b = (uint8_t)(leds_outer[i].b * PULSE_DECAY);
    }
  }
}

// Purple around the perimeter
void outerPurplePulse() {
  // set outer_led to have a blue
  fill_solid(leds_outer, leds_outer.len, CRGB(1,0,1));
  // set the head
  leds_outer[outterLEDPosition] = CRGB(pulseHeadBrightness, 0, pulseHeadBrightness);
  // set the opposing head
  int oppositePos = (outterLEDPosition + leds_outer.len / 2) % leds_outer.len;
  leds_outer[oppositePos] = CRGB(pulseHeadBrightness, 0, pulseHeadBrightness);
  // move the head 
  EVERY_N_MILLISECONDS(150) { outterLEDPosition = (outterLEDPosition + 1) % leds_outer.len; }
  // dim the tail
  EVERY_N_MILLISECONDS(50) {
    for (int i = 0; i < leds_outer.len; i++) {
      leds_outer[i].r = (uint8_t)(leds_outer[i].r * PULSE_DECAY);
      leds_outer[i].g = (uint8_t)(leds_outer[i].g * PULSE_DECAY);
      leds_outer[i].b = (uint8_t)(leds_outer[i].b * PULSE_DECAY);
    }
  }
}

// colored stripes pulsing at a defined Beats-Per-Minute (BPM)
void bpm() {
  uint8_t BeatsPerMinute = 62;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
  for (int i = 0; i < leds_outer.len; i++) {
    uint8_t colorIndex = (outterHuePosition + (i * (256 / leds_outer.len))) % 256;
    leds_outer[i] = ColorFromPalette(myRainbowPalette, colorIndex, beat-1+(i*10), LINEARBLEND);
  }
}

// All outer leds pulsing at a defined Beats-Per-Minute (BPM)
void bpmFlood() {
  uint8_t BeatsPerMinute = 62;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
  CRGB color = ColorFromPalette(myRainbowPalette, beat, 150);
  fill_solid(leds_outer, leds_outer.len, color);
}

// Rotating point with a tail
#define TAIL_FADE_DECAY 0.99f // Fade by % each step
uint8_t headLuma = BRIGHTNESS_OUTER - 5; // brightness for this pattern (white needs to be dimmed from BRIGHTNESS_OUTER)
void outerComet() {
  // set the head
  leds_outer[outterLEDPosition] = CRGB(headLuma, headLuma, headLuma);
  // move the head 
  EVERY_N_MILLISECONDS(150) { outterLEDPosition = (outterLEDPosition + 1) % leds_outer.len; }
  // dim the tail
  EVERY_N_MILLISECONDS(50) {
    for (int i = 0; i < leds_outer.len; i++) {
      leds_outer[i].r = (uint8_t)(leds_outer[i].r * TAIL_FADE_DECAY);
      leds_outer[i].g = (uint8_t)(leds_outer[i].g * TAIL_FADE_DECAY);
      leds_outer[i].b = (uint8_t)(leds_outer[i].b * TAIL_FADE_DECAY);
    }
  }
}

// Blue over Red as seen on the rendering
void innerBlOnRd() {
  fill_solid(leds_inner_front, leds_inner_front.len, CRGB(0, 0, BRIGHTNESS_INNER_FRONT));
  fill_solid(leds_inner_back, leds_inner_back.len, CRGB(BRIGHTNESS_INNER_BACK, 0, 0));
}

// Rainbow fade where F/R stay 180deg out of phase 
void innerDiametricFade() {
  fill_solid(leds_inner_front, leds_inner_front.len, CHSV(innerHuePosition, 255, 255));
  fill_solid(leds_inner_back, leds_inner_back.len, CHSV(innerHuePosition-127, 255, 255));
  setSegBrightness(leds_inner_front, BRIGHTNESS_INNER_FRONT);
  setSegBrightness(leds_inner_back, BRIGHTNESS_INNER_BACK);
  EVERY_N_MILLISECONDS( 100 ) { innerHuePosition++; } 
}

/* END PATTERNS */

/*
 List of patterns to cycle through on button press.  Each is defined as a separate function below.
 */
typedef void (*PatternList[])();
// add new patterns here
PatternList outerPatternList = { 
  outerRainbow, bpm, bpmFlood, outerBluePulse, outerPurplePulse, outerComet 
}; 
PatternList innerPatternList = { innerBlOnRd, innerDiametricFade }; // add new patterns here

void outerPatternAdvance() {
  // add one to the current pattern number, and wrap around at the end
  outterCurrentPattern = (outterCurrentPattern + 1) % ARRAY_SIZE( outerPatternList );
}

void innerPatternAdvance() {
  // add one to the current pattern number, and wrap around at the end
  innerCurrentPattern = (innerCurrentPattern + 1) % ARRAY_SIZE( innerPatternList );
}

void loop() {
  button_1.update();
  button_2.update();

  if ( button_1.fell() ) {
    FastLED.clear(); 
    outerPatternAdvance(); 
  }
  if ( button_2.fell() ) { innerPatternAdvance(); }

  outerPatternList[outterCurrentPattern]();
  innerPatternList[innerCurrentPattern]();

  FastLED.show();  
  FastLED.delay(1000/AMINATION_FPS); 
}
