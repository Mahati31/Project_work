import os
import tensorflow as tf
from tensorflow.keras.preprocessing.image import load_img, img_to_array, save_img
import subprocess


def create_jpeg_dir(quality):

    # Specify the directory path
    directory = "./val_data_"+str(quality)+"/"

    # Create the directory
    os.makedirs(directory, exist_ok=True)

    print(f"Directory '{directory}' created successfully.")

    with open('list_32_classes', 'r') as file:
        # Loop through each line in the file
        for line in file:
            # Strip whitespace characters (like '\n') from the beginning and end of the line
            name = line.strip()
            # Process the name as needed
            print(name)  # For example, print the name
            input_dir = "./val_data_og/" + name
            output_dir = directory + name
            jpeg_compress_images(input_dir, output_dir,quality)



create_jpeg_dir(15)



