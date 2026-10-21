/*
  Electronic Door Lock with RGB Indicator
  Version 3 - infrared

  New in version 3:
    The 4x4 keypad is replaced by an IR remote and receiver.
    Only one remote is accepted, identified by its NEC address.
    Repeated frames from a held button are filtered out.

  Everything below the input layer is unchanged from version 2. The
  state machine, PIN comparison, colours and timers never learn that
  the input stopped being a keypad.

  The PIN can still be changed at runtime and is still lost on reset.
  Storing it in EEPROM is the next job.

  Pin connections:
  D2  -> IR receiver signal
  D10 -> Micro servo signal
  D11 -> RGB LED red channel through 220 ohm resistor
  D12 -> RGB LED green channel through 220 ohm resistor
  D13 -> RGB LED blue channel through 220 ohm resistor

  IR receiver VCC -> Arduino 5V
  IR receiver GND -> Arduino GND
  Servo power     -> Arduino 5V
  Servo GND       -> Arduino GND
  RGB common cathode -> Arduino GND

  D3 to D9 are now free. The keypad used to occupy D2 to D9.

  Note: the IR receiver claims Timer2, which disables PWM on D3 and
  D11. The RGB channels are driven with digitalWrite, so this costs
  nothing here, but analogWrite colour mixing is no longer possible
  without moving to a board with more timers.

  Remote buttons:
  0 to 9      Enter a digit
  Play/Pause  Submit
  Previous    Clear the digits typed so far
  FUNC/STOP   Start changing the PIN
  Power       Cancel and return to the locked state

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

#include <IRremote.hpp>
#include <Servo.h>


// ----- Pins -----

const int IR_RECEIVE_PIN = 2;

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


// ----- IR configuration -----

/*
  Every button on one remote shares an address. Checking it means a
  different remote in the room cannot drive this lock. It is a weak
  form of pairing, since the address is trivially discoverable, but
  it costs nothing and removes accidental interference.
*/
const uint16_t REMOTE_ADDRESS = 0xBF00;

// Ignore a second press of the same button within this window.
const unsigned long IR_DEBOUNCE_MS = 250;

/*
  Maps an IR command to the character the lock logic already uses.

  The digit codes were read off the remote with a reader sketch. The
  remote lays its buttons out in a grid of three columns, so the codes
  run 0x00 0x01 0x02, 0x04 0x05 0x06, and so on down the rows. Note
  that 0 is at 0x0C, in the row above the 1 to 9 block, not with them.

  The four function buttons are a choice rather than a property of the
  remote. These were picked so the printed icons match what they do.
*/
struct IRKey {
  uint8_t command;
  char key;
};

const IRKey IR_KEYMAP[] = {
  { 0x10, '1' }, { 0x11, '2' }, { 0x12, '3' },
  { 0x14, '4' }, { 0x15, '5' }, { 0x16, '6' },
  { 0x18, '7' }, { 0x19, '8' }, { 0x1A, '9' },
  { 0x0C, '0' },

  { 0x05, '#' },   // Play/Pause, submit
  { 0x04, '*' },   // Previous, clear
  { 0x02, 'A' },   // FUNC/STOP, start PIN change
  { 0x00, 'B' }    // Power, cancel
};

const byte IR_KEYMAP_SIZE = sizeof(IR_KEYMAP) / sizeof(IR_KEYMAP[0]);

// Remembers the last accepted press so repeats can be filtered.
uint8_t lastCommand = 0xFF;
unsigned long lastCommandTime = 0;


// ----- Objects -----

Servo doorServo;


// ----- System states -----

/*
  Version 1 could only do one thing: collect digits and compare them.
  Later versions have to ask for the old PIN, then a new PIN, then the
  new PIN again, so the sketch needs to remember which question it is
  asking.

  These are plain numbers rather than an enum on purpose. Arduino
  writes function prototypes for you and pastes them near the top of
  the sketch, above anything declared here, so a function that takes a
  custom type as an argument fails to compile in Tinkercad. Using byte
  avoids that.
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

  D12 and D13 are not PWM pins, and D11 lost its PWM to the IR
  receiver's timer, so each channel is simply on or off. That gives
  seven colours: the three channels alone, three pairs, and all three
  together. All seven are used.
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


// ----- Reading the remote -----

// Returns the mapped character, or 0 if the command is not a known button.
char irKeyFor(uint8_t command) {
  for (byte i = 0; i < IR_KEYMAP_SIZE; i++) {
    if (IR_KEYMAP[i].command == command) {
      return IR_KEYMAP[i].key;
    }
  }
  return 0;
}


/*
  Reads the receiver and returns a key character, or 0 if nothing
  usable arrived. This is the only part of the sketch that knows the
  input is infrared. Everything downstream sees the same characters
  the keypad used to produce, which is why the rest of version 2
  survived the change untouched.

  Two separate filters are needed. NEC marks held buttons with a
  repeat flag, but a held button also sends one genuine frame first,
  and a real remote often emits a second genuine frame on a slightly
  long press. The flag catches the first case and the time check
  catches the second. Without both, "1234" easily becomes "11223344".
*/
char readIRKey() {
  if (!IrReceiver.decode()) {
    return 0;
  }

  char key = 0;

  bool isRepeat =
    IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;

  bool fromOurRemote =
    IrReceiver.decodedIRData.address == REMOTE_ADDRESS;

  if (!isRepeat && fromOurRemote) {

    uint8_t command = IrReceiver.decodedIRData.command;
    unsigned long now = millis();

    bool tooSoon = (command == lastCommand) &&
                   (now - lastCommandTime < IR_DEBOUNCE_MS);

    if (!tooSoon) {
      key = irKeyFor(command);

      lastCommand = command;
      lastCommandTime = now;
    }
  }

  IrReceiver.resume();

  return key;
}


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

  Serial.println(F("Blue: locked. Enter PIN, then Play/Pause."));
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
  Serial.println(F("4 to 8 digits, then Play/Pause."));
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
  work at all. It also matters more with IR than it did with a keypad,
  since a frozen sketch misses incoming frames entirely.
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
    return;  // pressing submit on its own should do nothing
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

  // Power always cancels and returns to the locked state.
  if (key == 'B') {
    Serial.println(F("Cancelled."));
    enterLockedState();
    return;
  }

  // FUNC/STOP starts a PIN change, but only from the locked state.
  if (key == 'A') {
    if (state == STATE_LOCKED) {
      enterChangeOldState();
    }
    return;
  }

  // Previous clears the digits typed so far without leaving the
  // current question.
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

  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);

  // The system always starts on the default PIN.
  strcpy(currentPIN, DEFAULT_PIN);

  lastKeyTime = millis();

  Serial.println(F("Electronic Door Lock v3, infrared"));
  Serial.println(F("Play/Pause submits, Previous clears."));
  Serial.println(F("FUNC/STOP changes the PIN, Power cancels."));

  enterLockedState();
}


void loop() {
  char key = readIRKey();

  if (key) {
    lastKeyTime = millis();
    handleKey(key);
  }

  // Runs every pass, so nothing waits on a delay().
  updateTimers();
}
