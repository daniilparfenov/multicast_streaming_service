#include "sender.h"

#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>
#include <opencv2/opencv.hpp>
#include <random>

namespace MulticastLib {

Sender::Sender(const std::string& multicastIP, int port)
    : multicastIP_(multicastIP), port_(port), isStreaming_(false), sockfd_(-1) {}

Sender::~Sender() {
    if (sockfd_ != -1) close(sockfd_);
    if (streamThread_.joinable()) streamThread_.join();
    if (camera_.isOpened()) camera_.release();
}

bool Sender::setupSocket() {
    // Создание и конфигурация сокета
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // time to live для мультикаст пакетов = 1 позволяет доставлять пакеты только локально
    int ttl = 1;
    if (setsockopt(sockfd_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        perror("setsockopt IP_MULTICAST_TTL failed");
        return false;
    }

    memset(&multicastAddr_, 0, sizeof(multicastAddr_));
    multicastAddr_.sin_family = AF_INET;
    multicastAddr_.sin_addr.s_addr = inet_addr(multicastIP_.c_str());
    multicastAddr_.sin_port = htons(port_);
    return true;
}

bool Sender::startStream() {
    // Начинает стрим
    if (isStreaming_) return false;
    if (!setupSocket()) return false;

    // Включение камеры
    camera_.open(0, cv::CAP_ANY);
    if (!camera_.isOpened()) {
        std::cerr << "Failed to open camera_" << std::endl;
        return false;
    }

    isStreaming_ = true;

    // Запуск потока для захвата и отправки видео
    streamThread_ = std::thread(&Sender::streamLoop, this);
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
