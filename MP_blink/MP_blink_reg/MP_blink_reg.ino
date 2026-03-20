void setup() {
  DDRB |= (1 << DDB5);          // PB5 output (D13)
}

void loop() {
  PORTB |=  (1 << PORTB5);      // HIGH
  //delay(500);

  PORTB &= ~(1 << PORTB5);      // LOW
  //delay(500);
}
