#include "sender.h"

#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>
#include <opencv2/opencv.hpp>
#include <random>

namespace MulticastLib {

Sender::Sender(const std::string& multicastAddress, int port) 
    : multicastIP_(multicastAddress), port_(port), sockfd_(INVALID_SOCKET), 
      heartbeatSock_(INVALID_SOCKET), isStreaming_(false) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

Sender::~Sender() {
    stopStream();
    WSACleanup();
}

bool Sender::setupSocket() {
    // Multicast socket
    sockfd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd_ == INVALID_SOCKET) {
        std::cerr << "Failed to create socket: " << WSAGetLastError() << std::endl;
        return false;
    }

    // Heartbeat socket
    heartbeatSock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (heartbeatSock_ == INVALID_SOCKET) {
        std::cerr << "Failed to create heartbeat socket: " << WSAGetLastError() << std::endl;
        return false;
    }

    // Bind heartbeat socket
    sockaddr_in heartbeatAddr{};
    heartbeatAddr.sin_family = AF_INET;
    heartbeatAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    heartbeatAddr.sin_port = htons(port_ + 1);

    if (bind(heartbeatSock_, (sockaddr*)&heartbeatAddr, sizeof(heartbeatAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        return false;
    }

    // Configure multicast socket
    multicastAddr_.sin_family = AF_INET;
    multicastAddr_.sin_addr.s_addr = inet_addr(multicastIP_.c_str());
    multicastAddr_.sin_port = htons(port_);

    int ttl = 1;
    if (setsockopt(sockfd_, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl)) == SOCKET_ERROR) {
        std::cerr << "setsockopt failed: " << WSAGetLastError() << std::endl;
        return false;
    }

    return true;
}

void Sender::startHeartbeatServer() {
    heartbeatThread_ = std::thread([this]() {
        char buffer[1024];
        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);

        while (isStreaming_) {
            int bytes = recvfrom(heartbeatSock_, buffer, sizeof(buffer), 0,
                (sockaddr*)&clientAddr, &addrLen);

            if (bytes > 0) {
                char clientIP[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);

                std::lock_guard<std::mutex> lock(clientsMutex_);
                activeClients_[clientIP] = std::chrono::system_clock::now();
                activeUsersCount_ = activeClients_.size();
            }

            // Cleanup every 5 seconds
            std::this_thread::sleep_for(std::chrono::seconds(5));
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

    camera_.open(0, cv::CAP_DSHOW);
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
    // Останавливает стрим и освобождает ресурсы
    isStreaming_ = false;
    if (streamThread_.joinable()) streamThread_.join();
    camera_.release();
    close(sockfd_);
}

void Sender::streamLoop() {
    while (isStreaming_) {
        // Захват кадра
        cv::Mat frame;
        camera_ >> frame;
        if (frame.empty()) {
            std::cerr << "Failed to capture frame" << std::endl;
            continue;
        }

        // Запоминание последнего кадра (для вывода превью)
        {
            std::lock_guard<std::mutex> lock(lastFrameMutex_);
            this->lastFrame_ = frame.clone();
        }

        // Отправка кадра по multicast
        sendFrameToMulticast(frame.clone());
        std::this_thread::sleep_for(
            std::chrono::milliseconds(33));  // Задержка, выходит примерно 30 FPS
    }
}

void Sender::sendFrameToMulticast(const cv::Mat& frame) {
    // Downgrade фрейма, чтобы влез в один UDP пакет
    const size_t CHUNK_SIZE = 1024;  // Размер чанка в байтах

    struct {
        uint8_t frame_id[8];
        uint16_t chunk_num;
        uint16_t total_chunks;
    } header;

    // Сжимаем кадр в JPEG
    std::vector<uchar> buffer;
    std::vector<int> params{cv::IMWRITE_JPEG_QUALITY, 80};
    cv::imencode(".jpg", frame, buffer, params);

    // Генерируем 8-байтовый UUID
    std::array<uint8_t, 8> frame_id;
    std::random_device rd;
    std::generate(frame_id.begin(), frame_id.end(), [&]() { return rd() % 256; });

    // Рассчитываем количество чанков
    const size_t total_chunks = (buffer.size() + CHUNK_SIZE - 1) / CHUNK_SIZE;

    // Отправляем чанки
    ssize_t sent = 0;
    for (size_t i = 0; i < total_chunks; ++i) {
        // Формируем заголовок
        memcpy(header.frame_id, frame_id.data(), 8);
        header.chunk_num = htons(static_cast<uint16_t>(i));
        header.total_chunks = htons(static_cast<uint16_t>(total_chunks));

        // Формируем пакет
        std::vector<uchar> packet(sizeof(header) + CHUNK_SIZE);
        memcpy(packet.data(), &header, sizeof(header));

        // Копируем данные чанка
        size_t offset = i * CHUNK_SIZE;
        size_t chunk_size = std::min(CHUNK_SIZE, buffer.size() - offset);
        memcpy(packet.data() + sizeof(header), buffer.data() + offset, chunk_size);

        // Отправка
        sent += sendto(sockfd_, packet.data(), packet.size(), 0, (struct sockaddr*)&multicastAddr_,
                       sizeof(multicastAddr_));
    }

    if (sent < 0) {
        perror("sendto failed");
    } else {
        std::cout << "Sent " << sent << " bytes in " << total_chunks << " chunks" << std::endl;
    }
}

cv::Mat Sender::getPreviewFrame() {
    std::lock_guard<std::mutex> lock(lastFrameMutex_);
    return lastFrame_.clone();  // Возвращаем копию последнего кадра
}

}  // namespace MulticastLib
