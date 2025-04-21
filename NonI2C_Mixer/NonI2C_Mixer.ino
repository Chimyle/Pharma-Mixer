#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#define LED_BUILTIN 1

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS3231 rtc;

// Keypad setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {19, 18, 5, 17};  // R1-R4
byte colPins[COLS] = {16, 4, 0, 2};    // C1-C4

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Time and control variables
String timeBuffer = "";  // MMSS input buffer
int mins = 0;
int secs = 0;
int Setpoint = 0;

DateTime endTime;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // Turn LED off (active low)

  Wire.begin();
  lcd.init();
  lcd.backlight();

  if (!rtc.begin()) {
    lcd.setCursor(0, 0);
    lcd.print("RTC not found!");
    while (1);
  }

  Reset();
}

void loop() {
  char key = keypad.getKey();
  if (key) {
    if (key >= '0' && key <= '9') {
      InputTimer(key);
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

void InputTimer(char key) {
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
    char key = keypad.getKey();
    if (key) {
      switch (key) {
        case 'A':
          Setpoint = 25;
          break;
        case 'B':
          Setpoint = 40;
          break;
        case 'C':
          Setpoint = 70;
          break;
        case 'D':
          Setpoint = 90;
          break;
        case '#':
          if (Setpoint > 0){
            if (mins > 0 || secs > 0) {
              endTime = rtc.now() + TimeSpan(0, 0, mins, secs);
              RunMotor();
              return;
            }
          }
          break;
        case '*':
          Reset();
          return;
          break;
      }

      if (Setpoint > 0) {
        lcd.setCursor(5, 1);
        lcd.print("      "); // Clear old RPM
        lcd.setCursor(5, 1);
        lcd.print(Setpoint);
      }
    }
  }
}

void RunMotor() {
  lcd.setCursor(0, 0);
  lcd.print("Time Remaining:");
  digitalWrite(LED_BUILTIN, LOW);  // Turn on LED (active low)

  while (rtc.now() < endTime) {
    char key = keypad.getKey();
    if (key) {
      digitalWrite(LED_BUILTIN, HIGH);  // Turn off LED
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
    if (keypad.getKey()) {
      Reset();
      return;
    }
  }
}

void Reset() {
  timeBuffer = "";
  mins = 0;
  secs = 0;
  Setpoint = 0;

  lcd.clear();
  lcd.print("Timer:         ");
  lcd.setCursor(8, 0);
  lcd.print("__:__  ");
  //lcd.setCursor(0, 1);
  //lcd.print("RPM:         ");
}
