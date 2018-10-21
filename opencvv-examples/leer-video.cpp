//
// Created by victor on 07-10-18.
//
#include "opencv2/highgui.hpp"
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include <iostream>

using namespace cv;

int main(int argc, char **argv) {
    VideoCapture cap("comerciales/ballerina.mpg");
    if (!cap.isOpened()) {
        return -1;
    }

    while(cap.grab()){
        Mat frame;
        cap.retrieve(frame);
        imshow("ballerina.mpg",frame);
        waitKey(0);
    }
    return 0;
}

