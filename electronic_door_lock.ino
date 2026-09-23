/*
  Electronic Door Lock with RGB Indicator

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

  Demonstration PIN: 1234
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


// ----- Circuit values -----

// Each RGB LED channel uses a 220 ohm current-limiting resistor.
const int RGB_RESISTOR_OHMS = 220;


// ----- Settings -----

const int LOCKED_POSITION = 0;
const int UNLOCKED_POSITION = 90;

const unsigned long UNLOCK_TIME_MS = 5000;
const unsigned long DENIED_TIME_MS = 2000;

// This PIN is hard-coded because this is a demonstration project.
const String CORRECT_PIN = "1234";

// Prevent an excessively long input from being stored.
const byte MAX_PIN_LENGTH = 10;


// ----- Keypad configuration -----

const byte ROWS = 4;
const byte COLS = 4;

// This map represents the labels printed on the 4x4 keypad.
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {
  ROW_1_PIN,
  ROW_2_PIN,
  ROW_3_PIN,
  ROW_4_PIN
};

byte colPins[COLS] = {
  COL_1_PIN,
  COL_2_PIN,
  COL_3_PIN,
  COL_4_PIN
};

Keypad keypad = Keypad(
  makeKeymap(keys),
  rowPins,
  colPins,
  ROWS,
  COLS
);


// ----- Objects and variables -----

Servo doorServo;

// Stores the digits entered by the user.
String enteredPIN = "";


// Sets the common-cathode RGB LED to red, green, or blue values.
void setRGB(int red, int green, int blue) {
  /*
    The project only uses values of 0 and 255.

    D12 and D13 are not PWM pins on the Arduino Uno, but analogWrite()
    still works for these two extreme values as simple LOW/HIGH outputs.
  */
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, blue);
}


// Shows the normal locked state using the blue LED.
void showLockedState() {
  setRGB(0, 0, 255);
}


// Grants access, unlocks the servo, waits, and then locks it again.
void grantAccess() {
  Serial.println("ACCESS GRANTED");

  // Green indicates that the entered PIN was correct.
  setRGB(0, 255, 0);

  // Rotate the servo by 90 degrees to represent unlocking the door.
  doorServo.write(UNLOCKED_POSITION);

  Serial.println("Door unlocked");

  // Keep the simulated door unlocked for five seconds.
  delay(UNLOCK_TIME_MS);

  // Return the servo to its original locked position.
  doorServo.write(LOCKED_POSITION);

  // Blue indicates that the system is locked and ready again.
  showLockedState();

  Serial.println("Door locked");
}


// Denies access and displays the red status for two seconds.
void denyAccess() {
  Serial.println("ACCESS DENIED");

  // Red indicates that the entered PIN was incorrect.
  setRGB(255, 0, 0);

  // Keep the warning visible long enough to be noticed.
  delay(DENIED_TIME_MS);

  // Return to the normal locked state.
  showLockedState();
}


// Compares the entered PIN with the demonstration PIN.
void checkPIN() {
  if (enteredPIN == CORRECT_PIN) {
    grantAccess();
  } else {
    denyAccess();
  }

  // Forget the previous attempt before accepting another PIN.
  enteredPIN = "";

  Serial.println("Ready for next PIN.");
}


// Clears the PIN currently being entered.
void clearPIN() {
  enteredPIN = "";

  // The system remains locked and returns to its normal blue state.
  showLockedState();

  Serial.println("PIN entry cleared.");
}


void setup() {
  // Configure the three RGB LED channels as outputs.
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  // Connect the Servo library to the servo signal pin.
  doorServo.attach(SERVO_PIN);

  // Start with the physical lock in its locked position.
  doorServo.write(LOCKED_POSITION);

  // Blue is the startup indication for locked and ready.
  showLockedState();

  // Start serial communication for debugging and observing system events.
  Serial.begin(9600);

  Serial.println("Electronic Door Lock");
  Serial.println("System started.");
  Serial.println("Blue = locked and waiting.");
  Serial.println("Enter PIN, press # to submit, or * to clear.");
}


void loop() {

  // ----- 1) Read keypad -----

  // getKey() returns a character when a key has been pressed.
  char key = keypad.getKey();

  // If no key was pressed, there is nothing else to process this cycle.
  if (!key) {
    return;
  }


  // ----- 2) Process control keys -----

  // The * key clears all digits entered so far.
  if (key == '*') {
    clearPIN();
    return;
  }

  // The # key submits the current PIN.
  if (key == '#') {
    Serial.println("PIN submitted.");
    checkPIN();
    return;
  }


  // ----- 3) Store numeric input -----

  // Only number keys are accepted as part of the PIN.
  if (key >= '0' && key <= '9') {

    // Stop accepting digits when the maximum input length is reached.
    if (enteredPIN.length() < MAX_PIN_LENGTH) {
      enteredPIN += key;

      /*
        Do not print the actual entered digit.
        Printing * demonstrates how a real system can hide PIN input.
      */
      Serial.print("*");
    }
  }


  // ----- 4) Serial Monitor -----

  /*
    Access results are printed by grantAccess(), denyAccess(),
    checkPIN(), and clearPIN().

    The entered PIN itself is intentionally not sent to the
    Serial Monitor because PINs should not normally be exposed.
  */
}
