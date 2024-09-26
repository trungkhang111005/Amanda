# Amanda Project

Amanda is an automated system designed to exchange a hidden sticker in Nesters Market for a token, which can be used to play on a gacha machine. This project involves integrating an ESP32-CAM, an ESP32 microcontroller, an ultrasonic sensor, a servo motor, and a custom 3D-printed enclosure to automate the process. 

The system detects when a sticker is inserted into the enclosure, takes a picture using the ESP32-CAM, and sends the image to a Google Cloud Function for processing. Based on the result from Firebase Realtime Database, the ESP32 then controls a servo to release a token if the sticker is correct.

## Features

- **Automated Sticker Recognition**: Uses an ultrasonic sensor to detect sticker insertion and triggers the ESP32-CAM to capture the image.
- **Cloud-based Image Processing**: Images are sent to a Google Cloud Function for processing, which checks whether the sticker is correct.
- **Firebase Integration**: The result from the Google Cloud Function is stored in Firebase Realtime Database, which the ESP32 reads to determine if the token should be released.
- **Servo Control**: The ESP32 controls a servo to push out a token based on the sticker recognition result.
- **Custom 3D-Printed Enclosure**: A custom-built enclosure houses the components and facilitates the token dispensing process.
- **Image Enhancement**: The quality of the ESP32-CAM images is improved using CLAHE (Contrast Limited Adaptive Histogram Equalization) before image recognition.
- **Image Recognition**: Image recognition is developed using OpenCV's BFMatcher, which matches features between the sticker image and predefined templates.

## Project Structure

- `main.py`: Contains the Google Cloud Function that processes the image captured by the ESP32-CAM.
- `requirements.txt`: Lists the dependencies for the Google Cloud Function (e.g., Firebase, OpenCV).
- `Jupyter_notebook/`: Includes Jupyter Notebooks for testing and image augmentation.
  - `augmentation.ipynb`: For augmenting training images.
  - `test.ipynb`: For testing the image recognition process.
- `esp_cam/`: Code for the ESP32-CAM.
  - `esp_cam.ino`: Captures images and sends them to the Google Cloud Function.
- `esp_receiver/`: Code for the ESP32.
  - `esp_receiver.ino`: Handles sensor input, receives data from Firebase, and controls the servo.

## Hardware Setup

- **ESP32-CAM**: Captures an image of the inserted sticker.
- **ESP32**: Receives input from the ultrasonic sensor, controls the ESP32-CAM, communicates with Firebase, and drives the servo.
- **Ultrasonic Sensor**: Detects when a sticker is inserted into the enclosure.
- **Servo Motor**: Releases the token when a valid sticker is detected.
- **Custom 3D-Printed Enclosure**: Houses the electronics and provides a mechanism for token dispensing.

## System Workflow

1. The **ultrasonic sensor** detects when a person inserts a sticker into the enclosure.
2. The **ESP32** signals the **ESP32-CAM** to capture an image of the sticker.
3. The image is sent to a **Google Cloud Function** for processing.
4. The **Cloud Function** processes the image and updates the result (correct/incorrect sticker) in **Firebase Realtime Database**.
5. The **ESP32** reads the result from Firebase.
6. Based on the result, the **ESP32** controls the **servo** to either dispense a token (if the sticker is correct) or do nothing (if incorrect).

## Image Processing and Enhancement

- The image captured by the **ESP32-CAM** is first enhanced using **CLAHE** (Contrast Limited Adaptive Histogram Equalization) to improve its quality. 
- **Image Recognition** is done using OpenCV's **BFMatcher**, which compares the key features of the captured image to predefined sticker templates and determines whether the sticker is correct or incorrect.

## Usage

- Insert a sticker into the 3D-printed enclosure.
- The ultrasonic sensor will trigger the ESP32-CAM to take a picture.
- The image is sent to Google Cloud for processing.
- Based on the result from Firebase, the ESP32 will either dispense a token (if the sticker is correct) or do nothing (if the sticker is incorrect).

## Contributing

Feel free to submit issues, fork the repository, and create pull requests if you'd like to contribute to Amanda!

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
