#include "receiver.h"

#include <arpa/inet.h>
#include <unistd.h>

#include <iomanip>
#include <iostream>
#define LISTENING_TIMEOUT_S 3
namespace MulticastLib {

Receiver::Receiver(const std::string& multicastIP, int port)
    : multicastIP_(multicastIP), port_(port), isReceiving_(false), sockfd_(-1), data_received(0) {}

Receiver::~Receiver() {
    stop();
    if (sockfd_ != -1) close(sockfd_);
}

bool Receiver::setupSocket() {
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        perror("socket failed");
        return false;
    }

    struct timeval tv{.tv_sec = LISTENING_TIMEOUT_S, .tv_usec = 0};

    if (setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt SO_RCVTIMEO failed");
        close(sockfd_);
        return false;
    }

    int reuse = 1;
    if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        return false;
    }

    memset(&localAddr_, 0, sizeof(localAddr_));
    localAddr_.sin_family = AF_INET;
    localAddr_.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr_.sin_port = htons(port_);

    if (bind(sockfd_, (struct sockaddr*)&localAddr_, sizeof(localAddr_)) < 0) {
        perror("bind failed");
        return false;
    }

    mreq_.imr_multiaddr.s_addr = inet_addr(multicastIP_.c_str());
    mreq_.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sockfd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq_, sizeof(mreq_)) < 0) {
        perror("setsockopt IP_ADD_MEMBERSHIP failed");
        return false;
    }

    return true;
}

bool Receiver::start() {
    if (isReceiving_) return false;
    if (!setupSocket()) return false;

    isReceiving_ = true;
    receiveThread_ = std::thread(&Receiver::receiveLoop, this);
    return true;
}

void Receiver::stop() {
    isReceiving_ = false;
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        lastFrame_ = cv::Mat();
    }
    if (sockfd_ != -1) {
        close(sockfd_);
    }

    if (receiveThread_.joinable()) receiveThread_.join();
}

bool Receiver::receiveLoop() {
    std::vector<uint8_t> buffer(65507);
    auto lastPacketTime = std::chrono::steady_clock::now();
    while (isReceiving_) {
        sockaddr_in senderAddr{};
        socklen_t addrLen = sizeof(senderAddr);
        ssize_t recvLen =
            recvfrom(sockfd_, buffer.data(), buffer.size(), 0, (sockaddr*)&senderAddr, &addrLen);

        if (recvLen > 0) {
            processPacket(buffer, recvLen);
            cleanupExpiredFrames();
            lastPacketTime = std::chrono::steady_clock::now();
        } else {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastPacketTime);
            if (elapsed.count() >= LISTENING_TIMEOUT_S) {
                std::cout << "Stream is not available" << std::endl;
                isReceiving_ = false;
            }
        }
    }
    return false;
}

void Receiver::processPacket(const std::vector<uint8_t>& buffer, ssize_t recvLen) {
    if (recvLen < 12) return;

    uint8_t frame_id[8];
    uint16_t chunk_no, total_chunks;

    memcpy(frame_id, buffer.data(), 8);
    memcpy(&chunk_no, buffer.data() + 8, 2);
    memcpy(&total_chunks, buffer.data() + 10, 2);
    data_received += 12;
    chunk_no = ntohs(chunk_no);
    total_chunks = ntohs(total_chunks);
    std::string fid(reinterpret_cast<char*>(frame_id), 8);

    {
        std::lock_guard<std::mutex> lock(framesMutex_);
        auto& frame = frames_[fid];
        frame.expected_chunks = total_chunks;
        frame.timestamp = std::chrono::steady_clock::now();
        std::vector<uint8_t> chunk(buffer.begin() + 12, buffer.begin() + recvLen);
        data_received += chunk.size();
        frame.chunks[chunk_no] = std::move(chunk);

        if (frame.chunks.size() == total_chunks) {
            std::vector<uint8_t> ordered_data;
            for (uint16_t i = 0; i < total_chunks; ++i) {
                if (frame.chunks.count(i)) {
                    auto& chunk = frame.chunks[i];
                    ordered_data.insert(ordered_data.end(), chunk.begin(), chunk.end());
                }
            }
            std::cout << "Received " << data_received << " bytes in " << total_chunks << " chunks"
                      << std::endl;
            cv::Mat frame = cv::imdecode(ordered_data, cv::IMREAD_COLOR);
            if (!frame.empty()) {
                std::lock_guard<std::mutex> frameLock(frameMutex_);
                lastFrame_ = frame.clone();
            }
            data_received = 0;
            frames_.erase(fid);
        }
    }
}

void Receiver::cleanupExpiredFrames() {
    auto now = std::chrono::steady_clock::now();
    std::vector<std::string> to_remove;

    {
        std::lock_guard<std::mutex> lock(framesMutex_);
        for (auto& [fid, frame] : frames_) {
            auto elapsed =
                std::chrono::duration_cast<std::chrono::seconds>(now - frame.timestamp).count();

            if (elapsed > 5) {
                uint16_t lost = frame.expected_chunks - frame.chunks.size();
                std::cout << "Dropping frame " << std::hex << std::setfill('0');
                for (uint8_t c : fid) std::cout << std::setw(2) << static_cast<int>(c);
                std::cout << " (lost " << std::dec << lost << " chunks)\n";
                to_remove.push_back(fid);
            }
        }
    }

    std::lock_guard<std::mutex> lock(framesMutex_);
    for (auto& fid : to_remove) {
        frames_.erase(fid);
    }
}

cv::Mat Receiver::getLatestFrame() {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return lastFrame_.clone();
}

bool Receiver::isReceiving() {
    if (isReceiving_) return true;
    return false;
}

}  // namespace MulticastLib
