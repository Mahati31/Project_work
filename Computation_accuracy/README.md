This directory consists of the experiments done for to evaluate the computation accuracy of both jpeg and jpeg2000 compression schemes.

data_og_png consists of the original 50 images of class tench which will be used as input to convert and compress to different quality factors in both the schemes.

j2k_data consists of the jpeg2000 images at different quality level. create_directories.py is the script to do the compression and create the input image datasets, the jupyter notebook results_j2k.ipynb consists of the experimental setup including the loading of the pre-trained vgg16 model, preprocessing of the input image dataset and running the classification on the images, calculating both the top1 and top5 accuracy.

jpeg_data consists of the jpeg images at different quality level. create_directories.py is the script to do the compression and create the input image datasets, the jupyter notebook results_jpeg.ipynb consists of the experimental setup including the loading of the pre-trained vgg16 model, preprocessing of the input image dataset and running the classification on the images, calculating both the top1 and top5 accuracy.
