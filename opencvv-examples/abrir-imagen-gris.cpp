//
// Created by victor on 07-10-18.
//
#include "opencv2/highgui.hpp"
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include <iostream>

using namespace cv;

int main( int argc, char** argv ){
    Mat image;
    char* imageName = argv[1];
    image = imread(imageName,IMREAD_COLOR);

    if(!image.data){
        printf("No image data\n");
        return -1;
    }
    Mat gray_image;
    cvtColor(image,gray_image,COLOR_BGR2GRAY);
    imshow("Gray image" , gray_image);
    waitKey(0);
    return 0;
}
