#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <thread>

using namespace std;

// 1. إلزام المترجم بضغط الحزمة تماماً لتتطابق مع السيرفر والعميل الأصلي
#pragma pack(push, 1)
struct packethdr {
    char magic[3];
    int type;
    int length;
};
#pragma pack(pop)

void launch_test_client(int client_id) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) return;

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    saddr.sin_port = htons(8888);

    if (connect(sock, (struct sockaddr*)&saddr, sizeof(saddr)) != 0) {
        close(sock);
        return;
    }

    // 2. تصفير الحزمة تماماً في الذاكرة قبل تعبئتها
    packethdr header;
    memset(&header, 0, sizeof(header));

    // 3. التعبئة الصارمة والمباشرة للحروف لمنع وصولها فارغة
    header.magic[0] = 'F';
    header.magic[1] = 'C';
    header.magic[2] = 'P';
    header.type = 1; // Login

    string password = "password";
    header.length = password.length();

    // 4. إرسال الحجم الجديد بالكامل (المشتمل على الـ Magic وباقي البيانات)
    send(sock, &header, sizeof(header), 0);
    send(sock, password.c_str(), header.length, 0);

    int login_status = -1;
    int bytes = recv(sock, &login_status, sizeof(login_status), 0);

    if (bytes > 0 && login_status == 0) {
        cout << "Client " << client_id << " (Socket " << sock << "): Login SUCCESS!" << endl;

        char dummy_buffer[1024];
        while (true) {
            int r = recv(sock, dummy_buffer, sizeof(dummy_buffer), 0);
            if (r <= 0) break;
        }
    }
    close(sock);
}

void send_packet(int sockfd, int packet_type, const string& message) {
    packethdr header;
    memset(&header, 0, sizeof(header));

    header.type = packet_type;
    header.length = strlen(message.c_str());
    strncpy(header.magic, "FCP", 3);
    strncpy(header.version, "1.0", 3);

    if (header.length > 0) {
        header.checksum = checksum(message.c_str(), header.length);
    } else {
        header.checksum = 0;
    }

    send(sockfd, &header, sizeof(header), 0);

    if (header.length > 0) {
        send(sockfd, message.c_str(), header.length, 0);
    }
}

int main() {
    vector<thread> test_threads;
    for (int i = 1; i <= 3; ++i) {
        test_threads.push_back(thread(launch_test_client, i));
        this_thread::sleep_for(chrono::milliseconds(500));
    }
    for (auto& th : test_threads) {
        if (th.joinable()) th.join();
    }
    return 0;
}