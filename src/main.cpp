#include <Arduino.h>
#define FASTLED_INTERNAL // Supress FastLED compile banner
#include <FastLED.h>

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


// rotary encoder (RE) variables
const uint8_t BRIGHTNESS_TOGGLE_PIN = 6;
const uint8_t TAP_BPM_PIN = 7;
const uint8_t BEAT_SYNC_PIN = 2;
const uint8_t BRIGHTNESS_RE_PIN_A = 5; 
const uint8_t BRIGHTNESS_RE_PIN_B = 4;
uint8_t pin_A_current_state = HIGH;
uint8_t pin_A_previous_state = pin_A_current_state;

// brightness parameters
const uint8_t BRIGHTNESS_LIMIT = 90;
volatile uint8_t brightness = 90;
volatile uint8_t oldbrightness;
volatile bool brightness_adjusted = false;
volatile bool bpm_adjusted = false;
volatile bool brightness_muted = false;


CRGB leds[NUM_LEDS] = {0};
// max x and y values of LED matrix
const uint8_t max_x = 5;
const uint8_t max_y = 4;

// variables for timing and causing repeat of effect every beat_length ms
unsigned long beat_start_time = millis();
unsigned long temp_beat_time;

// syncing variables
unsigned long lastSyncButtonTime = 0;
volatile bool sync_attempt = false;

//  Header files containing different elements of total code
#include "bounce_round.h"

//-------------- live adjust variables //--------------
double bpm = 121;
String colour = "Red"; // predefined colour palletes containing 3 colours each
volatile bool performance_mode = false;
/* effects encoded in integers
first digit of intiger specifying beat length (1 for whole beat 2 for half) and following digits specifying effect
0 will end the effects sequence and should be used for null entries
1: wave_effect
2: flash_and_fade
3: twinkle_shaker
*/
enum effect_choice : int {
  wave_whole = 1,
  wave_half = 2,
  flash_whole  = 3,
  flash_half = 4,
  twinkle_whole = 5,
  twinkle_half = 6,
};

int fourth_beat_double[] = {wave_whole, flash_whole, wave_whole, flash_half, flash_half};


/*reset_mode options: "bar" or "beat". 
If "bar" chosen beat reset will reset to the start of bar. 
If "beat" selected reset will reset to next beat.*/
String sync_mode = "bar"; 


unsigned long bpm_beat_length = round((60/bpm)*1000); // length of one beat in milliseconds
unsigned long performance_beat_length = 0;
unsigned long beat_length = bpm_beat_length;
CRGB colour1 = CRGB::Red; // default all colours to red
CRGB colour2 = CRGB::Red;
CRGB colour3 = CRGB::Red;

//-------------- Function prototypes //--------------
void twinkle_shaker(int a);
void flash_and_fade(int a);
void wave_effect(int a);

//-------------- OLED display functions //--------------
void update_bpm_brightness_display() {
  if (brightness_adjusted || bpm_adjusted) {
    display.clearDisplay();
    display.setFont(&FONT);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 24);
    // Display static text
    display.print("BPM: ");
    display.println(bpm);
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
  }
}

//-------------- User interaction and inputs //--------------

// Interrupt function to adjust brightness of LEDs using rotary encoder
void adjust_brightness() {
  if (!brightness_muted) {
    /*BRIGHTNESS_RE_PIN_A and BRIGHTNESS_RE_PIN_B both defualt to HIGH
    On rotation both pass through LOW to HIGH*/
    pin_A_current_state = digitalRead(BRIGHTNESS_RE_PIN_A);

    if ((pin_A_previous_state == LOW) && (pin_A_current_state == HIGH)) {

      // if counter clockwise BRIGHTNESS_RE_PIN_B returns to HIGH before BRIGHTNESS_RE_PIN_A and so will be HIGH
      if ((digitalRead(BRIGHTNESS_RE_PIN_B) == HIGH) && (brightness>= 3)) {
            brightness-= 3;
        }
      // if clockwise BRIGHTNESS_RE_PIN_B returns to HIGH after BRIGHTNESS_RE_PIN_A and thus will still be low
      else if ((digitalRead(BRIGHTNESS_RE_PIN_B) == LOW) && brightness< BRIGHTNESS_LIMIT) {
        brightness+= 3;
      }
      FastLED.setBrightness(brightness);
      Serial.println(brightness);
      brightness_adjusted = true;
    }
    pin_A_previous_state = pin_A_current_state;
  }
}

// Interrupt to toggle the brightness to 0 and back to its original value before toggle
void toggle_LED() {
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
}

// helper function for sync button
 void attempt_sync() {
   if ( (lastSyncButtonTime - millis()) > 100) {
    sync_attempt = true;
    Serial.println("Beat sync attempt");
   }
 }

 // 
 unsigned long temp_tap_time;
 unsigned long new_tap_time;
 void tap_bpm() {


 }
 
// logic for selection of different colour pallettes     
void colour_select() {
  if (colour == "Red") {
    colour1 = colour2 = colour3 = CRGB::Red;
  }
  else if (colour == "Blue") {
    colour1 = colour2 = colour3 = CRGB::Blue;
  }
  else if (colour == "Green") {
    colour1 = colour2 = colour3 = CRGB::Green;
  }
  else if (colour == "Purple") {
    colour1 = colour2 = colour3 = CRGB::Purple;
  }
  else if (colour == "White") {
    colour1 = colour2 = colour3 = CRGB::White;
  }
  else if (colour == "RedWhite") {
    colour1 = colour2 = CRGB::Red;
    colour3 = CRGB::White;
  }
  else if (colour == "GreenWhite") {
    colour1 = colour2 = CRGB::Green;
    colour3 = CRGB::White;
  }
  else if (colour == "BlueWhite") {
    colour1 = colour2 = CRGB::Blue;
    colour3 = CRGB::White;
  }
  else if (colour == "2C") {
    colour1 = CRGB::Orange;
    colour2 = CRGB::Green;
    colour3 = CRGB::Purple;
  }
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

//-------------- SETUP //--------------//--------------//--------------//--------------//--------------//--------------//--------------
void setup()
{
  Serial.begin(9600);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.clearDisplay();
  display.setFont(&FONT);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 24);
  // Display static text
  display.print("BPM: ");
  display.println(bpm);
  display.print("Brightness: ");
  display.println(brightness);
  display.display(); 


  // This is our rotary encoder setup
  // Initialize the pins as an input with arduino nano internal pullup resistors activated
  pinMode(BRIGHTNESS_TOGGLE_PIN, INPUT_PULLUP);
  pinMode(BEAT_SYNC_PIN, INPUT_PULLUP);
  pinMode(BRIGHTNESS_RE_PIN_A, INPUT_PULLUP);
  pinMode(BRIGHTNESS_RE_PIN_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BRIGHTNESS_RE_PIN_A), adjust_brightness, CHANGE); // Interupt set to call the brightness adjustment function
  attachInterrupt(digitalPinToInterrupt(BRIGHTNESS_TOGGLE_PIN), toggle_LED, FALLING);
  attachInterrupt(digitalPinToInterrupt(BEAT_SYNC_PIN), attempt_sync, FALLING);

  
  
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
  play_effect_sequence(fourth_beat_double);
  // for (int x = 0; x <= max_x; x +=1) { // fill x 
  //   for(int y = 0; y<= max_y; y++){    // fill y
  //     leds[map_XY(x,y)] = CRGB::Red;
  //   } 
  // }
  // FastLED.show();
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
      update_bpm_brightness_display(); // check if bpm_brightness display needs updating
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
void flash_and_fade(int note_length) { // Flash LED and decay to 0 every beat_length seconds
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
      update_bpm_brightness_display();
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
      update_bpm_brightness_display();
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

