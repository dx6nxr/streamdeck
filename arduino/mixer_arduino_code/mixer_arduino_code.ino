// --- Configuration ---
const int NUM_SLIDERS = 4; // Number of potentiometers/sliders
const int SLIDER_PINS[NUM_SLIDERS] = {A0, A1, A2, A3}; // Analog pins for sliders

const int NUM_BUTTONS = 8; // Number of buttons
const int BUTTON_PINS[NUM_BUTTONS] = {2, 3, 4, 5, 6, 7, 8, 9}; // Digital pins for buttons

const unsigned long SEND_INTERVAL_MS = 20; // How often to send data (milliseconds)
unsigned long lastSendTime = 0;

const int SLIDER_THRESHOLD = 3; // Minimum change to trigger sending
int lastSliderValues[NUM_SLIDERS];
int lastButtonStates[NUM_BUTTONS];
bool dataChanged = true; // Start by sending initial state

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < NUM_SLIDERS; i++) {
    pinMode(SLIDER_PINS[i], INPUT);
    lastSliderValues[i] = -1; // Initialize last values if using change detection
  }

  // Configure button pins
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLDOWN); // Use internal pull-up resistors (button connects pin to GND when pressed)
    lastButtonStates[i] = -1; // Initialize last states if using change detection
  }
}

void loop() {
  unsigned long currentTime = millis();

  // Send data at a regular interval
  if (currentTime - lastSendTime >= SEND_INTERVAL_MS) {
    lastSendTime = currentTime;

    String dataString = "";
    dataChanged = false; // Reset change flag if using change detection

    // --- Read Sliders ---
    for (int i = 0; i < NUM_SLIDERS; i++) {
      int sliderValue = analogRead(SLIDER_PINS[i]);
      dataString += String(sliderValue);
      if (i < NUM_SLIDERS - 1 || NUM_BUTTONS > 0) { // Add comma if not the very last value
          dataString += ",";
      }

      // Optional: Change detection for sliders
      if (abs(sliderValue - lastSliderValues[i]) > SLIDER_THRESHOLD) {
        dataChanged = true;
        lastSliderValues[i] = sliderValue;
      }
    }

    // --- Read Buttons ---
    for (int i = 0; i < NUM_BUTTONS; i++) {
      // digitalRead returns HIGH (1) if open, LOW (0) if pressed (due to INPUT_PULLUP)
      // We often want 1 for pressed, 0 for not pressed.
      int buttonState = digitalRead(BUTTON_PINS[i]) == HIGH ? 1 : 0;
      dataString += String(buttonState);
      if (i < NUM_BUTTONS - 1) { // Add comma if not the last button
          dataString += ",";
      }

      // Optional: Change detection for buttons
      if (buttonState != lastButtonStates[i]) {
        dataChanged = true;
        lastButtonStates[i] = buttonState;
      }
    }

    // --- Send Data ---
    // Only send if data has changed significantly (if using change detection)
    if (dataChanged) {
      Serial.println(dataString); // println automatically adds '\r\n'
    // }
  }

  // You can do other non-blocking things in the loop here if needed
}