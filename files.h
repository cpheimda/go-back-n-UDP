#define PACKET_HEADER_SIZE (sizeof(uint32_t) + 4*sizeof(uint8_t))

struct packet_header{
    uint32_t size;
    uint8_t seqno;
    uint8_t ackno;
    uint8_t flag;
    uint8_t unused;
};

struct bytes{
    uint32_t requestno;
    uint32_t fnamelength;
};

char* readSingleFile(char* filename);