# main.py - Image Processing and Firebase interaction script
# This script handles Firebase initialization and image processing logic.
# Sensitive paths have been replaced with placeholders.
# Ensure you replace 'YOUR_SERVICE_ACCOUNT_KEY_PATH' with the actual path to your service account key.

import os
from datetime import datetime
import firebase_admin
from firebase_admin import credentials, db
import cv2
import numpy as np
import logging
from flask import Flask, request, jsonify
from google.cloud import storage
from collections import deque
import tempfile

app = Flask(__name__)

# Parameters
MATCH_THRESHOLD = 11
DISTANCE_THRESHOLD = 47

# Configure logging
logging.basicConfig(level=logging.DEBUG)

# Google Cloud Storage bucket name and template path
BUCKET_NAME = 'YOUR-BUCKET'
TEMPLATE_PATH = 'template.jpg'
LOCAL_TEMPLATE_PATH = '/tmp/template.jpg'

# Function to download the service account key from Cloud Storage
def download_service_account_key(bucket_name, blob_name):
    storage_client = storage.Client()
    bucket = storage_client.bucket(bucket_name)
    blob = bucket.blob(blob_name)
    
    temp_key_path = os.path.join(tempfile.gettempdir(), 'service-account-key.json')
    blob.download_to_filename(temp_key_path)
    return temp_key_path

# Update this path to include the folder name
json_folder = 'process-image-firebase-adminsdk-az2hj-9920516ba0.json'
json_file_name = 'process-image-firebase-adminsdk-az2hj-9920516ba0.json'
key_path = download_service_account_key(BUCKET_NAME, f'{json_folder}/{json_file_name}')

# Initialize Firebase Admin SDK
try:
    cred = credentials.Certificate(key_path)
    firebase_admin.initialize_app(cred, {
        'databaseURL': 'https://process-image-default-rtdb.firebaseio.com/'
    })
    logging.info("Firebase initialized successfully")
except Exception as e:
    logging.error(f"Failed to initialize Firebase: {str(e)}")

# Rest of the code remains unchanged...

def enhance_image(image):
    """Enhance image quality using CLAHE."""
    clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))
    enhanced_image = clahe.apply(image)
    return enhanced_image

def download_template(bucket_name, source_blob_name, destination_file_name):
    """Downloads a blob from the bucket."""
    try:
        storage_client = storage.Client()
        bucket = storage_client.bucket(bucket_name)
        blob = bucket.blob(source_blob_name)
        blob.download_to_filename(destination_file_name)
        logging.info(f"Downloaded storage object {source_blob_name} from bucket {bucket_name} to local file {destination_file_name}.")
    except Exception as e:
        logging.error(f"Error downloading template: {str(e)}")
        raise

# Download the template image
download_template(BUCKET_NAME, TEMPLATE_PATH, LOCAL_TEMPLATE_PATH)

# Load template image
try:
    template_img = cv2.imread(LOCAL_TEMPLATE_PATH, cv2.IMREAD_GRAYSCALE)
    if template_img is None:
        raise ValueError(f"Template image not found at path: {LOCAL_TEMPLATE_PATH}")

    # Enhance the template image
    template_img = enhance_image(template_img)

    # Initialize ORB detector
    orb = cv2.ORB_create()
    template_keypoints, template_descriptors = orb.detectAndCompute(template_img, None)
except Exception as e:
    logging.error(f"Error processing template image: {str(e)}")
    raise

# Initialize a deque to store the last three images
last_three_images = deque(maxlen=3)

def identify_object(image, match_threshold, distance_threshold):
    try:
        # Enhance the image
        enhanced_image = enhance_image(image)

        # Detect keypoints and descriptors in the enhanced image
        keypoints, descriptors = orb.detectAndCompute(enhanced_image, None)

        # Use BFMatcher to match descriptors
        bf = cv2.BFMatcher(cv2.NORM_HAMMING, crossCheck=True)
        matches = bf.match(template_descriptors, descriptors)

        # Sort matches by distance (best matches first)
        matches = sorted(matches, key=lambda x: x.distance)

        logging.info(f"Total matches: {len(matches)}")

        # Filter good matches based on distance
        good_matches = [m for m in matches if m.distance < distance_threshold]
        logging.info(f"Good matches: {len(good_matches)}")

        # Determine if object matches based on threshold
        if len(good_matches) > match_threshold:
            result = "desired_object"
        else:
            result = "other_object"
    except Exception as e:
        logging.error(f"Error in identify_object: {str(e)}")
        result = str(e)
    return result

def send_signal_to_firebase(signal):
    """Send a signal to Firebase."""
    try:
        ref = db.reference('/signal')
        timestamp = datetime.utcnow().isoformat()
        ref.set({
            "value": signal,
            "timestamp": timestamp
        })
        logging.info(f"Signal sent to Firebase: {signal} at {timestamp}")
    except Exception as e:
        logging.error(f"Error sending signal to Firebase: {str(e)}")
        raise

def save_image_to_gcs(image, image_name):
    """Save an image to Google Cloud Storage."""
    try:
        storage_client = storage.Client()
        bucket = storage_client.bucket(BUCKET_NAME)
        blob = bucket.blob(image_name)
        _, img_encoded = cv2.imencode('.jpg', image)
        img_bytes = img_encoded.tobytes()
        blob.upload_from_string(img_bytes, content_type='image/jpeg')
        logging.info(f"Image {image_name} uploaded to {BUCKET_NAME}")
    except Exception as e:
        logging.error(f"Error saving image to GCS: {str(e)}")
        raise

@app.route('/upload', methods=['POST'])
def upload_image():
    try:
        logging.info("Received a request")
        file = request.files['file']
        file_content = file.read()
        logging.info(f"File size: {len(file_content)} bytes")
        npimg = np.frombuffer(file_content, np.uint8)
        img = cv2.imdecode(npimg, cv2.IMREAD_COLOR)
        if img is None:
            logging.error("Invalid image")
            return jsonify({'error': 'Invalid image'}), 400

        logging.info("Image successfully decoded")
        logging.info(f"Image shape: {img.shape}")

        # Convert to grayscale
        gray_img = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        
        # Enhance the image
        gray_img = enhance_image(gray_img)

        # Add the image to the deque
        last_three_images.append(gray_img)
        
        # Save the image to GCS with a unique name
        image_name = f"image_{len(last_three_images)}.jpg"
        save_image_to_gcs(gray_img, image_name)

        # Call the object identification function
        result = identify_object(gray_img, MATCH_THRESHOLD, DISTANCE_THRESHOLD)
        logging.info(f"Result: {result}")

        # Send the signal to Firebase
        send_signal_to_firebase(result)

        return jsonify({'result': result})
    except Exception as e:
        logging.error(f"Error in upload_image: {str(e)}")
        return jsonify({'error': str(e)}), 500

def process_image(request):
    with app.request_context(request.environ):
        response = app.full_dispatch_request()
        return response

if __name__ == "__main__":
    app.run(debug=True, host='0.0.0.0', port=8080)
