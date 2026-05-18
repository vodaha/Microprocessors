/*
  Board B - I2C Slave

  Purpose:
  - Receive LED control data from Board A.
  - Control the local LED on D8.
  - Send Button B state when Master asks.

  Hardware:
  - Button B: D2 to GND with internal pull-up.
  - LED B: D8 -> resistor -> LED -> GND.
  - I2C:
      A4 = SDA
      A5 = SCL
      GND is shared with Board A.
*/

#include <Wire.h>
#include <avr/io.h>

#define SLAVE_ADDRESS 0x12
#define CMD_SET_LED   0xA1

#define BUTTON_B_PIN  2
#define LED_B_PIN     8

void receiveEvent(int byteCount)
{
  /*
    Called when Master sends data.
    No delay() or Serial.print() is used here because this runs inside I2C interrupt context.
  */

  if (byteCount < 2) {
    // Ignore incomplete messages.
    while (Wire.available()) {
      Wire.read();
    }
    return;
  }

  uint8_t command = Wire.read();
  uint8_t value   = Wire.read();

  // Remove extra received bytes, if any.
  while (Wire.available()) {
    Wire.read();
  }

  if (command == CMD_SET_LED) {
    if (value == 1) {
      digitalWrite(LED_B_PIN, HIGH);
    } else {
      digitalWrite(LED_B_PIN, LOW);
    }
  }
}

void requestEvent()
{
  /*
    Called when Master requests data.
    Slave sends one byte:
      0 = Button B not pressed
      1 = Button B pressed
  */

  uint8_t buttonState;

  // INPUT_PULLUP gives HIGH when released and LOW when pressed.
  if (digitalRead(BUTTON_B_PIN) == LOW) {
    buttonState = 1;
  } else {
    buttonState = 0;
  }

  Wire.write(buttonState);
}

void setup()
{
  pinMode(BUTTON_B_PIN, INPUT_PULLUP);
  pinMode(LED_B_PIN, OUTPUT);

  digitalWrite(LED_B_PIN, LOW);

  // Start I2C as Slave with address 0x12.
  Wire.begin(SLAVE_ADDRESS);

  // Attach I2C callback functions.
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
}

void loop()
{
  // Main loop is empty.
  // I2C work is done in the callback functions.
}
