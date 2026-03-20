#include <Arduino.h>

static bool readTwoUint8(uint8_t &a, uint8_t &b) {
  if (!Serial.available()) return false;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return false;

  int space = line.indexOf(' ');
  if (space < 0) return false;

  long x = line.substring(0, space).toInt();
  long y = line.substring(space + 1).toInt();

  // Делаем именно uint8_t, чтобы underflow был виден как wrap-around 0..255
  if (x < 0) x = 0; if (x > 255) x = 255;
  if (y < 0) y = 0; if (y > 255) y = 255;

  a = (uint8_t)x;
  b = (uint8_t)y;
  return true;
}

void setup() {
  Serial.begin(9600);
  while (!Serial) {}

  Serial.println("Task 2: SUB using CPU registers (R16,R17,R18) + read SREG.");
  Serial.println("Enter: a b  (0..255), e.g. 10 3 or 3 10 or 5 5");
}

void loop() {
  uint8_t a, b;
  if (!readTwoUint8(a, b)) return;

  uint8_t r16_val = 0, r17_val = 0, r18_val = 0;
  uint8_t sreg_after = 0;

  // ВАЖНО: вычитание делаем внутри asm, чтобы реально было "CPU registers only"
  asm volatile(
    // load inputs into CPU registers
    "mov r16, %[A]       \n\t"
    "mov r17, %[B]       \n\t"

    // clear r18 then compute r18 = r16 - r17 using SUB
    "mov r18, r16        \n\t"
    "sub r18, r17        \n\t"   // updates SREG (Z,C,...)

    // read SREG immediately after SUB
    "in  %[S], __SREG__  \n\t"

    // copy reg values to C vars for printing
    "mov %[O16], r16     \n\t"
    "mov %[O17], r17     \n\t"
    "mov %[O18], r18     \n\t"
    : [O16] "=r"(r16_val),
      [O17] "=r"(r17_val),
      [O18] "=r"(r18_val),
      [S]   "=r"(sreg_after)
    : [A] "r"(a),
      [B] "r"(b)
    : "r16", "r17", "r18"
  );

  bool Z = (sreg_after & (1 << 1)) != 0; // SREG bit1 = Z
  bool C = (sreg_after & (1 << 0)) != 0; // SREG bit0 = C

  Serial.println("---- Result ----");
  Serial.print("Input A="); Serial.print(a); Serial.print(" (0x"); Serial.print(a, HEX); Serial.println(")");
  Serial.print("Input B="); Serial.print(b); Serial.print(" (0x"); Serial.print(b, HEX); Serial.println(")");

  Serial.print("R16="); Serial.print(r16_val); Serial.print("  R17="); Serial.print(r17_val);
  Serial.print("  R18(A-B)="); Serial.print(r18_val); Serial.print(" (0x"); Serial.print(r18_val, HEX); Serial.println(")");

  Serial.print("SREG=0x"); Serial.println(sreg_after, HEX);
  Serial.print("Z="); Serial.print(Z ? "1" : "0");
  Serial.print("  C="); Serial.println(C ? "1" : "0");

  // Human explanation line
  if (C) Serial.println("C=1 means occurred (unsigned underflow: A < B).");
  else   Serial.println("C=0 means no borrow (A >= B).");

  if (Z) Serial.println("Z=1 means result is exactly zero.");
  else   Serial.println("Z=0 means result is non-zero.");

  Serial.println();
}
