#include <multicast_core.h>

#include <chrono>
#include <iostream>
#include <opencv2/opencv.hpp>

#define MULTICAST_GRP "224.0.0.1"
#define MULTICAST_PORT 5000

int main() {
    MulticastLib::Sender s(MULTICAST_GRP, MULTICAST_PORT);

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
