/*
  Electronic Door Lock with RGB Indicator
  Version 2

  New in version 2:
    The user can change the PIN from the keypad by pressing A.
    The RGB LED now uses all seven of its colours to show which
    question the system is asking.

  The PIN starts as 1234 every time the simulation starts. A changed PIN
  lasts until the simulation is stopped. 

  The 16x2 I2C LCD was attempted again in this version and dropped. See
  the README for what was tried.

  Pin connections:
  D2  -> Keypad Row 1
  D3  -> Keypad Row 2
  D4  -> Keypad Row 3
  D5  -> Keypad Row 4
  D6  -> Keypad Column 1
  D7  -> Keypad Column 2
  D8  -> Keypad Column 3
  D9  -> Keypad Column 4
  D10 -> Micro servo signal
  D11 -> RGB LED red channel through 220 ohm resistor
  D12 -> RGB LED green channel through 220 ohm resistor
  D13 -> RGB LED blue channel through 220 ohm resistor

  Servo power -> Arduino 5V
  Servo GND   -> Arduino GND
  RGB common cathode -> Arduino GND

  Keys:
  0 to 9  Enter a digit
  #       Submit
  *       Clear the digits typed so far
  A       Start changing the PIN
  B       Cancel and return to the locked state

  Colours:
  Blue     Locked, waiting for a PIN
  Green    Access granted, door open
  Red      Refused, or an entry was rejected
  Magenta  Change PIN, step 1, asking for the current PIN
  Cyan     Change PIN, step 2, asking for the new PIN
  Yellow   Change PIN, step 3, asking for the new PIN again
  White    The PIN was changed

  Default PIN: 1234
*/

#include <Keypad.h>
#include <Servo.h>


// ----- Pins -----

const byte ROW_1_PIN = 2;
const byte ROW_2_PIN = 3;
const byte ROW_3_PIN = 4;
const byte ROW_4_PIN = 5;

const byte COL_1_PIN = 6;
const byte COL_2_PIN = 7;
const byte COL_3_PIN = 8;
const byte COL_4_PIN = 9;

const int SERVO_PIN = 10;

const int RED_PIN = 11;
const int GREEN_PIN = 12;
const int BLUE_PIN = 13;


// ----- Settings -----

const int LOCKED_POSITION = 0;
const int UNLOCKED_POSITION = 90;

const unsigned long UNLOCK_TIME_MS = 5000;     // how long the door stays open
const unsigned long MESSAGE_TIME_MS = 2000;    // how long a result colour is held
const unsigned long CHANGE_TIMEOUT_MS = 15000; // an abandoned PIN change gives up

// A new PIN must be between these lengths.
const byte MIN_PIN_LENGTH = 4;
const byte MAX_PIN_LENGTH = 8;

// The PIN the system always starts with.
const char DEFAULT_PIN[] = "1234";


// ----- Keypad configuration -----

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = { ROW_1_PIN, ROW_2_PIN, ROW_3_PIN, ROW_4_PIN };
byte colPins[COLS] = { COL_1_PIN, COL_2_PIN, COL_3_PIN, COL_4_PIN };

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);


// ----- Objects -----

Servo doorServo;


// ----- System states -----

/*
  Version 1 could only do one thing: collect digits and compare them.
  Version 2 has to ask for the old PIN, then a new PIN, then the new PIN
  again, so the sketch needs to remember which question it is asking.

  These are plain numbers rather than an enum on purpose. Arduino writes
  function prototypes for you and pastes them near the top of the sketch,
  above anything declared here, so a function that takes a custom type as
  an argument fails to compile in Tinkercad. Using byte avoids that.
*/
const byte STATE_LOCKED = 0;          // waiting for a PIN
const byte STATE_UNLOCKED = 1;        // servo open, counting down to relock
const byte STATE_MESSAGE = 2;         // holding a result colour for a moment
const byte STATE_CHANGE_OLD = 3;      // asking for the current PIN
const byte STATE_CHANGE_NEW = 4;      // asking for the new PIN
const byte STATE_CHANGE_CONFIRM = 5;  // asking for the new PIN a second time

byte state = STATE_LOCKED;

// Where to go once the current result colour has been shown long enough.
byte stateAfterMessage = STATE_LOCKED;


// ----- Variables -----

// The PIN that currently opens the door.
char currentPIN[MAX_PIN_LENGTH + 1];

// The digits typed so far.
char entry[MAX_PIN_LENGTH + 1];
byte entryLength = 0;

// The new PIN, held between the "new PIN" and "repeat it" questions.
char newPIN[MAX_PIN_LENGTH + 1];

// Used for the unlock timer and the result colour timer.
unsigned long stateTimer = 0;

// Used to abandon a half finished PIN change.
unsigned long lastKeyTime = 0;


// ----- Colours -----

/*
  The LED is common cathode, so each channel lights when its pin is HIGH.

  D12 and D13 are not PWM pins, so each channel is simply on or off. That
  gives seven colours: the three channels alone, three pairs, and all
  three together. Version 2 uses all seven.
*/
void setRGB(byte red, byte green, byte blue) {
  digitalWrite(RED_PIN, red);
  digitalWrite(GREEN_PIN, green);
  digitalWrite(BLUE_PIN, blue);
}

void colourBlue()    { setRGB(LOW,  LOW,  HIGH); }  // locked
void colourGreen()   { setRGB(LOW,  HIGH, LOW ); }  // granted
void colourRed()     { setRGB(HIGH, LOW,  LOW ); }  // refused
void colourMagenta() { setRGB(HIGH, LOW,  HIGH); }  // change step 1
void colourCyan()    { setRGB(LOW,  HIGH, HIGH); }  // change step 2
void colourYellow()  { setRGB(HIGH, HIGH, LOW ); }  // change step 3
void colourWhite()   { setRGB(HIGH, HIGH, HIGH); }  // PIN changed


// ----- Small helpers -----

// Forgets the digits typed so far.
void clearEntry() {
  entryLength = 0;
  entry[0] = '\0';
}


// ----- Entering each state -----

void enterLockedState() {
  clearEntry();
  colourBlue();
  state = STATE_LOCKED;

  Serial.println(F("Blue: locked. Enter PIN, then #."));
}


void enterUnlockedState() {
  clearEntry();
  colourGreen();
  doorServo.write(UNLOCKED_POSITION);

  stateTimer = millis();
  state = STATE_UNLOCKED;

  Serial.println(F("ACCESS GRANTED"));
  Serial.println(F("Green: door unlocked."));
}


void enterChangeOldState() {
  clearEntry();
  colourMagenta();
  state = STATE_CHANGE_OLD;

  Serial.println(F("Magenta: changing PIN."));
  Serial.println(F("Step 1 of 3, enter the current PIN."));
}


void enterChangeNewState() {
  clearEntry();
  colourCyan();
  state = STATE_CHANGE_NEW;

  Serial.println(F("Cyan: step 2 of 3, enter a new PIN."));
  Serial.println(F("4 to 8 digits, then #."));
}


void enterChangeConfirmState() {
  clearEntry();
  colourYellow();
  state = STATE_CHANGE_CONFIRM;

  Serial.println(F("Yellow: step 3 of 3, enter the new PIN again."));
}


// Sends the system to one of the states above.
void applyState(byte target) {
  switch (target) {
    case STATE_CHANGE_OLD:     enterChangeOldState();     break;
    case STATE_CHANGE_NEW:     enterChangeNewState();     break;
    case STATE_CHANGE_CONFIRM: enterChangeConfirmState(); break;
    default:                   enterLockedState();        break;
  }
}


/*
  Holds a result colour for a moment, then moves on by itself.

  Version 1 used delay() here, which froze the whole sketch for two or
  five seconds at a time. Nothing is frozen now: the colour simply
  expires in updateTimers(), which is what lets a three step PIN change
  work at all.
*/
void showResult(byte red, byte green, byte blue, byte nextState) {
  setRGB(red, green, blue);
  stateAfterMessage = nextState;
  stateTimer = millis();
  state = STATE_MESSAGE;
}


// ----- Handling a key press -----

void handleDigit(char key) {
  if (entryLength >= MAX_PIN_LENGTH) {
    return;  // the buffer is full, ignore further digits
  }

  entry[entryLength] = key;
  entryLength++;
  entry[entryLength] = '\0';

  // The digit itself is never printed, only a mask.
  Serial.print('*');
}


void handleSubmitLocked() {
  if (entryLength == 0) {
    return;  // pressing # on its own should do nothing
  }

  Serial.println();

  if (strcmp(entry, currentPIN) == 0) {
    enterUnlockedState();
  } else {
    Serial.println(F("ACCESS DENIED"));
    Serial.println(F("Red: wrong PIN."));
    showResult(HIGH, LOW, LOW, STATE_LOCKED);
  }
}


void handleSubmitChangeOld() {
  if (entryLength == 0) {
    return;
  }

  Serial.println();

  if (strcmp(entry, currentPIN) == 0) {
    enterChangeNewState();
  } else {
    Serial.println(F("Red: wrong PIN, change refused."));
    showResult(HIGH, LOW, LOW, STATE_LOCKED);
  }
}


void handleSubmitChangeNew() {
  Serial.println();

  if (entryLength < MIN_PIN_LENGTH) {
    Serial.println(F("Red: too short, use 4 to 8 digits."));
    showResult(HIGH, LOW, LOW, STATE_CHANGE_NEW);
    return;
  }

  // Remember this attempt so it can be compared with the repeat.
  strcpy(newPIN, entry);
  enterChangeConfirmState();
}


void handleSubmitChangeConfirm() {
  Serial.println();

  if (strcmp(entry, newPIN) == 0) {
    strcpy(currentPIN, newPIN);
    Serial.println(F("White: PIN changed."));
    showResult(HIGH, HIGH, HIGH, STATE_LOCKED);
  } else {
    Serial.println(F("Red: the two entries did not match."));
    showResult(HIGH, LOW, LOW, STATE_CHANGE_NEW);
  }
}


void handleKey(char key) {
  // While the door is open or a result colour is showing, keys are ignored.
  if (state == STATE_UNLOCKED || state == STATE_MESSAGE) {
    return;
  }

  // B always cancels and returns to the locked state.
  if (key == 'B') {
    Serial.println(F("Cancelled."));
    enterLockedState();
    return;
  }

  // A starts a PIN change, but only from the normal locked state.
  if (key == 'A') {
    if (state == STATE_LOCKED) {
      enterChangeOldState();
    }
    return;
  }

  // C and D are not used in this version.
  if (key == 'C' || key == 'D') {
    return;
  }

  // * clears the digits typed so far without leaving the current question.
  if (key == '*') {
    clearEntry();
    Serial.println(F("Entry cleared."));
    return;
  }

  if (key == '#') {
    switch (state) {
      case STATE_LOCKED:         handleSubmitLocked();        break;
      case STATE_CHANGE_OLD:     handleSubmitChangeOld();     break;
      case STATE_CHANGE_NEW:     handleSubmitChangeNew();     break;
      case STATE_CHANGE_CONFIRM: handleSubmitChangeConfirm(); break;
      default: break;
    }
    return;
  }

  if (key >= '0' && key <= '9') {
    handleDigit(key);
  }
}


// ----- Things that happen on their own -----

void updateTimers() {
  unsigned long now = millis();

  if (state == STATE_UNLOCKED) {
    if (now - stateTimer >= UNLOCK_TIME_MS) {
      doorServo.write(LOCKED_POSITION);
      Serial.println(F("Door locked"));
      enterLockedState();
    }
    return;
  }

  if (state == STATE_MESSAGE) {
    if (now - stateTimer >= MESSAGE_TIME_MS) {
      applyState(stateAfterMessage);
    }
    return;
  }

  // A PIN change left half finished should not stay open forever.
  if (state == STATE_CHANGE_OLD ||
      state == STATE_CHANGE_NEW ||
      state == STATE_CHANGE_CONFIRM) {

    if (now - lastKeyTime >= CHANGE_TIMEOUT_MS) {
      Serial.println(F("PIN change timed out. PIN unchanged."));
      showResult(HIGH, LOW, LOW, STATE_LOCKED);
    }
  }
}


// ----- Setup and main loop -----

void setup() {
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  doorServo.attach(SERVO_PIN);
  doorServo.write(LOCKED_POSITION);

  Serial.begin(9600);

  // The system always starts on the default PIN.
  strcpy(currentPIN, DEFAULT_PIN);

  lastKeyTime = millis();

  Serial.println(F("Electronic Door Lock v2"));
  Serial.println(F("# submits, * clears."));
  Serial.println(F("A changes the PIN, B cancels."));

  enterLockedState();
}


void loop() {
  char key = keypad.getKey();

  if (key) {
    lastKeyTime = millis();
    handleKey(key);
  }

  // Runs every pass, so nothing waits on a delay().
  updateTimers();
}
