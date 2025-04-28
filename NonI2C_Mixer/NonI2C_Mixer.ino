#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>

#define LED_BUILTIN 1
#define PUL_PIN 33
#define DIR_PIN 32
#define ENA_PIN 35

// PWM config
#define PWM_CHANNEL 0
#define PWM_RESOLUTION 8  // 8-bit resolution
#define PWM_DUTY_CYCLE 127 // 50% duty cycle

LiquidCrystal_I2C lcd(0x27, 20, 4);
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
String timeBuffer = "";
int mins = 0;
int secs = 0;
int Setpoint = 0;

DateTime endTime;

// Gear ratio constants
const float gear_ratio = 60.0 / 14.0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // Turn LED off (active low)
  pinMode(PUL_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENA_PIN, OUTPUT);

  digitalWrite(ENA_PIN, LOW);  // Enable motor driver
  digitalWrite(DIR_PIN, HIGH); // Set default direction

  Wire.begin();
  lcd.init();
  lcd.backlight();

  if (!rtc.begin()) {
    lcd.setCursor(0, 1);
    lcd.print("RTC not found!");
    while (1);
  }

  SetupPWM();
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
        lcd.setCursor(0, 1);
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

  lcd.setCursor(8, 1);
  lcd.print(formatted);
}

void SelectRPM() {
  lcd.setCursor(0, 2);
  lcd.print(" RPM :           ");  // Clear RPM line

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
          if (Setpoint > 0) {
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
      }

      if (Setpoint > 0) {
        lcd.setCursor(8, 2);
        lcd.print("      ");
        lcd.setCursor(8, 2);
        lcd.print(Setpoint);
      }
    }
  }
}

void RunMotor() {
  lcd.setCursor(0, 1);
  lcd.print("Time Left: ");
  lcd.setCursor(0, 2);
  lcd.print("Current RPM:        ");
  lcd.setCursor(0, 3);
  lcd.print("  *-PAUSE   #-STOP  ");

  digitalWrite(LED_BUILTIN, LOW);  // Turn on LED
  digitalWrite(ENA_PIN, LOW);      // Enable motor driver

  unsigned long lastUpdateLCD = 0;

  SetMotorPWM(Setpoint); // Start PWM based on selected RPM

  while (rtc.now() < endTime) {
    char key = keypad.getKey();
    if (key) {
      switch (key) {
        case '#':
          StopMotorPWM();
          digitalWrite(ENA_PIN, HIGH); // Disable motor
          Reset();
          return;
          break;
        case '*':
          StopMotorPWM();
          digitalWrite(ENA_PIN, HIGH); // Disable motor
          DateTime now = rtc.now();
          TimeSpan remaining = endTime - now;
          lcd.setCursor(0, 2);
          lcd.print("    -- PAUSED --    ");
          while (true) {
            endTime = rtc.now() + TimeSpan(0, remaining.hours(), remaining.minutes(), remaining.seconds());
            if (keypad.getKey()) {
              lcd.setCursor(0, 2);
              lcd.print("RPM:                ");  
              RunMotor();
              return;
            }
          }
          break;
      }
    }

    if (millis() - lastUpdateLCD >= 200) {
      lastUpdateLCD = millis();
      UpdateCountdownLCD();
    }
  }

  StopMotorPWM();
  digitalWrite(ENA_PIN, HIGH); // Disable motor

  lcd.clear();
  lcd.print("  {MIXER  CONTROL}  ");
  lcd.setCursor(0, 2);
  lcd.print("Done! Press to Reset");

  while (true) {
    if (keypad.getKey()) {
      Reset();
      return;
    }
  }
}

void UpdateCountdownLCD() {
  DateTime now = rtc.now();
  TimeSpan remaining = endTime - now;

  //Update Time
  char buffer[9];
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", remaining.hours(), remaining.minutes(), remaining.seconds());

  lcd.setCursor(11, 1);
  lcd.print("         ");
  lcd.setCursor(12, 1);
  lcd.print(buffer);
  //Update RPM
  lcd.setCursor(13, 2);
  lcd.print("      ");
  lcd.setCursor(13, 2);
  lcd.print(Setpoint);
}

void Reset() {
  timeBuffer = "";
  mins = 0;
  secs = 0;
  Setpoint = 0;

  lcd.clear();
  lcd.print("  {MIXER  CONTROL}  ");
  lcd.setCursor(0, 1);
  lcd.print("Timer:         ");
  lcd.setCursor(8, 1);
  lcd.print("MM:SS  ");
  lcd.setCursor(0, 3);
  lcd.print("   *-CLEAR   #-OK   ");
}

// PWM setup
void SetupPWM() {
  ledcSetup(PWM_CHANNEL, 1, PWM_RESOLUTION); // initial dummy freq
  ledcAttachPin(PUL_PIN, PWM_CHANNEL);
}

void SetMotorPWM(int rpm_target) {
  int steps_per_rev = 800; // Adjust if your driver uses microstepping
  float motor_rpm = rpm_target * gear_ratio;
  float revs_per_sec = motor_rpm / 60.0;
  int pulse_freq = steps_per_rev * revs_per_sec;

  if (pulse_freq < 1) pulse_freq = 1; // Prevent 0 frequency

  ledcSetup(PWM_CHANNEL, pulse_freq, PWM_RESOLUTION);
  ledcWrite(PWM_CHANNEL, PWM_DUTY_CYCLE);
}

void StopMotorPWM() {
  ledcWrite(PWM_CHANNEL, 0); // No pulses
}
