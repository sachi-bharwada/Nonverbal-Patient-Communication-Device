#include <LiquidCrystal.h>
#include <string.h>

// Pin definitions
const int DATA_PIN = 8;
const int CLOCK_PIN = 9;
const int LED_PIN = 11; // SOS indicator LED
const int PIEZO_PIN = 12; // Passive piezo buzzer
const int LED_PIN_UNDO = 10; // UNDO indicator LED

// Timing constants
const unsigned long NORMAL_DISPLAY_TICKS = 10000; // 1 second
const unsigned long LED_BLINK_INTERVAL_TICKS = 5000; // 500ms
const unsigned long UNDO_TIME_WINDOW = 150000; // 15 seconds
const unsigned int  UNDO_THRESHOLD = 4; // > 4 events triggers LED
const unsigned long SIREN_DELAY_TICKS = 100000; // 10 seconds delay before buzzer starts

// Tone half-periods (in ticks)
const unsigned int HIGH_TONE_HALF_PERIOD = 8; // ~600 Hz => 8 ticks (approx.) 1/T = 1/16ms = 625 Hz 
const unsigned int LOW_TONE_HALF_PERIOD  = 13;// ~400 Hz => 13 ticks (approx.) 1/T = 1/26ms = 385 Hz 

volatile unsigned long g_tick = 0;  // Global timer tick, increments every 100µs

bool sosDisplayed = false; // Current SOS state
unsigned long sosDisplayStartTick = 0; 
bool buzzerState = false; // Current buzzer output state
unsigned long lastToneToggleTick = 0; // Stores last tick when buzzer pin was toggled 
unsigned long lastModToggleTick = 0; // Stores last time when we switched between high and low tone frequencies 
bool toneModulationState = false; // false = low tone, true = high tone

// SOS LED variables
bool ledState = false;
unsigned long lastLEDBlinkTick = 0;

// UNDO tracking & LED variables 
const int MAX_UNDO_EVENTS = 10;
unsigned long undoTimestamps[MAX_UNDO_EVENTS];
int undoEventCount = 0;
bool undoLedActive = false;
unsigned long undoLedOnTick = 0;

LiquidCrystal lcd(7, 6, 5, 4, 3, 2); //LCD setup

ISR(TIMER1_COMPA_vect) {
  g_tick++;  // Increment every 100µs

  // Buzzer siren 
  if (sosDisplayed && (g_tick - sosDisplayStartTick >= SIREN_DELAY_TICKS)) { // SOS displayed and elapsed time >= 10 seconds
    if ((g_tick - lastModToggleTick) >= 5000) { // Check if 500 ms have passed since the last tone modulation toggle
      lastModToggleTick = g_tick; 
      toneModulationState = !toneModulationState; // Switch from high to low tone
    }
    
    // Choose correct tone based on toneModulationState
    unsigned int currentHalfPeriod;
    if (toneModulationState) {
      currentHalfPeriod = HIGH_TONE_HALF_PERIOD;
    } else {
      currentHalfPeriod = LOW_TONE_HALF_PERIOD;
    }
    
    // If enough time has passed, toggle the buzzer ON/OFF to create the square wave tone
    if ((g_tick - lastToneToggleTick) >= currentHalfPeriod) {
      lastToneToggleTick = g_tick;
      if (!buzzerState) {
        buzzerState = true;
        digitalWrite(PIEZO_PIN, HIGH);
      } else {
        buzzerState = false;
        digitalWrite(PIEZO_PIN, LOW);
      }
    }
  } else {  // If SOS not active or elapsed time not high enough 
    toneModulationState = false;
    buzzerState = false;
    digitalWrite(PIEZO_PIN, LOW);
    lastModToggleTick = g_tick;
    lastToneToggleTick = g_tick;
  }
  
  // SOS LED blinking
  if (sosDisplayed) {
    if ((g_tick - lastLEDBlinkTick) >= LED_BLINK_INTERVAL_TICKS) { // Check if 500 ms have passed since the last LED toggle
      lastLEDBlinkTick = g_tick; // Update last toggle time
      if (!ledState) {
        ledState = true;
        digitalWrite(LED_PIN, HIGH);
      } else {
        ledState = false;
        digitalWrite(LED_PIN, LOW);
      }
    }
  } else {
    digitalWrite(LED_PIN, LOW);
    lastLEDBlinkTick = g_tick;
    ledState = false;
  }
}

void waitTicks(unsigned long ticks) {
  unsigned long start = g_tick;
  while ((g_tick - start) < ticks) {
    updateUndoLED(); // Calls updateUndoLED() in the loop so the UNDO LED timing is updated.
  }
}

void recordUndoEvent(unsigned long currentTick) {
  if (undoEventCount < MAX_UNDO_EVENTS) { // If there’s space in the undoTimestamps array, add the current tick
    undoTimestamps[undoEventCount++] = currentTick;
  } else {
    // If the array is full, shift all entries left (first in first out approach, since first in is oldest timestamp)
    for (int i = 1; i < MAX_UNDO_EVENTS; i++) {
      undoTimestamps[i - 1] = undoTimestamps[i];
    }
    undoTimestamps[MAX_UNDO_EVENTS - 1] = currentTick; // Add the new event
  }
}

int countRecentUndoEvents(unsigned long currentTick) {
  int count = 0;
  for (int i = 0; i < undoEventCount; i++) { // Loop through the timestamps
    if ((currentTick - undoTimestamps[i]) < UNDO_TIME_WINDOW) { // Count only those within the last 15 seconds
      count++;
    }
  }
  return count;
}

// Updates Undo LED on for 2 seconds when UNDO events exceed threshold
void updateUndoLED() {
  int count = countRecentUndoEvents(g_tick); // How many undos in last 15 seconds
  
  // If over threshold and LED is not already on, activate the LED
  if (!undoLedActive && (count > UNDO_THRESHOLD)) {
    digitalWrite(LED_PIN_UNDO, HIGH);
    undoLedOnTick = g_tick;
    undoLedActive = true;
    undoEventCount = 0;  // Clear events to avoid immediate retrigger
  }
  
  if (undoLedActive && ((g_tick - undoLedOnTick) >= 20000)) {  // 2 seconds = 20000 ticks
    digitalWrite(LED_PIN_UNDO, LOW);
    undoLedActive = false;
  }
}

// readByte() polls CLOCK_PIN; updateUndoLED() is called to maintain LED timing.
byte readByte() {
  byte result = 0; // Store the 8-bit byte
  for (int i = 0; i < 8; i++) {
    while (digitalRead(CLOCK_PIN) == LOW) {
      updateUndoLED(); // LED logic does not freeze 
    }
    waitTicks(1);  // Wait ~100µs for stability
    
    result = (result * 2) + digitalRead(DATA_PIN); // Shift  bits left by 1 and add new bit frm DATA line 
    
    while (digitalRead(CLOCK_PIN) == HIGH) {
      updateUndoLED(); // LED logic does not freeze 
    }
  }
  return result;
}

void setup() {
  Serial.begin(9600);
  Serial.println("Receiver setup...");
  
  cli(); // Disable interrupts while configuring Timer1
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;
  OCR1A = 199; // Set the compare match value for 100µs timing
  
  TCCR1B |= _BV(WGM12) | _BV(CS11); // Set Timer1 to CTC mode and prescaler = 8 
  
  TIMSK1 |= _BV(OCIE1A); // Enable Timer1 Compare Match A interrupt

  sei(); // Re-enable interrupts
  
  pinMode(DATA_PIN, INPUT);
  pinMode(CLOCK_PIN, INPUT);
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(PIEZO_PIN, OUTPUT);
  pinMode(LED_PIN_UNDO, OUTPUT);
  
  digitalWrite(LED_PIN, LOW);
  digitalWrite(PIEZO_PIN, LOW);
  digitalWrite(LED_PIN_UNDO, LOW);
  
  lcd.begin(16, 2);
  lcd.clear();
}

void loop() {
  byte startMarker = readByte();
  if (startMarker != 0xAA) {
    return; // Ignore invalid frame
  }
  
  byte len = readByte(); 
  char msgBuffer[65]; // Buffer to hold the actual message text (64 character and 1 for null terminator)
  byte checksum = len;
  
  for (int i = 0; i < len; i++) {
    byte c = readByte(); // Read 1 character
    msgBuffer[i] = c; // Store it in the buffer
    checksum += c; 
  }
  
  byte receivedChecksum = readByte();
  if (receivedChecksum != checksum) {
    return; // Checksum error
  }
  
  byte endMarker = readByte();
  if (endMarker != 0x55) {
    return; // Invalid end marker
  }
  
  msgBuffer[len] = '\0'; // Marks the end of the string, otherwise other functions will not work
  Serial.print("Received: ");
  Serial.println(msgBuffer);
  
  // Reset received --> reset all states, turns off SOS and undo, clear LCD
  if (strcmp(msgBuffer, "RST") == 0) { // Compares message with RST 
    lcd.clear();
    sosDisplayed = false;
    digitalWrite(LED_PIN, LOW);
    digitalWrite(PIEZO_PIN, LOW);
    undoEventCount = 0;
    digitalWrite(LED_PIN_UNDO, LOW);
    undoLedActive = false;
    return;
  }
  
  // Undo received --> add new undo event and trigger LED if too many events happened
  if (strcmp(msgBuffer, "UNDO") == 0) { // Compares message with UNDO 
    recordUndoEvent(g_tick);
    updateUndoLED();
    Serial.println("UNDO event recorded.");
    return;
  }
  
  // If already in SOS mode, ignore any and all other messages
  if (sosDisplayed) {
    updateUndoLED();
    return;
  }
  
  // SOS received
  if (strcmp(msgBuffer, "SOS!!!") == 0) { // Compares message with SOS 
    lcd.clear();
    lcd.print(msgBuffer);
    sosDisplayed = true; // Set SOS boolean as active/true
    sosDisplayStartTick = g_tick; // Start the timer for the buzzer
    lastToneToggleTick = g_tick; // Reset tone toggle
    lastModToggleTick = g_tick; // Reset modulation toggle
    toneModulationState = false; // Start with low tone
    digitalWrite(PIEZO_PIN, LOW);
    Serial.println("SOS displayed. Waiting for reset.");
    updateUndoLED();
    return;
  }
  
  // Sequence was not sent after >= 5 seconds 
  if (strcmp(msgBuffer, "Cleared Sequence") == 0) { // Compares message with Cleared Sequence
    lcd.clear();
    lcd.print("Cleared Sequence");
    unsigned long startTime = g_tick;

    // Keep the message on screen for 1 second
    while ((g_tick - startTime) < NORMAL_DISPLAY_TICKS) {
      updateUndoLED();
    }
    lcd.clear();
    updateUndoLED();
    return;
  }
  
  // Regular message (not SOS, UNDO, RST, etc.)
  lcd.clear();
  lcd.print(msgBuffer);
  unsigned long startTime = g_tick;

  // Keep the message on screen for 1 second
  while ((g_tick - startTime) < NORMAL_DISPLAY_TICKS) {
    updateUndoLED();
  }
  lcd.clear();
  updateUndoLED();
}
