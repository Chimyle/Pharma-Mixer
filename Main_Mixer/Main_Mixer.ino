// Libraries
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>

// Pin Assignment
#define LED_BUILTIN 1
#define PUL_PIN 33
#define DIR_PIN 32
#define ENA_PIN 35
#define SENSOR_PIN 25
#define SQW_PIN 34

// PWM config
#define PWM_CHANNEL 0
#define PWM_RESOLUTION 8  // 8-bit resolution
#define PWM_DUTY_CYCLE 127 // 50% duty cycle

LiquidCrystal_I2C lcd(0x27, 20, 4);     //Setup LCD Display
RTC_DS3231 rtc;                         //Setup RTC

// Keypad setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {2, 0, 4, 16};  // R1-R4
byte colPins[COLS] = {17, 5, 18, 19};    // C1-C4

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Setup RTC
String timeBuffer = "";
int mins = 0;
int secs = 0;
int Setpoint = 0;

DateTime endTime;

// Gear ratio constants
const float gear_ratio = 60.0 / 20.0;

//IR Sensor Variables
const float SampRate = 3.0; //RPM Calc interval in seconds
const float PPR = 12.0; //Pulse per revolution of IR disc encoder
volatile unsigned long pulseCount = 0;
volatile unsigned long calctimer = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
volatile unsigned long lastPulseTime = 0;

void IRAM_ATTR handlePulse() {        //Interrup Service Routine (ISR) for Counting RPM pulses
  unsigned long now = micros();
  if (now - lastPulseTime > 5000) {  // debounce: ignore pulses within 5ms
    portENTER_CRITICAL_ISR(&mux);
    pulseCount++;
    portEXIT_CRITICAL_ISR(&mux);
    lastPulseTime = now;
  }
}


void IRAM_ATTR sqwISR() {            //ISR for RTC counter
  calctimer++;
}

void setup() {
  //Pin Setup
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // Turn LED off (active low)
  pinMode(PUL_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENA_PIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), handlePulse, FALLING);
  
  //Motor Driver Initial Conditions
  digitalWrite(ENA_PIN, LOW);  // Enable motor driver
  digitalWrite(DIR_PIN, HIGH); // Set default direction

  //LCD Initialization
  Wire.begin();
  lcd.init();
  lcd.backlight();

  //RTC Initialization
  if (!rtc.begin()) {
    lcd.setCursor(0, 1);
    lcd.print("RTC not found!");
    while (1);
  }
  rtc.writeSqwPinMode(DS3231_SquareWave1Hz);
  pinMode(SQW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SQW_PIN), sqwISR, FALLING);
  
  SetupPWM();    //Runs the function that sets up Motor Configuration
  Reset();       //Reset Function
}

void loop() {
  char key = keypad.getKey();             //Stores character pressed on the keypad
  if (key) {                              //Executes when any key is pressed
    if (key >= '0' && key <= '9') {       
      InputTimer(key);                    //Accepts input for timer duration. Only works if a number is pressed
    } 
    else if (key == '#') {                //OK Key
      if (timeBuffer.length() == 4) {     //Checks if time is input completely/correctly
        mins = timeBuffer.substring(0, 2).toInt();
        secs = timeBuffer.substring(2, 4).toInt();
        SelectRPM();                      //Moves to selecting the RPM after storing duration
      } else {
        lcd.setCursor(0, 1);
        lcd.print("Invalid Time   ");     //Error if incomplete timer input
        delay(2000);
        Reset();                          //Resets after 2 seconds
      }
    } 
    else if (key == '*') {                //Clear Key
      Reset();
    }
  }
}

void InputTimer(char key) {
  if (timeBuffer.length() >= 4) {        //If the input is more than 4 digits, reset
    timeBuffer = "";
  }

  timeBuffer += key;                     //digits are concatenated after every press

  //Codes for displaying the input
  String displayTime = timeBuffer;
  while (displayTime.length() < 4) displayTime += "_";
  String formatted = displayTime.substring(0, 2) + ":" + displayTime.substring(2, 4);

  lcd.setCursor(8, 1);
  lcd.print(formatted);
}

void SelectRPM() {
  lcd.setCursor(0, 2);
  lcd.print(" RPM :           ");  // Clear RPM line

  //RPM selection based on the key pressed
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
        case '#':                    //OK Key
          if (Setpoint > 0) {
            if (mins > 0 || secs > 0) {
              //Setting end time and starting motor. Only works if RPM and duration is not 0
              endTime = rtc.now() + TimeSpan(0, 0, mins, secs);            
              RunMotor();
              return;
            }
          }
          break;
        case '*':        //Reset key. Clears all variables
          Reset();
          return;
      }

      //Displaying the RPM Value
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
  //Update Display to show time left, RPM, and controls
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

  while (rtc.now() < endTime) {          //Timer. This system runs only this when timer is active.
    char key = keypad.getKey();
    if (key) {
      switch (key) {                     // Checking for Stop and Pause keypress
        case '#':                        //Stop button. Stops the motor and clears the variables
          StopMotorPWM();
          digitalWrite(ENA_PIN, HIGH); // Disable motor
          Reset();
          return;
          break;
        case '*':                        // Pause button
          StopMotorPWM();
          digitalWrite(ENA_PIN, HIGH); // Disable motor
          DateTime now = rtc.now();
          TimeSpan remaining = endTime - now;          //Calculate remaining time

          //Display for Pause Screen
          lcd.setCursor(0, 2);
          lcd.print("    -- PAUSED --    ");
          lcd.setCursor(0, 3);
          lcd.print("  *-RESUME  #-STOP  ");
          
          while (true) {
            endTime = rtc.now() + TimeSpan(0, remaining.hours(), remaining.minutes(), remaining.seconds());      //continuously update the end time while paused
            char unpause = keypad.getKey();
            if (unpause) {
              if (unpause == '#'){            //Stop button. Stops the motor and clears the variables
                StopMotorPWM();
                digitalWrite(ENA_PIN, HIGH); // Disable motor
                Reset();
                return;
                break;
              }else if(unpause == '*'){       //Unpause key. Continues the current process until timer
                lcd.setCursor(0, 2);
                lcd.print("RPM:                ");  
                //lcd.setCursor(0, 3);
                //lcd.print("  *-PAUSE   #-STOP  ");
                RunMotor();
                return;
              }
            }
          }
          break;
      }
    }
    //Updates the display every 0.2 seconds
    if (millis() - lastUpdateLCD >= 200) {
      lastUpdateLCD = millis();
      UpdateCountdownLCD();
    }
  }
  
  /* TIMER ENDS HERE */
  
  StopMotorPWM();
  digitalWrite(ENA_PIN, HIGH); // Disable motor

  lcd.clear();
  lcd.print("  {MIXER  CONTROL}  ");
  lcd.setCursor(0, 2);
  lcd.print("Done! Press to Reset");

  while (true) {              //Waits for keypress before going back to input screen
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
  if (calctimer >= SampRate) {
    lcd.setCursor(13, 2);
    lcd.print("      ");
    lcd.setCursor(13, 2);
    lcd.print(CalculateRPM());
    calctimer = 0;
  }
}

unsigned long CalculateRPM() {
  // RPM calculation
  unsigned long count;
  portENTER_CRITICAL(&mux);
  count = pulseCount;
  pulseCount = 0;
  portEXIT_CRITICAL(&mux);

  unsigned long rpm = (count / PPR) * (60 / SampRate); 
  return rpm;
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
