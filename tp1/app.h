#define MAX_SIZE 253
#define MAX_PAYLOAD_SIZE 1000

// MISC
#define FALSE 0
#define TRUE 1



typedef enum
{
    TRANSMITTER,
    RECEIVER
} LinkLayerRole;

struct applicationLayer{
    int fileDescriptor;
    int status;
};

typedef struct{
    char port[20];                  /*Dispositivo /dev/ttySx, x = 0, 1*/
    int baudRate;                   /*Velocidade de transmissão*/
    unsigned int sequenceNumber;    /*Número de sequência da trama: 0, 1*/
    unsigned int timeout;           /*Valor do temporizador: 1 s*/
    unsigned int numTransmissions;  /*Número de tentativas em caso de falha*/
    char frame[MAX_SIZE];           /*Trama*/

}LinkLayer;


void applicationLayer(char* serialPort, LinkLayerRole role, int BAUDRATE, int N_TRIES, int TIMEOUT, char* filename);
int llopen(char* port, LinkLayerRole role);
int llwrite(int fd, char * buffer, int length);
int llread(int fd, char * buffer);
int llclose(int fd,LinkLayerRole role);
