#include <Arduino.h>

#define SERIAL_BAUD 115200

// put function declarations here:
int myFunction(int, int);
void handleSerial();

void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial) {
    delay(10);
  }
  Serial.println();
  Serial.println("IoT Temperature Monitor - serial ready");

  int result = myFunction(2, 3);
  Serial.printf("myFunction(2, 3) = %d\n", result);
}

void loop() {
  handleSerial();
}

// Reads a line from serial and echoes it back
void handleSerial() {
  if (Serial.available() > 0) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      Serial.print("Received: ");
      Serial.println(line);
    }
  }
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}
