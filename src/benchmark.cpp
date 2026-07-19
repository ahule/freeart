#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>
#include <climits>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include "fcp.h"

using namespace std;

string SERVER_IP = "192.168.1.107";
int MSGS_COUNT = 1000;
int TOTAL_CLIENTS = 100;

vector<vector<long long>> all_results(TOTAL_CLIENTS, vector<long long>(MSGS_COUNT));

void client(int i) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        return;
    }

    struct sockaddr_in sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));

    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = inet_addr(SERVER_IP.c_str());
    sock_addr.sin_port = htons(8888);

    if (connect(sockfd, (struct sockaddr*) &sock_addr, sizeof(sock_addr)) == -1) {
        close(sockfd);
        return;
    }

    send_packet(sockfd, LOGIN, "password");

    packethdr header;

    for (int j = 0; j < MSGS_COUNT; j++) {
        auto start = chrono::high_resolution_clock::now();
        send_packet(sockfd, MESSAGE, "message content");
        char* msg = recv_packet(sockfd, header);
        auto end = chrono::high_resolution_clock::now();
        auto elapsed = chrono::duration_cast<chrono::microseconds>(end - start).count();
        delete[] msg;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        all_results[i][j] = elapsed;
    }
    close(sockfd);
}

int main() {

    vector<thread> threads;
    for (int i = 0; i < TOTAL_CLIENTS; ++i) {
        threads.push_back(thread(client, i));
    }
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }


    long long total_sum = 0;
    long long max_latency = 0;
    long long min_latency = LLONG_MAX;

    for (const auto& client_res : all_results) {
        for (long long j : client_res) {
            total_sum += j;
            if (j > max_latency) max_latency = j;
            if (j < min_latency) min_latency = j;
        }
    }

    int total_messages = TOTAL_CLIENTS * MSGS_COUNT;
    double avg_latency = static_cast<double>(total_sum) / total_messages;

    cout << "\n--- Benchmark Results ---" << endl;
    cout << "Total Messages Processed: " << total_messages << endl;
    cout << "Avg Latency: " << avg_latency << " us (" << avg_latency / 1000.0 << " ms)" << endl;
    cout << "Min Latency: " << min_latency << " us (" << min_latency / 1000.0 << " ms)" << endl;
    cout << "Max Latency: " << max_latency << " us (" << max_latency / 1000.0 << " ms)" << endl;
}