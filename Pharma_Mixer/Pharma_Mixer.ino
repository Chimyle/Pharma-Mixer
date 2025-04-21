#include "Wire.h"
#include "I2CKeyPad.h"
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#define LED_BUILTIN 1

// I2C Addresses
const uint8_t KEYPAD_ADDRESS = 0x20;
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Adjust if needed
I2CKeyPad keyPad(KEYPAD_ADDRESS);
RTC_DS3231 rtc;

// Keypad map
char keys[] = "123A456B789C*0#DNF";

volatile bool keyChange = false;

// Time and control variables
String timeBuffer = "";  // MMSS input buffer
int mins = 0;
int secs = 0;
int Setpoint = 0;

DateTime endTime;

// ISR for keypad change
void keyChanged() {
  keyChange = true;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(3, INPUT_PULLUP);
  attachInterrupt(1, keyChanged, FALLING); //Check interrupt pin
  keyChange = false;

  Wire.begin();
  Wire.setClock(100000);

  lcd.init();
  lcd.backlight();

  if (!rtc.begin()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RTC not found!");
    while (1);
  }
  
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

      if (key >= '0' && key <= '9') {
        InputTimer(index);
      } 
      else if (key == '#') {
        if (timeBuffer.length() == 4) {
          mins = timeBuffer.substring(0, 2).toInt();
          secs = timeBuffer.substring(2, 4).toInt();
          SelectRPM();
        } else {
          lcd.setCursor(0, 0);
          lcd.print("Invalid Time   ");
          delay(2000);
          Reset();
        }
      } 
      else if (key == '*') {
        Reset();
      }
    }
  }
}

void InputTimer(int index) {
  char key = keys[index];

  if (timeBuffer.length() >= 4) {
    timeBuffer = "";
  }

  timeBuffer += key;

  String displayTime = timeBuffer;
  while (displayTime.length() < 4) displayTime += "_";
  String formatted = displayTime.substring(0, 2) + ":" + displayTime.substring(2, 4);

  lcd.setCursor(8, 0);
  lcd.print(formatted);
}

void SelectRPM() {
  lcd.setCursor(0, 1);
  lcd.print("RPM:           ");  // Clear RPM line

  while (true) {
    if (keyChange) {
      uint8_t index = keyPad.getKey();
      keyChange = false;

      char key = keys[index];

      switch (key) {
        case 'A':
          Setpoint = 25;
          lcd.setCursor(5, 1);
          lcd.print("      "); // Clear old RPM
          lcd.setCursor(5, 1);
          lcd.print(Setpoint);
          break;
        case 'B':
          Setpoint = 40;
          lcd.setCursor(5, 1);
          lcd.print("      "); // Clear old RPM
          lcd.setCursor(5, 1);
          lcd.print(Setpoint);
          break;
        case 'C':
          Setpoint = 70;
          lcd.setCursor(5, 1);
          lcd.print("      "); // Clear old RPM
          lcd.setCursor(5, 1);
          lcd.print(Setpoint);
          break;
        case 'D':
          Setpoint = 90;
          lcd.setCursor(5, 1);
          lcd.print("      "); // Clear old RPM
          lcd.setCursor(5, 1);
          lcd.print(Setpoint);
          break;
        case '#':
          if (mins > 0 || secs > 0) {
            endTime = rtc.now() + TimeSpan(0, 0, mins, secs);
            RunMotor();
            return;
          }
          break;
        case '*':
          Reset();
          return;
          break;
      }
    }
  }
}

void RunMotor() {
  lcd.setCursor(0, 0);
  lcd.print("Time Remaining:");
  digitalWrite(LED_BUILTIN, LOW);  // Turn on LED during motor run

  while (rtc.now() < endTime) {
    if (keyChange) {
      keyChange = false;
      digitalWrite(LED_BUILTIN, HIGH);  // Turn off LED before exit
      Reset();
      return;
    }

    DateTime now = rtc.now();
    TimeSpan remaining = endTime - now;

    char buffer[9];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", remaining.hours(), remaining.minutes(), remaining.seconds());

    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.print(buffer);

    delay(250);
  }

  digitalWrite(LED_BUILTIN, HIGH);  // Turn off LED after timer ends
  lcd.setCursor(0, 1);
  lcd.print("Done!           ");

  // Wait for any key to be pressed before Reset
  while (true) {
    if (keyChange) {
      keyChange = false;
      Reset();
      return;
    }
  }
}



void Reset() {
  // Clear variables
  timeBuffer = "";
  mins = 0;
  secs = 0;
  Setpoint = 0;
  digitalWrite(LED_BUILTIN, HIGH);

  // Reset display fields only (no full clear)
  lcd.clear();
  lcd.print("Timer:         ");
  lcd.setCursor(8, 0);
  lcd.print("__:__  ");

  keyChange = false;
}
