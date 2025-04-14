/*
 * WASAPI Volume Control - Arduino Example Sketch
 * 
 * This sketch reads analog values from 4 potentiometers (A0-A3)
 * and digital values from 8 buttons (pins 2-9).
 * It sends the data to the PC as a comma-separated string.
 * 
 * Format: slider1,slider2,slider3,slider4,button1,button2,button3,button4,button5,button6,button7,button8
 * Example: 512,0,1023,767,0,1,0,0,0,0,0,0
 * 
 * Connect:
 * - 4 potentiometers to analog pins A0-A3
 * - 8 pushbuttons to digital pins 2-9 (with pull-down resistors)
 */

// Pin definitions
const int SLIDER_PINS[] = {A0, A1, A2, A3};  // Analog pins for sliders/potentiometers
const int BUTTON_PINS[] = {2, 3, 4, 5, 6, 7, 8, 9};  // Digital pins for buttons

// Constants
const int NUM_SLIDERS = 4;
const int NUM_BUTTONS = 8;
const int SERIAL_BAUD_RATE = 115200;
const int LOOP_DELAY_MS = 10;    // Time between updates (avoid overwhelming the PC)
const int DEBOUNCE_TIME = 50;   // Button debounce time in milliseconds

// Variables
int sliderValues[NUM_SLIDERS] = {0};
int buttonStates[NUM_BUTTONS] = {0};
int lastButtonStates[NUM_BUTTONS] = {0};
unsigned long lastDebounceTime[NUM_BUTTONS] = {0};

void setup() {
  // Initialize serial communication
  Serial.begin(SERIAL_BAUD_RATE);
  
  // Initialize slider pins as inputs
  for (int i = 0; i < NUM_SLIDERS; i++) {
    pinMode(SLIDER_PINS[i], INPUT);
  }
  
  // Initialize button pins as inputs with internal pull-up resistors
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);  // Using pull-up, buttons connect to GND when pressed
  }
  
  // Wait for serial connection to establish
  while (!Serial) {
    ; // wait for serial port to connect
  }
  
  // Send initial values
  updateAndSendValues();
}

void loop() {
  // Update and send values
  updateAndSendValues();
  
  // Small delay to avoid flooding the PC with data
  delay(LOOP_DELAY_MS);
}

void updateAndSendValues() {
  // Read slider values
  for (int i = 0; i < NUM_SLIDERS; i++) {
    sliderValues[i] = analogRead(SLIDER_PINS[i]);
  }
  
  // Read button states with debouncing
  for (int i = 0; i < NUM_BUTTONS; i++) {
    // Read the current raw state (inverted because of pull-up resistors)
    int reading = !digitalRead(BUTTON_PINS[i]);  // Invert because LOW = pressed with pull-up
    
    // Check if button state has changed
    if (reading != lastButtonStates[i]) {
      // Reset debounce timer
      lastDebounceTime[i] = millis();
    }
    
    // Check if enough time has passed since the last state change
    if ((millis() - lastDebounceTime[i]) > DEBOUNCE_TIME) {
      // Update button state if it has changed
      if (reading != buttonStates[i]) {
        buttonStates[i] = reading;
      }
    }
    
    // Save the current reading for the next comparison
    lastButtonStates[i] = reading;
  }
  
  // Send all values as comma-separated string
  for (int i = 0; i < NUM_SLIDERS; i++) {
    Serial.print(sliderValues[i]);
    Serial.print(",");
  }
  
  for (int i = 0; i < NUM_BUTTONS; i++) {
    Serial.print(buttonStates[i]);
    if (i < NUM_BUTTONS - 1) {
      Serial.print(",");
    }
  }
  
  // End the message with a newline
  Serial.println();
} 