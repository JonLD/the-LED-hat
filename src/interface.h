struct radioData_t {
    uint8_t colourOrEffect;
    uint8_t brightness;
    uint8_t beatLength;
    bool shouldAttemptResync;

    bool operator==(const radioData_t &other) const 
    {
    return (
        colourOrEffect == other.colourOrEffect && 
        brightness == other.brightness &&
        beatLength == other.beatLength &&
        shouldAttemptResync == other.shouldAttemptResync);
    }

};

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
  fire = 5,
  purue = 14,
  blue_red = 15,
  enum_wave_flash_double = 16,
  enum_just_flash = 17,
  enum_just_wave = 18,
  enum_wave_double = 19,
  enum_twinkle = 21,
  enum_strobe_bar = 20
};
