#include <multicast_core.h>

#include <iostream>
#include <opencv2/opencv.hpp>
#include <string>

#define MCAST_GRP "224.0.0.1"
#define MCAST_PORT 5000

int main() {
    MulticastLib::Receiver receiver(MCAST_GRP, MCAST_PORT);

    if (!receiver.start()) {
        std::cerr << "Failed to start receiver" << std::endl;
        return 1;
    }

    while (true) {
        cv::Mat frame = receiver.getLatestFrame();
        if (!frame.empty()) {
            cv::imshow("Receiver Preview", frame);
        }
        if (cv::waitKey(1) == 27) break;

        MulticastLib::ReceiverStatistics stats = receiver.getStatistics();
        std::cout << stats.avgFps << std::endl;
        std::cout << "Общее количество полученных пакетов" << stats.totalPacketsReceived
                  << std::endl;
        std::cout << "Общее количество битых пакетов" << stats.totalCorruptedPackets << std::endl;
        std::cout << "Общее количество декодированных кадров" << stats.totalFramesDecoded
                  << std::endl;
        std::cout << "Средний FPS" << stats.avgFps << std::endl;
    }

    receiver.stop();
    return 0;
}
