#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <conio.h>
#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <windows.h>

#ifdef _DEBUG
#pragma comment(lib, "opencv_world4100d.lib")
#else
#pragma comment(lib, "opencv_world4100.lib")
#endif

namespace fs = std::filesystem;

const uint8_t WIDTH = 128;
const uint8_t HEIGHT = 64;
const int TOTAL_IMAGES = 600;
const int FRAME = 1024;

int image_index = 0;
unsigned char** all_images = new unsigned char* [TOTAL_IMAGES];

int gifToPng(std::string gifFilePath, std::string outputDir)
{
    cv::VideoCapture gif(gifFilePath);

    int frameIndex = 0;
    cv::Mat frame, binaryFrame;

    // Проходим по всем кадрам GIF
    while (gif.read(frame)) {
        // Конвертируем кадр в оттенки серого
        cv::Mat grayFrame;
        cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);

        // Преобразуем в черно-белое изображение с помощью пороговой обработки
        cv::threshold(grayFrame, binaryFrame, 128, 255, cv::THRESH_BINARY);

        // Проверяем размеры и изменяем только если они больше 128x64
        cv::Mat finalFrame(64, 128, CV_8UC1, cv::Scalar(255));
        if (binaryFrame.cols > 128 || binaryFrame.rows > 64) {
            // Изменяем размер изображения до 128x64 с сохранением пропорций
            cv::Mat resizedFrame;
            cv::resize(binaryFrame, resizedFrame, cv::Size(128, 64), 0, 0, cv::INTER_LINEAR);

            // Копируем измененное изображение в центр
            resizedFrame.copyTo(finalFrame(cv::Rect((128 - resizedFrame.cols) / 2, (64 - resizedFrame.rows) / 2, resizedFrame.cols, resizedFrame.rows)));
        }
        else {
            // Если изображение меньше, копируем его без изменения
            binaryFrame.copyTo(finalFrame(cv::Rect((128 - binaryFrame.cols) / 2, (64 - binaryFrame.rows) / 2, binaryFrame.cols, binaryFrame.rows)));
        }

        std::string frameFilePath = outputDir + "frame_" + std::to_string(frameIndex) + "_binary.png";
        cv::imwrite(frameFilePath, finalFrame);
        frameIndex++;
    }

    std::cout << "Conversion complete! Total frames saved: " << frameIndex << std::endl;
    return 0;
}

int framesToBin(std::string gifFilePath) {
    // Инициализация каждого подмассива
    for (int i = 0; i < TOTAL_IMAGES; ++i) {
        all_images[i] = new unsigned char[FRAME](); // Каждый подмассив имеет размер 1024
    }

    image_index = 0;

    for (const auto& entry : fs::directory_iterator(gifFilePath)) {
        if (image_index >= TOTAL_IMAGES) {
            std::cout << "The maximum number of images has been reached" << std::endl;
            break;
        }

        // Чтение изображения
        cv::Mat img = cv::imread(entry.path().string(), cv::IMREAD_GRAYSCALE);
        if (img.empty()) {
            std::cerr << "Error reading image: " << entry.path() << std::endl;
            continue;
        }

        // Преобразование изображения в монохромный массив
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                // Проверка, чтобы избежать выхода за границы изображения
                if (x < img.cols && y < img.rows) {
                    uint8_t pixel = img.at<uint8_t>(y, x) < 128 ? 0 : 1; // Монохромизация
                    // Установка пикселя в массив
                    if (pixel == 1) {
                        all_images[image_index][(y * WIDTH + x) / 8] |= (1 << (7 - (x % 8)));
                    }
                    else {
                        all_images[image_index][(y * WIDTH + x) / 8] &= ~(1 << (7 - (x % 8))); // Сбрасываем бит (хотя это может не требоваться, если инициализация была правильной)
                    }
                }
            }
        }
        image_index++;
    }

    return 0;
}

int Transmit_COM_Bluetooth()
{
    HANDLE hSerial = CreateFile(
        L"COM6",                  // Имя порта
        GENERIC_WRITE,            // Доступ только на запись
        0,                        // Без совместного доступа
        NULL,                     // Без защиты
        OPEN_EXISTING,            // Только если порт существует
        0,                        // Атрибуты
        NULL                      // Шаблон файла
    );

    if (hSerial == INVALID_HANDLE_VALUE) {
        std::cerr << "Error connecting COM port.\n";
        return 1;
    }

    // Настройки порта
    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Error at receive COM-port parameters.\n";
        CloseHandle(hSerial);
        return 1;
    }

    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Error at setting COM-port parameters.\n";
        CloseHandle(hSerial);
        return 1;
    }

    DWORD bytesWritten;

    std::cout << "Press any key to finish the transmit..." << std::endl;

    while (!_kbhit()) {
        for (int i = 0; i < image_index; ++i) {
            if (!WriteFile(hSerial, all_images[i], FRAME, &bytesWritten, NULL)) {
                std::cerr << "Transmit data error.\n";
                CloseHandle(hSerial);
                return 1;
            }
            Sleep(110);
        }
    }

    // Закрытие порта
    CloseHandle(hSerial);
    std::cout << "Transmit over.\n";

    for (int i = 0; i < TOTAL_IMAGES; ++i) {
        delete[] all_images[i];
    }
    delete[] all_images;
    return 0;
}


int main() {
    int error;
    std::string gifFilePath;
    std::string outputDir;

    std::cout << "Enter gif file path: ";
    std::cin >> gifFilePath;
    size_t lastSlash = gifFilePath.find_last_of("\\/");

    if (lastSlash != std::string::npos) {
        outputDir = gifFilePath.substr(0, lastSlash);
    }

    outputDir = outputDir + "\\frames\\";

    if (!std::filesystem::exists(outputDir)) std::filesystem::create_directory(outputDir);
    else {
        for (const auto& entry : std::filesystem::directory_iterator(outputDir)) {
            if (std::filesystem::is_regular_file(entry.status())) {
                std::filesystem::remove(entry.path());
            }
        }
    }

    error = gifToPng(gifFilePath, outputDir);
    error = framesToBin(outputDir);
    error = Transmit_COM_Bluetooth();

    return 0;
}