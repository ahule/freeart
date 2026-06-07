#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/ip.h>
#include <linux/udp.h>

using namespace std;

unsigned short csum(unsigned short *buf, int nwords)
{
    unsigned long sum;
    for(sum=0; nwords>0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum &0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

int main() {
    uint32_t src_addr = inet_addr("127.0.0.1");
    uint32_t dst_addr = inet_addr("192.168.1.254");
    uint16_t src_port = htons(8888);
    uint16_t dst_port = htons(8889);
    int PACKET_LEN = sizeof(struct iphdr) + sizeof(struct udphdr);
    const int one = 1;

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sock == -1) {
        cout << "Error creating socket" << endl;
        return 1;
    }else {
        cout << "Socket created" << endl;
    }

    if(setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt() error");
        exit(2);
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = dst_addr;
    sin.sin_port = dst_port;


    char buffer[PACKET_LEN];
    memset(buffer, 0, PACKET_LEN);
    struct iphdr* ip = (struct iphdr*) buffer;
    struct udphdr* udp = (struct udphdr*) (buffer + sizeof(struct iphdr));

    // fabricate the IP header
    ip->ihl      = 5;
    ip->version  = 4;
    ip->tos      = 16; // low delay
    ip->tot_len  = sizeof(struct iphdr) + sizeof(struct udphdr);
    ip->id       = htons(54321);
    ip->ttl      = 64; // hops
    ip->protocol = 17; // UDP
    // source IP address, can use spoofed address here
    ip->saddr = src_addr;
    ip->daddr = dst_addr;

    // fabricate the UDP header
    udp->source = src_port;
    // destination port number
    udp->dest = dst_port;
    udp->len = htons(sizeof(struct udphdr));

    // calculate the checksum for integrity
    ip->check = csum((unsigned short *)buffer,sizeof(struct iphdr) + sizeof(struct udphdr));

    if (sendto(sock, buffer, ip->tot_len, 0, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        cout << "Error sendto" << endl;
        return 1;
    }
    cout << "OK: one packet is sent.\n";

    close(sock);
    return 0;
}