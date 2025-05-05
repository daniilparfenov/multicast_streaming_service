#ifndef SENDER_H
#define SENDER_H

#include <netinet/in.h>

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <thread>

namespace MulticastLib {

class Sender {
   public:
    Sender(const std::string& multicastAddress, int port);
    ~Sender();

    bool startStream();
    void stopStream();

    cv::Mat getPreviewFrame();

    int getActiveClientCount() const;

   private:
    void streamLoop();
    bool setupSocket();
    void sendFrameToMulticast(const cv::Mat& frame);
    void startControlListener();
    void cleanupInactiveClients();

    std::string multicastIP_;
    int port_;
    int sockfd_;
    struct sockaddr_in multicastAddr_;

    cv::VideoCapture camera_;
    std::atomic<bool> isStreaming_;
    std::thread streamThread_;

    cv::Mat lastFrame_;
    std::mutex lastFrameMutex_;

    std::thread controlThread_;
    std::mutex clientMutex_;
    std::map<std::string, std::chrono::steady_clock::time_point> clientHeartbeats_;
    std::atomic<int> activeClientCount_{0};

    std::thread cleanupThread_;
};

}  // namespace MulticastLib

#endif  // SENDER_H
