#include <Arduino.h>
#define FASTLED_INTERNAL // Supress FastLED compile banner
#include <FastLED.h>
#include <ArduinoSTL.h>

// oled libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_I2CDevice.h>
#include <Fonts/FreeSans9pt7b.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1    // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define FONT FreeSans9pt7b

// Declaration for an SSD1306 display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// Initialise varialbes needed for FastLED 
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
const long NUM_LEDS = 30;
const uint8_t LED_data_pin = 3;

// colour and effect buttons
const uint8_t EFFECT_PIN_1 = 12;
const uint8_t EFFECT_PIN_2 = 2;


// rotary encoder (RE) variables
const uint8_t BRIGHTNESS_TOGGLE_PIN = 6;
const uint8_t SET_BPM_PIN = 7;
const uint8_t TAP_BPM_PIN = 10;
const uint8_t BEAT_SYNC_PIN = 12;
const uint8_t ROUND_BPM_PIN = 11;
const uint8_t BPM_RE_PIN_A = 8;
const uint8_t BPM_RE_PIN_B = 9;
uint8_t bpm_pin_a_current_state = HIGH;
uint8_t bpm_pin_a_previous_state = bpm_pin_a_current_state;

const uint8_t BRIGHTNESS_RE_PIN_A = 5; 
const uint8_t BRIGHTNESS_RE_PIN_B = 4;
uint8_t brightness_pin_a_current_state = HIGH;
uint8_t brightness_pin_a_previous_state = brightness_pin_a_current_state;

// brightness parameters
const uint8_t BRIGHTNESS_LIMIT = 90;
volatile uint8_t brightness = 30;
volatile uint8_t oldbrightness;
volatile bool brightness_muted = false;
volatile bool brightness_adjusted = true;
volatile bool bpm_adjusted = true;
volatile bool tap_bpm_adjusted = true;
volatile bool toggle_brightness_flag = false;


CRGB leds[NUM_LEDS] = {0};
// max x and y values of LED matrix
const uint8_t max_x = 5;
const uint8_t max_y = 4;

// variables for timing and causing repeat of effect every beat_length ms
unsigned long beat_start_time = millis();
unsigned long temp_beat_time;
volatile double tap_bpm_average = 0;

// syncing variables
unsigned long lastSyncButtonTime = 0;
volatile bool sync_attempt_flag = false;
volatile bool sync_attempt = false;
volatile bool tap_bpm_flag = false;


//-------------- live adjust variables //--------------
double bpm = 123;
String colour = "Blue"; // predefined colour palletes containing 3 colours each
volatile bool performance_mode = false;

enum effect_choice : int { // enum encoding effect with whole and half denoting 1 and 1/2 beat length of effect respectively
  clear_display = 0, // clear all leds
  wave_whole = 1, // vertically rising horizontal wave with fading tail
  wave_half = 2,
  flash_whole  = 3, // flash of vertical lines
  flash_half = 4,
  twinkle_whole = 5, // bright spots lighting every 16th note and slowing fading
  twinkle_half = 6,
};

// array holding sequence of effects
int wave_flash_double[] = {wave_whole, clear_display, flash_whole, clear_display, wave_whole, clear_display, flash_half, flash_half};
int just_flash[] = {flash_whole, clear_display,flash_whole, clear_display,flash_whole, clear_display,flash_whole, clear_display};
int *selected_array = wave_flash_double;

enum cycle_choice : int {
  enum_wave_flash_double = 0,
  enum_just_flash = 1
};
int selected_effect = enum_wave_flash_double;

/*reset_mode options: "bar" or "beat". 
If "bar" chosen beat reset will reset to the start of bar. 
If "beat" selected reset will reset to next beat.*/
String sync_mode = "bar"; 

// bPM and beat varialbes
double bpm_beat_length = round((60/bpm)*1000); // length of one beat in milliseconds
double performance_beat_length = 0;
double beat_length = bpm_beat_length;

enum colour_choice: int {
  white = 0,
  red = 1,
  green = 2,
  blue = 3,
  red_white = 4,
  green_white = 5,
  blue_white = 6,
  purple = 7,
  cb = 8
};

int selected_colour = red;

CRGB colour1 = CRGB::Red; // default all colours to red
CRGB colour2 = CRGB::Red;
CRGB colour3 = CRGB::Red;

//-------------- Function prototypes //--------------
void twinkle_shaker(int a);
void flash_and_fade(int a);
void wave_effect(int a);

//-------------- OLED display functions //--------------
void update_bpm_brightness_display() {
  if (brightness_adjusted || bpm_adjusted || tap_bpm_adjusted) {
    display.clearDisplay();
    display.setFont(&FONT);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 24);
    // Display static text
    display.print(bpm);
    display.print(" ");
    display.println(tap_bpm_average);
    display.print("Brightness: ");
    if (brightness_muted) {
      display.println("---");
    }
    else {
      display.println(brightness);
    }
    display.display();
    brightness_adjusted = false;
    bpm_adjusted = false;
    tap_bpm_adjusted = false;
  }
}

//-------------- User interaction and inputs //--------------
//--------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------//-------------


//-------------- BRIGHTNESS //--------------
// Interrupt function to adjust brightness of LEDs using rotary encoder
void adjust_brightness() {
  if (!brightness_muted) {
    /*BRIGHTNESS_RE_PIN_A and BRIGHTNESS_RE_PIN_B both defualt to HIGH
    On rotation both pass through LOW to HIGH*/
    brightness_pin_a_current_state = digitalRead(BRIGHTNESS_RE_PIN_A);

    if ((brightness_pin_a_previous_state == LOW) && (brightness_pin_a_current_state == HIGH)) {

      // if counter clockwise BRIGHTNESS_RE_PIN_B returns to HIGH before BRIGHTNESS_RE_PIN_A and so will be HIGH
      if ((digitalRead(BRIGHTNESS_RE_PIN_B) == HIGH) && (brightness>= 3)) {
            brightness-= 3;
        }
      // if clockwise BRIGHTNESS_RE_PIN_B returns to HIGH after BRIGHTNESS_RE_PIN_A and thus will still be low
      else if ((digitalRead(BRIGHTNESS_RE_PIN_B) == LOW) && brightness< BRIGHTNESS_LIMIT) {
        brightness+= 3;
      }
      FastLED.setBrightness(brightness);
      brightness_adjusted = true;
    }
    brightness_pin_a_previous_state = brightness_pin_a_current_state;
  }
}

void flag_toggle_brightness() {
  toggle_brightness_flag = true;
}

long previous_button_press = 0;
// Interrupt to toggle the brightness to 0 and back to its original value before toggle
void toggle_brightness() {
  if ((millis() - previous_button_press) > 300){
    if (!brightness_muted) {              // mute brightness if non 0
      oldbrightness = brightness;
      brightness = 0;
      FastLED.setBrightness(brightness);
      brightness_adjusted = true;
      brightness_muted = true;
    }
    else {                               // unmute the brightness
      brightness = oldbrightness;
      FastLED.setBrightness(brightness);
      brightness_adjusted = true;
      brightness_muted = false;
    } 
    previous_button_press = millis();
  }
  
}

//-------------- TIMING //--------------
void adjust_bpm() {
  /*BRIGHTNESS_RE_PIN_A and BRIGHTNESS_RE_PIN_B both defualt to HIGH
  On rotation both pass through LOW to HIGH*/
  bpm_pin_a_current_state = digitalRead(BPM_RE_PIN_A);

  if ((bpm_pin_a_previous_state == LOW) && (bpm_pin_a_current_state == HIGH)) {

    // if counter clockwise BRIGHTNESS_RE_PIN_B returns to HIGH before BRIGHTNESS_RE_PIN_A and so will be HIGH
    if ((digitalRead(BPM_RE_PIN_B) == HIGH) && (bpm >= 0.1)) {
          bpm -= 0.5;
          bpm_beat_length = round((60/bpm)*1000);
      }
    // if clockwise BRIGHTNESS_RE_PIN_B returns to HIGH after BRIGHTNESS_RE_PIN_A and thus will still be low
    else if (digitalRead(BPM_RE_PIN_B) == LOW) {
      bpm += 0.5;
      bpm_beat_length = round((60/bpm)*1000);
    }
    bpm_adjusted = true;
  }
  bpm_pin_a_previous_state = bpm_pin_a_current_state;
}

// helper function for sync button
void attempt_sync_flag() {
  sync_attempt_flag = true; // signal button interrupt flag
}

// seperate function from the flag as affect causes exit of loop containing flag polling
void attempt_sync() {// enact affect of button interrupt 
  if (millis() - lastSyncButtonTime > 200) {
    sync_attempt = true;
  }
  lastSyncButtonTime = millis();
}


void tap_bpm_isr() {
  tap_bpm_flag = true;
} 

volatile bool first_tap = true;
volatile unsigned long last_tap_time = 0;
volatile double first_tap_time = 0;
volatile uint8_t tap_count = 0;

void tap_bpm() {

  if (millis() - last_tap_time > 800){
    first_tap_time = 0;
    tap_count = 0;
  }

  if (!first_tap_time) {
    first_tap_time = millis();
  }

  else if (millis() - last_tap_time > 200) {
    tap_count++;
    tap_bpm_average = 60000 / ((millis() - first_tap_time) / tap_count);
    tap_bpm_adjusted = true;
  }
  last_tap_time = millis();
}


// Set BPM to tapped BPM
void set_bpm() {
  bpm = tap_bpm_average;
  bpm_beat_length = round((60/bpm)*1000);
  bpm_adjusted = true;
}

void round_tap_bpm() {
  tap_bpm_average = round(tap_bpm_average);
  tap_bpm_adjusted = true;
}

//-------------- Effect Control //-------------- 



// logic for selection of different colour pallettes     
void colour_select() {
  switch(selected_colour) {
    case red:
      colour1 = colour2 = colour3 = CRGB::Red;
      break;
    case blue:
      colour1 = colour2 = colour3 = CRGB::Blue;
      break;
    case green:
      colour1 = colour2 = colour3 = CRGB::Green;
      break;
    case purple:
      colour1 = colour2 = colour3 = CRGB::Purple;
      break;
    case white:
      colour1 = colour2 = colour3 = CRGB::White;
      break;
    case red_white:
      colour1 = colour2 = CRGB::Red;
      colour3 = CRGB::White;
      break;
    case green_white:
      colour1 = colour2 = CRGB::Green;
      colour3 = CRGB::White;
      break;
    case blue_white:
      colour1 = colour2 = CRGB::Blue;
      colour3 = CRGB::White;
      break;
    case cb:
      colour1 = CRGB::Orange;
      colour2 = CRGB::Green;
      colour3 = CRGB::Purple;
      break;
  }
}

void update_isr_flags() {
  if (toggle_brightness_flag) {
    toggle_brightness();
    toggle_brightness_flag = false;
  }
  if (tap_bpm_flag) {
    tap_bpm();
    tap_bpm_flag = false;
  }
  if (sync_attempt_flag) {
    attempt_sync();
    sync_attempt_flag = false;
  }
  update_bpm_brightness_display();
}

//-------------- Effect creation functions //--------------

// Map any x, y coordinate on LED matrix to LED array index
int map_XY(int x, int y) {
  int i;
  if (y % 2 == 0) {
      i = x + (max_x+1)*y; // i steps up by max_x+1 each row
  }
  // LED strips setup in alternating direction so x value flips sign
  else {
      i = (max_x+1)*(y + 1) - (x + 1);
  }

  return i;
}

template <size_t N>
void play_effect_sequence(int (&effects_array)[N]){
  for (int effect: effects_array) { // play effects in the effect_array in order
    switch(effect) {
      case clear_display:
        display.clearDisplay();
        break;
      case wave_whole:
        wave_effect(1);
        break;
      case wave_half:
        wave_effect(2);
        break;
      case flash_whole:
        flash_and_fade(1);
        break;
      case flash_half:
        flash_and_fade(2);
        break;
      case twinkle_whole:
        twinkle_shaker(1);
        break;
      case twinkle_half:
        twinkle_shaker(2);
        break;
    }
  }
}

void effect_button_1() {
  selected_effect = enum_wave_flash_double;
Serial.println("attempting select 1");
}

void effect_button_2() {
  selected_effect = enum_just_flash;
Serial.println("attempting select 2");
}

// logic for selection of next pre-set effect
void play_selected_effect() {
  switch(selected_effect) {
    case enum_wave_flash_double:
      play_effect_sequence(wave_flash_double);
      break;
    case enum_just_flash:
      play_effect_sequence(just_flash);
      break;
  }
}

//-------------- SETUP //--------------//--------------//--------------//--------------//--------------//--------------//--------------
void setup()
{
  Serial.begin(9600);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  update_bpm_brightness_display(); // initialise OLED display for brightness and BPM

  // Button and Rotary Ecnoder pin setup
  // Initialize the pins as an input with arduino nano internal pullup resistors activated
  pinMode(BRIGHTNESS_RE_PIN_A, INPUT_PULLUP);
  pinMode(BRIGHTNESS_RE_PIN_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BRIGHTNESS_RE_PIN_A), adjust_brightness, CHANGE); // Interupt set to call the brightness adjustment function
  pinMode(BRIGHTNESS_TOGGLE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BRIGHTNESS_TOGGLE_PIN), flag_toggle_brightness, FALLING);
 
  pinMode(BPM_RE_PIN_A, INPUT_PULLUP);
  pinMode(BPM_RE_PIN_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BPM_RE_PIN_A), adjust_bpm, CHANGE);
  pinMode(SET_BPM_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SET_BPM_PIN), set_bpm, FALLING);
  pinMode(ROUND_BPM_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ROUND_BPM_PIN), round_tap_bpm, FALLING);
  pinMode(TAP_BPM_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TAP_BPM_PIN), tap_bpm, FALLING);
  pinMode(BEAT_SYNC_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BEAT_SYNC_PIN), attempt_sync_flag, FALLING);

  pinMode(EFFECT_PIN_1, INPUT_PULLUP);
  pinMode(EFFECT_PIN_2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(EFFECT_PIN_1), effect_button_1, FALLING);
  attachInterrupt(digitalPinToInterrupt(EFFECT_PIN_2), effect_button_2, FALLING);

  
  // FastLED setup
  FastLED.addLeds<LED_TYPE,LED_data_pin,COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(brightness);
}

//-------------- LOOP //--------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------
/* loop should contain one bar of effects
ensure that all the first arguments (note length) sum to 4*/
void loop()
{
  colour_select();
  if (sync_mode == "bar") {
    sync_attempt = false;
  }

  if (performance_mode == true) {
    beat_length = performance_beat_length;
  }
  else if (performance_mode == false) {
    beat_length = bpm_beat_length;
  }
  update_bpm_brightness_display();
  play_selected_effect();
} 


//-------------- LED effects //--------------

// 1
void wave_effect(int note_length) {    // note length is the fraction of a beat you want effect to play for
  if (sync_mode == "beat") {           // if sync is on beat mode
      sync_attempt = false;
  }
  if (!sync_attempt) {
    beat_start_time = millis();
    temp_beat_time = beat_start_time;
    int y = max_y;
    while (((temp_beat_time - beat_start_time) < beat_length/note_length) && (!sync_attempt)) {
      update_isr_flags(); // check if bpm_brightness display needs updating
      if (y >= 0) {
        EVERY_N_MILLISECONDS(60) {
          for (int x = 0; x <= max_x; x +=1) { // Fill x row
            // sequence of logic to create pattern
            if ((y % 2 == 0) && (x % 2 != 0 )) {
              leds[map_XY(x,y)] = colour1;
            }
            else if ((y % 2 == 0) && (x%2 ==0 )) {
              leds[map_XY(x,y)] = colour3;
            }
            else if (x%2==0) {
              leds[map_XY(x,y)] = colour2;
            }
            else {
              leds[map_XY(x,y)] = colour3;
            }
          }
          FastLED.show();
          y-=1;
        }
      }
      EVERY_N_MILLIS(2) {
        fadeToBlackBy(leds, NUM_LEDS, 5);
        FastLED.show();
      }
      temp_beat_time = millis();
    }
  }
}

// 2
unsigned long flash_start_time; 
void flash_and_fade(int note_length) { // Flash LED and decay to 0 every beat_length seconds
  flash_start_time = millis();
  if (sync_mode == "beat") {           // if sync is on "beat" mode reset
      sync_attempt = false;
  }
  if (!sync_attempt) { 
    beat_start_time = millis();
    temp_beat_time = beat_start_time;      
    for (int x = 0; x <= max_x; x +=2) { // fill x 
      for(int y = 0; y<= max_y; y++){    // fill y
        leds[map_XY(x,y)] = colour1;
      } 
    }
    FastLED.show();
    while (((temp_beat_time - beat_start_time) < beat_length / note_length) && (!sync_attempt)) {
      update_isr_flags();
      EVERY_N_MILLISECONDS(5) {
        fadeToBlackBy(leds, NUM_LEDS, 7);
        FastLED.show();
      }

      temp_beat_time = millis();
    }
  }
}
 // 3
void twinkle_shaker(int note_length){ // add new bright spots every quarter note
  if (sync_mode == "beat") {           // if sync is on "beat" mode reset
      sync_attempt = false;
  }
  if (!sync_attempt) { 
    beat_start_time = millis();
    temp_beat_time = beat_start_time; 
    while (((temp_beat_time - beat_start_time) < beat_length / note_length) && (!sync_attempt)) {
      update_isr_flags();
      EVERY_N_MILLIS(beat_length/8){
        fadeToBlackBy(leds, NUM_LEDS, 25);
        FastLED.show();
        int rand_x1 = random(max_x+1);
        int rand_y1 = random(max_y+1);
        leds[map_XY(rand_x1,rand_y1)] = colour1;
        int rand_x2 = random(max_x+1);
        int rand_y2 = random(max_y+1);
        leds[map_XY(rand_x2,rand_y2)] = colour1;
        FastLED.show();
      }
      // EVERY_N_MILLISECONDS(50) {
      //   fadeToBlackBy(leds, NUM_LEDS, 20);
      //   FastLED.show();
      // }
      temp_beat_time = millis();
    }
  }
}

