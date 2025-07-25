/*

 MegaTinyCore => ATtiny1616 @ 16MHz (internal)

*/

#include <Arduino.h>
#include <Bounce2.h> 
#include <FastLED.h> // re: below, see https://github.com/FastLED/FastLED/issues/1754
#include <EEPROM.h>

// Hardware specific macros
#define BTN_1_PIN 3 // megaTinyCore # for PA7
#define BTN_2_PIN 8 // megaTinyCore # for PB1
#define DATA_PIN 1 // megaTinyCore # for PA5
#define NUM_LEDS 20
#define ANIMATION_FPS 129 // This is the typical BPM of EDM music
#define EEPROM_ADDR_OUTER 0
#define EEPROM_ADDR_INNER 1
#define EEPROM_ADDR_BRIGHTNESS 2
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

// Global brightness macros
#define BRIGHTNESS_CYCLE_LEN 4

// LED Segments - Use to simplify control of outer/acrylic front/back
CRGB leds_raw[NUM_LEDS];
CRGBSet leds(leds_raw, NUM_LEDS);
CRGBSet leds_outer(leds(0, 15)); 
CRGBSet leds_inner_front(leds(16, 17)); 
CRGBSet leds_inner_back(leds(18, 19));

// Pattern specific global variables
Bounce button_1 = Bounce(); 
Bounce button_2 = Bounce();

uint8_t outerCurrentPattern = 0; // Index number of which pattern is current
uint8_t innerCurrentPattern = 0; // Index number of which pattern is current
uint8_t outerHuePosition = 0;    // Rotating "base color" used by many of the patterns
uint8_t innerHuePosition = 0;    // Rotating "base color" used by many of the patterns
int outerLEDPosition = 0;        // Marker for point-based animations

// Patter brightness specific global variables
uint8_t brightnessLevelIndex = 0;     // Used to iterate through the button 2 brightness level presses

const uint8_t BRIGHTNESS_LEVELS_OUTER[BRIGHTNESS_CYCLE_LEN] = {20, 50, 100, 0};
const uint8_t BRIGHTNESS_LEVELS_OUTER_PULSE_HEAD[BRIGHTNESS_CYCLE_LEN] = {25, 75, 150, 0};
const uint8_t BRIGHTNESS_LEVELS_INNER_FRONT[BRIGHTNESS_CYCLE_LEN] = {10, 60, 125, 125};
const uint8_t BRIGHTNESS_LEVELS_INNER_BACK[BRIGHTNESS_CYCLE_LEN] = {10, 60, 125, 125};

uint8_t BRIGHTNESS_OUTER = BRIGHTNESS_LEVELS_OUTER[0];                       // Sets the "Low" brightness level for the outer leds
uint8_t BRIGHTNESS_OUTER_PULSE_HEAD = BRIGHTNESS_LEVELS_OUTER_PULSE_HEAD[0]; // Sets the "Low" brightness level for the pulse heads 
uint8_t BRIGHTNESS_INNER_FRONT = BRIGHTNESS_LEVELS_INNER_FRONT[0];           // Sets the "Low" brightness level for the inner front leds
uint8_t BRIGHTNESS_INNER_BACK = BRIGHTNESS_LEVELS_INNER_BACK[0];             // Sets the "Low" brightness level for the inner back leds




// Need to forward declarations for the patterns here -- update with new patterns
// Outer patterns
void wispyRainbow();
void berlinMode();
void cyanMode();
void magentaMode();
void wmTiamat();
void sinelonDualEffect();
void bpm();
void bpmFlood();
void outerCycle();

// Inner patterns
void innerCycle();
void innerCrossfadePalette();
void innerCrossfadeRedWhite();
void innerCrossfadeOrangeCyan();
void innerCrossfadeMagentaTurquoise();
void innerEDMSoundReactive_Rainbow();
void innerEDMSoundReactive_Cyan();
void innerEDMSoundReactive_Magenta();
void innerComplementaryCycle();

// Helper functions
void dualSinePulsePattern(uint8_t red, uint8_t green, uint8_t blue);
void washingMachineEffect(CRGBPalette16 palette);

/*
 * List of patterns to cycle through on button press.  Each is defined as a separate function below.
 * The outer patterns and the inner patters will be 1-to-1.
 */

typedef void (*PatternList[])();

PatternList outerPatternList = {
  outerCycle,
  wispyRainbow,
  berlinMode, 
  cyanMode, 
  magentaMode, 
  wmTiamat, 
  sinelonDualEffect, 
  bpm, 
  bpmFlood
}; 

PatternList innerPatternList = { 
  innerCycle,                     // outerCycle
  innerComplementaryCycle,        // wispyRainbow
  innerCrossfadeRedWhite,         // berlin mode
  innerCrossfadeOrangeCyan,       // cyanMode
  innerCrossfadeMagentaTurquoise, // magentaMode
  innerCrossfadePalette,          // wmTiamat
  innerCrossfadePalette,          // sinelonDualEffect
  innerEDMSoundReactive_Rainbow,  // bpm
  innerCrossfadePalette,  // bpmFlood
}; 

/* 
 ---  Custom Palette Definitions ---
*/

// rainbow for various patterns, heavily tweaked to look ok on the tomorrowland pendant outer ring
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

// Tiamat palette adapted from WLED
// A bright meteor with blue, teal and magenta hues
DEFINE_GRADIENT_PALETTE(tiamatAgroGamma) {
    0,   1,  2, 14,   // Very dark navy (nearly black-blue)
   33,   2,  5, 35,   // Midnight blue
  100,  13,135, 92,   // Teal green (deep jade)
  120,  43,255,193,   // Bright aqua mint
  140, 247,  7,249,   // Neon pink-violet
  160, 193, 17,208,   // Electric purple
  180,  39,255,154,   // Bright seafoam green
  200,   4,213,236,   // Electric cyan (vivid)
  220,  39,252,135,   // Bright spring green
  240, 193,213,253,   // Light periwinkle / icy blue
  255, 255,249,255    // Near white with pink tint (pastel)
};

CRGBPalette16 tiamatPalette = tiamatAgroGamma;

// Yelmag-inspired palette (warm yellows with magenta and red)
DEFINE_GRADIENT_PALETTE(yelmagAgroGamma) {
    0,   0,   0,   0, // Black
   42, 113,   0,   0, // Dark Red / Maroon
   84, 255,   0,   0, // Pure Red
  127, 255,   0, 117, // Hot Pink / Red-Magenta mix
  170, 255,   0, 255, // Magenta
  212, 255, 128, 117, // Light Red-Orange / Coral
  255, 255, 255,   0  // Yellow
};

CRGBPalette16 yelmagPalette = yelmagAgroGamma;

// Sherbet palette from WLED (soft pinks, oranges, and whites)
DEFINE_GRADIENT_PALETTE(rainbowSherbetAgroGamma) {
    0,   255, 102,  41,   // dark orange
   43,   255, 140,  90,   // peach
   86,   255,  51,  90,   // hot pink
  127,   255, 153, 169,   // soft pink
  170,   255, 255, 249,   // off-white
  209,   113, 255,  85,   // green-lime
  255,   157, 255, 137    // mint-lime
};

CRGBPalette16 sherbetPalette = rainbowSherbetAgroGamma;

void setup() {
  FastLED.addLeds<WS2812,DATA_PIN,GRB>(leds, NUM_LEDS);

  pinMode(BTN_1_PIN,INPUT_PULLUP);
  button_1.attach(BTN_1_PIN);
  button_1.interval(100); // interval in ms
  
  pinMode(BTN_2_PIN,INPUT_PULLUP);
  button_2.attach(BTN_2_PIN);
  button_2.interval(100); // interval in ms

  // Read saved pattern indices
  outerCurrentPattern = EEPROM.read(EEPROM_ADDR_OUTER);
  innerCurrentPattern = EEPROM.read(EEPROM_ADDR_INNER);
  brightnessLevelIndex = EEPROM.read(EEPROM_ADDR_BRIGHTNESS);

  // Safety bounds check
  if (outerCurrentPattern >= ARRAY_SIZE(outerPatternList)) outerCurrentPattern = 0;
  if (innerCurrentPattern >= ARRAY_SIZE(innerPatternList)) innerCurrentPattern = 0;

  BRIGHTNESS_OUTER = BRIGHTNESS_LEVELS_OUTER[brightnessLevelIndex];
  BRIGHTNESS_OUTER_PULSE_HEAD = BRIGHTNESS_LEVELS_OUTER_PULSE_HEAD[brightnessLevelIndex];
  BRIGHTNESS_INNER_FRONT = BRIGHTNESS_LEVELS_INNER_FRONT[brightnessLevelIndex];
  BRIGHTNESS_INNER_BACK = BRIGHTNESS_LEVELS_INNER_BACK[brightnessLevelIndex];
}

// This is not used here - but we need to add it in order to support FastLED requirements
volatile unsigned long timer_millis = 0;
void update_millis() {
    static unsigned long last_micros = 0;
    unsigned long current_micros = micros();
    if (current_micros - last_micros >= 1000) {
        timer_millis++;
        last_micros = current_micros;
    }
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
 --- Outer LED Patterns ---
*/

#define PULSE_DECAY 0.85f // Fade by % each step

void dualSinePulsePattern(uint8_t red, uint8_t green, uint8_t blue) {
  // This is a master timer that moves the waves. The number controls the speed.
  uint8_t master_phase = beat8(15);

  for (int i = 0; i < leds_outer.len; i++) {
    // Map the LED's physical position to a point on a circle (0-255).
    uint8_t led_angle = map(i, 0, leds_outer.len, 0, 255);

    // Calculate the brightness from the two opposing waves.
    uint8_t brightness1 = sin8(led_angle + master_phase);
    uint8_t brightness2 = sin8(led_angle + master_phase + 128);

    // Take the brighter of the two waves as our base.
    uint8_t raw_brightness = max(brightness1, brightness2);

    // Extra contrast boost and smooth easing.
    uint8_t contrast_brightness = scale8(raw_brightness, raw_brightness);
    contrast_brightness = scale8(contrast_brightness, contrast_brightness); // extra contrast
    uint8_t eased_brightness = ease8InOutCubic(contrast_brightness);

    // Apply the specified color, scaled by our adjusted brightness.
    leds_outer[i] = CRGB(red, green, blue);
    leds_outer[i].nscale8(scale8(eased_brightness, BRIGHTNESS_OUTER));
  }
}


// Wispy Dynamic Rainbow Spin Pattern
void wispyRainbow() {
  const uint8_t WISPY_BRIGHTNESS_SCALING = 150;
  // set outer_led to have a rainbow pattern
  //fill_rainbow(leds_outer, leds_outer.len, 0, 360/leds_outer.len, 240, 100);
  for (int i = 0; i < leds_outer.len; i++) {
    uint8_t colorIndex = (outerHuePosition + (i * (256 / leds_outer.len))) % 256;
    leds_outer[i] = ColorFromPalette(myRainbowPalette, colorIndex, 110, LINEARBLEND);
  }
  setSegBrightness(leds_outer, scale8(BRIGHTNESS_OUTER, WISPY_BRIGHTNESS_SCALING));
  EVERY_N_MILLISECONDS( 20 ) { outerHuePosition++; }

  // set the head
  leds_outer[outerLEDPosition] = leds_outer[outerLEDPosition].nscale8_video(BRIGHTNESS_OUTER_PULSE_HEAD);

  // set the opposing head
  int oppositePos = (outerLEDPosition + leds_outer.len / 2) % leds_outer.len;
  leds_outer[oppositePos] = leds_outer[oppositePos].nscale8_video(BRIGHTNESS_OUTER_PULSE_HEAD);

   // move the head with dynamic movement speed using a sine wave
  static uint16_t lastMoveTime = 0;
  uint16_t now = millis();

  // Speed oscillates between 30ms and 150ms per move at ~0.25Hz (i.e., ~4s full cycle)
  uint16_t dynamicSpeed = beatsin16(15, 30, 150); 

  if (now - lastMoveTime > dynamicSpeed) {
    outerLEDPosition = (outerLEDPosition + 1) % leds_outer.len;
    lastMoveTime = now;
  }

  // dim the tail
  EVERY_N_MILLISECONDS(50) {
    for (int i = 0; i < leds_outer.len; i++) {
      leds_outer[i].r = (uint8_t)(leds_outer[i].r * PULSE_DECAY);
      leds_outer[i].g = (uint8_t)(leds_outer[i].g * PULSE_DECAY);
      leds_outer[i].b = (uint8_t)(leds_outer[i].b * PULSE_DECAY);
    }
  }
}

void berlinMode() {
  const uint8_t BERLIN_BRIGHTNESS_SCALING = 200;  // Adjust this value (0–255)
  uint8_t scaledBrightness = scale8(BRIGHTNESS_OUTER, BERLIN_BRIGHTNESS_SCALING);
  dualSinePulsePattern(scaledBrightness, 0, 0);
}

void cyanMode() {
  dualSinePulsePattern(0, BRIGHTNESS_OUTER, BRIGHTNESS_OUTER);
}

void magentaMode() {
  dualSinePulsePattern(BRIGHTNESS_OUTER, 0, BRIGHTNESS_OUTER);
}

// Imitates a washing machine, rotating same waves forward,
// then pause, then backwards.
// Adapted from WLED: https://github.com/wled/WLED/blob/main/wled00/FX.cpp#L4478
void washingMachineEffect(CRGBPalette16 palette) {
  static uint8_t wmSpeed = 8;                     // Lower is slower
  uint8_t wmIntensity = BRIGHTNESS_OUTER;  // Brightness peak (0–255)
  static CRGBPalette16 wmPalette = palette;

  // Position moves back and forth like a washer drum oscillating
  uint16_t pos = beatsin16(wmSpeed, 0, leds_outer.len - 1);
  // Brightness pulsing
  uint8_t bri = beatsin8(wmSpeed * 2, wmIntensity / 4, wmIntensity);

  // Fade existing frame for trailing effect
  fadeToBlackBy(leds_outer, leds_outer.len, 20);

  // Main color from palette
  CRGB c = ColorFromPalette(wmPalette, (uint8_t)(pos * (255 / leds_outer.len)), bri);

  // Light up the head
  leds_outer[pos] = c;

  // Light up the opposing head (180 degrees apart)
  uint16_t pos2 = (pos + leds_outer.len / 2) % leds_outer.len;
  leds_outer[pos2] = c;
}

void wmTiamat() {
  washingMachineEffect(tiamatPalette);
}


// Dual Sinelon effect for leds_outer
void sinelonDualEffect() {
  // Static state variables
  static uint8_t beatA = 10;        // speed of first comet
  static uint8_t beatB = 7;         // speed of second comet
  static uint8_t indexA = 0;        // Color index A
  static uint8_t indexB = 127;      // Color index B

  // Fade existing frame by a small amount for trails
  fadeToBlackBy(leds_outer, leds_outer.len, 20);

  // Calculate positions using sinewave / ping-pong motion
  uint16_t posA = beatsin16(beatA, 0, leds_outer.len - 1);
  uint16_t posB = beatsin16(beatB, 0, leds_outer.len - 1);

  // Colors from Sherbet palette
  for (int i = 0; i < leds_outer.len; i++) {
    uint8_t colorIndexA = (indexA + (i * (256 / leds_outer.len))) % 256;
    uint8_t colorIndexB = (indexB + (i * (256 / leds_outer.len))) % 256;
    leds_outer[posA] = ColorFromPalette(sherbetPalette, colorIndexA, 110, LINEARBLEND);
    leds_outer[posB] = ColorFromPalette(sherbetPalette, colorIndexB, 110, LINEARBLEND);
  }
  leds_outer[posA].nscale8(BRIGHTNESS_OUTER);
  leds_outer[posB].nscale8(BRIGHTNESS_OUTER);

  // Advance palette indices slowly
  EVERY_N_MILLISECONDS(20) {
    indexA += 1;
    indexB += 1;
  }
}

// colored stripes pulsing at a defined Beats-Per-Minute (BPM)
void bpm() {
  const uint8_t BPM_BRIGHTNESS_SCALING = 150;
  uint8_t BeatsPerMinute = 32;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
  for (int i = 0; i < leds_outer.len; i++) {
    uint8_t colorIndex = (outerHuePosition + (i * (256 / leds_outer.len))) % 256;
    leds_outer[i] = ColorFromPalette(myRainbowPalette, colorIndex, beat-1+(i*10), LINEARBLEND);
    leds_outer[i].nscale8(scale8(BRIGHTNESS_OUTER, BPM_BRIGHTNESS_SCALING));
  }
}

// All outer leds pulsing at a defined Beats-Per-Minute (BPM)
void bpmFlood() {
  const uint8_t BPM_BRIGHTNESS_SCALING = 150;
  uint8_t BeatsPerMinute = 32;
  uint8_t beat = beatsin8(BeatsPerMinute, 32, 128);
  CRGB color = ColorFromPalette(myRainbowPalette, beat, 110);
  fill_solid(leds_outer, leds_outer.len, color);
  setSegBrightness(leds_outer, scale8(BRIGHTNESS_OUTER, BPM_BRIGHTNESS_SCALING));
}

void outerCycle() {
  void (**outerPatternListCycle)() = outerPatternList;
  static uint8_t current = 2; // Start at index 2 to avoid first two patterns
  static unsigned long lastChangeTime = 0;
  static bool firstRun = true;

  // Call the current pattern
  outerPatternListCycle[current]();

  // Every 10 seconds, advance to the next pattern, or on first run
  unsigned long now = millis();
  if (now - lastChangeTime >= 10000 || firstRun) {
    lastChangeTime = now;
    current++;
    if (current >= ARRAY_SIZE(outerPatternList)) current = 1;
    FastLED.clear();  // Optional: wipe leftover LEDs when switching
    firstRun = false;
  }
}

/* 
 --- Inner LED Patterns ---
*/

void innerCrossfadePalette() {
  // --- Static variables to hold state ---
  static uint8_t back_palette_index = 0;
  static uint8_t front_palette_index = 1; // Start one step ahead for color separation
  static bool back_is_active = false;
  static bool front_is_active = false;

  const uint8_t INNER_CROSSFADE_BRIGHTNESS_SCALING = 150;

  // --- TIMING & CONFIGURATION ---
  const uint8_t BPM = 15;
  const uint8_t LED1_DURATION = 100;
  const uint8_t LED2_DURATION = 100;
  const uint8_t PALETTE_STEP = 50;
  const uint8_t TOTAL_CYCLE_DURATION = 160;
  // This offset shifts the front panel's animation in time.
  // An offset of half the cycle duration makes it perfectly opposite.
  const uint8_t FRONT_PANEL_OFFSET = 60; //TOTAL_CYCLE_DURATION / 2; // e.g., 80

  // --- Master Beat Timers ---
  uint8_t back_beat = map(beat8(BPM), 0, 255, 0, TOTAL_CYCLE_DURATION);
  // The front panel's timer is the same, just offset.
  uint8_t front_beat = (back_beat + FRONT_PANEL_OFFSET) % TOTAL_CYCLE_DURATION;


  // --- Helper function to calculate animation for a single panel ---
  // This avoids code duplication and keeps the logic clean.
  auto calculatePanelAnimation = [](uint8_t beat, uint8_t duration1, uint8_t duration2, CRGB& color1, CRGB& color2, uint8_t& brightness1, uint8_t& brightness2) {
    // Determine the start times based on the duration. This is just for calculation.
    const uint8_t start1 = 0;
    const uint8_t start2 = (duration1 / 2) - (duration1 / 4); // Overlap logic

    if ((beat >= start1) && (beat < (start1 + duration1))) {
        uint8_t progress = beat - start1;
        uint8_t wave = triwave8(map(progress, 0, duration1 - 1, 0, 255));
        brightness1 = ease8InOutCubic(wave);
    }
    if ((beat >= start2) && (beat < (start2 + duration2))) {
        uint8_t progress = beat - start2;
        uint8_t wave = triwave8(map(progress, 0, duration2 - 1, 0, 255));
        brightness2 = ease8InOutCubic(wave);
    }
  };

  // --- Color Assignment ---
  // Check if the BACK panel's animation is starting a new cycle
  if (back_beat < 2 && !back_is_active) { // Use a small window to catch the start
    back_is_active = true;
    back_palette_index += (PALETTE_STEP * 2);
  } else if (back_beat > 2) {
    back_is_active = false;
  }
  // Check if the FRONT panel's animation is starting a new cycle
  if (front_beat < 2 && !front_is_active) {
    front_is_active = true;
    front_palette_index += (PALETTE_STEP * 2);
  } else if (front_beat > 2) {
    front_is_active = false;
  }

  // --- Brightness Calculation ---
  CRGB back_color1 = ColorFromPalette(myRainbowPalette, back_palette_index, 255, LINEARBLEND);
  CRGB back_color2 = ColorFromPalette(myRainbowPalette, back_palette_index + PALETTE_STEP, 255, LINEARBLEND);
  uint8_t b_bright1 = 0, b_bright2 = 0;
  calculatePanelAnimation(back_beat, LED1_DURATION, LED2_DURATION, back_color1, back_color2, b_bright1, b_bright2);

  CRGB front_color1 = ColorFromPalette(myRainbowPalette, front_palette_index, 255, LINEARBLEND);
  CRGB front_color2 = ColorFromPalette(myRainbowPalette, front_palette_index + PALETTE_STEP, 255, LINEARBLEND);
  uint8_t f_bright1 = 0, f_bright2 = 0;
  calculatePanelAnimation(front_beat, LED1_DURATION, LED2_DURATION, front_color1, front_color2, f_bright1, f_bright2);


  // --- Apply final values ---
  leds_inner_back[0] = back_color1;
  leds_inner_back[0].nscale8(scale8(scale8(b_bright1, BRIGHTNESS_INNER_BACK), INNER_CROSSFADE_BRIGHTNESS_SCALING));
  leds_inner_back[1] = back_color2;
  leds_inner_back[1].nscale8(scale8(scale8(b_bright2, BRIGHTNESS_INNER_BACK), INNER_CROSSFADE_BRIGHTNESS_SCALING));

  leds_inner_front[0] = front_color1;
  leds_inner_front[0].nscale8(scale8(scale8(f_bright1, BRIGHTNESS_INNER_FRONT), INNER_CROSSFADE_BRIGHTNESS_SCALING));
  leds_inner_front[1] = front_color2;
  leds_inner_front[1].nscale8(scale8(scale8(f_bright2, BRIGHTNESS_INNER_FRONT), INNER_CROSSFADE_BRIGHTNESS_SCALING));
}

void innerCrossfadeTwoColorCore(CRGB back_color, CRGB front_color) {
  // --- TIMING & CONFIGURATION ---
  const uint8_t BPM = 15;
  const uint8_t LED1_DURATION = 75;
  const uint8_t LED2_DURATION = 75;
  const uint8_t TOTAL_CYCLE_DURATION = 120;
  const uint8_t FRONT_PANEL_OFFSET = 40; //TOTAL_CYCLE_DURATION / 2; // 80

  // --- Master Beat Timers ---
  uint8_t back_beat = map(beat8(BPM), 0, 255, 0, TOTAL_CYCLE_DURATION);
  uint8_t front_beat = (back_beat + FRONT_PANEL_OFFSET) % TOTAL_CYCLE_DURATION;

  // --- Brightness variables (default to off) ---
  uint8_t back_brightness1 = 0, back_brightness2 = 0;
  uint8_t front_brightness1 = 0, front_brightness2 = 0;

  // --- Helper function to calculate brightness for a single panel ---
  auto calculatePanelBrightness = [](uint8_t beat, uint8_t duration1, uint8_t duration2, uint8_t& brightness1, uint8_t& brightness2) {
    // These start times are relative to the panel's own beat (0)
    const uint8_t start1 = 0;
    // This calculation creates the overlapping wipe effect
    const uint8_t start2 = (duration1 / 2) - (duration1 / 4);

    if ((beat >= start1) && (beat < (start1 + duration1))) {
        uint8_t progress = beat - start1;
        uint8_t wave = triwave8(map(progress, 0, duration1 - 1, 0, 255));
        brightness1 = ease8InOutCubic(wave);
    }
    if ((beat >= start2) && (beat < (start2 + duration2))) {
        uint8_t progress = beat - start2;
        uint8_t wave = triwave8(map(progress, 0, duration2 - 1, 0, 255));
        brightness2 = ease8InOutCubic(wave);
    }
  };

  // --- Calculate Brightness for Both Panels ---
  calculatePanelBrightness(back_beat, LED1_DURATION, LED2_DURATION, back_brightness1, back_brightness2);
  calculatePanelBrightness(front_beat, LED1_DURATION, LED2_DURATION, front_brightness1, front_brightness2);

  // --- Apply final values ---
  leds_inner_back[0] = back_color;
  leds_inner_back[0].nscale8(scale8(back_brightness1, BRIGHTNESS_INNER_BACK));
  leds_inner_back[1] = back_color;
  leds_inner_back[1].nscale8(scale8(back_brightness2, BRIGHTNESS_INNER_BACK));

  leds_inner_front[0] = front_color;
  leds_inner_front[0].nscale8(scale8(front_brightness1, BRIGHTNESS_INNER_FRONT));
  leds_inner_front[1] = front_color;
  leds_inner_front[1].nscale8(scale8(front_brightness2, BRIGHTNESS_INNER_FRONT));
}

// --- WRAPPER for Red/White Berlin Mode crossfade animation ---
void innerCrossfadeRedWhite() {
  innerCrossfadeTwoColorCore(CRGB::Red, CRGB::White);
}

// --- WRAPPER for Orange/Cyan crossfade animation ---
void innerCrossfadeOrangeCyan() {
  innerCrossfadeTwoColorCore(CRGB::Orange, CRGB::Cyan);
}

// --- WRAPPER for Magenta/Turquoise crossfade animation ---
void innerCrossfadeMagentaTurquoise() {
  innerCrossfadeTwoColorCore(CRGB::Magenta, CRGB::Turquoise);
}


// Complementary Rainbow Cycle
// Slowly cycles through the rainbow, with the front and back panels
// always showing complementary colors (180 degrees apart on the color wheel).
void innerComplementaryCycle() {
  // --- CONFIGURATION ---
  // Controls the speed of the color cycle. A higher number means a slower change.
  const uint8_t CYCLE_SPEED_MS = 50; 
  
  // --- SPARKLE CONFIGURATION (UPDATED) ---
  // How often new sparkles can be triggered. Higher value = more sparkles.
  const uint8_t SPARKLE_CHANCE = 4;
  // The brightness of the white sparkles.
  const uint8_t SPARKLE_BRIGHTNESS = 220;
  // How long each sparkle lasts in milliseconds to make it perceptible.
  const uint16_t SPARKLE_DURATION_MS = 100;

  // --- STATE ---
  // Use a static variable to hold the current hue. It persists between calls.
  static uint8_t current_hue = 0;
  EVERY_N_MILLISECONDS(CYCLE_SPEED_MS) { current_hue++; }
  
  // --- SPARKLE STATE (NEW) ---
  static int8_t sparkle_led_index = -1; // Which LED is sparkling (-1 for none)
  static uint32_t sparkle_start_time = 0; // When the sparkle started

  // --- COLOR CALCULATION ---
  // The front color is based on the current hue.
  CRGB front_color = CHSV(current_hue, 255, 255);
  // The back color is offset by 128, which is 180° on the 0-255 hue scale.
  CRGB back_color = CHSV(current_hue + 128, 255, 255);

  // --- APPLY BASE COLORS ---
  // Set the base colors first. Sparkles will be layered on top.
  fill_solid(leds_inner_front, leds_inner_front.len, front_color);
  fill_solid(leds_inner_back, leds_inner_back.len, back_color);

  // --- MANAGE SPARKLE EFFECT (UPDATED LOGIC) ---
  // Check if a sparkle is currently active
  if (sparkle_led_index != -1) {
    // If the sparkle's duration has passed, turn it off.
    if (millis() - sparkle_start_time > SPARKLE_DURATION_MS) {
      sparkle_led_index = -1;
    } else {
      // Otherwise, keep drawing the sparkle on the correct LED.
      if (sparkle_led_index < 2) {
        leds_inner_front[sparkle_led_index] = CRGB(SPARKLE_BRIGHTNESS, SPARKLE_BRIGHTNESS, SPARKLE_BRIGHTNESS);
      } else {
        leds_inner_back[sparkle_led_index - 2] = CRGB(SPARKLE_BRIGHTNESS, SPARKLE_BRIGHTNESS, SPARKLE_BRIGHTNESS);
      }
    }
  } else {
    // If no sparkle is active, try to trigger a new one.
    if (random8() < SPARKLE_CHANCE) {
      sparkle_led_index = random8(4); // Pick a new LED to sparkle (0-3)
      sparkle_start_time = millis();  // Record the start time
      // The sparkle itself will be drawn on the next frame loop.
    }
  }
  
  // --- APPLY MASTER BRIGHTNESS ---
  // Scale the final output (both base colors and sparkles) by the master brightness settings.
  setSegBrightness(leds_inner_front, BRIGHTNESS_INNER_FRONT);
  setSegBrightness(leds_inner_back, BRIGHTNESS_INNER_BACK);
}


// This is the new "core" function that accepts a color parameter.
// It contains all the logic for the animation.
void innerEDMSoundReactive_core(CRGB base_color) {
  // --- CORE CONFIGURATION ---
  const uint8_t BPM = 128;

  // --- RHYTHM CONFIGURATION (BACK PANEL) ---
  const CRGB KICK_COLOR = CRGB::White;
  const CRGB SNARE_COLOR = CRGB::Gold;
  
  // If the base color is black, treat it as a trigger for a rainbow cycle.
  // Otherwise, use the provided static color.
  CRGB hihat_color;
  if (base_color == CRGB::Black) {
    static uint8_t hihat_rainbow_hue = 0;
    EVERY_N_MILLISECONDS(30) { hihat_rainbow_hue++; } // Speed of the rainbow change
    hihat_color = CHSV(hihat_rainbow_hue, 240, 255);
  } else {
    hihat_color = base_color;
  }

  const uint16_t KICK_DECAY_MS = 150;
  const uint16_t SNARE_DECAY_MS = 120;

  // --- MELODIC CONFIGURATION (FRONT PANEL) ---
  static uint8_t synth_hue = 0;
  const uint8_t SYNTH_PAD_SPEED_DIVISOR = 8;

  // --- STRUCTURE & FX CONFIGURATION ---
  const uint8_t BUILD_UP_CYCLE = 32;
  const CRGB BUILD_UP_COLOR = CRGB::Orange;
  const uint8_t SIDECHAIN_DEPTH = 120;
  const uint16_t PRE_DROP_SILENCE_MS = 100;

  // --- BEAT TRACKING ---
  static uint32_t beat_counter = 0;
  static uint32_t last_beat_time = 0;
  uint32_t current_time = millis();
  uint32_t beat_interval = 60000 / BPM;

  bool new_beat = false;
  if (current_time - last_beat_time >= beat_interval) {
    new_beat = true;
    last_beat_time = current_time;
    beat_counter++;
  }
  uint32_t time_since_beat = current_time - last_beat_time;

  // --- KICK DRUM SIMULATION ---
  uint8_t kick_brightness = 0;
  if (time_since_beat < KICK_DECAY_MS) {
    kick_brightness = 255 * exp(-(float)time_since_beat / (KICK_DECAY_MS / 4.0f));
  }

  // --- SNARE/CLAP SIMULATION ---
  uint8_t snare_brightness = 0;
  static uint32_t last_snare_time = 0;
  if (new_beat && (beat_counter % 2 != 0)) {
    last_snare_time = current_time;
  }
  uint32_t time_since_snare = current_time - last_snare_time;
  if (time_since_snare < SNARE_DECAY_MS) {
    snare_brightness = 255 * exp(-(float)time_since_snare / (SNARE_DECAY_MS / 5.0f));
  }

  // --- HI-HAT SIMULATION ---
  uint8_t hihat_brightness = 30; // Base brightness to prevent flickering
  uint8_t sixteen_step = beat16(BPM) % 16;
  switch (sixteen_step) {
    case 0:  hihat_brightness = 150; break;
    case 4:  hihat_brightness = 220; break;
    case 8:  hihat_brightness = 150; break;
    case 12: hihat_brightness = 220; break;
    case 2: case 6: case 10: case 14: hihat_brightness = 80; break;
  }

  // --- SMOOTH SYNTH PAD SIMULATION ---
  uint8_t synth_speed = BPM / SYNTH_PAD_SPEED_DIVISOR;
  uint8_t min_bright = 40;
  uint8_t max_bright = 200;
  uint8_t synth_brightness1 = beatsin8(synth_speed, min_bright, max_bright, 0, 0);
  uint8_t synth_brightness2 = beatsin8(synth_speed, min_bright, max_bright, 0, 128);
  EVERY_N_MILLISECONDS(40) { synth_hue++; }
  CRGB synth_color1 = CHSV(synth_hue, 240, 255);
  CRGB synth_color2 = CHSV(synth_hue + 85, 240, 255);

  // --- BUILD-UP / DROP STRUCTURE ---
  uint8_t build_phase = beat_counter % BUILD_UP_CYCLE;
  bool is_in_build = (build_phase >= BUILD_UP_CYCLE - 8);
  bool is_drop = (build_phase == 0 && beat_counter > 0);
  bool is_pre_drop = (build_phase == BUILD_UP_CYCLE - 1 && time_since_beat > beat_interval - PRE_DROP_SILENCE_MS);
  uint8_t roll_brightness = 0;
  if (is_in_build) {
    float build_progress = (float)(build_phase - (BUILD_UP_CYCLE - 8)) / 8.0f;
    uint8_t roll_speed = map(build_progress * 255, 0, 255, 8, 2);
    if (beat16(BPM) % roll_speed == 0) { roll_brightness = 255; }
    snare_brightness = max(snare_brightness, roll_brightness);
    uint8_t filter_amount = map(build_progress * 255, 0, 255, 100, 255);
    hihat_brightness = scale8(hihat_brightness, filter_amount);
    uint8_t saturation = map(build_progress * 255, 0, 255, 180, 255);
    synth_color1.setHSV(synth_hue, saturation, 255);
    synth_color2.setHSV(synth_hue + 85, saturation, 255);
  }

  // --- DYNAMIC EFFECTS (FX) ---
  if (kick_brightness > 100) {
    uint8_t sidechain_amount = map(kick_brightness, 100, 255, 255 - SIDECHAIN_DEPTH, 255);
    synth_brightness1 = scale8(synth_brightness1, sidechain_amount);
    synth_brightness2 = scale8(synth_brightness2, sidechain_amount);
  }

  // --- FINAL OUTPUT MAPPING ---
  if (is_drop) {
    fill_solid(leds_inner_back, 4, CRGB::White);
    fill_solid(leds_inner_front, 2, CRGB::White);
    // Apply global brightness to the drop flash
    setSegBrightness(leds_inner_back, BRIGHTNESS_INNER_BACK);
    setSegBrightness(leds_inner_front, BRIGHTNESS_INNER_FRONT);
  } else if (is_pre_drop) {
    fill_solid(leds_inner_back, 4, CRGB::Black);
    fill_solid(leds_inner_front, 2, CRGB::Black);
  } else {
    // Back Panel - Apply global brightness to each effect
    leds_inner_back[0] = KICK_COLOR;
    leds_inner_back[0].nscale8(scale8(kick_brightness, BRIGHTNESS_INNER_BACK));
    leds_inner_back[1] = hihat_color;
    leds_inner_back[1].nscale8(scale8(hihat_brightness, BRIGHTNESS_INNER_BACK));
    leds_inner_back[2] = (roll_brightness > 0) ? BUILD_UP_COLOR : SNARE_COLOR;
    leds_inner_back[2].nscale8(scale8(snare_brightness, BRIGHTNESS_INNER_BACK));
    leds_inner_back[3] = CRGB::Black;

    // Front Panel - Apply global brightness to each effect
    leds_inner_front[0] = synth_color1;
    leds_inner_front[0].nscale8(scale8(synth_brightness1, BRIGHTNESS_INNER_FRONT));
    leds_inner_front[1] = synth_color2;
    leds_inner_front[1].nscale8(scale8(synth_brightness2, BRIGHTNESS_INNER_FRONT));
  }
}

// --- WRAPPER FUNCTIONS ---
// Create one of these for each color you want to use.

void innerEDMSoundReactive_Cyan() {
  innerEDMSoundReactive_core(CRGB::Cyan);
}

void innerEDMSoundReactive_Magenta() {
  innerEDMSoundReactive_core(CRGB::Magenta);
}

void innerEDMSoundReactive_Rainbow() {
  innerEDMSoundReactive_core(CRGB::Black);
}


void innerCycle() {
  void (**innerPatternListCycle)() = innerPatternList;
  static uint8_t current = 2; // Start at index 2 to avoid first two patterns
  static unsigned long lastChangeTime = 0;
  static bool firstRun = true;

  // Call the current pattern
  innerPatternListCycle[current]();

  // Every 10 seconds, advance to the next pattern, or on first run
  unsigned long now = millis();
  if (now - lastChangeTime >= 10000 || firstRun) {
    lastChangeTime = now;
    current++;
    if (current >= ARRAY_SIZE(innerPatternList)) current = 1;
    FastLED.clear();  // Optional: wipe leftover LEDs when switching
    firstRun = false;
  }
}

/* END PATTERNS */

void outerPatternAdvance() {
  // add one to the current pattern number, and wrap around at the end
  outerCurrentPattern = (outerCurrentPattern + 1) % ARRAY_SIZE( outerPatternList );
  EEPROM.update(EEPROM_ADDR_OUTER, outerCurrentPattern); // save to EEPROM
}

void innerPatternAdvance() {
  // add one to the current pattern number, and wrap around at the end
  innerCurrentPattern = (innerCurrentPattern + 1) % ARRAY_SIZE( innerPatternList );
  EEPROM.update(EEPROM_ADDR_INNER, innerCurrentPattern); // save to EEPROM
}

void patternBrightnessAdvance() {
  // advance the brightness cycle
  brightnessLevelIndex = (brightnessLevelIndex + 1) % BRIGHTNESS_CYCLE_LEN;

  BRIGHTNESS_OUTER = BRIGHTNESS_LEVELS_OUTER[brightnessLevelIndex];
  BRIGHTNESS_OUTER_PULSE_HEAD = BRIGHTNESS_LEVELS_OUTER_PULSE_HEAD[brightnessLevelIndex];
  BRIGHTNESS_INNER_FRONT = BRIGHTNESS_LEVELS_INNER_FRONT[brightnessLevelIndex];
  BRIGHTNESS_INNER_BACK = BRIGHTNESS_LEVELS_INNER_BACK[brightnessLevelIndex];

  EEPROM.update(EEPROM_ADDR_BRIGHTNESS, brightnessLevelIndex); //save to EEPROM
}

void loop() {
  button_1.update();
  button_2.update();

  if ( button_1.fell() ) {
    outerPatternAdvance();
    innerPatternAdvance();
    FastLED.clear(); 
  }

  if ( button_2.fell() ) {
    patternBrightnessAdvance(); 
    FastLED.clear();
  }

  outerPatternList[outerCurrentPattern]();
  innerPatternList[innerCurrentPattern]();

  FastLED.show();  
  FastLED.delay(1000/ANIMATION_FPS); 
}