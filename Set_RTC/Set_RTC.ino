#include <Wire.h>
#include <RTClib.h>

// Create an RTC_DS3231 object
RTC_DS3231 rtc;

void setup() {
  Serial.begin(115200);
  
  // Initialize the RTC module
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  // Set the RTC time to the compile time (F(__DATE__), F(__TIME__) is the compile time)
  Serial.println("Setting Time...");
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  delay(500);
  Serial.println("Done!");
  delay(500);
  DateTime now = rtc.now();
  Serial.print("Current time: ");
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.println(now.second(), DEC);

  // End setup
}

void loop() {
  // No looping required for setting time, just print it once during setup
}
