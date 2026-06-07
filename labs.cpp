#include <iostream>
#include <ostream>
#include <cstring>
#include <new>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <unistd.h>

using namespace std;

struct node {
    node* next;
};

class memorypool {
private:
    size_t chunk_size;
    size_t pool_size;
    void* poolstart;
    node* freelisthead;
public:
    memorypool(size_t chunk_size, int chunksnum) {
        if (chunk_size >= sizeof(node*)) {
            this->chunk_size = chunk_size;
        }else {
            this->chunk_size = sizeof(node*);
        }

        pool_size = chunk_size * chunksnum;
        poolstart = malloc(pool_size);

        freelisthead = static_cast<node*>(poolstart);
        node* current = freelisthead;
        for (int i = 1; i < chunksnum; i++) {
            current->next = reinterpret_cast<node*>(static_cast<char*>(poolstart) + (i * this->chunk_size));
            current = current->next;
        }
        current->next = nullptr;
    }
    ~memorypool() {
        free(poolstart);
    }

    void* allocate() {
        if (!freelisthead) {
            return nullptr;
        }

        node* current = freelisthead;
        freelisthead = freelisthead->next;
        return current;
    }

    void deallocate(void* ptr) {
        if (!ptr) return;
        node* current = static_cast<node*>(ptr);
        current->next = freelisthead;
        freelisthead = current;
    }
};

unsigned short csum(unsigned short *buf, int nwords)
{
    unsigned long sum;
    for(sum=0; nwords>0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum &0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

void sumbits() {
    struct hdr {
        int num1;
        int num2;
        int num3;
    };
    struct bdy {
        int num1;
        int num2;
        int num3;
    };

    char buffer[sizeof(struct hdr) + sizeof(struct bdy)];
    struct hdr* h = (struct hdr*) buffer;
    struct bdy* b = (struct bdy*) (buffer + sizeof(struct hdr));

    h->num1 = 10;
    h->num2 = 20;
    h->num3 = 30;
    b->num1 = 1.1;
    b->num2 = 2.2;
    b->num3 = 3.3;

    cout << "buffer size: " << sizeof(buffer) << "\n";
    char* ptr = nullptr;
    for (int i = 1; i <= 2; i++) {
        char mini_buffer[sizeof(struct hdr)];
        memset(mini_buffer, 0, sizeof(mini_buffer));
        if (i == 1) {
            for (int w = 0; w <= sizeof(struct hdr); w++) {
                mini_buffer[w] = buffer[w];
            }
        }else if (i == 2) {
            for (int w = 0; w <= sizeof(struct bdy); w++) {
                mini_buffer[w] = buffer[sizeof(struct hdr) + w];
            }
        }else {
            cout << "for loop error \n";
        }

        cout << "mini buffer size: " << sizeof(mini_buffer) << "\n";

        for (int j = 0; j < sizeof(mini_buffer); j += 4) {
            char buffarr[4];
            memset(buffarr, 0, sizeof(buffarr));

            memcpy(&buffarr[0], &mini_buffer[j], 4);

            uint8_t t1 = buffarr[0];
            uint8_t t2 = buffarr[1];
            uint8_t t3 = buffarr[2];
            uint8_t t4 = buffarr[3];

            uint32_t tmax = t1 + t2 + t3 + t4;

            cout << tmax << endl;
        }
    }
    cout << "\n";
}

int main() {

    sumbits();

    uint32_t src_addr = inet_addr("127.0.0.1");
    uint32_t dst_addr = inet_addr("192.168.1.254");
    uint16_t src_port = htons(8888);
    uint16_t dst_port = htons(8889);
    int PACKET_LEN = sizeof(struct iphdr) + sizeof(struct udphdr);


    int sock = socket(PF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sock < 0) {
        cout << "Error creating socket" << endl;
        return 1;
    }else {
        cout << "Socket created" << endl;
    }

    const int one = 1;
    if(setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt() error");
        exit(2);
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = src_addr;
    sin.sin_port = src_port;

    char buffer[PACKET_LEN];
    memset(buffer, 0, sizeof(buffer));
    struct iphdr* ip = (struct iphdr*) buffer;
    struct udphdr* udp = (struct udphdr*) (buffer + sizeof(struct iphdr));

    ip->ihl      = 5;
    ip->version  = 4;
    ip->tos      = 16;
    ip->tot_len  = sizeof(struct iphdr) + sizeof(struct udphdr);
    ip->id       = htons(54321);
    ip->ttl      = 64;
    ip->protocol = 17; // UDP
    // source IP address, can use spoofed address here
    ip->saddr = src_addr;
    ip->daddr = dst_addr;

    // fabricate the UDP header
    udp->source = src_port;
    // destination port number
    udp->dest = dst_port;
    udp->len = htons(sizeof(struct udphdr));

    ip->check = csum((unsigned short*) buffer, PACKET_LEN);

    if (sendto(sock, buffer, ip->tot_len, 0, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        cout << "Error sendto" << endl;
        return 1;
    }
    cout << "OK: one packet is sent.\n";

    close(sock);
    return 0;
}