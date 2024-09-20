#include <WiFi.h>
#include <FirebaseESP32.h>   // Correct header for ESP32 Firebase client
#include <ESP32Servo.h>      // Include the ESP32Servo library
#include <NewPing.h>         // Include NewPing library for Ultrasonic sensor

// Firebase project credentials
#define FIREBASE_HOST "process-image-default-rtdb.firebaseio.com"
#define FIREBASE_API_KEY "AIzaSyCE5DBeoY09knnk81QtFfoJ7Abm-RyeT5M"

// Provide the token generation process info
#include "addons/TokenHelper.h"

// Provide the RTDB payload printing info and other helper functions
#include "addons/RTDBHelper.h"

const char* ssid = "TELUSWiFi7618";
const char* password = "mLY74KcYu3";

FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

// Variables to store the last received signal and timestamp
String lastSignal = "";
String lastTimestamp = "";

// Servo setup
Servo myServo;               // Create a Servo object
int servoPin = 27;            // Pin to which the servo is connected
int servoPosition = 88;      // Initial servo position (90 degrees)
int activePosition = 2;      // Position to move when "desired_object" is detected

// Ultrasonic Sensor Setup
#define TRIG_PIN 12          // TRIG pin for Ultrasonic Sensor
#define ECHO_PIN 13          // ECHO pin for Ultrasonic Sensor
#define MAX_DISTANCE 200     // Maximum distance to measure (in cm)
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);
const float detectionThreshold = 5.40; // Distance threshold for detection in cm

// ESP32-CAM Trigger Pin
#define CAM_TRIGGER_PIN 14   // GPIO pin to send a signal to the ESP32-CAM
#define LED_MOSFET_PIN 16    // GPIO pin to control the MOSFET for the LED

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    // Set Firebase configuration
    config.database_url = FIREBASE_HOST;
    config.api_key = FIREBASE_API_KEY;
    config.token_status_callback = tokenStatusCallback;  // Assign the callback function for token generation

    // Set email and password for Firebase authentication
    auth.user.email = "trungkhang1110@gmail.com";  // Replace with your email
    auth.user.password = "khangpro1110";   // Replace with your password

    // Initialize Firebase
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    Serial.println("Firebase initialized");

    // Initialize the servo
    myServo.attach(servoPin);          // Attach the servo to the specified pin
    myServo.write(servoPosition);      // Move the servo to the initial position

    // Initialize the pin used to send signal to ESP32-CAM
    pinMode(CAM_TRIGGER_PIN, OUTPUT);
    digitalWrite(CAM_TRIGGER_PIN, LOW); // Start with the trigger signal LOW

    // Initialize the pin for MOSFET control (for LED)
    pinMode(LED_MOSFET_PIN, OUTPUT);
    digitalWrite(LED_MOSFET_PIN, LOW);  // Start with the LED MOSFET off
}

void loop() {
    // Get the duration of the echo pulse in microseconds
    long duration = sonar.ping();
    
    // Calculate the distance in cm (0.0343 cm per microsecond)
    float distance = duration * 0.0343 / 2.0;  // Divide by 2 to account for the pulse traveling to the object and back
    
    // Print the distance to 2 decimal places
    Serial.print("Distance: ");
    Serial.print(distance, 2);  // Print the distance with 2 decimal places
    Serial.println(" cm");

    // If object is detected within threshold, send a signal to ESP32-CAM
    if (distance > 0 && distance < detectionThreshold) {
        Serial.println("Object detected within threshold. Sending signal to ESP32-CAM.");
        digitalWrite(CAM_TRIGGER_PIN, HIGH);  // Send HIGH signal to trigger ESP32-CAM
        delay(100);                           // Small delay to ensure the signal is received
        digitalWrite(CAM_TRIGGER_PIN, LOW);   // Reset the signal

        // After sending signal to ESP32-CAM, wait for 2 second
        delay(2000);

        // Turn on the MOSFET to power the LED
        Serial.println("Turning on LED (MOSFET controlled)");
        digitalWrite(LED_MOSFET_PIN, HIGH);   // Turn on the MOSFET

        // Keep the LED on for 3 seconds
        delay(3000);

        // Turn off the LED (MOSFET)
        Serial.println("Turning off LED (MOSFET controlled)");
        digitalWrite(LED_MOSFET_PIN, LOW);    // Turn off the MOSFET

        delay(500);  // Prevent rapid triggering
    }

    // Firebase functionality (unchanged)
    if (Firebase.ready()) {
        if (Firebase.getJSON(firebaseData, "/signal")) {
            if (firebaseData.dataType() == "json") {
                FirebaseJsonData jsonData;

                // Extract the "value" field
                String currentSignal = "";
                if (firebaseData.jsonObject().get(jsonData, "value")) {
                    currentSignal = jsonData.stringValue;
                }

                // Extract the "timestamp" field
                String currentTimestamp = "";
                if (firebaseData.jsonObject().get(jsonData, "timestamp")) {
                    currentTimestamp = jsonData.stringValue;
                }

                // Check if the timestamp is different from the last timestamp
                if (currentTimestamp != lastTimestamp) {
                    Serial.println("New or updated signal received: " + currentSignal + " at " + currentTimestamp);

                    if (currentSignal == "desired_object") {
                        Serial.println("Desired object detected");

                        // Move the servo to the active position
                        myServo.write(activePosition);
                        delay(3000);  // Keep the servo in the active position for 3 seconds

                        // Move the servo back to the initial position
                        myServo.write(servoPosition);
                    }

                    // Update the last signal and timestamp with the current ones
                    lastSignal = currentSignal;
                    lastTimestamp = currentTimestamp;
                } else {
                    Serial.println("Signal is the same as the last one and not updated");
                }
            }
        } else {
            Serial.println("Failed to get signal, reason: " + firebaseData.errorReason());
        }
    }

    delay(1000);  // Poll Firebase every second (adjust as needed)
}
