#include <arpa/inet.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <unordered_map>
#include <vector>

#define MCAST_GRP "224.0.0.1"
#define MCAST_PORT 5000
#define BUFFER_SIZE 65507

struct Frame {
    std::unordered_map<uint16_t, std::vector<uint8_t>> chunks;
    uint16_t expected_chunks = 0;
    std::chrono::steady_clock::time_point timestamp;
};

int main() {
    // Создаем сокет
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket failed");
        return 1;
    }

    // Разрешаем повторное использование адреса
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        close(sockfd);
        return 1;
    }

    // Привязываем сокет к порту
    struct sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);  // принимать со всех интерфейсов
    localAddr.sin_port = htons(MCAST_PORT);

    if (bind(sockfd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        perror("bind failed");
        close(sockfd);
        return 1;
    }

    // Присоединяемся к multicast-группе
    struct ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr((const char*)MCAST_GRP);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0) {
        perror("setsockopt IP_ADD_MEMBERSHIP failed");
        close(sockfd);
        return 1;
    }

    std::cout << "Listening for multicast stream on " << MCAST_GRP << ":" << MCAST_PORT
              << std::endl;

    std::unordered_map<std::string, Frame> frames;
    std::vector<uint8_t> buffer(BUFFER_SIZE);

    while (true) {
        sockaddr_in senderAddr{};
        socklen_t addrLen = sizeof(senderAddr);
        ssize_t recvLen =
            recvfrom(sockfd, buffer.data(), BUFFER_SIZE, 0, (sockaddr*)&senderAddr, &addrLen);

        if (recvLen < 12) continue;  // слишком короткий пакет

        // Извлекаем ID кадра и номер чанка
        uint8_t frame_id[8];
        uint16_t chunk_no, total_chunks;

        memcpy(frame_id, buffer.data(), 8);
        memcpy(&chunk_no, buffer.data() + 8, 2);
        memcpy(&total_chunks, buffer.data() + 10, 2);

        chunk_no = ntohs(chunk_no);
        total_chunks = ntohs(total_chunks);

        std::string fid(reinterpret_cast<char*>(frame_id), 8);
        auto& frame = frames[fid];
        frame.expected_chunks = total_chunks;
        frame.timestamp = std::chrono::steady_clock::now();

        std::vector<uint8_t> chunk_data(buffer.begin() + 12, buffer.begin() + recvLen);
        frame.chunks[chunk_no] = std::move(chunk_data);

        if (frame.chunks.size() == total_chunks) {
            std::vector<uint8_t> ordered_data;
            for (uint16_t i = 0; i < total_chunks; ++i) {
                auto it = frame.chunks.find(i);
                if (it != frame.chunks.end()) {
                    ordered_data.insert(ordered_data.end(), it->second.begin(), it->second.end());
                }
            }

            cv::Mat image = cv::imdecode(ordered_data, cv::IMREAD_COLOR);
            if (!image.empty()) {
                std::cout << "Received complete frame (" << ordered_data.size() << " bytes)"
                          << std::endl;
                cv::imshow("RECV CAM", image);
                if (cv::waitKey(1) == 27) {
                    break;
                }
            }

            frames.erase(fid);
        }

        // Удаляем устаревшие кадры
        auto now = std::chrono::steady_clock::now();
        std::vector<std::string> expired_keys;
        for (auto& [key, f] : frames) {
            auto elapsed =
                std::chrono::duration_cast<std::chrono::seconds>(now - f.timestamp).count();
            if (elapsed > 5) {
                uint16_t lost = f.expected_chunks - f.chunks.size();
                std::cout << "Dropping incomplete frame " << std::hex;
                for (char c : key)
                    std::cout << std::setw(2) << std::setfill('0') << (int)(uint8_t)c;
                std::cout << " (lost " << lost << " packets)" << std::endl;
                expired_keys.push_back(key);
            }
        }

        for (const auto& key : expired_keys) {
            frames.erase(key);
        }
    }

    close(sockfd);
    return 0;
}
