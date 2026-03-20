// 55/109
volatile uint8_t countdown = 0; 

void setup() {
  // pin 13 output
  DDRB |= (1 << DDB5); 
  
  //led is off?
  PORTB &= ~(1 << PORTB5); 

  // 1. Configuring external iterrupt (INT0 on D2/PD2)

  // D2 as input
  DDRD &= ~(1 << DDD2); 
  // setting bit 2 in PORTD -> internal pull-up resistor
  PORTD |= (1 << PORTD2); 

  // EICRA  (External Interrupt Control Register A) -> trigger condition for INT0
  // Falling Edge (ISC01 = 1, ISC00 = 0)
  EICRA = (EICRA & 0xFC) | 0x02; 
  
  // mask register, setting bit 0 enables INT0.
  EIMSK |= (1 << INT0); 

  // 2. CONFIGURE TIMER1 FOR 1ms INTERRUPTS

  // Clear the control registers to start fresh
  TCCR1A = 0; 
  TCCR1B = 0; 
  TCNT1  = 0; // Reset the actual timer counter to 0

  // Set the Output Compare Register for our 1ms target (249)
  OCR1A = 249; 

  // CTC (Clear Timer on Compare Match) mode 
  TCCR1B |= (1 << WGM12);  //3, 1, 0 are all 0 since resetted

  // prescaler to 64
  TCCR1B |= (1 << CS11); 

  // Enable the Timer1 Compare Match A interrupt in the mask register.
  TIMSK1 |= (1 << OCIE1A); 

  // 2 Enable GLOBAL INTERRUPTS
  // This tells the CPU to actually listen to the hardware interrupts we just set up.
  sei(); 
}

void loop() {
  // The main loop is completely empty! 
  // All behavior is handled by the hardware and ISRs in the background.
}

// INTERRUPT SERVICE ROUTINES (ISRs)

// This ISR is triggered when D2 detects a falling edge (button press)
ISR(INT0_vect) {
  TCNT1=0;
  // Immediately set D13 (PB5) HIGH to turn the LED on
  PORTB |= (1 << PORTB5); 
  
  // Start the 10 millisecond countdown
  countdown = 22; 
}

// ISR is triggered by Timer1 exactly every 1 ms
ISR(TIMER1_COMPA_vect) {
  // checking if a countdown is currently active
  if (countdown > 0) {
    countdown--; // reduing the countdown by 1 (since 1ms has passed)
    
    // If the countdown just reached exactly 0, 10ms have passed.
    if (countdown == 0) {
      // Set D13 (PB5) LOW to turn the LED off automatically
      PORTB &= ~(1 << PORTB5); 
    }
  }
}