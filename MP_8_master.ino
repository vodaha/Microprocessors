/*
  Board A - I2C Master

  Purpose:
  - Read Button A on D2.
  - Send Button A state to Board B.
  - Board B uses it to control LED B.
  - Request Button B state from Board B.
  - Control local LED A on D8 using Button B state.

  Hardware:
  - Button A: D2 to GND with internal pull-up.
  - LED A: D8 -> resistor -> LED -> GND.
  - I2C:
      A4 = SDA
      A5 = SCL
      GND is shared with Board B.

  Speed:
  - I2C speed is set using ATmega328P TWI registers:
      TWBR = TWI Bit Rate Register
      TWPS1:0 = prescaler bits in TWSR
*/


#include <Wire.h>
#include <avr/io.h>

#define SLAVE_ADDRESS 0x12 // 0001 0010
#define CMD_SET_LED   0xA1  // LED control command // 1010 0001

#define BUTTON_A_PIN  2
#define LED_A_PIN     8

// I2C speed settings
// Start with 100 kHz for stable testing.
// For 16 MHz Arduino UNO:
// f_SCL = f_CPU / (16 + 2 * TWBR * prescaler)
// f_SCL = 16 MHz / (16 + 2 * 72 * 1) = 100 kHz
void setI2CSpeed_100kHz()
{
  // Enable TWI. PRTWI = 1 turns it off, PRTWI = 0 turns it on.
  PRR &= ~(1 << PRTWI);

  // Set TWI prescaler to 1: TWPS1 = 0, TWPS0 = 0.
  TWSR &= ~((1 << TWPS0) | (1 << TWPS1));

  // Set SCL close to 100 kHz.
  TWBR = 72;

  // SDA/SCL as inputs with pull-ups enabled.
  // A4 = PC4 = SDA, A5 = PC5 = SCL.
  DDRC &= ~((1 << DDC4) | (1 << DDC5));
  PORTC |= (1 << PORTC4) | (1 << PORTC5);

  // Enable TWI hardware.
  TWCR |= (1 << TWEN);
}

// Use 400 kHz for the second oscilloscope test.
// f_SCL = 16 MHz / (16 + 2 * 12 * 1) = 400 kHz
void setI2CSpeed_400kHz()
{
  PRR &= ~(1 << PRTWI);

  TWSR &= ~((1 << TWPS0) | (1 << TWPS1));

  TWBR = 12;

  DDRC &= ~((1 << DDC4) | (1 << DDC5));
  PORTC |= (1 << PORTC4) | (1 << PORTC5);

  TWCR |= (1 << TWEN);
}

uint8_t readButtonA()
{
  // With INPUT_PULLUP, HIGH = released and LOW = pressed.
  if (digitalRead(BUTTON_A_PIN) == LOW) {
    return 1;
  } else {
    return 0;
  }
}

void sendLedCommandToSlave(uint8_t ledState)
{
  /*
    Master sends two bytes:
      byte 1 = command 0xA1
      byte 2 = LED state, 0 or 1
  */

  // START -> address -> data -> STOP

  Wire.beginTransmission(SLAVE_ADDRESS); // start transmission
  Wire.write(CMD_SET_LED);               // command byte
  Wire.write(ledState);                  // LED state byte
  // Example bytes: 0xA1 and 0x01
  uint8_t result = Wire.endTransmission();

  // result = 0 means success
  Serial.print("Sent Button A state = ");
  Serial.print(ledState);
  Serial.print(" | I2C result = ");
  Serial.println(result);

  /*
    If result is not 0, check wiring, slave address, GND, SDA, and SCL.
  */
}

uint8_t requestButtonBFromSlave()
{
  /*
    Master requests one byte from Slave:
      0 = Button B not pressed
      1 = Button B pressed
  */

  uint8_t receivedState = 0;

  Wire.requestFrom(SLAVE_ADDRESS, 1); // Ask Slave 0x12 for 1 byte.

  if (Wire.available()) {
    receivedState = Wire.read();
  }

  return receivedState;
}

void setup()
{
  Serial.begin(115200);

  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(LED_A_PIN, OUTPUT);

  digitalWrite(LED_A_PIN, LOW);

  // Start I2C as Master.
  Wire.begin(); // no address means Master mode

  // Start with 100 kHz. For faster test, use setI2CSpeed_400kHz().
  setI2CSpeed_100kHz();
  //setI2CSpeed_400kHz();

  Serial.println("Board A / Master started.");
}

void loop()
{
  static uint8_t lastButtonAState = 0;
  static uint32_t lastPollTime = 0;

  uint8_t buttonAState = readButtonA();

  // Send only if Button A changed.
  if (buttonAState != lastButtonAState) {
    lastButtonAState = buttonAState;
    sendLedCommandToSlave(buttonAState);
    delay(30); // simple debounce
  }

  // Check Button B every 50 ms.
  if (millis() - lastPollTime >= 50) {
    lastPollTime = millis();

    uint8_t buttonBState = requestButtonBFromSlave();

    if (buttonBState == 1) {
      digitalWrite(LED_A_PIN, HIGH);
    } else {
      digitalWrite(LED_A_PIN, LOW);
    }
  }
}
