import os
import tensorflow as tf
from tensorflow.keras.preprocessing.image import load_img, img_to_array, save_img
import subprocess


def create_jpeg_dir(quality):

    # Specify the directory path
    directory = "./data_jpeg_"+str(quality)+"/"

    # Create the directory
    os.makedirs(directory, exist_ok=True)

    print(f"Directory '{directory}' created successfully.")

    name = "n01440764"
    input_dir = "../data_og_png/" + name
    output_dir = directory + name

    print(input_dir)
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    image_files = [f for f in os.listdir(input_dir) if f.endswith(('.png'))]
    image_names = [os.path.splitext(filename)[0] for filename in image_files]

    # Resize and save each image
    for filename in image_names:
        input_path = os.path.join(input_dir, filename + ".png")
        output_path = os.path.join(output_dir, filename + ".JPEG")
        print(input_path)
        # JPEG compress images
        subprocess.run(["convert", input_path, "-quality", str(quality), output_path])

        print(f"Image '{filename}' compressed and saved to '{output_path}'")

def create_j2k_dir(quality):

    # Specify the directory path
    directory = "./data_j2k_"+str(quality)+"/"

    # Create the directory
    os.makedirs(directory, exist_ok=True)

    print(f"Directory '{directory}' created successfully.")

    name = "n01440764"
    input_dir = "../data_og_png/" + name
    output_dir = directory + name

    print(input_dir)
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    image_files = [f for f in os.listdir(input_dir) if f.endswith(('.png'))]
    image_names = [os.path.splitext(filename)[0] for filename in image_files]

    # Resize and save each image
    for filename in image_names:
        input_path = os.path.join(input_dir, filename + ".png")
        output_path = os.path.join(output_dir, filename + ".j2k")
        print(input_path)
        # JPEG compress images
        subprocess.run(["convert", input_path, "-quality", str(quality), output_path])

        print(f"Image '{filename}' compressed and saved to '{output_path}'")

def create_j2k_jpeg_dir(quality):

    # Specify the directory path
    directory = "./data_j2k_jpeg_"+str(quality)+"/"
    inp_directory = "./data_j2k_"+str(quality)+"/"

    # Create the directory
    os.makedirs(directory, exist_ok=True)

    print(f"Directory '{directory}' created successfully.")

    name = "n01440764"
    input_dir = inp_directory + name
    output_dir = directory + name

    print(input_dir)
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    image_files = [f for f in os.listdir(input_dir) if f.endswith(('.j2k'))]
    image_names = [os.path.splitext(filename)[0] for filename in image_files]

    # Resize and save each image
    for filename in image_names:
        input_path = os.path.join(input_dir, filename + ".j2k")
        output_path = os.path.join(output_dir, filename + ".JPEG")
        print(input_path)
        # JPEG compress images
        subprocess.run(["convert", input_path, "-quality", 100, output_path])

        print(f"Image '{filename}' compressed and saved to '{output_path}'")



"""#Creating jpeg data directories run from within jpeg_data directory
for quality in range(2, 29, 2):
    create_jpeg_dir(quality)

for quality in range(30, 101, 10):
    create_jpeg_dir(quality)
"""

#Creating j2k data directories run from within j2k_data directory
for quality in range(20, 41, 2):
    create_j2k_dir(quality)

for quality in range(10, 101, 10):
    create_j2k_dir(quality)

#Creating j2k data directories into jpeg data for acc calculations run from within j2k_data directory
for quality in range(20, 41, 2):
    create_j2k_jpeg_dir(quality)

for quality in range(10, 101, 10):
    create_j2k_jpeg_dir(quality)