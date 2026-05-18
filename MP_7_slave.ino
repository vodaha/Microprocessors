#include <avr/io.h>
#include <stdint.h>

// SPI Slave setup
void SPI_SlaveInit(void)
{
  DDRB |= (1 << DDB4);  // MISO output

  DDRB &= ~(1 << DDB2); // SS input
  DDRB &= ~(1 << DDB3); // MOSI input
  DDRB &= ~(1 << DDB5); // SCK input

  /*
    Enable SPI.
    Since MSTR stays 0, the board works as Slave.
  */
  SPCR = (1 << SPE);

  // Default data for transfer
  SPDR = 0x00;
}

// Receive one SPI byte
uint8_t SPI_SlaveReceive(void)
{
  /*
    Wait until 8 bits are received.
    SPIF becomes 1 when the transfer is done.
  */
  while (!(SPSR & (1 << SPIF)))
  {
    // Wait here
  }

  /*
    SPDR now contains the received byte.
  */
  uint8_t received = SPDR;

  /*
    Load dummy data for the next transfer.
  */
  SPDR = 0x00;

  return received;
}

// Arduino setup
void setup()
{
  Serial.begin(9600);
  SPI_SlaveInit();

  Serial.println("SPI Slave started");
  Serial.println("Waiting for data...");
}

// Main loop
void loop()
{
  uint8_t receivedValue = SPI_SlaveReceive();

  Serial.print("Slave received DEC: ");
  Serial.print(receivedValue);

  Serial.print(" | HEX: 0x");
  if (receivedValue < 16)
  {
    Serial.print("0");
  }
  Serial.print(receivedValue, HEX);

  Serial.print(" | BIN: ");
  Serial.println(receivedValue, BIN);
}
