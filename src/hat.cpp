#include <Arduino.h>
#include <FastLED.h>
#include <ArduinoSTL.h>

#include <interface.h>

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
RF24 radio(7, 8); // CE, CSN
const byte address[6] = "00001";
radioData_t radioData;

// oled libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_I2CDevice.h>

volatile uint8_t brightness = 30;
double bpm = 123;

bool sync_attempt = false;
long beat_length = lround((60/bpm)*1000);
long timing_error = 44;


// default all colours to blue
CRGB colour1 = CRGB::Blue;
CRGB colour2 = CRGB::Blue;
CRGB colour3 = CRGB::Blue;

// Initialise varialbes needed for FastLED 
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
const long NUM_LEDS = 140;
const uint8_t LED_DATA_PIN = 2;


CRGB leds[NUM_LEDS] = {0};
// max x and y values of LED matrix
const uint8_t MAX_X = 34;
const uint8_t MAX_Y = 3;

enum effect_choice : int { // enum encoding effect with whole and half denoting 1 and 1/2 beat length of effect respectively
    clear_display, // clear all leds
    wave_whole, // vertically rising horizontal wave with fading tail
    wave_half,
    flash_whole, // flash of vertical lines
    flash_half,
    twinkle_whole, // bright spots lighting every 16th note and slowing fading
    twinkle_half,
    strobe_whole
};

int wave_flash_double[] = {wave_whole, clear_display, flash_whole, clear_display, wave_whole,
                           clear_display, flash_half, clear_display, flash_half, clear_display};
int just_flash[] = {flash_whole, flash_whole, flash_whole, flash_whole};
int twinkle[] = {twinkle_whole, twinkle_whole, twinkle_whole, twinkle_whole};
int just_wave[] = {wave_whole, wave_whole, wave_whole, wave_whole,};
int wave_double[] = {wave_whole,wave_whole, wave_whole, wave_half, wave_half};
int double_flash_n_wave = {};
int strobe_bar[] = {strobe_whole, strobe_whole, strobe_whole, strobe_whole};
int strobe_colour[] = {};

enum colour_effect : long {  
  white = 0,
  red = 7,
  green = 11,
  blue = 3,
  red_white = 6,
  green_white = 10,
  blue_white = 2,
  purple = 1,
  cb = 12,
  yellow = 8,
  orange = 5,
  cd = 13,
  fire = 4,
  purue = 14,
  blue_red = 15,
  enum_wave_flash_double = 16,
  enum_just_flash = 17,
  enum_just_wave = 18,
  enum_wave_double = 19,
  enum_twinkle = 21,
  enum_strobe_bar = 20
};

enum colour_effect selected_effect = enum_just_wave;
enum colour_effect selected_colour = blue; // choose colour pallette (will be used in)

//-------------- Function prototypes //--------------
void twinkle_shaker(int);
void flash_and_fade(int);
void wave_effect(int);
void strobe(int);

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
    case yellow:
      colour1 = colour2 = colour3 = CRGB::Yellow;
      break;
    case orange:
      colour1 = colour2 = colour3 = CRGB::OrangeRed;
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
      colour1 = CRGB::OrangeRed;
      colour2 = CRGB::Green;
      colour3 = CRGB::Purple;
      break;
    case cd:
      colour1 = CRGB::Yellow;
      colour2 = CRGB::Blue;
      colour3 = CRGB::Purple;
      break;
    case fire:
      colour1 = CRGB::Yellow;
      colour2 = CRGB::Red;
      colour3 = CRGB::OrangeRed;
      break;
    case purue:
      colour1 = CRGB::Purple;
      colour2 = CRGB::Red;
      colour3 = CRGB::Blue;
      break;
    case blue_red:
      colour1 = colour2 = CRGB::Blue;
      colour3 = CRGB::Red;
  }
}

long text = "";
bool read_radio() {
    bool radio_available = radio.available();
    if (radio_available) {
        radio.read(&text, sizeof(text));
        Serial.println(text);
    }
    return radio_available;
}


void poll_radio() {
  if (read_radio()) 
  {
    if (text == 3000) 
    {
      sync_attempt = true;
    }
    else if (text > 1000 && text < 2000) 
    {
      beat_length = text - 1000;
      Serial.print("new beat_length");
      Serial.println(beat_length);
    }
    else if (text <= 256) 
    {
      brightness = text;
      FastLED.setBrightness(brightness);
      Serial.print("new brightness");
      Serial.println(brightness);
    }
    else if (text >= 2000 && text < 3000) 
    {
      long selection = text - 2000;
      if (selection <= 15) {
        selected_colour = selection;
        Serial.print("selected_colour\t");
        Serial.println(selected_colour);
      }
      else if (selection >= 16 && selection <= 31) 
      {
        selected_effect = selection;
        Serial.print("selected_effect\t");
        Serial.println(selection);
      }
    }
  }
}

//-------------- Effect creation functions //--------------

// Map any x, y coordinate on LED matrix to LED array index
int map_XY(int x, int y) {
  int i;
  if (y % 2 == 0) {
      i = x + (MAX_X+1)*y; // i steps up by MAX_X+1 each row
  }
  // LED strips setup in alternating direction so x value flips sign
  else {
      i = (MAX_X+1)*(y + 1) - (x + 1);
  }

  return i;
}

template <size_t N>
void play_effect_sequence(int (&effects_array)[N]){
  for (int effect: effects_array) { // play effects in the effect_array in order
    
    switch(effect) {
      case clear_display:
        FastLED.clear();
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
      case strobe_whole:
        strobe(1);
        break;
    }
  }
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
    case enum_twinkle:
      play_effect_sequence(twinkle);
      break;
    case enum_just_wave:
      play_effect_sequence(just_wave);
      break;
    case enum_wave_double:
      play_effect_sequence(wave_double);
      break;
    case enum_strobe_bar:
      play_effect_sequence(strobe_bar);
      break;
  }
}

void setup() {

  // radio setup
  Serial.begin(9600);
  radio.begin();
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_MIN);
  radio.startListening();

   // FastLED setup
  FastLED.addLeds<LED_TYPE,LED_DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(brightness);
}
unsigned long start_time;
void loop() {
  start_time = millis();
  sync_attempt = false;
  colour_select();
  play_selected_effect();

}




//-------------- LED effects //--------------
unsigned long beat_start_time;
unsigned long temp_beat_time;
void guarded_fastled_show(int note_length) {
  if ((((beat_length - timing_error) /note_length) - (temp_beat_time - beat_start_time)) > 7) {
    FastLED.show();
  }
  else {
    Serial.print("Ahhh");
  }
}

// 1
void wave_effect(int note_length) {    // note length is the fraction of a beat you want effect to play for
  if (!sync_attempt) {
    beat_start_time = millis();
    temp_beat_time = beat_start_time;
    int y = MAX_Y;
    while (((temp_beat_time - beat_start_time) < (beat_length - timing_error)/note_length) && (!sync_attempt)) {
      poll_radio(); // check if bpm_brightness display needs updating
      if (y >= 0) {
        EVERY_N_MILLISECONDS(60) {
          for (int x = 0; x <= MAX_X; x +=1) { // Fill x row
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
          guarded_fastled_show(note_length);
          y-=1;
        }
      }
      EVERY_N_MILLIS(2) {
        fadeToBlackBy(leds, NUM_LEDS, 10);
        guarded_fastled_show(note_length);
      }
      temp_beat_time = millis();
    }
    unsigned long actual_beat_length = (temp_beat_time - beat_start_time);
    unsigned long adjust_target_length = (beat_length - timing_error) / note_length;
    Serial.print("Actual Beat length:\t");
    Serial.println(actual_beat_length);
    Serial.print("Target Beat length:\t");
    Serial.println(beat_length);
  }
}

// 2
unsigned long demon_adjuster = 0;
void flash_and_fade(int note_length) { // Flash LED and decay to 0 every beat_length seconds
  if (!sync_attempt) { 
    unsigned long beat_start_time = millis();
    unsigned long temp_beat_time = beat_start_time;      
  if (note_length == 1) {
    demon_adjuster = 5;
  }
  else {
    demon_adjuster = 0;
  }

    for (int x = 0; x <= MAX_X; x +=2) { // fill x 
      for(int y = 0; y<= MAX_Y; y++){    // fill y
        leds[map_XY(x,y)] = colour1;
      } 
    }
    FastLED.show();
    while (((temp_beat_time - beat_start_time) < ((beat_length - (timing_error + demon_adjuster)) / note_length)) && (!sync_attempt)) {
      poll_radio();
      EVERY_N_MILLIS(5) {
        fadeToBlackBy(leds, NUM_LEDS, 10);
        if (note_length == 1) {
          FastLED.show();
        }
        else {
          guarded_fastled_show(note_length);
        }
      }
      temp_beat_time = millis();
    }
    unsigned long actual_beat_length = (temp_beat_time - beat_start_time);
    unsigned long adjust_target_length = (beat_length - timing_error) / note_length;
    Serial.print("Actual Beat length:\t");
    Serial.println(actual_beat_length);
    Serial.print("Target Beat length:\t");
    Serial.println(beat_length);
    
    // if (actual_beat_length > adjust_target_length) {
    //   timing_error -= actual_beat_length - adjust_target_length;
    // }
    // else if(actual_beat_length < adjust_target_length){
    //   timing_error += actual_beat_length - adjust_target_length;
    // }
    // Serial.print("timing_error:\t");
    // Serial.println(timing_error);
  }
}

 // 3
void twinkle_shaker(int note_length){ // add new bright spots every quarter note
  if (!sync_attempt) { 
    unsigned long beat_start_time = millis();
    unsigned long temp_beat_time = beat_start_time; 
    while (((temp_beat_time - beat_start_time) < beat_length / note_length) && (!sync_attempt)) {
      poll_radio();
      EVERY_N_MILLIS(beat_length/8){
        fadeToBlackBy(leds, NUM_LEDS, 25);
        FastLED.show();
        int rand_x1 = random(MAX_X+1);
        int rand_y1 = random(MAX_Y+1);
        leds[map_XY(rand_x1,rand_y1)] = colour1;
        int rand_x2 = random(MAX_X+1);
        int rand_y2 = random(MAX_Y+1);
        leds[map_XY(rand_x2,rand_y2)] = colour2;
        int rand_x3 = random(MAX_X+1);
        int rand_y3 = random(MAX_Y+1);
        leds[map_XY(rand_x3,rand_y3)] = colour3;
        int rand_x4 = random(MAX_X+1);
        int rand_y4 = random(MAX_Y+1);
        leds[map_XY(rand_x4,rand_y4)] = colour1;

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

// 2

void strobe(int note_length) { // Flash LED and decay to 0 every beat_length seconds
  if (!sync_attempt) { 
    unsigned long beat_start_time = millis();
    unsigned long temp_beat_time = beat_start_time;      
    while (((temp_beat_time - beat_start_time) < beat_length) && (!sync_attempt)) {
      poll_radio();

      FastLED.clear();
      EVERY_N_MILLISECONDS(50) {
        fill_solid(leds, NUM_LEDS, CRGB::White);
        guarded_fastled_show(note_length);
      }

      EVERY_N_MILLISECONDS(100) {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        guarded_fastled_show(note_length);
      }
      temp_beat_time = millis();
    }
  }
}



