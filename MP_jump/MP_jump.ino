#include <Arduino.h>
#include <avr/io.h>
#include <stdint.h>

const uint8_t LED_PIN = 13;   // Built-in LED on PB5
const uint8_t BTN_PIN = 8;    // D8 = PB0, button to GND, INPUT_PULLUP

volatile uint8_t actionState = 0;     // 0=normal, 1=invert, 2=dim-like sparse pulses
volatile uint8_t actionSelector = 0;  // cycles 1,2,3,1,2,3...

enum Mode : uint8_t {
  MODE_A = 0,   // slow blink
  MODE_B = 1,   // double blink
  MODE_C = 2    // fast strobe
};

// ------------------------------------------------------------
// Real AVR IJMP dispatch helper implemented as assembler.
// Input : r24 = requested action index (0..3)
// Return: r24 = resulting action state
//
// Action0 -> 0 (normal)
// Action1 -> 1 (invert)
// Action2 -> 2 (brightness simulation / sparse pulses)
// Action3 -> 0 (reset back to normal)
//
// Jump table entries are RJMP instructions, matching the lecture style.
// ------------------------------------------------------------
extern "C" uint8_t dispatchActionAsm(uint8_t idx);

__asm__(
R"ASM(
.global dispatchActionAsm
dispatchActionAsm:
    ldi r30, lo8(pm(ActionTable))
    ldi r31, hi8(pm(ActionTable))
    add r30, r24
    adc r31, __zero_reg__
    ijmp

ActionTable:
    rjmp Action0
    rjmp Action1
    rjmp Action2
    rjmp Action3

Action0:
    ldi r24, 0
    ret

Action1:
    ldi r24, 1
    ret

Action2:
    ldi r24, 2
    ret

Action3:
    ldi r24, 0
    ret
)ASM");

// ------------------------------------------------------------
// Basic LED helper
// ------------------------------------------------------------
void setLed(bool on) {
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

// ------------------------------------------------------------
// TASK 5A: RJMP boot wait loop
// ------------------------------------------------------------
void bootWait_RJMP() {
  asm volatile(
    "wait_btn:\n\t"
    "sbic %[pinreg], %[bit]\n\t"   // if PB0 is clear (pressed), skip next
    "jmp wait_btn\n\t"            // otherwise keep looping
    :
    : [pinreg] "I" (_SFR_IO_ADDR(PINB)),
      [bit]    "I" (PB0)
  );
}

// ------------------------------------------------------------
// Small boot confirmation flash
// ------------------------------------------------------------
void startDetectedFlash() {
  setLed(true);
  delay(200);
  setLed(false);
}

// ------------------------------------------------------------
// Wait for release only after the FIRST boot press
// ------------------------------------------------------------
void waitReleaseAfterBoot() {
  while (digitalRead(BTN_PIN) == LOW) {
    delay(5);
  }
  delay(25);
}

// ------------------------------------------------------------
// TASK 5B: count presses during a 2-second window
// 0 or 1 press -> Mode A
// 2 presses    -> Mode B
// 3+ presses   -> Mode C
// ------------------------------------------------------------
Mode chooseModeIn2Seconds() {
  uint8_t pressCount = 0;

  bool lastReading = HIGH;
  bool stableState = HIGH;
  unsigned long lastChange = 0;

  unsigned long t0 = millis();

  while (millis() - t0 < 2000) {
    bool reading = digitalRead(BTN_PIN);

    if (reading != lastReading) {
      lastReading = reading;
      lastChange = millis();
    }

    if (millis() - lastChange > 25) {
      if (reading != stableState) {
        stableState = reading;

        if (stableState == LOW) {   // new press detected
          pressCount++;
        }
      }
    }

    delay(2);
  }

  if (pressCount <= 1) return MODE_A;
  if (pressCount == 2) return MODE_B;
  return MODE_C;
}

// ------------------------------------------------------------
// 5C button polling
// Each short press cycles:
// press 1 -> Action1 (invert)
// press 2 -> Action2 (dim/sparse)
// press 3 -> Action3 (reset to normal)
// press 4 -> Action1 again ...
// ------------------------------------------------------------
void pollActionButton() {
  static bool lastReading = HIGH;
  static bool stableState = HIGH;
  static unsigned long lastChange = 0;

  bool reading = digitalRead(BTN_PIN);

  if (reading != lastReading) {
    lastReading = reading;
    lastChange = millis();
  }

  if (millis() - lastChange > 25) {
    if (reading != stableState) {
      stableState = reading;

      if (stableState == LOW) {   // new short press
        actionSelector++;
        if (actionSelector > 3) actionSelector = 1;

        actionState = dispatchActionAsm(actionSelector);
      }
    }
  }
}

// ------------------------------------------------------------
// Run one logical segment while still checking the button.
// actionState:
// 0 = normal
// 1 = invert
// 2 = sparse short pulses during logical ON time
// ------------------------------------------------------------
void runSegment(bool logicalOn, unsigned long durationMs) {
  unsigned long start = millis();

  while (millis() - start < durationMs) {
    pollActionButton();

    bool out = logicalOn;

    if (actionState == 1) {
      out = !out;
    } else if (actionState == 2) {
      if (logicalOn) {
        // Very visible "dim simulation":
        // short ON, long OFF repeating inside the ON segment
        unsigned long phase = (millis() - start) % 120;
        out = (phase < 18);   // ~18 ms ON, ~102 ms OFF
      } else {
        out = false;
      }
    }

    setLed(out);
    delay(2);
  }
}

// ------------------------------------------------------------
// The three mode patterns
// ------------------------------------------------------------
void slowBlinkPattern() {
  runSegment(true, 400);
  runSegment(false, 400);
}

void doubleBlinkPattern() {
  runSegment(true, 120);
  runSegment(false, 120);
  runSegment(true, 120);
  runSegment(false, 500);
}

void fastStrobePattern() {
  runSegment(true, 60);
  runSegment(false, 60);
}

// ------------------------------------------------------------
// TASK 5B: explicit JMP into chosen mode
// ------------------------------------------------------------
void runSelectedMode(Mode mode) {
  // reset 5C state when mode starts
  actionState = 0;
  actionSelector = 0;

  if (mode == MODE_A) {
    asm goto("jmp %l0" : : : : modeA_entry);
  } else if (mode == MODE_B) {
    asm goto("jmp %l0" : : : : modeB_entry);
  } else {
    asm goto("jmp %l0" : : : : modeC_entry);
  }

modeA_entry:
  while (true) {
    slowBlinkPattern();
  }

modeB_entry:
  while (true) {
    doubleBlinkPattern();
  }

modeC_entry:
  while (true) {
    fastStrobePattern();
  }
}

// ------------------------------------------------------------
// Arduino setup
// ------------------------------------------------------------
void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
  setLed(false);

  // Phase 1: RJMP boot wait
  bootWait_RJMP();

  delay(20);
  waitReleaseAfterBoot();

  startDetectedFlash();

  // Phase 2: choose mode
  Mode selectedMode = chooseModeIn2Seconds();

  // Phase 3: run selected mode forever, action changes via IJMP
  runSelectedMode(selectedMode);
}

void loop() {
  // never used
}