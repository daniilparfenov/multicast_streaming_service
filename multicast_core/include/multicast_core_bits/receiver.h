#ifndef RECEIVER_H
#define RECEIVER_H

#include <netinet/in.h>

#include <atomic>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <thread>
#include <unordered_map>

namespace MulticastLib {

class Receiver {
   public:
    Receiver(const std::string& multicastAddress, int port);
    ~Receiver();

    bool start();
    void stop();
    cv::Mat getLatestFrame();

   private:
    struct FrameData {
        std::unordered_map<uint16_t, std::vector<uint8_t>> chunks;
        uint16_t expected_chunks;
        std::chrono::steady_clock::time_point timestamp;
    };

    bool receiveLoop();
    bool setupSocket();
    void processPacket(const std::vector<uint8_t>& buffer, ssize_t recvLen);
    void cleanupExpiredFrames();

    std::string multicastIP_;
    int port_;
    int sockfd_;
    int data_received;
    struct sockaddr_in localAddr_;
    struct ip_mreq mreq_;

    std::atomic<bool> isReceiving_;
    std::thread receiveThread_;

    std::unordered_map<std::string, FrameData> frames_;
    std::mutex framesMutex_;

    cv::Mat lastFrame_;
    std::mutex frameMutex_;
};

}  // namespace MulticastLib

#endif  // RECEIVER_H
