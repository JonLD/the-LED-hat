#include <Arduino.h>
#define FASTLED_INTERNAL // Supress FastLED compile banner
#include <ArduinoSTL.h>
#include <FastLED.h>

#include <interface.h>

#include <RF24.h>
#include <SPI.h>
#include <nRF24L01.h>

// oled libraries
#include <Adafruit_GFX.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Wire.h>
#define FONT FreeSans9pt7b
const uint8_t SCREEN_WIDTH = 128; // OLED display width, in pixels
const uint8_t SCREEN_HEIGHT = 64; // OLED display height, in pixels
const int OLED_RESET = -1; // Reset pin # (or -1 if sharing Arduino reset pin)
// See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
const int SCREEN_ADDRESS = 0x3C;

// Declaration for an SSD1306 display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Transciever variables
RF24 radio(7, 8); // CE, CSN
const byte address[6] = "00001";
radioData_t radioData;

bool isInSetup = true;

// rotary encoder (RE) variables
const uint8_t BRIGHTNESS_TOGGLE_PIN = 4;
const uint8_t SET_BPM_PIN = 5;
const uint8_t TAP_BPM_PIN = 10;
const uint8_t BEAT_SYNC_PIN = 21;
const uint8_t BPM_RE_PIN_A = 6;
const uint8_t BPM_RE_PIN_B = 9;

const uint8_t BRIGHTNESS_RE_PIN_A = 2;
const uint8_t BRIGHTNESS_RE_PIN_B = 3;

const uint8_t BRIGHTNESS_LIMIT = 252;
// adjustment flags initialised as true to flag OLED display update on setup
volatile bool shouldUpdateDisplay = true;
volatile bool should_mute_brightness = false;
volatile bool isBrightnessMuted = false;

// variables for timing and causing repeat of effect every beat_length ms
volatile double tap_bpm_average = 124;

// syncing variables
volatile bool sync_attempt_flag = false;
volatile bool sync_attempt = false;
volatile bool tap_bpm_flag = false;

//-------------- live adjust variables //--------------
volatile double bpm = 124;

// bPM and beat variables
double bpm_beat_length =
    lround((60 / bpm) * 1000); // length of one beat in milliseconds
long beat_length = bpm_beat_length;

// ------ Radio sends --------
bool sendRadioData() {
  bool successfullySent = radio.write(&radioData, sizeof(radioData));
  if (successfullySent) {
    Serial.println("Successfully sent radio data");
  }
  return successfullySent;
}

// --------- Trellis -----------
#include <Adafruit_NeoTrellis.h>

#define Y_DIM 8 // number of rows of key
#define X_DIM 4 // number of columns of keys

// create a matrix of trellis panels
Adafruit_NeoTrellis t_array[Y_DIM / 4][X_DIM / 4] = {

    {Adafruit_NeoTrellis(0x2E)}, {Adafruit_NeoTrellis(0x30)}

};

// pass this matrix to the multitrellis object
Adafruit_MultiTrellis trellis((Adafruit_NeoTrellis *)t_array, Y_DIM / 4,
                              X_DIM / 4);

// Input a value 0 to 255 to get a color value.
// The colors are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if (WheelPos < 85) {
    return seesaw_NeoPixel::Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if (WheelPos < 170) {
    WheelPos -= 85;
    return seesaw_NeoPixel::Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
    WheelPos -= 170;
    return seesaw_NeoPixel::Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  return 0;
}

// define a callback for key presses
TrellisCallback blink(keyEvent evt) {

  if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
    trellis.setPixelColor(evt.bit.NUM, Wheel(map(evt.bit.NUM, 0, X_DIM * Y_DIM,
                                                 0, 255))); // on rising
    radioData.colourOrEffect = evt.bit.NUM;
  } else if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_FALLING)
    trellis.setPixelColor(evt.bit.NUM, 0); // off falling2

  trellis.show();
  return 0;
}

//-------------- OLED display functions //--------------
void updateDisplay() {
  display.clearDisplay();
  display.setFont(&FONT);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 24);
  display.print(bpm);
  display.print(" ");
  display.println(tap_bpm_average);
  display.print("Brightness: ");
  if (isBrightnessMuted) {
    display.println("---");
  } else {
    display.println(radioData.brightness);
  }
  display.display();
}

//-------------- User interaction and inputs //--------------

//-------------- BRIGHTNESS //--------------
// Interrupt function to adjust brightness of LEDs using rotary encoder
void adjust_brightness() {
  static uint8_t brightness_pin_a_current = HIGH;
  static uint8_t brightness_pin_a_previous = brightness_pin_a_current;

  if (!isBrightnessMuted) {
    /*BRIGHTNESS_RE_PIN_A and BRIGHTNESS_RE_PIN_B both defualt to HIGH
    On rotation both pass through LOW to HIGH*/
    brightness_pin_a_current = digitalRead(BRIGHTNESS_RE_PIN_A);

    if ((brightness_pin_a_previous == LOW) &&
        (brightness_pin_a_current == HIGH)) {
      // if counter clockwise BRIGHTNESS_RE_PIN_B returns to HIGH before
      // BRIGHTNESS_RE_PIN_A and so will be HIGH
      if ((digitalRead(BRIGHTNESS_RE_PIN_B) == HIGH) &&
          (radioData.brightness >= 3)) {
        radioData.brightness -= 3;
      }
      // if clockwise BRIGHTNESS_RE_PIN_B returns to HIGH after
      // BRIGHTNESS_RE_PIN_A and thus will still be low
      else if ((digitalRead(BRIGHTNESS_RE_PIN_B) == LOW) &&
               (radioData.brightness < BRIGHTNESS_LIMIT)) {
        radioData.brightness += 3;
      }
    }
    brightness_pin_a_previous = brightness_pin_a_current;
  }
}

void mute_brightness() {
  static volatile uint8_t old_brightness;
  static unsigned long previous_button_press = 0;
  if ((millis() - previous_button_press) > 300) {
    if (!isBrightnessMuted) {
      old_brightness = radioData.brightness;
      radioData.brightness = 0;
      isBrightnessMuted = true;
    } else {
      radioData.brightness = old_brightness;
      isBrightnessMuted = false;
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
    // if counter clockwise BRIGHTNESS_RE_PIN_B returns to HIGH before
    // BRIGHTNESS_RE_PIN_A and so will be HIGH
    if ((digitalRead(BPM_RE_PIN_B) == HIGH) && (bpm >= 0.1)) {
      bpm -= 0.1;
      radioData.beatLength = round((60 / bpm) * 1000);
    }
    // if clockwise BRIGHTNESS_RE_PIN_B returns to HIGH after
    // BRIGHTNESS_RE_PIN_A and thus will still be low
    else if (digitalRead(BPM_RE_PIN_B) == LOW) {
      bpm += 0.1;
      radioData.beatLength = round((60 / bpm) * 1000);
    }
  }
  bpm_pin_a_previous_state = bpm_pin_a_current_state;
}

void attempt_sync() {
  static unsigned long lastSyncButtonTime = 0;
  if (millis() - lastSyncButtonTime > 200) {
    radioData.shouldAttemptResync = true;
  }
  lastSyncButtonTime = millis();
}

void tap_bpm() {
  static volatile float first_tap_time;
  static volatile unsigned long last_tap_time;
  static volatile uint8_t tap_count;

  if (millis() - last_tap_time > 2000) {
    first_tap_time = millis();
    tap_count = 0;
  }

  else if (millis() - last_tap_time > 200) {
    tap_count++;
    tap_bpm_average = 60000 / ((millis() - first_tap_time) / tap_count);
    shouldUpdateDisplay = true;
  }
  last_tap_time = millis();
}

// Set BPM to tapped BPM
void set_bpm_from_tap() {
  bpm = round(tap_bpm_average);
  radioData.beatLength = lround((60 / bpm) * 1000);
}

void attempt_sync_flag() { sync_attempt_flag = true; }

void flag_mute_brightness() { should_mute_brightness = true; }

void tap_bpm_isr() { tap_bpm_flag = true; }

void poll_isr_flags() {
  if (should_mute_brightness) {
    mute_brightness();
    should_mute_brightness = false;
  }
  if (tap_bpm_flag) {
    tap_bpm();
    tap_bpm_flag = false;
  }
  if (sync_attempt_flag) {
    attempt_sync();
    sync_attempt_flag = false;
  }
}

void setup() {
  Serial.begin(9600);
  // radio setup
  radio.begin();
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MIN);
  radio.stopListening();

  // trellis setup
  if (!trellis.begin()) {
    Serial.println("failed to begin trellis");
    while (1)
      delay(1);
  }

  /* the array can be addressed as x,y or with the key number */
  for (int i = 0; i < Y_DIM * X_DIM; i++) {
    trellis.setPixelColor(
        i, Wheel(map(i, 0, X_DIM * Y_DIM, 0, 100))); // addressed with keynum
    trellis.show();
    delay(50);
  }

  for (int y = 0; y < Y_DIM; y++) {
    for (int x = 0; x < X_DIM; x++) {
      // activate rising and falling edges on all keys
      trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_RISING, true);
      trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_FALLING, true);
      trellis.registerCallback(x, y, blink);
      trellis.setPixelColor(x, y, 0x000000); // addressed with x,y
      trellis.show();                        // show all LEDs
      delay(50);
    }
  }

  // OLED setup address 0x3D for 128x64
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  updateDisplay();

  // Button and rotary Encoder pin setup
  // Initialize the pins as an input with arduino nano internal pullup resistors
  // activated
  pinMode(BRIGHTNESS_RE_PIN_A, INPUT_PULLUP);
  pinMode(BRIGHTNESS_RE_PIN_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BRIGHTNESS_RE_PIN_A), adjust_brightness,
                  CHANGE);
  pinMode(BRIGHTNESS_TOGGLE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BRIGHTNESS_TOGGLE_PIN),
                  flag_mute_brightness, FALLING);

  // pin and interrupt setup for rotary encoders
  pinMode(BPM_RE_PIN_A, INPUT_PULLUP);
  pinMode(BPM_RE_PIN_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BPM_RE_PIN_A), adjust_bpm, CHANGE);
  pinMode(SET_BPM_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SET_BPM_PIN), set_bpm_from_tap,
                  FALLING);
  pinMode(TAP_BPM_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TAP_BPM_PIN), tap_bpm_isr, FALLING);
  pinMode(BEAT_SYNC_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BEAT_SYNC_PIN), attempt_sync_flag,
                  FALLING);

  isInSetup = false;
}

void loop() {
  static radioData_t oldRadioData = radioData;
  static bool shouldSend = false;

  poll_isr_flags();
  trellis.read();

  if (!(oldRadioData == radioData)) {
    shouldSend = true;
    shouldUpdateDisplay = true;
  }

  if (shouldUpdateDisplay) {
    updateDisplay();
    shouldUpdateDisplay = false;
  }

  if (shouldSend) {
    shouldSend = !sendRadioData();
  }
}
