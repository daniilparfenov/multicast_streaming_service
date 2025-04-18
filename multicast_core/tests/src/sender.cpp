#include <multicast_core.h>

#include <chrono>
#include <iostream>
#include <opencv2/opencv.hpp>

int main() {
    MulticastLib::Sender s("224.0.0.1", 5000);

    // Запуск стрима
    if (!s.startStream()) {
        std::cerr << "failed to start stream" << std::endl;
        return -1;
    }

    while (1) {
        // Отображение превью
        cv::Mat frame = s.getPreviewFrame();
        if (frame.empty()) {
            std::cerr << "Empty Frame" << std::endl;
            continue;
        }
        cv::imshow("SEND PREVIEW CAM", frame);

        // Выход по нажатию ESC
        if (cv::waitKey(1) == 27) {  // 27 — ESC
            break;
        }
    }

    s.stopStream();
    return 0;
}
