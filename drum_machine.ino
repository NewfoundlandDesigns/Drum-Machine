#include <Arduino.h>
#include <BLEMidi.h>

// --- Pin Configuration ---
#define NUM_BUTTONS 12

// Avoid GPIO 0, 3, 19, 20, 26-32, 45, 46 on ESP32-S3
const uint8_t BUTTON_PINS[NUM_BUTTONS] = {
  1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
};

// LED pins — each needs a 220Ω resistor to LED anode
const uint8_t LED_PINS[NUM_BUTTONS] = {
  14, 15, 16, 17, 18, 21, 38, 39, 40, 41, 42, 43
};

// General MIDI drum map (channel 10)
const uint8_t DRUM_NOTES[NUM_BUTTONS] = {
  36,  // Kick
  38,  // Snare
  42,  // Closed Hi-Hat
  46,  // Open Hi-Hat
  45,  // Low Tom
  47,  // Mid Tom
  50,  // High Tom
  49,  // Crash Cymbal
  51,  // Ride Cymbal
  39,  // Hand Clap
  37,  // Side Stick / Rim
  56,  // Cowbell
};

const char* DRUM_NAMES[NUM_BUTTONS] = {
  "Kick", "Snare", "HH Closed", "HH Open",
  "Tom Lo", "Tom Mid", "Tom Hi", "Crash",
  "Ride", "Clap", "Rim", "Cowbell"
};

#define MIDI_CHANNEL 9  // Channel 10 in 0-indexed
#define VELOCITY     127
#define DEBOUNCE_MS  30
#define LED_HOLD_MS  80  // Minimum LED flash duration for quick taps

// --- Button & LED State ---
bool buttonState[NUM_BUTTONS];
bool lastButtonState[NUM_BUTTONS];
unsigned long lastDebounce[NUM_BUTTONS];
unsigned long ledOnTime[NUM_BUTTONS];

bool connected = false;

void onConnect() {
  connected = true;
  Serial.println("BLE MIDI connected!");
  // Flash all LEDs to confirm connection
  for (int i = 0; i < NUM_BUTTONS; i++) digitalWrite(LED_PINS[i], HIGH);
  delay(200);
  for (int i = 0; i < NUM_BUTTONS; i++) digitalWrite(LED_PINS[i], LOW);
}

void onDisconnect() {
  connected = false;
  Serial.println("BLE MIDI disconnected.");
  for (int i = 0; i < NUM_BUTTONS; i++) digitalWrite(LED_PINS[i], LOW);
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32-S3 BLE MIDI Drum Pad");

  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
    buttonState[i] = HIGH;
    lastButtonState[i] = HIGH;
    lastDebounce[i] = 0;
    ledOnTime[i] = 0;
  }

  BLEMidiServer.begin("DrumBox");
  BLEMidiServer.setOnConnectCallback(onConnect);
  BLEMidiServer.setOnDisconnectCallback(onDisconnect);

  Serial.println("Advertising as 'DrumBox'...");
}

void loop() {
  unsigned long now = millis();

  for (int i = 0; i < NUM_BUTTONS; i++) {
    bool reading = digitalRead(BUTTON_PINS[i]);

    if (reading != lastButtonState[i]) {
      lastDebounce[i] = now;
    }

    if ((now - lastDebounce[i]) > DEBOUNCE_MS) {
      if (reading != buttonState[i]) {
        buttonState[i] = reading;

        if (buttonState[i] == LOW) {
          // Button pressed — LED on, send note
          digitalWrite(LED_PINS[i], HIGH);
          ledOnTime[i] = now;

          if (connected) {
            BLEMidiServer.noteOn(MIDI_CHANNEL, DRUM_NOTES[i], VELOCITY);
            Serial.printf("ON:  %s (note %d)\n", DRUM_NAMES[i], DRUM_NOTES[i]);
          }
        } else {
          // Button released — send note off, keep LED on for minimum flash
          if (connected) {
            BLEMidiServer.noteOff(MIDI_CHANNEL, DRUM_NOTES[i], 0);
          }
        }
      }
    }

    // Turn off LED after hold time (ensures visible flash even on quick taps)
    if (ledOnTime[i] > 0 && buttonState[i] == HIGH &&
        (now - ledOnTime[i]) >= LED_HOLD_MS) {
      digitalWrite(LED_PINS[i], LOW);
      ledOnTime[i] = 0;
    }

    lastButtonState[i] = reading;
  }

  delay(1);
}
