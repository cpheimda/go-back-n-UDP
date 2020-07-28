#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <libgen.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <inttypes.h>

#include "client.h"
#include "files.h"
#include "send_packet.h"
#include "linkedlist.h"

#define WSIZE 7

int noOfFiles(char* filename){
    int n = 0;
    int ch = 0;
    FILE* f;
    f = fopen(filename, "r");
    if(!f){
        fprintf(stderr, "Error opening file %s.", filename);
        exit(1);
    }
    while(!feof(f)){
        ch = fgetc(f);
        if(ch == '\n'){
            n++;
        }
    }
    fclose(f);
    return n;
}

char* readFileList(char* filename, int select){
    // Leser en fil med en liste av filnavn, "returner" fil nr select
    FILE *f;
    f = fopen(filename, "r");
    if(!f){
        fprintf(stderr, "Error opening file %s.", filename);
        exit(1);
    }
    char line[128];
    char fname[128];
    int fileno = 0;
    while(fgets(line, sizeof(line), f)){
        strtok(line, "\n");
        //printf("%s\n", line);
        if(select == fileno){
            strcpy(fname, line);
        }
        fileno++;
    }
    fclose(f);
    return (char*)strdup(fname);
}

char* create_packet(int seqno, char flag, char *bytes, int request_no, char *fname){
    struct packet_header *header;
    header = (struct packet_header*) malloc(sizeof(struct packet_header));
    
    int bytes_len = 2*sizeof(uint32_t) + strlen(bytes) + 1 + strlen(fname) + 1;
    header->size = (uint32_t) htonl(PACKET_HEADER_SIZE + bytes_len);
    //header->size = (PACKET_HEADER_SIZE + bytes_len);
    //printf("Size in header is %d\n", header->size);
    header->seqno = (uint8_t) seqno;
    header->ackno = (uint8_t) 0;
    header->flag = (uint8_t) flag;
    header->unused = (uint8_t) 0x7f;
    
    char* packet = malloc(PACKET_HEADER_SIZE + bytes_len);
    memcpy(&(packet[0]), &(header->size), sizeof(uint32_t));
    //printf("Size in packet is %d\n", (packet[0] + (packet[1]<<8) + (packet[2]<<16) + (packet[3]<<24)));
    memcpy(&(packet[4]), &(header->seqno), sizeof(uint8_t));
    memcpy(&(packet[5]), &(header->ackno), sizeof(uint8_t));
    memcpy(&(packet[6]), &(header->flag), sizeof(uint8_t));
    memcpy(&(packet[7]), &(header->unused), sizeof(uint8_t));
    
    // Hvis datapakke
    if(flag == 1){
        struct bytes* data;
        data = (struct bytes*) malloc(sizeof(struct bytes));

        data->requestno = (uint32_t) htonl(request_no);
        data->fnamelength = (uint32_t) htonl(strlen(fname)+1);
        //data->requestno = (uint32_t) (request_no);
        //data->fnamelength = (uint32_t) (strlen(fname)+1);

        memcpy(&(packet[8]), &(data->requestno), sizeof(uint32_t));
        memcpy(&(packet[12]), &(data->fnamelength), sizeof(uint32_t));
        memcpy(&(packet[16]), fname, strlen(fname)+1);
        memcpy(&(packet[17+strlen(fname)]), bytes, strlen(bytes)+1);
        free(data);
    }
    free(header);
    return packet;
}
/*
void sendPacket(struct sockaddr_in server_address, struct in_addr ip_address, int sockfd,
char* servIP, int portno, char* packet){
    
    // Listen to servIP
    inet_pton(AF_INET, servIP, &ip_address);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(portno);
    server_address.sin_addr = ip_address;

    // Send packet to server
    uint32_t packet_len;
    memcpy(&packet_len, &packet[0], sizeof(uint32_t));
    packet_len = ntohl(packet_len);
    printf("Created packet of %d bytes.\n", packet_len);
    sendto(sockfd, packet, packet_len,
            0, (const struct sockaddr *) &server_address,
            sizeof(server_address));
    printf("Message sent to server.\n");
}
*/
int main(int argc, char *argv[])
{
    //appendNode(0, 0);
  
    // Check argc
    if(argc != 5)
    {
        fprintf(stderr, "Usage: %s [server IP or hostname] [server portnumber]"
        " [images list filename] [drop percentage(0-20)]\n", argv[0]);
        exit(1);
    }
    // Store argv
    char *servIP = argv[1];
    int portno = atoi(argv[2]);
    char *imglist = argv[3]; // List of filenames
    float drop = (float)atoi(argv[4])/100; // Drop percentage
    printf("Drop is %f percent\n", drop*100);

    // Create client socket (IPv4)
    int sockfd;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    // Specify address
    struct sockaddr_in server_address;
    struct in_addr ip_address;
    // Listen to servIP
    memset(&server_address, '\0', sizeof(server_address));
    inet_pton(AF_INET, servIP, &ip_address);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(portno);
    server_address.sin_addr = ip_address;
    int ack_recv = 1;
    int frameID = 0;
    int fileno = 0;
    int files = noOfFiles(imglist);
    printf("There are %d files in file %s\n", files, imglist);
    char recv_buff[PACKET_HEADER_SIZE];

    // Stop and wait loop for sending packet and receiving ack
    while(fileno <= files)
    {
        // Set up select
        fd_set select_fds;          // file descriptors used by select
        struct timeval timeout;     // Time value for timeout
        FD_ZERO(&select_fds);       // Clear file descriptors 
        FD_SET(sockfd, &select_fds);// Set relevant file descriptor
        timeout.tv_sec = 1;         // Timeout is 5 seconds
        timeout.tv_usec = 0;

        int terminated = 0;
        if(ack_recv == 1)
        {
            char* filename;
            char* pic;
            char* packet;
            if(fileno == files)
            {
                // Create termination packet
                packet = create_packet(frameID, 0x4, "0", 0, "0");
                filename = "Termination";
                printf("Created termination packet\n");
                uint32_t packet_len;
                memcpy(&packet_len, &packet[0], sizeof(uint32_t));
                packet_len = ntohl(packet_len);
                
                printf("Created packet no %d from %s of length %d, frameID: %d\n",
                fileno, filename, packet_len, frameID);
                set_loss_probability(drop);
                send_packet(sockfd, packet, packet_len, 0,
                           (const struct sockaddr *) &server_address,
                            sizeof(server_address));
                terminated = 1;
            } else
            {
                // Create file packet
                filename = readFileList(imglist, fileno);
                pic = readSingleFile(filename);
                packet = create_packet(frameID, 1, pic, 1, basename(filename));
                free(pic); // Free image bytes
            
                // Send packet
                uint32_t packet_len;
                memcpy(&packet_len, &packet[0], sizeof(uint32_t));
                packet_len = ntohl(packet_len);
                printf("Created packet no %d from %s of length %d, frameID: %d\n",
                        fileno, filename, packet_len, frameID);
                free(filename); // free filename from strdup
                
                // Append to linked list
                appendNode(fileno, fileno);
                //printList();
                deleteFirst();
                /*
                sendto(sockfd, packet, packet_len, 0,
                    (const struct sockaddr *) &server_address,
                    sizeof(server_address));
                printf("[+]Packet sent to server.\n");
                */
                // Lossy packet sending
                set_loss_probability(drop);
                send_packet(sockfd, packet, packet_len, 0,
                    (const struct sockaddr *) &server_address,
                    sizeof(server_address));
                
                // +++Send 7 packets from fileno to fileno+6
                
                // +++Append packets to linked list (whole packets?)

            }
            fileno++;
            free(packet);
        }
        if(!terminated)
        {
            // if timeout
            if(select(32, &select_fds, NULL, NULL, &timeout) == 0)
            {
                printf("Select has timed out.\n");
                fileno--; // Will resend file
                frameID--; // Dropped packet. Resend frame.

                // +++
            } else
            {
                unsigned int len = sizeof(server_address);
                int f_recv_size = recvfrom(sockfd, (char*)recv_buff, PACKET_HEADER_SIZE, 0,
                (struct sockaddr *) &server_address, &len);

                /*+++ If ack seqno corresponds to seqno of oldest(first) packet in list,
                remove from list and davance window*/

                if(f_recv_size > 0 && recv_buff[4] == 0 && recv_buff[5] == frameID)
                {
                    printf("[+] ACK received for packet %d\n", recv_buff[5]);
                    ack_recv = 1;
                } else
                {
                printf("[-] ACK not received\n");
                ack_recv = 0;
                }
            }
            frameID++;
        }
    }
    //freeList();
    close(sockfd);
    return 0;
}