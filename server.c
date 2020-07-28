#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <libgen.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>

#include "server.h"
#include "pgmread.h"
#include "files.h"

void lookForMatch(char* clientfname, char* clientimage, char* serverpath, char* matches){
    // Create struct from received bytes
    struct Image* received = Image_create(clientimage);

    // Read from directory
    struct dirent* readd; // Pointer for dir entry
    struct stat file_info;
    DIR* dir;
    dir = opendir(serverpath);
    if(!dir)
    {
        fprintf(stderr, "Could not open directory %s\n", serverpath);
        exit(1);
    }
    char* local_image = malloc(1024);
    char* filename = malloc(128);
    char* fullpath = malloc(128);
    FILE* f;
            f = fopen(matches, "ab");
            if(!f)
            {
                fprintf(stderr, "Could not open file %s", matches);
            }
    int found = 0;
    while((readd = readdir(dir)))
    {
        strncpy(filename, readd->d_name, strlen(readd->d_name)+1);
        strncpy(fullpath, serverpath, strlen(serverpath)+1);
        strncat(fullpath, filename, strlen(filename)+1);
        lstat(fullpath, &file_info);
        if(S_ISREG(file_info.st_mode))
        {
            local_image = readSingleFile(fullpath);
            struct Image* compareto = Image_create(local_image);
            free(local_image);
            if(Image_compare(received, compareto))
            {
                printf("\nFound identical images %s and %s\n\n",filename, clientfname);
                fprintf(f, "%s %s\n", clientfname, filename);
                found = 1;
            }
            Image_free(compareto);
        }
    }
    if(!found)
    {
        fprintf(f, "%s %s\n", clientfname, "UNKNOWN");
    }
    fclose(f);
    closedir(dir); 
    Image_free(received);
    free(filename);
    free(fullpath);
}

char* createAck(char ackno){
    struct packet_header * ackStruct;
    ackStruct = (struct packet_header*)malloc(sizeof(struct packet_header));
    ackStruct->size = (uint32_t) htonl(sizeof(int) + 4*sizeof(char));
    ackStruct->seqno = (uint8_t) 0;
    ackStruct->ackno = (uint8_t) ackno;
    ackStruct->flag = (uint8_t) 0x2;
    ackStruct->unused = (uint8_t) 0x7f;
    
    char* ackPacket = malloc(sizeof(int) + 4*sizeof(char));
    memcpy(&ackPacket[0], &(ackStruct->size), sizeof(uint32_t));
    memcpy(&ackPacket[4], &(ackStruct->seqno), sizeof(uint8_t));
    memcpy(&ackPacket[5], &(ackStruct->ackno), sizeof(uint8_t));
    memcpy(&ackPacket[6], &(ackStruct->flag), sizeof(uint8_t));
    memcpy(&ackPacket[7], &(ackStruct->unused), sizeof(uint8_t));

    free(ackStruct);
    return ackPacket;
}

int main(int argc, char *argv[]){
    // Check argc
    if(argc != 4)
    {
        fprintf(stderr, "Usage: %s [portnumber] [images directory]"
        "[matches filename]\n", argv[0]);
        exit(1);
    }
    // Store argv
    int portno = atoi(argv[1]);
    char* dir = argv[2];
    char* matches = argv[3];

    // Create server socket
    int frame_id = 0;
    int sockfd;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    // Sette serveradresse
    struct sockaddr_in server_address, client_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(portno);
    server_address.sin_addr.s_addr = INADDR_ANY;

    // Bind socket with server address
    bind(sockfd, (const struct sockaddr*) &server_address,
    sizeof(server_address));
    unsigned int len = sizeof(client_address);

    char buff[1522];
    int terminated = 0;
    while(!terminated)
    {
        int f_recv_size = recvfrom(sockfd, &buff, sizeof(buff), 0,
        (struct sockaddr *) &client_address, &len);
        printf("incoming packet with flag %d and seqno %d. frameID: %d\n", buff[6], buff[4], frame_id);

        // Send ACK
        if(f_recv_size > 0 && buff[6] == 1 && buff[4] == frame_id)
        {
            printf("[+] Packet received\n");
            printf("Received: %s\n", buff);
            char* ack = createAck(buff[4]);
            sendto(sockfd, (const char*)ack, PACKET_HEADER_SIZE,
            0, (const struct sockaddr *) &client_address, sizeof(client_address));
            printf("[+] ACK sent: %d\n", buff[4]);
            free(ack);

            // Compare pgm files
            int size = ntohl(buff[0] + (buff[1]<<8) + (buff[2]<<16) + (buff[3]<<24));
            //int size = (buff[0] + (buff[1]<<8) + (buff[2]<<16) + (buff[3]<<24));
            int fnamelength = ntohl(buff[12] + (buff[13]<<8) + (buff[14]<<16) + (buff[15]<<24));
            //int fnamelength = (buff[12] + (buff[13]<<8) + (buff[14]<<16) + (buff[15]<<24));
            char *fname = (char*)malloc(fnamelength);
            memcpy(fname, &(buff[16]), fnamelength);
            int imgsize = size - fnamelength - 16;
            char * img = (char*)malloc(imgsize);
            memcpy(img, &(buff[16+fnamelength]),imgsize);
            // Check for matching image on server directory
            lookForMatch(fname, img, dir, matches);
            free(fname);
            free(img);

        }else if(buff[6] & 0x4)
        {
            terminated = 1;
            printf("Terminating server.\n");
        }else
        {
            printf("[-] ACK not sent (packet not received)\n");
        }
        frame_id++;
    }
    close(sockfd);

    // Receive message
    /*
    unsigned int len, n;
    len = sizeof(client_address);
    char buffer[1522]; // max ethernet frame size
    n = recvfrom(sockfd, (char *) buffer, sizeof(buffer), 0,
    (struct sockaddr *) &client_address, &len);
    buffer[n] = '\0';
    printf("received message");
    
    // Read buffer
    int size = ntohl(buffer[0] + (buffer[1]<<8) + (buffer[2]<<16) + (buffer[3]<<24));
    char seqno = buffer[4];
    char ackno = buffer[5];
    char flag = buffer[6];
    char unused = buffer[7];
    int requestno = ntohl(buffer[8] + (buffer[9]<<8) + (buffer[10]<<16) + (buffer[11]<<24));
    int fnamelength = ntohl(buffer[12] + (buffer[13]<<8) + (buffer[14]<<16) + (buffer[15]<<24));
    char *fname = (char*)malloc(fnamelength);
    memcpy(fname, &(buffer[16]), fnamelength);
    int imgsize = size - fnamelength - 16;
    char * img = (char*)malloc(imgsize);
    memcpy(img, &(buffer[16+fnamelength]),imgsize);
    printf("Reiceved packet with seqno %d\n", seqno);

    // Send ack
    
    char* ackpack = createAck(seqno);
    sendto(sockfd, (const char*)ackpack, PACKET_HEADER_SIZE,
            0, (const struct sockaddr *) &client_address,
            sizeof(client_address));
    printf("Ack sent to client.\n");
    free(ackpack);

    // Check for matching image on server directory
    //lookForMatch(fname, img, dir, matches);
    free(fname);
    free(img);
    */
    return 0;
}