/*
  Arduino Multiplexing 4-Digit 7-Segment Display Countdown Timer
  Easy change between Common Cathode and Common Anode Displays
  Stores values into the EEPROM
  max. countdown time 99:59 [min:sec]
  
  https://github.com/KernelPanicGR/Arduino-UV_Box-7-Segment-Display-Countdown-Timer
  
  https://vimeo.com/608781008
  
  
   _________________________________
  | Arduino: IDE 1.8.15             |
  | Board  : Arduino Nano           |
  | Project: UV_Box_Timer_7seg      |
  | Version: 1.05                   |
  | First version: 11/11/2017       |
  | Last update  : 17/09/2021       |
  | Author       : kernel panic     |
  |_________________________________|
   _________________________________
  |   7-Segment Display map         |
  |        AAA                      |
  |       F   B                     |
  |       F   B                     |
  |        GGG                      |
  |       E   C                     |
  |       E   C                     |
  |        DDD                      |
  |_________________________________|
*/

//============================================================
#include <EEPROM.h>

//============================================================
#define PIN_UP      0   // Button UP Countdown
#define PIN_DOWN    1   // Button Down Countdown
#define PIN_START   2   // Button Start or STOP
#define PIN_PAUSE   A4  // Reed switch for Pause
#define POWER_OUT   A3  // output pin For on off 
#define BUZZER      A2  // output BUZZER pin
//     arduino pins for segments
#define A       7
#define B       5
#define C       8
#define D       11
#define E       12
#define F       10
#define G       6
#define DP      9
//     arduino pins for digits
#define DIG_1   3
#define DIG_2   4
#define DIG_3   A1
#define DIG_4   A0

#define BEEP_SEC        6   // Beep for the last seconds
#define DB_MS           30  // Buttons Debounce time in milliseconds
#define EEPROM_MINUTE   0   // address of the EEPROM
#define EEPROM_SEC      1   // address of the EEPROM
#define SPEED_COUNTER   350 // milliseconds

#define ON  HIGH
#define OFF LOW
#define YES true
#define NO  false

// set clock -> start = 0x04,  stop = 0x00
#define START_TIMER TCCR1B = 0x04;
#define STOP_TIMER  TCCR1B = 0x00;
//============================================================

bool My_Display_is_CA = NO, // Change type Displays, YES=CA,  NO=CC

     FlagBeep = false,
     FlagEnd  = false,
     FlagSetSec = false,
     FlagSetMin = false,
     FlagStartCountdown = false,
     FlagSetUpDownTime = false;

// create an array of pins for BUTTONS
const uint8_t BUTTONS[] = {PIN_UP, PIN_DOWN, PIN_START, PIN_PAUSE};
const size_t  NBR_BTNS  = sizeof(BUTTONS);

uint16_t count, Speed; // for buttons  actions

//int8_t Minutes, Seconds;
int     Seconds;
uint8_t Minutes;

/*
  7 segments leds
  bits representing segments A through G and decimal point
  for numerals 0-9 and some characters
*/
const uint8_t SEGMENTS[21] = {
  //ABCDEFGdp segments
  B11111100, // 0 /  0
  B01100000, // 1 /  1
  B11011010, // 2 /  2
  B11110010, // 3 /  3
  B01100110, // 4 /  4
  B10110110, // 5 /  5
  B00111110, // 6 /  6
  B11100000, // 7 /  7
  B11111110, // 8 /  8
  B11100110, // 9 /  9
  B10110110, // S / 10
  B10011110, // E / 11
  B00011110, // t / 12
  B01101110, // H / 13
  B00001000, // i / 14
  B11001110, // P / 15
  B00000010, // - / 16
  B00000000, //   / 17 - shows nothing
  B00101010, // n / 18
  B01111010, // d / 19
  B00111010, // o / 20
};

const uint8_t SEGMENT[] = {DP, G, F, E, D, C, B, A};
const uint8_t NBR_DIGITS = 4; // the number of digits in the LED display
const uint8_t DIGIT[NBR_DIGITS] = {DIG_1, DIG_2, DIG_3, DIG_4};


/* ======================================================================
  Timer interrupt
  https://www.electronicsblog.net/examples-of-using-arduinoatmega-16-bit-hardware-timer-for-digital-clock/
  ======================================================================= */
ISR(TIMER1_OVF_vect) {
  Seconds --;

  if (Minutes == 0 && Seconds < BEEP_SEC) {
    FlagBeep = true;
  }

  digitalWrite(DP, !digitalRead(DP));
  TCNT1 = 0x0BDC ; // 0x0BDC set initial value to remove time error (16bit counter register)
}

//========================================================
// setup function
//========================================================
void setup() {
  /* make buttons pins as input */
  for (uint8_t btn = 0; btn < NBR_BTNS; btn++) {
    pinMode(BUTTONS[btn], INPUT_PULLUP);
  }

  /* setup pins as output */
  for (uint8_t s = 0; s < 8; s++)   {
    pinMode(SEGMENT[s], OUTPUT); //
    digitalWrite(SEGMENT[s], OFF);
  }

  for (uint8_t d = 0; d < NBR_DIGITS; d++) {
    pinMode(DIGIT[d], OUTPUT);
    digitalWrite(DIGIT[d], OFF);
  }

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(POWER_OUT, OUTPUT);

  digitalWrite(BUZZER, OFF);
  digitalWrite(POWER_OUT, OFF);
  digitalWrite(LED_BUILTIN, OFF);

  // Display Hi
  for (uint8_t i = 0; i < 60; i++) {
    showDigit(13, 1); // H
    showDigit(14, 2); // i
  }

  // timer
  TIMSK1 = 0x01;    // enabled global and timer overflow interrupt;
  TCCR1A = 0x00;    // normal operation (mode0);
  TCNT1  = 0x0BDC;   // 0x0BDC set initial value to remove time error (16bit counter register)
  STOP_TIMER;

  // EEPROM
  Minutes = EEPROM.read(EEPROM_MINUTE);  // read minute from address of the EEPROM
  Seconds = EEPROM.read(EEPROM_SEC);     // read seconds from address of the EEPROM

  if (Minutes > 99 || Seconds > 59)
    Minutes = 1, Seconds = 23;

  digitalWrite(DP, ON);
  StartMelody();
}//== Close setup =====

//========================================================
// loop
//========================================================
void loop() {
  if (!FlagSetUpDownTime && WasPressed(PIN_START))  {

    if (WasReleased(PIN_PAUSE)) {   // Pause
      Pause();
    } else {                        // Start Countdown
      FlagStartCountdown = true;
      StartCountdown();
    }

    if (FlagEnd)                    // End Countdown
      CountdownEND();
  }

  UpDownCounter();
  UpdateDisplay();

}//== Close loop =====

//========================================================
// StartCountdown
//========================================================
void StartCountdown() {
  //--- Store EEPROM only if different ---
  FlagBeep = false;
  if (EEPROM.read(EEPROM_MINUTE) != Minutes || EEPROM.read(EEPROM_SEC) != Seconds) {
    digitalWrite(DP, OFF);
    EEPROM.update(EEPROM_MINUTE, Minutes);
    EEPROM.update(EEPROM_SEC, Seconds);
    //-- Display SEt --
    for (uint8_t i = 0; i < 60; i++)   {
      showDigit (10, 0); // S
      showDigit (11, 1); // E
      showDigit (12, 2); // t
    }
  } //--Close Store EEPROM -----

  PlayStart();
  START_TIMER;
  digitalWrite(POWER_OUT, ON);

  //--- Countdown Loop ----
  while (FlagStartCountdown) {
    if (Minutes == 0 && Seconds == 0) {
      FlagEnd = true ;
      FlagStartCountdown = false ;
    }

    if (FlagBeep) {
      Beep(1760, 24);
      FlagBeep = false;
    }

    if (Seconds < 0) {
      Minutes --;
      Seconds = 59;
    }

    //--- Pause ----
    if (WasReleased(PIN_PAUSE)) {
      Pause();
      START_TIMER;
      digitalWrite(POWER_OUT, ON);
    }

    //-- STOP ----
    if (WasPressed(PIN_START)) {
      digitalWrite(POWER_OUT, OFF);
      STOP_TIMER;
      digitalWrite(DP, OFF);
      for (uint8_t i = 0; i < 80; i++) {
        showDigit (10, 0); // S
        showDigit (12, 1); // t
        showDigit (20, 2); // 0
        showDigit (15, 3); // P
      }
      FlagEnd = true ;
      FlagStartCountdown = false ;
    }

    UpdateDisplay();
  }//--Close while Countdown Loop ----

}// == Close StartCountdown() ===

//========================================================
// CountdownEND
//========================================================
void CountdownEND() {
  digitalWrite(POWER_OUT, OFF);
  STOP_TIMER;
  digitalWrite(DP, OFF);
  EndMelody();

  do {
    showDigit(11, 0); // E
    showDigit(18, 1); // n
    showDigit(19, 2); // d

    if (WasPressed(PIN_START)) {
      Minutes = EEPROM.read(EEPROM_MINUTE);
      Seconds = EEPROM.read(EEPROM_SEC);
      FlagEnd = false ;
    }
  }
  while (FlagEnd) ; // End
  digitalWrite(DP, ON);
}

//========================================================
// Set Up and Down Counter Time
//========================================================
void UpDownCounter() {
  // buttons state
  static bool BTN_UP = true,
              BTN_DOWN = true,
              oldBTN_UP = false,
              oldBTN_DOWN = false,
              BTN_Press = false;

  static uint32_t BtnTime; // buttons press time

  // Read the buttons
  BTN_UP   = digitalRead(PIN_UP);
  BTN_DOWN = digitalRead(PIN_DOWN);

  // if state changed since last read
  if (BTN_UP != oldBTN_UP | BTN_DOWN != oldBTN_DOWN) {
    oldBTN_UP   = BTN_UP;
    oldBTN_DOWN = BTN_DOWN;

    if (BTN_UP && BTN_DOWN) {
      BtnTime = millis();
      count = 0 ;
      Speed = SPEED_COUNTER;
      BTN_Press = false;
    }
  }

  if (!BTN_Press && !BTN_UP && !BTN_DOWN && millis() - BtnTime > SPEED_COUNTER) {
    FlagSetUpDownTime = true;
    FlagSetMin = true;
    FlagSetSec = false;
    BTN_Press  = true;
    Beep(1976, 32);
    Beep(1, 24);
    Beep(3951, 20);
    BtnTime = millis();
  }

  if (FlagSetUpDownTime && !BTN_Press) {
    if (WasPressed(PIN_START)) {
      if (FlagSetMin ) {
        FlagSetMin = false;
        FlagSetSec = true;
      }
      else {
        FlagSetMin = false;
        FlagSetSec = false;
        FlagSetUpDownTime = false;
        digitalWrite(LED_BUILTIN, OFF);
      }
      Beep(880, 20);
      Beep(1, 24);
      Beep(1760, 32);
    }

    // change Minutes
    if (FlagSetMin) {
      if (!BTN_UP && millis() - BtnTime > Speed) {
        Minutes ++ ;
        Minutes %= 100; //  After 99 Reset Minutes to 0
        ProcessSpeed();
        BtnTime = millis();
      }

      if (!BTN_DOWN && millis() - BtnTime > Speed) {
        if (Minutes > 99) Minutes = 0;
        else if (Minutes == 0) Minutes = 99;
        else Minutes --;

        ProcessSpeed();
        BtnTime = millis();
      }
    }

    // change Seconds
    if (FlagSetSec) {
      if (!BTN_UP && millis() - BtnTime > Speed) {
        Seconds ++ ;
        Seconds %= 60; // Reset Seconds after 59
        ProcessSpeed();
        BtnTime = millis();
      }

      if (!BTN_DOWN && millis() - BtnTime > Speed) {
        if (Seconds > 59) Seconds = 0;
        else if (Seconds == 0) Seconds = 59;
        else Seconds --;

        ProcessSpeed();
        BtnTime = millis();
      }
    }
  }

  // Blink led
  if (FlagSetUpDownTime)
    digitalWrite(LED_BUILTIN, (millis() / 250) % 2 ? HIGH : LOW);

}//== Close UpDownCounter() ===

//========================================================
// ProcessSpeed
//========================================================
void ProcessSpeed() {
  count++;
  if        (count > 30) Speed = 40;
  else if   (count > 20) Speed = 70;
  else if   (count > 9 ) Speed = 150; //
} //== Close ProcessSpeed() ===


//========================================================
// Pause
//========================================================
void Pause() {
  STOP_TIMER;
  digitalWrite(POWER_OUT, OFF);
  digitalWrite(DP, OFF);

  //-- Animation P --
  while (isReleased(PIN_PAUSE)) { //-- while --
    for (int8_t digit = NBR_DIGITS - 1; digit >= 0; digit--) {
      for (uint8_t i = 0; i < 50; i++) {
        showDigit (15, digit); // P
      }
    }
    for (uint8_t Dig = 1; Dig <= 2; Dig++) {
      for (uint8_t i = 0; i < 50; i++) {
        showDigit (15, Dig); // P
      }
    }
    Beep(3520, 36);
  }//--Close while Animation P ---
}//== Close Pause() =====

//======================================================================
// UpdateDisplay
//======================================================================
void UpdateDisplay() {  // mmss
  if (!FlagSetSec) {
    showDigit (Minutes / 10, 0);
    showDigit (Minutes % 10, 1);
  }
  if (!FlagSetMin) {
    showDigit (Seconds / 10, 2);
    showDigit (Seconds % 10, 3);
  }
}//==Close UpdateDisplay ====

//======================================================================
// showDigit
//======================================================================
// Displays given number on a 7-segment display at the given digit position
void showDigit(uint8_t number, uint8_t dig) {
  digitalWrite(DIGIT[dig], HIGH );

  for (uint8_t seg = 1; seg < 8; seg++) {
    // isBitSet will be true if given bit is 1
    bool isBitSet = bitRead(SEGMENTS[number], seg);

    // for Common Anode Display
    if (My_Display_is_CA)
      isBitSet = !isBitSet;

    digitalWrite(SEGMENT[seg], isBitSet);
  }

  delay(5); // delay before disabling the current digit so the human eye can observe it

  digitalWrite(DIGIT[dig], LOW);
}//==Close showDigit ====

/* ======================================================================
  Melody
  url: http://www.arduino.cc/en/Tutorial/Tone
  ======================================================================= */
//uint16_t EndMelody_note[]		= { 262, 196, 196, 220, 196, 0, 247, 262 }; // original
//uint16_t EndMelody_note[]		= {1047, 784, 784, 880, 784, 0, 988, 1047};
uint16_t EndMelody_note[]		= {4186, 3136, 3136, 3520, 3136, 0, 3951, 4186};

uint16_t EndMelody_durations[]	= {4, 8, 8, 4, 4, 4, 4, 4};

void EndMelody() {
  //iterate over the notes of the melody
  for (uint16_t thisNote = 0; thisNote < 8; thisNote++) {
    //to calculate the note duration, take one second. Divided by the note type
    uint16_t noteDuration = 1000 / EndMelody_durations[thisNote];
    tone(BUZZER, EndMelody_note [thisNote], noteDuration);

    //to distinguish the notes, set a minimum time between them
    uint16_t pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);

    //stop the tone playing
    noTone(BUZZER);
  }
}//==Close EndMelody ====

//=======================================================
// StartMelody
//=======================================================
uint16_t Start_note[]      = { 1175, 1047, 880, 1319, 1175 };
uint8_t Start_duration[]   = {10,  12, 10, 10, 4 };

void StartMelody() {
  for (uint16_t thisNote = 0; thisNote < (sizeof(Start_note) / sizeof(int)); thisNote++) {

    uint16_t noteDuration = 1000 / Start_duration[thisNote]; //convert duration to time delay
    tone(BUZZER, Start_note[thisNote], noteDuration);

    uint16_t pauseBetweenNotes = noteDuration * 1.30;//Here 1.30 is tempo, decrease to play it faster
    delay(pauseBetweenNotes);
    noTone(BUZZER); //stop music on pin
  }
}

//=======================================================
// PlayStart
//=======================================================
uint16_t PlayStart_note[]     = {2349, 2637 };
uint8_t  PlayStart_duration[] = {12,  8  };

void PlayStart() {
  for (uint16_t thisNote = 0; thisNote < (sizeof(PlayStart_note) / sizeof(int)); thisNote++) {

    uint16_t noteDuration = 1000 / PlayStart_duration[thisNote]; //convert duration to time delay
    tone(BUZZER, PlayStart_note[thisNote], noteDuration);

    uint16_t pauseBetweenNotes = noteDuration * 1.30;//Here 1.30 is tempo, decrease to play it faster
    delay(pauseBetweenNotes);
    noTone(BUZZER); //stop music on pin
  }
}

//=======================================================
// Beep
//=======================================================
void Beep(uint16_t freq, uint16_t duration) {
  uint16_t noteDuration = 1000 / duration; //convert duration to time delay
  tone(BUZZER, freq, noteDuration);

  uint16_t pauseBetweenNotes = noteDuration * 1.30;//Here 1.30 is tempo, decrease to play it faster
  delay(pauseBetweenNotes);

  noTone(BUZZER);
}

//=======================================================
// Buttons
//=======================================================
bool StableBtnLevel[20]; //
//=== CheckButton =======================================
bool CheckButton(uint8_t Btn_pin) {
  bool prvBtnLevel    = StableBtnLevel[Btn_pin];    // the previous steady state from the input pin
  bool newLevel       = getBtnStableLevel(Btn_pin);
  return prvBtnLevel != newLevel;
}//== Close CheckButton ==

bool getBtnStableLevel(uint8_t Btn_pin) {
  static bool OLDbtnLevels[20]; //
  static uint32_t BtnPressTime;
  // Reads a digital pin and filters it, returning the stable button position
  bool  pinLevel = digitalRead(Btn_pin);
  if (pinLevel != OLDbtnLevels[Btn_pin]) {
    BtnPressTime = millis();
    OLDbtnLevels[Btn_pin] = pinLevel;
  }
  // Debounce
  if (( millis() - BtnPressTime) > DB_MS) {
    StableBtnLevel[Btn_pin] = pinLevel;
  }
  return StableBtnLevel[Btn_pin];
}

// Read the state input pin
bool ReadThePin( uint8_t Pin ) {
  return digitalRead(Pin);
}

bool WasPressed( uint8_t Pin ) {
  return CheckButton(Pin) && !getBtnStableLevel(Pin);
}

bool WasReleased( uint8_t Pin ) {
  return CheckButton(Pin) && getBtnStableLevel(Pin);
}
// The button is pressed (with debounce)
bool isPressedDB( uint8_t Pin ) {
  return !getBtnStableLevel(Pin);
}
// The button is released (with debounce)
bool isReleasedDB( uint8_t Pin ) {
  return getBtnStableLevel(Pin);
}
// The button is pressed (without debounce)
bool isPressed( uint8_t Pin ) {
  return !ReadThePin(Pin);
}
// The button is released (without debounce)
bool isReleased( uint8_t Pin ) {
  return ReadThePin(Pin);
}
//== End Buttons ===


/*********( END Code )***********/
