from flask import Flask, request, jsonify
import tensorflow as tf
import numpy as np
import cv2
import base64
from PIL import Image
from io import BytesIO

app = Flask(__name__)

# Load the model
model = tf.keras.models.load_model('waste_model.h5')

# Class labels
classes = ['paper', 'plastic', 'can', 'other']

@app.route('/predict', methods=['POST'])
def predict():
    try:
        # Get base64 image from request
        data = request.json
        img_data = data['image'].split(',')[1]  # Remove data:image/jpeg;base64,
        img_bytes = base64.b64decode(img_data)
        
        # Decode and preprocess
        nparr = np.frombuffer(img_bytes, np.uint8)
        img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
        img = cv2.resize(img, (224, 224))
        img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        img = Image.fromarray(img)
        img = np.array(img) / 255.0  # Normalize
        img = np.expand_dims(img, axis=0)  # Batch dimension
        
        # Predict
        predictions = model.predict(img)
        predicted_class = classes[np.argmax(predictions[0])]
        confidence = float(np.max(predictions[0]))
        
        return jsonify({
            'class': predicted_class,
            'confidence': confidence
        })
    except Exception as e:
        return jsonify({'error': str(e)}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
