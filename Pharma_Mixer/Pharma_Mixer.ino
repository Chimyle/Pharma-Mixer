#include "Wire.h"
#include "I2CKeyPad.h"
#include <LiquidCrystal_I2C.h>

const uint8_t KEYPAD_ADDRESS = 0x20;
I2CKeyPad keyPad(KEYPAD_ADDRESS);
char keys[] = "123A456B789C*0#DNF";

volatile bool keyChange = false;

String timeBuffer = "";  // MMSS input buffer
int mins = 0;
int secs = 0;
int Setpoint = 0;

LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C address might vary (0x27 is common)

void keyChanged() {
  keyChange = true;
}

void setup() {
  pinMode(3, INPUT_PULLUP);
  attachInterrupt(1, keyChanged, FALLING);
  keyChange = false;

  Wire.begin();
  Wire.setClock(100000);

  lcd.init();
  lcd.backlight();

  if (!keyPad.begin()) {
    lcd.setCursor(0, 0);
    lcd.print("Keypad ERROR!");
    while (1);
  }

  Reset();
}

void loop() {
  if (keyChange) {
    uint8_t index = keyPad.getKey();
    keyChange = false;

    if (index < 16) {
      char key = keys[index];

      if (key == '#') {
        if (timeBuffer.length() == 4) {
          mins = timeBuffer.substring(0, 2).toInt();
          secs = timeBuffer.substring(2, 4).toInt();
          SelectRPM();
        } else {
          lcd.setCursor(0, 0);
          lcd.print("INVALID");
          delay(2000);
          Reset();
        }
      } 
      else if (key == '*') {
        Reset();
      } 
      else {
        // Let InputTimer handle filtering and display
        InputTimer(index);
      }
    }
  }
}

void InputTimer(int index) {
  char key = keys[index];  // Get the key corresponding to index

  if (key >= '0' && key <= '9') {
    if (timeBuffer.length() >= 4) {
      timeBuffer = "";  // Reset buffer on overflow
    }

    timeBuffer += key;

    // Format MM:SS string
    String displayTime = timeBuffer;
    while (displayTime.length() < 4) displayTime += "_";
    String formatted = displayTime.substring(0, 2) + ":" + displayTime.substring(2, 4);

    // Display on top row, starting at col 8 (for MM:SS)
    lcd.setCursor(8, 0);
    lcd.print(formatted);
  }
}

void SelectRPM() {  
  while (true) {
    if (keyChange) {
      uint8_t index = keyPad.getKey();
      keyChange = false;

      char key = keys[index];

      switch (key) {
        case 'A':
          Setpoint = 600;
          break;
        case 'B':
          Setpoint = 1200;
          break;
        case 'C':
          Setpoint = 1800;
          break;
        case 'D':
          Setpoint = 2400;
          break;
        case '#':
          RunMotor();
          return;
        case '*':
          Reset();
          return;
        default:
          continue;  // ignore other keys
      }

      // Clear only the RPM value area (column 8 to 12)
      lcd.setCursor(8, 1);
      lcd.print("     ");  // 5 spaces to clear old value
      lcd.setCursor(8, 1);
      lcd.print(Setpoint);
    }
  }
}

void RunMotor(){
  while (rtc.now() < endTime){
    if (keyChange){
      PWM = 0;
      keyChange = false;
      break;
    }  
    lcd.setCursor(0, 0);
    lcd.print("Time Remaining:");
    lcd.setCursor(0, 1);
    lcd.print(endTime - rtc.now());
  }
}

void Reset() {
  // Clear variables
  timeBuffer = "";
  mins = 0;
  secs = 0;
  Setpoint = 0;

  // Clear and reset LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Duration:");
  lcd.setCursor(8, 0);  // position where MM:SS is shown
  lcd.print("__:__");
  lcd.setCursor(0, 1);
  lcd.print("RPM:");


  // Reset keyChange flag in case it was still true
  keyChange = false;
}
