#include <multicast_core.h>

#include <opencv2/opencv.hpp>

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
    }

    receiver.stop();
    return 0;
}
