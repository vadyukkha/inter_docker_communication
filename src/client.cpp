// Клиент(producer) делит изображение и отправляет его на сервер

#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <opencv2/opencv.hpp>
#include <string>

#include "image_inverting.h"

// Файловый дескриптор сокета клиента
int sockfd;

void error(const char *msg) {
    perror(msg);
    exit(0);
}

// Обработчик сигнала
void handle_signal(int sig) {
    if (sig == SIGINT) {
        printf("\n[Error] Received SIGINT. Exiting...\n");
        close(sockfd);
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    // Обработчик сигнала SIGINT, чтобы корректно закрывать сокет при остановке процесса через
    // терминал
    signal(SIGINT, handle_signal);

    int portno, n;          // Номер порта + переменная для кол-ва полученных, отправленных байт
    sockaddr_in serv_addr;  // Структура для хранения адреса сервера, к которому подключимся
    hostent *server;        // Структура для хранения информации о хосте

    if (argc < 3) {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(0);
    }

    portno = atoi(argv[2]);  // Номер порта для подключения берется из консольных аргументов
    sockfd = socket(AF_INET, SOCK_STREAM, 0);  // Создание сокета клиента
    if (sockfd < 0) error("ERROR opening socket");

    server = gethostbyname(argv[1]);  // По имени хоста в интернете получаем информацию о нем
    if (server == NULL) error("ERROR, no provided host");

    // Заполняем информацию о адресе сервера полученными данными
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);  // Перевод из host byte order в network byte order!

    // Соединяемся с сервером
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    // Загрузка изображения для обработки
    std::string path = RESOURCES_DIR "/cute_dog.jpg";
    cv::Mat image = cv::imread(path);
    if (image.empty()) error("ERROR loading image");

    // Делим изображение
    std::vector<Task> dividedImage;
    divideImage(image, 16, dividedImage);

    // Отправка кусков изображение серверу для обработки и запись результата в image
    for (Task &task : dividedImage) {
        // Сериализация куска изображения в формат JPEG
        std::vector<uchar> buffer;
        cv::imencode(".jpg", task.imagePart, buffer);

        // Отправка размера данных
        size_t dataSize = buffer.size();
        n = send(sockfd, &dataSize, sizeof(dataSize), 0);
        if (n < 0) error("ERROR writing to socket");

        // Отправка данных
        n = send(sockfd, buffer.data(), buffer.size(), 0);
        if (n < 0) error("ERROR writing to socket");

        // Получение инвертированного куска изображения
        n = recv(sockfd, &dataSize, sizeof(dataSize), 0);
        if (n < 0) error("ERROR reading from socket");
        buffer.resize(dataSize);
        n = recv(sockfd, buffer.data(), dataSize, 0);
        if (n < 0) error("ERROR reading from socket");

        // Десериализация куска изображения
        task.imagePart = cv::imdecode(buffer, cv::IMREAD_COLOR);
        if (task.imagePart.empty()) error("ERROR deserialization image");

        // Запись обработанного куска в результат
        task.imagePart.copyTo(image(cv::Range(task.rowOffset, task.rowOffset + task.imagePart.rows),
                                    cv::Range::all()));
    }

    cv::imshow("Inverted Image", image);
    cv::waitKey(0);
    close(sockfd);
    return 0;
}
