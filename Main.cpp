#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <thread>
#include <stdio.h>
#include <chrono>
#include <Windows.h>
#include <mutex>
#include <conio.h>

using namespace std;
using namespace cv;
using namespace chrono;
using namespace this_thread;

Point center;

void processFrame(Mat& frame, Mat& output, mutex& mtx) {
    int rows, cols;
    int gr_value = 2;
    int bg_value = 4;
    int rb_value = 4;
    int avg, bg, gr, rb;

    for (cols = 0; cols < frame.cols; cols++) {
        for (rows = 0; rows < frame.rows; rows++) {
            Vec3b pixel = frame.at<Vec3b>(rows, cols); // B G R

            avg = (pixel[0] + pixel[1] + pixel[3]) / 3;
            bg = abs(pixel[0] - pixel[1]);
            gr = abs(pixel[1] - pixel[2]);
            rb = abs(pixel[0] - pixel[2]);

            if ((cols > (frame.cols / 3) && cols < (frame.cols / 3 * 2)) && (rows > (frame.rows / 3) && rows < (frame.rows / 3 * 2))) {
                if (avg > 123 && bg > bg_value && gr <= gr_value && rb > rb_value) {
                    mtx.lock();
                    output.at<Vec3b>(rows, cols) = Vec3b(255, 255, 255);
                    mtx.unlock();
                }
                else {
                    mtx.lock();
                    output.at<Vec3b>(rows, cols) = Vec3b(0, 0, 0);
                    mtx.unlock();
                }
            }
            else {
                mtx.lock();
                output.at<Vec3b>(rows, cols) = Vec3b(0, 0, 0);
                mtx.unlock();
            }
        }
    }
}

void findVein(Mat& output) {
    int maxWhitePixels = 0;
    int whitePixels = 0;
    int row, col; // maxPixel
    int rows, cols; // Default
    int box = 15;

    // Find maxPixels Center (Search in Process Area)
    for (cols = (output.cols / 3 + box); cols < (output.cols / 3 * 2 - box); cols++) {
        for (rows = (output.rows / 3 + box); rows < (output.rows / 3 * 2 - box); rows++) {
            if (output.at<Vec3b>(rows, cols) == Vec3b(255, 255, 255)) {
                for (col = cols - box; col < cols + box; col++) {
                    for (row = rows - box; row < rows + box; row++) {
                        if (output.at<Vec3b>(row, col) == Vec3b(255, 255, 255)) {
                            whitePixels++;
                            if (whitePixels > maxWhitePixels) {
                                maxWhitePixels = whitePixels;
                                center.x = (uint16_t)cols;
                                center.y = (uint16_t)rows;
                            }
                        }
                    }
                }
                whitePixels = 0;
            }
        }
    }
}

void overlay(Mat& frame, Mat& view, mutex& mtx) {
    int rows, cols;

    for (cols = 0; cols < frame.cols; cols++) {
        for (rows = 0; rows < frame.rows; rows++) {
            view.at<Vec3b>(rows, cols) = frame.at<Vec3b>(rows, cols);

            circle(view, center, 3, Scalar(0, 0, 255), -1);
        }
    }
}

int main() {
    int cycle = 0;
    uint16_t temp_x, temp_y;
    int limit = 20;
    int cap_delay = 0;

    HANDLE hComm;
    char port[] = "COM3"; // serial port name
    DWORD baudRate = CBR_9600; // desired baud rate
    DCB dcb;
    DWORD bytesRead;

    VideoCapture cap(1);
    cap.set(CAP_PROP_FRAME_WIDTH, 1920);
    cap.set(CAP_PROP_FRAME_HEIGHT, 1080);
    cap.set(CAP_PROP_FPS, 60);

    if (!cap.isOpened()) {
        cout << "Cannot open camera" << endl;
        return -1;
    }
    else {
        cout << "Cam set 1080p60" << endl;
    }

    // open serial port
    wchar_t wPort[10];
    MultiByteToWideChar(CP_UTF8, 0, port, -1, wPort, 10);
    hComm = CreateFile(wPort, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hComm == INVALID_HANDLE_VALUE) {
        cout << "Error opening serial port" << endl;
        return 1;
    }

    // configure serial port
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(hComm, &dcb)) {
        cout << "Error getting serial port state" << endl;
        CloseHandle(hComm);
        return 1;
    }
    dcb.BaudRate = baudRate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    if (!SetCommState(hComm, &dcb)) {
        cout << "Error setting serial port state" << endl;
        CloseHandle(hComm);
        return 1;
    }

    uint8_t data = 0x00;
    uint8_t buffer;
    uint16_t x_value = 0;
    uint16_t y_value = 0;
    DWORD bytesWritten;

    cout << "Press SpaceBar to Start" << endl;
    while (_getch() != 32) {

    }
    buffer = 0xEE;

    cout << "System Start" << endl << endl;

    while (true) {
        Mat frame;
        cap.read(frame);

        if (frame.empty()) {
            cout << "End of video" << endl;
            break;
        }

        Mat output = Mat::zeros(frame.rows, frame.cols, CV_8UC3);
        Mat view = Mat::zeros(frame.rows, frame.cols, CV_8UC3);
        mutex mtx;

        if (buffer == 0xEE) {
            if (cycle < 7) {
                cout << "Process Image" << endl;

                thread t(processFrame, ref(frame), ref(output), ref(mtx));
                t.join();

                findVein(output);

                cout << "Process cycle: " << cycle << endl;
                cout << "x= " << center.x << " y= " << center.y << endl << endl;

                if (cycle == 0) {
                    if (center.x != 0 && center.y != 0) {
                        temp_x = center.x;
                        temp_y = center.y;
                        cycle++;
                    }
                    else {
                        cout << "Process Error" << endl;
                        cout << "Process Restart" << endl << endl;
                    }
                }
                else {
                    cout << "Before" << endl;
                    cout << "x= " << temp_x << " y= " << temp_y << endl << endl;
                    if (abs(temp_x - center.x) < limit && abs(temp_y - center.y) < limit) {
                        cycle++;
                    }
                    else {
                        cout << "Miss Match" << endl;
                        cout << "Process Restart" << endl << endl;
                        cycle = 0;
                    }
                }
            }
            else {
                cout << "Data Match" << endl << endl;

                x_value = (uint16_t)center.x;
                y_value = (uint16_t)center.y;

                buffer = 0x02;
            }
        }
        else {
            data = 0xFF;
            if (WriteFile(hComm, &data, sizeof(data), &bytesWritten, NULL)) {
                sleep_for(seconds(1));
                cout << "Data Send Start" << endl << endl;
            }

            data = (x_value >> 8) & 0xFF;
            if (WriteFile(hComm, &data, sizeof(data), &bytesWritten, NULL)) {
                sleep_for(seconds(1));
            }

            data = x_value & 0xFF;
            if (WriteFile(hComm, &data, sizeof(data), &bytesWritten, NULL)) {
                sleep_for(seconds(1));
            }

            data = (y_value >> 8) & 0xFF;
            if (WriteFile(hComm, &data, sizeof(data), &bytesWritten, NULL)) {
                sleep_for(seconds(1));
            }

            data = y_value & 0xFF;
            if (WriteFile(hComm, &data, sizeof(data), &bytesWritten, NULL)) {
                sleep_for(seconds(1));
            }

            cout << "x: " << center.x << " y: " << center.y << endl;

            sleep_for(seconds(1));
            cout << "Data Send Finish" << endl << endl;
            sleep_for(seconds(1));

            if (ReadFile(hComm, &buffer, sizeof(buffer), &bytesRead, NULL)) {
                while (cap_delay < 10) {
                    cap.read(frame);
                    cap_delay++;
                }
                cap_delay = 0;

                thread g(overlay, ref(frame), ref(view), ref(mtx));
                g.join();

                system("taskkill /f /im PhotosApp.exe");
                imwrite("D:/Capstone/Image/vein.jpg", view);
                cout << "Data Save File vein.jpg" << endl;
                cout << "Image Save in D:/Capstone/Image/vein.jpg" << endl << endl;

                system("start D:/Capstone/Image/vein.jpg");

                sleep_for(seconds(1));
                cout << endl << "Press SpaceBar to Start" << endl << endl;
            }

            while (_getch() != 32) {

            }
            cout << "System Start" << endl << endl;

            cycle = 0;
        }
    }

    cap.release();
    CloseHandle(hComm);

    return 0;
}# 2023ESWContest_free_1080
