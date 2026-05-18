#define F_CPU 16000000UL

#include <avr/io.h>         // register names
#include <avr/interrupt.h>  // ISR, sei, cli
#include <stdint.h>         // uint8_t, uint32_t
#include <stdlib.h>         // utoa

// Main state variables
volatile uint8_t current_digit = 0;
volatile uint8_t paused = 0;        // 0 = running, 1 = paused

// Debounce timing
volatile uint32_t ms_ticks = 0;         // increases every 1 ms
volatile uint32_t last_button_time = 0;

#define DEBOUNCE_MS 50 // debounce delay

// Debug message request from ISR to main
volatile uint8_t debug_event = 0;   // 0 = none, 1 = stopped, 2 = resumed
volatile uint16_t debug_tcnt1 = 0;

// 7-segment patterns
// bit0=a, bit1=b, bit2=c, bit3=d, bit4=e, bit5=f, bit6=g
static const uint8_t segLUT[10] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F  // 9
};

// UART debug functions
static void USART0_init(void)
{
    UBRR0H = 0;
    UBRR0L = 103; // 9600 baud at 16 MHz

    UCSR0B = (1 << TXEN0);                    // transmit only
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);  // 8N1 format
}

static void USART0_sendChar(char c)
{
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

static void USART0_sendString(const char *s)
{
    while (*s) {
        USART0_sendChar(*s++);
    }
}

static void USART0_sendUint16(uint16_t value)
{
    char buf[6]; // up to 65535 + '\0'
    utoa(value, buf, 10);
    USART0_sendString(buf);
}

static void USART0_sendLine(const char *label, uint16_t value)
{
    USART0_sendString(label);
    USART0_sendUint16(value);
    USART0_sendString("\r\n");
}

// Safely read 16-bit TCNT1
static uint16_t timer1_read_atomic(void)
{
    uint8_t sreg = SREG;
    cli();
    uint16_t value = TCNT1;
    SREG = sreg;
    return value;
}

// Display functions
static void display_digit(uint8_t digit)
{
    uint8_t pattern = segLUT[digit];

    // PORTB controls segments A to E
    // PB0 -> A
    // PB1 -> B
    // PB2 -> C
    // PB3 -> D
    // PB4 -> E

    // Update only lower 5 bits and keep the upper bits unchanged
    PORTB = (PORTB & ~0x1F) | (pattern & 0x1F);

    // PORTC controls segments F and G
    // PC0 -> F
    // PC1 -> G

    // bit 5 controls segment F
    if (pattern & (1 << 5)) PORTC |=  (1 << PC0);
    else                    PORTC &= ~(1 << PC0);

    if (pattern & (1 << 6)) PORTC |=  (1 << PC1);
    else                    PORTC &= ~(1 << PC1);
}

static void display_init(void)
{
    // Set segment pins as outputs
    DDRB |= (1 << DDB0) | (1 << DDB1) | (1 << DDB2) | (1 << DDB3) | (1 << DDB4);
    DDRC |= (1 << DDC0) | (1 << DDC1);

    // Turn all segments off first
    PORTB &= ~0x1F;
    PORTC &= ~((1 << PC0) | (1 << PC1));

    display_digit(0); // show 0 first
}

// Timer1 setup
// CTC mode with 0.5 s interrupt
static void timer1_init(void)
{
    // Clear Timer1 registers
    TCCR1A = 0x00;
    TCCR1B = 0x00;
    TCNT1  = 0x0000;

    OCR1A = 31249;

    TCCR1B |= (1 << WGM12);     // CTC mode
    TIMSK1 |= (1 << OCIE1A);    // enable compare A interrupt

    // Use prescaler 256
    TCCR1B |= (1 << CS12);
}

// Timer0 setup for debounce
// 16 MHz / 64 = 250 kHz
// 250 counts = 1 ms, so OCR0A = 249
static void timer0_init(void)
{
    TCCR0A = 0x00;
    TCCR0B = 0x00;
    TCNT0  = 0x00;

    OCR0A = 249;

    TCCR0A |= (1 << WGM01);                 // CTC mode
    TIMSK0 |= (1 << OCIE0A);                // enable compare A interrupt
    TCCR0B |= (1 << CS01) | (1 << CS00);   // prescaler 64
}

// INT0 button setup
// Button is on PD2/INT0 with internal pull-up
static void int0_init(void)
{
    DDRD  &= ~(1 << DDD2);      // PD2 input
    PORTD |=  (1 << PORTD2);    // enable pull-up

    // Trigger INT0 on falling edge
    EICRA &= ~(1 << ISC00);
    EICRA |=  (1 << ISC01);

    EIFR  |=  (1 << INTF0);     // clear pending INT0 flag
    EIMSK |=  (1 << INT0);      // enable INT0
}

// Interrupt routines
// Runs every 0.5 s
ISR(TIMER1_COMPA_vect)
{
    current_digit++;
    if (current_digit > 9) {
        current_digit = 0;
    }
    display_digit(current_digit);
}

// Runs every 1 ms
ISR(TIMER0_COMPA_vect)
{
    ms_ticks++;
}

// External interrupt from button
ISR(INT0_vect)
{
    uint32_t now = ms_ticks;

    // Ignore very fast repeated button changes
    if ((now - last_button_time) < DEBOUNCE_MS) {
        return;
    }

    last_button_time = now;
    paused ^= 1;  // toggle pause state

    if (paused) {
        // Stop Timer1 but keep its current count
        TCCR1B &= ~((1 << CS12) | (1 << CS11) | (1 << CS10));
        debug_tcnt1 = timer1_read_atomic();
        debug_event = 1; // stopped
    } else {
        // Read the value before continuing
        debug_tcnt1 = timer1_read_atomic();
        debug_event = 2; // resumed

        // Start Timer1 again with prescaler 256
        TCCR1B &= ~((1 << CS11) | (1 << CS10));
        TCCR1B |=  (1 << CS12);
    }
}

// Main function
int main(void)
{
    display_init();
    USART0_init();  // UART debug output
    timer0_init();  // debounce time base
    int0_init();
    timer1_init();

    sei();

    while (1) {
        uint8_t event_local = 0;
        uint16_t tcnt_local = 0;

        // Copy shared debug data safely
        uint8_t sreg = SREG;
        cli();
        if (debug_event != 0) {
            event_local = debug_event;
            tcnt_local = debug_tcnt1;
            debug_event = 0;
        }
        SREG = sreg;

        if (event_local == 1) {
            USART0_sendLine("Stopped TCNT1 = ", tcnt_local);
        } else if (event_local == 2) {
            USART0_sendLine("Resume from TCNT1 = ", tcnt_local);
        }
    }
}
