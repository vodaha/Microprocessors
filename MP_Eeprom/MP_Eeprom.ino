volatile uint8_t counter = 0;
unsigned long previousMillis = 0;

void setup() {
  Serial.begin(9600);

  // Reading from EEPROM at Addr 0 on Power-up
  while (EECR & 0x02);      // wait in case if EEPROM is busy.
                            // read Program Enable bit (EEPE) -> 1 when write (0x02)
  EEAR = 0;                 // Address Register
  EECR |= 0x01;             // Control Register => EERE to start read (0x01)
  uint8_t valueRead = EEDR; // getting value from data register
  
  if (valueRead == 255) {
    counter = 0;
  } else {
    counter = valueRead;
  }

  Serial.print("Started. Counter value: ");
  Serial.println(counter);
  Serial.println("Send 'S' to Save to EEPROM, 'R' to Reset.");
}

void loop() {
  unsigned long currentMillis = millis();

  // 1-second (1000 ms)
  if (currentMillis - previousMillis >= 1000) {
    previousMillis = currentMillis;

    // increment counter using avr inline assembly
    asm volatile (
      "lds r24, counter \n\t"  // Load from SRAM into register r24 => 2 clk cycles
      "inc r24 \n\t"           // Increment register value => like add - 1 clk cycle
      "sts counter, r24 \n\t"  // Store result back into SRAM => 2 clock cycles
    );

    Serial.print("Counter: ");
    Serial.println(counter);
  }

  // Serial command to store or reset values
  if (Serial.available() > 0) {
    char cmd = Serial.read();

    if (cmd == 'S' || cmd == 's') {
      // WRITE TO EEPROM (Address 0)
      while (EECR & 0x02);     // Wait if EEPROM is busy, EEPE (EEPROM Program Enable)
      EEAR = 0;                // Set address to 0
      EEDR = counter;          // Set data to write
      EECR |= 0x04;            // Setting EEMPE (Master Prite Enable), 
                               // stays active for 4 clk cycles.
      EECR |= 0x02;            // Setting EEPE (Program Enable) to start writing
      Serial.println(">>> Counter saved to EEPROM.");
    } 
    else if (cmd == 'R' || cmd == 'r') {
      counter = 0;
      // RESET EEPROM (Address 0) TO 0
      while (EECR & 0x02);     // Wait if EEPROM is busy
      EEAR = 0;                // Set address to 0
      EEDR = counter;          // Set data to write (0)
      EECR |= 0x04;            // Master write enable
      EECR |= 0x02;            // Start write
      Serial.println(">>> Counter and EEPROM reset to 0.");
    }
  }
}