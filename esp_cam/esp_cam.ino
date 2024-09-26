// esp_cam.ino - Code for ESP32-CAM
// This code handles image capture using ESP32-CAM and sending the image to a cloud function.
// Replace 'YOUR_FIREBASE_CREDENTIALS.json' with the path to your Firebase credentials.

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include "esp_system.h"  // Include the ESP-IDF system library

// Replace with your network credentials
const char* ssid = "TELUSWiFi7618";
const char* password = "mLY74KcYu3";

// Replace with your server host
const char* serverHost = "us-central1-function-name.cloudfunctions.net";
const int serverPort = 443;  // HTTPS port

// Root CA certificate for your server
const char* rootCACertificate = \
"-----BEGIN CERTIFICATE-----\n" \
"..."
"yourRootCert\n" \
"...\n" \
"-----END CERTIFICATE-----\n";

// GPIO pin to receive trigger signal from the ESP32
#define TRIGGER_PIN 14  // Define the pin to receive capture signal

WiFiClientSecure client;

bool initializeCamera() {
    // Camera setup (example configuration, modify according to your hardware)
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = 5;
    config.pin_d1 = 18;
    config.pin_d2 = 19;
    config.pin_d3 = 21;
    config.pin_d4 = 36;
    config.pin_d5 = 39;
    config.pin_d6 = 34;
    config.pin_d7 = 35;
    config.pin_xclk = 0;
    config.pin_pclk = 22;
    config.pin_vsync = 25;
    config.pin_href = 23;
    config.pin_sccb_sda = 26;
    config.pin_sccb_scl = 27;
    config.pin_pwdn = 32;
    config.pin_reset = -1;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    // Lower the resolution to reduce memory usage
    config.frame_size = FRAMESIZE_VGA;  // Lower resolution
    config.jpeg_quality = 12;           // Increase to reduce memory usage (lower quality)
    config.fb_count = 1;                // Reduce frame buffer count to save memory

    // Initialize the camera only if it has not been initialized
    esp_err_t err = esp_camera_init(&config);
    if (err == ESP_ERR_INVALID_STATE) {
        Serial.println("Camera already initialized");
        return true;  // Camera is already initialized, continue using it
    } else if (err != ESP_OK) {
        Serial.println("Camera init failed");
        return false;
    }

    // Adjust camera settings
    sensor_t * s = esp_camera_sensor_get();
    s->set_brightness(s, 2); // -2 to 2
    s->set_gain_ctrl(s, 1);  // 0 = disable, 1 = enable
    s->set_gainceiling(s, GAINCEILING_64X);  // Set gain ceiling to 16x
    s->set_sharpness(s, 2);  // Adjust sharpness level
    s->set_saturation(s, 0); // -2 to 2, where 2 is the highest saturation

    return true;
}

bool connectToServer() {
    if (!client.connected()) {
        Serial.println("Connecting to server...");
        client.setCACert(rootCACertificate);
        if (!client.connect(serverHost, serverPort)) {
            Serial.println("Connection to server failed.");
            return false;
        }
        Serial.println("Connected to server.");
    }
    return true;
}

void setup() {
    Serial.begin(115200);

    // Wi-Fi setup
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");

    // Initialize trigger pin
    pinMode(TRIGGER_PIN, INPUT); // Set the trigger pin as input

    // Initialize camera once
    if (!initializeCamera()) {
        Serial.println("Initial camera setup failed");
        return;  // Exit setup if initial setup fails
    }

    // Connect to server once during setup
    if (!connectToServer()) {
        Serial.println("Initial connection to server failed");
        return;  // Exit setup if connection fails
    }
}

void captureAndSendImage() {
    // Reinitialize the camera settings before each capture
    sensor_t *s = esp_camera_sensor_get();
    if (s != nullptr) {
        s->set_framesize(s, FRAMESIZE_QVGA); // Set to the desired resolution
        s->set_quality(s, 12);               // Set the desired JPEG quality
        s->set_brightness(s, 1);             // Adjust as needed
        s->set_contrast(s, 1);               // Adjust as needed
        s->set_saturation(s, 0);             // Adjust as needed
        s->set_gain_ctrl(s, 0);              // Disable automatic gain control
        s->set_exposure_ctrl(s, 0);          // Disable automatic exposure control
        s->set_agc_gain(s, 10);              // Set a fixed gain value
        s->set_aec_value(s, 500);            // Set a fixed exposure time
    }

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return;  // Exit function if capture fails
    }

    Serial.print("Image captured with size: ");
    Serial.println(fb->len);

    if (!sendImageToServer(fb)) {
        Serial.println("Failed to send image to server.");
    }

    esp_camera_fb_return(fb);  // Ensure to always return the frame buffer
}


bool sendImageToServer(camera_fb_t* fb) {
    if (fb == nullptr || fb->buf == nullptr) {
        Serial.println("Frame buffer is null");
        return false;  // Return false if frame buffer is null
    }

    if (!connectToServer()) {
        return false;  // Return if unable to connect to server
    }

    String boundary = "----boundary";
    String bodyStart = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"image.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String bodyEnd = "\r\n--" + boundary + "--\r\n";

    int contentLength = bodyStart.length() + fb->len + bodyEnd.length();

    client.print(String("POST ") + "/process_image/upload" + " HTTP/1.1\r\n");
    client.print("Host: " + String(serverHost) + "\r\n");
    client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
    client.print("Content-Length: " + String(contentLength) + "\r\n");
    client.print("\r\n");

    client.print(bodyStart);
    size_t bytesSent = client.write(fb->buf, fb->len);

    if (bytesSent != fb->len) {
        Serial.println("Failed to send full image data.");
        return false;  // Return false if sending fails
    }

    client.print(bodyEnd);

    // Wait for server response
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") {
            break;
        }
    }

    String response = client.readString();
    Serial.println("Response from server: ");
    Serial.println(response);

    client.stop();  // Stop the client after sending the image

    return true;
}

void loop() {
    // Check if the trigger pin is HIGH
    if (digitalRead(TRIGGER_PIN) == HIGH) {
        Serial.println("Trigger received. Starting countdown...");

        // Countdown from 3 to 1
        for (int i = 3; i > 0; i--) {
            Serial.print(i);
            Serial.println("...");
            delay(1000);  // Wait for 1 second
        }

        Serial.println("Taking picture now...");
        captureAndSendImage();

        delay(3000);  // Delay to prevent multiple captures of the same object
    }
    delay(100);  // Small delay to reduce polling frequency
}
