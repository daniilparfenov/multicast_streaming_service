#ifndef SENDER_H
#define SENDER_H

#include <netinet/in.h>

#include <atomic>
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

   private:
    void streamLoop();
    bool setupSocket();
    void sendFrameToMulticast(const cv::Mat& frame);

    std::string multicastIP_;
    int port_;
    int sockfd_;
    struct sockaddr_in multicastAddr_;

    cv::VideoCapture camera_;
    std::atomic<bool> isStreaming_;
    std::thread streamThread_;

    cv::Mat lastFrame_;
    std::mutex lastFrameMutex_;
};

}  // namespace MulticastLib

#endif  // SENDER_H
