#include "sender.h"
#include <iostream>
#include <random>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

namespace MulticastLib {

Sender::Sender(const std::string& multicastAddress, int port) 
    : multicastIP_(multicastAddress), port_(port), 
      sockfd_(-1), heartbeatSockfd_(-1), isStreaming_(false) {}

Sender::~Sender() {
    stopStream();
    if (sockfd_ != -1) close(sockfd_);
    if (heartbeatSockfd_ != -1) close(heartbeatSockfd_);
}

bool Sender::setupSocket() {
    // Multicast socket
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        perror("socket");
        return false;
    }

    // Heartbeat socket
    heartbeatSockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (heartbeatSockfd_ < 0) {
        perror("heartbeat socket");
        return false;
    }

    // Bind heartbeat socket
    struct sockaddr_in heartbeatAddr {};
    memset(&heartbeatAddr, 0, sizeof(heartbeatAddr));
    heartbeatAddr.sin_family = AF_INET;
    heartbeatAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    heartbeatAddr.sin_port = htons(port_ + 1);

    if (bind(heartbeatSockfd_, (struct sockaddr*)&heartbeatAddr, sizeof(heartbeatAddr)) < 0) {
        perror("bind");
        return false;
    }

    // Configure multicast socket
    int ttl = 1;
    if (setsockopt(sockfd_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        perror("setsockopt TTL");
        return false;
    }

    memset(&multicastAddr_, 0, sizeof(multicastAddr_));
    multicastAddr_.sin_family = AF_INET;
    multicastAddr_.sin_addr.s_addr = inet_addr(multicastIP_.c_str());
    multicastAddr_.sin_port = htons(port_);

    return true;
}

void Sender::startHeartbeatServer() {
    heartbeatThread_ = std::thread([this]() {
        struct pollfd fds[1];
        fds[0].fd = heartbeatSockfd_;
        fds[0].events = POLLIN;

        while (isStreaming_) {
            int rc = poll(fds, 1, 100); // Таймаут 100 мс
            if (rc > 0) {
                struct sockaddr_in clientAddr{};
                socklen_t addrLen = sizeof(clientAddr);
                char buffer[1024];

                ssize_t bytes = recvfrom(heartbeatSockfd_, buffer, sizeof(buffer), 0,
                                       (struct sockaddr*)&clientAddr, &addrLen);
                if (bytes > 0) {
                    char clientIP[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);

                    std::lock_guard<std::mutex> lock(clientsMutex_);
                    activeClients_[clientIP] = std::chrono::system_clock::now();
                    activeUsersCount_ = activeClients_.size();
                }
            }
            cleanupInactiveClients();
        }
    });
}

void Sender::cleanupInactiveClients() {
    auto now = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> lock(clientsMutex_);

    for (auto it = activeClients_.begin(); it != activeClients_.end();) {
        if (now - it->second > std::chrono::seconds(15)) {
            it = activeClients_.erase(it);
        } else {
            ++it;
        }
    }
    activeUsersCount_ = activeClients_.size();
}

bool Sender::startStream() {
    if (isStreaming_) return false;
    if (!setupSocket()) return false;

    camera_.open(0, cv::CAP_V4L2);
    if (!camera_.isOpened()) {
        std::cerr << "Failed to open camera" << std::endl;
        return false;
    }

    isStreaming_ = true;
    streamThread_ = std::thread(&Sender::streamLoop, this);
    startHeartbeatServer();
    return true;
}

void Sender::stopStream() {
    isStreaming_ = false;
    if (streamThread_.joinable()) streamThread_.join();
    if (heartbeatThread_.joinable()) heartbeatThread_.join();
    if (camera_.isOpened()) camera_.release();
}

void Sender::streamLoop() {
    while (isStreaming_) {
        cv::Mat frame;
        camera_ >> frame;
        if (frame.empty()) continue;

        {
            std::lock_guard<std::mutex> lock(lastFrameMutex_);
            lastFrame_ = frame.clone();
        }

        sendFrameToMulticast(frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
}

void Sender::sendFrameToMulticast(const cv::Mat& frame) {
    const size_t CHUNK_SIZE = 1024;
    struct {
        uint8_t frame_id[8];
        uint16_t chunk_num;
        uint16_t total_chunks;
    } header;

    std::vector<uchar> buffer;
    cv::imencode(".jpg", frame, buffer, {cv::IMWRITE_JPEG_QUALITY, 80});

    std::random_device rd;
    std::array<uint8_t, 8> frame_id;
    std::generate(frame_id.begin(), frame_id.end(), [&]() { return rd() % 256; });

    const size_t total_chunks = (buffer.size() + CHUNK_SIZE - 1) / CHUNK_SIZE;

    for (size_t i = 0; i < total_chunks; ++i) {
        memcpy(header.frame_id, frame_id.data(), 8);
        header.chunk_num = htons(static_cast<uint16_t>(i));
        header.total_chunks = htons(static_cast<uint16_t>(total_chunks));

        std::vector<uchar> packet(sizeof(header) + CHUNK_SIZE);
        memcpy(packet.data(), &header, sizeof(header));

        size_t offset = i * CHUNK_SIZE;
        size_t chunk_size = std::min(CHUNK_SIZE, buffer.size() - offset);
        memcpy(packet.data() + sizeof(header), buffer.data() + offset, chunk_size);

        sendto(sockfd_, packet.data(), packet.size(), 0, 
             (struct sockaddr*)&multicastAddr_, sizeof(multicastAddr_));
    }
}

cv::Mat Sender::getPreviewFrame() {
    std::lock_guard<std::mutex> lock(lastFrameMutex_);
    return lastFrame_.clone();
}

int Sender::getActiveUsers() const {
    return activeUsersCount_.load();
}

} // namespace MulticastLib
