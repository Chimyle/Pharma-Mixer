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

  lcd.setCursor(0, 0);
  lcd.print("Enter MMSS Time:");
  lcd.setCursor(15, 0);
  lcd.print("_:_");  // initial placeholder
}

void loop() {
  if (keyChange) {
    uint8_t index = keyPad.getKey();
    keyChange = false;

    if (index < 16) {
      InputTimer(index);  // Pass the index to InputTimer function
    } else if (keys[index] == '#') {  // Corrected: used keys[index] instead of undefined `key`
      if (timeBuffer.length() == 4) {
        mins = timeBuffer.substring(0, 2).toInt();
        secs = timeBuffer.substring(2, 4).toInt();
        SelectRPM();
      } else {
        lcd.setCursor(11, 0);
        lcd.print("INVALID INPUT");
        delay(2000);
        timeBuffer = "";
        lcd.setCursor(11, 0);
        lcd.print("__:__");
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

    // Display on top row, starting at col 11 (for the time part)
    lcd.setCursor(11, 0);
    lcd.print(formatted);
  }
}

void SelectRPM() {
  lcd.setCursor(0, 1);
  lcd.print("Select RPM: ");
  
  while (true) {
    if (keyChange) {
      uint8_t index = keyPad.getKey();
      keyChange = false;

      char key = keys[index];

      switch (key) {
        case 'A':
          Setpoint = 600;
          lcd.setCursor(0, 1);
          lcd.print("Select RPM: 600     ");
          break;
        case 'B':
          Setpoint = 1200;
          lcd.setCursor(0, 1);
          lcd.print("Select RPM:: 1200    ");
          break;
        case 'C':
          Setpoint = 1800;
          lcd.setCursor(0, 1);
          lcd.print("Select RPM: 1800    ");
          break;
        case 'D':
          Setpoint = 2400;
          lcd.setCursor(0, 1);
          lcd.print("Select RPM: 2400    ");
          break;
        case '#':
          RunMotor();
          return;  // Exit the loop and return to main loop
      }
    }
  }
}

void RunMotor(){
  
}