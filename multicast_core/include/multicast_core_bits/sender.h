#ifndef SENDER_H
#define SENDER_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <thread>
#include <unordered_map>

namespace MulticastLib {

class Sender {
public:
    Sender(const std::string& multicastAddress, int port);
    ~Sender();

    bool startStream();
    void stopStream();
    cv::Mat getPreviewFrame();
    int getActiveUsers() const;

private:
    void streamLoop();
    bool setupSocket();
    void startHeartbeatServer();
    void cleanupInactiveClients();
    void sendFrameToMulticast(const cv::Mat& frame);

    std::string multicastIP_;
    int port_;
    int sockfd_;
    int heartbeatSockfd_;
    struct sockaddr_in multicastAddr_;

    cv::VideoCapture camera_;
    std::atomic<bool> isStreaming_;
    std::thread streamThread_;
    std::thread heartbeatThread_;

    std::unordered_map<std::string, std::chrono::system_clock::time_point> activeClients_;
    mutable std::mutex clientsMutex_;
    std::atomic<int> activeUsersCount_{0};

    cv::Mat lastFrame_;
    std::mutex lastFrameMutex_;
};

} // namespace MulticastLib

#endif
