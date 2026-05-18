#include <avr/io.h>
#include <stdint.h>

// Values sent in the task
// 85  = 01010101 = 0x55
// 170 = 10101010 = 0xAA
// 255 = 11111111 = 0xFF

uint8_t values[] = {85, 170, 255};
uint8_t indexValue = 0; // 0 -> 85, 1 -> 170, 2 -> 255

// SPI Master setup
void SPI_MasterInit(void)
{
  DDRB |= (1 << DDB2);  // SS output
  DDRB |= (1 << DDB3);  // MOSI output
  DDRB |= (1 << DDB5);  // SCK output

  DDRB &= ~(1 << DDB4); // MISO input

  // Keep SS inactive
  PORTB |= (1 << PB2);

  /*
    SPI Control Register:
    SPE enables SPI.
    MSTR selects Master mode.
    SPR0 gives SCK = F_CPU / 16.

    Default CPOL = 0 and CPHA = 0, so it is SPI Mode 0.
    Default DORD = 0, so data is sent MSB first.
  */
  SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0);

  /*
    No double speed mode.
    With 16 MHz Arduino, SCK becomes 1 MHz.
  */
  SPSR &= ~(1 << SPI2X);
}

// Send one SPI byte
void SPI_MasterTransmit(uint8_t data)
{
  // Select Slave
  PORTB &= ~(1 << PB2);

  /*
    Writing to SPDR starts the transfer.
    The hardware shifts 8 bits through MOSI.
  */
  SPDR = data;

  /*
    Wait until the transfer is complete.
    SPIF becomes 1 at the end.
  */
  while (!(SPSR & (1 << SPIF)))
  {
    // Wait here
  }

  // Read dummy byte from Slave
  uint8_t dummy = SPDR;
  (void)dummy;

  // Release Slave
  PORTB |= (1 << PB2);
}

// Arduino setup
void setup()
{
  Serial.begin(9600);
  SPI_MasterInit();

  Serial.println("SPI Master started");
}

// Main loop
void loop()
{
  uint8_t dataToSend = values[indexValue];

  SPI_MasterTransmit(dataToSend);

  Serial.print("Master sent: ");
  Serial.println(dataToSend);

  indexValue++;

  if (indexValue >= 3)
  {
    indexValue = 0;
  }

  delay(1000);
}
