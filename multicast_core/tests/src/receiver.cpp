#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>
#include <opencv2/opencv.hpp>
#include <vector>

int main() {
    const std::string multicastIP = "224.0.0.1";  // такой же как у Sender
    const int port = 5000;

    // Создаем UDP сокет
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
    localAddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        perror("bind failed");
        close(sockfd);
        return 1;
    }

    // Присоединяемся к multicast группе
    struct ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(multicastIP.c_str());
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0) {
        perror("setsockopt IP_ADD_MEMBERSHIP failed");
        close(sockfd);
        return 1;
    }

    std::cout << "Listening for multicast stream on " << multicastIP << ":" << port << std::endl;

    // Буфер под принимаемые данные
    std::vector<uchar> buffer(65535);  // максимально возможный UDP пакет
    while (true) {
        ssize_t recvLen = recv(sockfd, buffer.data(), buffer.size(), 0);
        std::cout << "Received packet: " << recvLen << " bytes" << std::endl;
        if (recvLen < 0) {
            perror("recv failed");
            break;
        }

        // Декодируем изображение из буфера
        std::vector<uchar> frameData(buffer.begin(), buffer.begin() + recvLen);
        cv::Mat frame = cv::imdecode(frameData, cv::IMREAD_COLOR);
        if (frame.empty()) {
            std::cerr << "Failed to decode frame" << std::endl;
            continue;
        }

        // Отображаем кадр
        cv::imshow("RECV CAM", frame);
        // Выход по нажатию ESC
        if (cv::waitKey(1) == 27) {
            break;
        }
    }

    close(sockfd);
    return 0;
}
