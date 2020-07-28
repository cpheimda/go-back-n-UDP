// Header stuff tho
char* readFileList(char* filename, int select);
char * create_packet(int seqno, char flag, char *bytes, int request_no, char *fname);
//void sendPacket(struct sockaddr_in server_address, struct in_addr ip_address, int sockfd,
//char* servIP, int portno, char* packet);
int noOfFiles(char* filename);
