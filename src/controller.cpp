#include <Arduino.h>
#define FASTLED_INTERNAL // Supress FastLED compile banner
#include <FastLED.h>
#include <ArduinoSTL.h>


#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>


// oled libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_I2CDevice.h>
#include <Fonts/FreeSans9pt7b.h>
#define FONT FreeSans9pt7b
const uint8_t SCREEN_WIDTH = 128; // OLED display width, in pixels
const uint8_t SCREEN_HEIGHT = 64; // OLED display height, in pixels
const int OLED_RESET = -1;    // Reset pin # (or -1 if sharing Arduino reset pin)
const int SCREEN_ADDRESS  = 0x3C; ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32


// Declaration for an SSD1306 display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Transciever variables
RF24 radio(7, 8); // CE, CSN
const byte address[6] = "00001";



// rotary encoder (RE) variables
const uint8_t BRIGHTNESS_TOGGLE_PIN = 4;
const uint8_t SET_BPM_PIN = 5;
const uint8_t TAP_BPM_PIN = 10;
const uint8_t BEAT_SYNC_PIN = 21;
const uint8_t BPM_RE_PIN_A = 6;
const uint8_t BPM_RE_PIN_B = 9;


const uint8_t BRIGHTNESS_RE_PIN_A = 2; 
const uint8_t BRIGHTNESS_RE_PIN_B = 3;


// brightness parameters
const uint8_t BRIGHTNESS_LIMIT = 252;
volatile uint8_t brightness = 50;
volatile bool brightness_adjusted = true; // adjustment flags initialised as true to flag OLED display update on setup
volatile bool bpm_adjusted = true;
volatile bool tap_bpm_adjusted = true;
volatile bool mute_brightness_flag = false; // mute button flag
volatile bool brightness_muted = false; // mute brightness state



// variables for timing and causing repeat of effect every beat_length ms

volatile double tap_bpm_average = 124;

// syncing variables
unsigned long lastSyncButtonTime = 0;
volatile bool sync_attempt_flag = false;
volatile bool sync_attempt = false;
volatile bool tap_bpm_flag = false;


//-------------- live adjust variables //--------------
volatile double bpm = 124;
String colour = "yellow"; // predefined colour palletes containing 3 colours each
volatile bool performance_mode = false;

enum effect_choice : int { // enum encoding effect with whole and half denoting 1 and 1/2 beat length of effect respectively
  clear_display, // clear all leds
  wave_whole, // vertically rising horizontal wave with fading tail
  wave_half,
  flash_whole, // flash of vertical lines
  flash_half,
  twinkle_whole, // bright spots lighting every 16th note and slowing fading
  twinkle_half,
};

// array holding sequence of effects
int wave_flash_double[] = {wave_whole, clear_display, flash_whole, clear_display, wave_whole, clear_display, flash_half, flash_half};
int just_flash[] = {flash_whole, clear_display,flash_whole, clear_display, flash_whole, clear_display, flash_whole, clear_display};
int twinkle[] = {twinkle_whole, twinkle_whole, twinkle_whole, twinkle_whole};
int *selected_array = wave_flash_double;

enum colour_effect : long {  
  white,
  red,
  green,
  blue,
  red_white,
  green_white,
  blue_white,
  purple,
  cb,
  yellow,
  orange,
  enum_wave_flash_double = 16,
  enum_just_flash = 17,
  enum_twinkle = 18
};

enum colour_effect selected_effect = enum_wave_flash_double;
enum colour_effect selected_colour = blue;

// bPM and beat varialbes
double bpm_beat_length = lround((60/bpm)*1000); // length of one beat in milliseconds
long beat_length = bpm_beat_length;



CRGB colour1 = CRGB::Blue; // default all colours to red
CRGB colour2 = CRGB::Blue;
CRGB colour3 = CRGB::Blue;


// ------ Radio sends --------
void send_sync() {
  int text = 3000;
  if (radio.write(&text, sizeof(text))) {
    Serial.println("Successful 1");
    }
  else {
    Serial.println("Failed attempt 1");
  }
}

void send_beat_length(long new_beat_length) {
  long bpm_encoded = new_beat_length + 1000;
  if (radio.write(&bpm_encoded, sizeof(bpm_encoded))) {
    Serial.println("Successfully sent bpm");
  }
  else {
    Serial.println("Failed to send bpm");
  }
}

void send_brightness(long new_brightness) {
  if (radio.write(&new_brightness, sizeof(new_brightness))) {
    Serial.println("Successfully sent brightness");
  }
  else {
    Serial.println("Failed to send brightness");
  }
}

void send_colour_or_effect(long colour_effect) {
  long colour_effect_encoded = colour_effect + 2000;
  if (radio.write(&colour_effect_encoded, sizeof(colour_effect_encoded))) {
    Serial.println("Successfully sent colour_effect");
  }
  else {
    Serial.println("Failed to send colour_effect");
  }
}


// --------- Trellis -----------
#include <Adafruit_NeoTrellis.h>

#define Y_DIM 8 //number of rows of key
#define X_DIM 4 //number of columns of keys

//create a matrix of trellis panels
Adafruit_NeoTrellis t_array[Y_DIM/4][X_DIM/4] = {
  
  { Adafruit_NeoTrellis(0x2E)},
  {Adafruit_NeoTrellis(0x30)}
  
};


//pass this matrix to the multitrellis object
Adafruit_MultiTrellis trellis((Adafruit_NeoTrellis *)t_array, Y_DIM/4, X_DIM/4);

// Input a value 0 to 255 to get a color value.
// The colors are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return seesaw_NeoPixel::Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return seesaw_NeoPixel::Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return seesaw_NeoPixel::Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  return 0;
}

//define a callback for key presses
TrellisCallback blink(keyEvent evt){
  
  if(evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
    trellis.setPixelColor(evt.bit.NUM, Wheel(map(evt.bit.NUM, 0, X_DIM*Y_DIM, 0, 255))); //on rising
    send_colour_or_effect(evt.bit.NUM);
  }
  else if(evt.bit.EDGE == SEESAW_KEYPAD_EDGE_FALLING)
    trellis.setPixelColor(evt.bit.NUM, 0); //off falling2
    
  trellis.show();
  return 0;
}


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
  static uint8_t brightness_pin_a_current_state = HIGH;
  static uint8_t brightness_pin_a_previous_state = brightness_pin_a_current_state;

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
      send_brightness(brightness);
      brightness_adjusted = true;
    }
    brightness_pin_a_previous_state = brightness_pin_a_current_state;
  }
}

void flag_mute_brightness() {
  mute_brightness_flag = true;
}


// Interrupt to toggle the brightness to 0 and back to its original value before toggle
void mute_brightness() {
  static volatile uint8_t old_brightness;
  static unsigned long previous_button_press = 0;
  if ((millis() - previous_button_press) > 300){
    if (!brightness_muted) {              // mute brightness if non 0
      old_brightness = brightness;
      brightness = 0;
      send_brightness(brightness);
      brightness_adjusted = true;
      brightness_muted = true;
    }
    else {                               // unmute the brightness
      brightness = old_brightness;
      send_brightness(brightness);
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
  static uint8_t bpm_pin_a_current_state;
  static uint8_t bpm_pin_a_previous_state;

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
    send_beat_length(bpm_beat_length);
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
    send_sync();
  }
  lastSyncButtonTime = millis();
}


void tap_bpm_isr() { // flag tap bpm button press
  tap_bpm_flag = true;
} 


void tap_bpm() {
  static volatile float first_tap_time;
  static volatile unsigned long last_tap_time;
  static volatile uint8_t tap_count;

  if (millis() - last_tap_time > 800) { 
    first_tap_time = millis();
    tap_count = 0;
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
  long bpm_beat_length = lround((60/bpm)*1000);
  send_beat_length(bpm_beat_length);
  bpm = bpm;
  bpm_adjusted = true;
}

void round_tap_bpm() {
  tap_bpm_average = round(tap_bpm_average);
  tap_bpm_adjusted = true;
}



void poll_isr_flags() {
  if (mute_brightness_flag) {
    mute_brightness();
    mute_brightness_flag = false;
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



//-------------- SETUP //--------------//--------------//--------------//--------------//--------------//--------------//--------------
void setup() {
  Serial.begin(9600);
  // radio setup
  radio.begin();
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MIN);
  radio.stopListening();

  // trellis setup
  if(!trellis.begin()){
    Serial.println("failed to begin trellis");
    while(1) delay(1);
  }

  /* the array can be addressed as x,y or with the key number */
  for(int i=0; i<Y_DIM*X_DIM; i++){
      trellis.setPixelColor(i, Wheel(map(i, 0, X_DIM*Y_DIM, 0, 255))); //addressed with keynum
      trellis.show();
      delay(50);
  }
  
  for(int y=0; y<Y_DIM; y++){
    for(int x=0; x<X_DIM; x++){
      //activate rising and falling edges on all keys
      trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_RISING, true);
      trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_FALLING, true);
      trellis.registerCallback(x, y, blink);
      trellis.setPixelColor(x, y, 0x000000); //addressed with x,y
      trellis.show(); //show all LEDs
      delay(50);
    }
  }


  // oled setup
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  update_bpm_brightness_display(); // initialise OLED display for brightness and BPM

  // Button and rotary Encoder pin setup
  // Initialize the pins as an input with arduino nano internal pullup resistors activated
  pinMode(BRIGHTNESS_RE_PIN_A, INPUT_PULLUP);
  pinMode(BRIGHTNESS_RE_PIN_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BRIGHTNESS_RE_PIN_A), adjust_brightness, CHANGE); // Interupt set to call the brightness adjustment function
  pinMode(BRIGHTNESS_TOGGLE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BRIGHTNESS_TOGGLE_PIN), flag_mute_brightness, FALLING);
  
  // pin and interrupt setup for ify encoders
  pinMode(BPM_RE_PIN_A, INPUT_PULLUP);
  pinMode(BPM_RE_PIN_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BPM_RE_PIN_A), adjust_bpm, CHANGE);
  pinMode(SET_BPM_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SET_BPM_PIN), set_bpm, FALLING);
  pinMode(TAP_BPM_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TAP_BPM_PIN), tap_bpm, FALLING);
  pinMode(BEAT_SYNC_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BEAT_SYNC_PIN), attempt_sync_flag, FALLING);
 
}


//-------------- LOOP //--------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------
void loop() {
  update_bpm_brightness_display();
  poll_isr_flags();
  trellis.read();
} 

