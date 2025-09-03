// Pin definitions
const int DATA_PIN = 8;
const int CLOCK_PIN = 9;
const int INTERRUPT_PIN = 2; // For all buttons

// Button pins: Dot, Dash, Send, Undo, SOS, RESET
const int BUTTON_PINS[] = {7, 6, 5, 4, 3, 11};
const int NUM_BUTTONS = 6;

// Timing constants (each tick = 100µs)
const unsigned long BIT_TIME_TICKS = 1; // 100µs per bit phase
const unsigned long DEBOUNCE_TICKS = 500; // 50ms debounce
const unsigned long SOS_DELAY_TICKS = 30000; // 3 seconds for SOS delay
const unsigned long SEQUENCE_TIMEOUT_TICKS = 50000; // 5 seconds timeout

volatile unsigned long g_tick = 0;  // Global tick counter (increments every 100µs)

// Timer1 Compare Match A ISR: fires every 100µs.
ISR(TIMER1_COMPA_vect) {
  g_tick++; // Increment every 100µs
}

// Busy-wait for a specified number of ticks.
void waitTicks(unsigned long ticks) {
  unsigned long start = g_tick;
  while ((g_tick - start) < ticks) {
    // Do nothing (busy-wait)
  }
}

// --- Button & Morse Code Setup ---
volatile bool buttonPressed = false;
void handleInterrupt() {
  buttonPressed = true;
}

// Predefined dot/dash sequences and corresponding messages.
const String sequences[7] = { ".-", "-.", "..", "--", ".--", "-..", "..." };
const String messageOptions[7] = { "Yes", "No", "Tired", "Pain", "Food", "Water", "Bathroom" };
const String sosMessage = "SOS!!!";
String currentSequence = ""; // Current dot/dash sequence
unsigned long lastSequenceInputTick = 0; // Last time a dot/dash was pressed.

// SOS timing variables.
volatile bool sosPending = false;
volatile unsigned long sosStartTick = 0;

// --- Communication Functions ---
// Sends one byte (MSB first) via bit-banging.
void sendByte(byte data) {
  for (int i = 0; i < 8; i++) {
    if (data & 0x80) { // 0x80 == 10000000
      digitalWrite(DATA_PIN, HIGH);
    } else {
      digitalWrite(DATA_PIN, LOW);
    }
    // Read DATA bit now
    digitalWrite(CLOCK_PIN, HIGH);
    waitTicks(BIT_TIME_TICKS);
    digitalWrite(CLOCK_PIN, LOW);
    waitTicks(BIT_TIME_TICKS);

    data = data * 2; // Move next bit into MSB position 
  }
}

// Transmits a complete frame using the custom protocol.
void transmitMessage(String msg) {
  sendByte(0xAA); // Start marker
  byte len = msg.length();
  sendByte(len); // Message length
  byte checksum = len; // Checksum starts with length
  for (int i = 0; i < len; i++) {
    byte c = msg[i];
    sendByte(c); // Send each character
    checksum += c;
  }
  sendByte(checksum); // Checksum
  sendByte(0x55); // End marker
  Serial.print("Transmitted: ");
  Serial.println(msg);
}

// Compare currentSequence with predefined sequences and send corresponding message.
void sendMessageFunc() {
  bool matchFound = false;
  for (int i = 0; i < 7; i++) {
    if (currentSequence == sequences[i]) {
      transmitMessage(messageOptions[i]);
      matchFound = true;
      break;
    }
  }
  if (!matchFound) {
    transmitMessage("No match found");
  }
  currentSequence = ""; // Clear sequence after sending
}

// Immediately transmits the SOS message.
void sendSOSMessage() {
  transmitMessage(sosMessage);
  currentSequence = "";
}

void setup() {
  Serial.begin(9600);
  Serial.println("Transmitter setup complete!");

  /*Timer Setup Starts*/
  cli(); // Disable interrupts

  TCCR1A = 0; // Clear all previous configurations 
  TCCR1B = 0; // Clear all previous configurations 
  TCNT1  = 0; // Timer starts counting from zero 
  OCR1A = 199; // Set the compare match value for 100µs timing

  TCCR1B |= _BV(WGM12) | _BV(CS11); // WGM12 sets CTC (Clear timer on compare match) mode, CS11 sets prescaler to 8

  TIMSK1 |= _BV(OCIE1A); // Enable the Timer1 compare match interrupt

  sei(); // Re-enable interrupts
  /*Timer Setup Ends*/

  // Setup communication pins.
  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  digitalWrite(DATA_PIN, LOW);
  digitalWrite(CLOCK_PIN, LOW);

  // Setup the external interrupt pin.
  pinMode(INTERRUPT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), handleInterrupt, RISING);

  // Set button pins as inputs.
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(BUTTON_PINS[i], INPUT);
  }
}

void loop() {
  // Process button events if any
  if (buttonPressed) {
    waitTicks(DEBOUNCE_TICKS); // Debounce of 50ms
    buttonPressed = false;
    
    // Check each button.
    for (int i = 0; i < NUM_BUTTONS; i++) {
      if (digitalRead(BUTTON_PINS[i]) == HIGH) {
        if (i == 0) {  // Dot button
          currentSequence += ".";
          lastSequenceInputTick = g_tick;
          Serial.print("Dot added. Sequence: ");
          Serial.println(currentSequence);
        } else if (i == 1) {  // Dash button
          currentSequence += "-";
          lastSequenceInputTick = g_tick;
          Serial.print("Dash added. Sequence: ");
          Serial.println(currentSequence);
        } else if (i == 2) {  // Send button
          Serial.print("Send pressed. Sequence: ");
          Serial.println(currentSequence);
          sendMessageFunc();
        } else if (i == 3) {  // Undo button
          Serial.println("Undo pressed. Sending UNDO frame.");
          transmitMessage("UNDO");
          currentSequence = "";
        } else if (i == 4) {  // SOS button
          if (!sosPending) {
            sosPending = true;
            sosStartTick = g_tick;
            Serial.println("SOS pending, waiting 3 seconds...");
          } else {
            sosPending = false;
            Serial.println("SOS canceled.");
          }
        } else if (i == 5) {  // RESET button
          currentSequence = "";
          Serial.println("Reset pressed. Sending reset command.");
          transmitMessage("RST");
        }
      }
    }
  }
  
  // If SOS is pending and 3 seconds have elapsed, send SOS message.
  if (sosPending && ((g_tick - sosStartTick) >= 30000)) {
    sendSOSMessage();
    sosPending = false;
  }
  
  // If sequence is active and no send within 5 seconds, transmit "Cleared Sequence".
  if (currentSequence.length() > 0 && ((g_tick - lastSequenceInputTick) >= 50000)) {
    transmitMessage("Cleared Sequence");
    currentSequence = "";
    Serial.println("Sequence timeout: cleared due to inactivity.");
  }
}
