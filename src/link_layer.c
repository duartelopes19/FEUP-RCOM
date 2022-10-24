// Link layer protocol implementation

#include "link_layer.h"
#include "application_layer.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>


// MISC
// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256
#define FLAG 0x7E
#define A_RECEIVER 0x03
#define A_SENDER 0x01
#define C_SET 0x03
#define C_UA 0x07
#define START 0
#define STOP 1
#define FLAG_RCV 2
#define A_RCV 3
#define C_RCV 4
#define BCC_OK 5
#define BCCUA A_RECEIVER^C_UA
#define BCCRR A_RECEIVER^BCC_OK
#define BCCREJ A_RECEIVER^STOP

struct termios oldtio;
struct termios newtio;

void applicationLayer(char* serialPort, LinkLayerRole role, int baudrate, int n_tries, int timeout, char* filename){
    struct applicationLayer app;
    app.fileDescriptor = llopen(serialPort,role); // opens the serial Port and starts comunication || Transmitter send SET receiver responds with UA   
    if(app.fileDescriptor<0){
        perror("Could not open communication");
        exit(-1);
    }
    switch(role){
        FILE *f;
        char buf[251]; // 256 - 5 ; 5 = FLAG + BCC + FLAG + BCC2 + A
        case TRANSMITTER:
            f = fopen(filename,"r");
            while(!feof(f)){
                for(int i =0; i < 251; i++)
                    buf[i]=fgetc(f);
                llwrite(app.fileDescriptor, buf, 251);
            }
            break;
        case RECEIVER:
            f = fopen(filename,"w");
            int c = 1;
            while(c != 0){ //this only is equal to 0 when it receives a DISC message
                c = llread(app.fileDescriptor, buf);
                if(c<0){
                    fprintf(stderr,"%s:error reading from serial Port %s\n",strerror(-2),serialPort);
                    perror("error reading from serial Port");
                    exit(-1);
                }
                printf("%d written into file", c);
            }
            break;
        default:
            perror("Unable to receive role");
            exit(-1);
    }
    llclose(app.fileDescriptor);
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(char* serialPortName, LinkLayerRole role)
{
    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        printf("error here");
        perror(serialPortName);
        return fd;
    }

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        printf("error here tcgetattr");
        perror("tcgetattr");
        return -3;
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        printf("error here tcsetattr2");
        perror("tcsetattr");
        return -3;
    }

    printf("New termios structure set\n");

    switch(role){
        unsigned char buf[BUF_SIZE +1];
        case TRANSMITTER:
            buf[0] = FLAG;
            buf[1] = A_SENDER;
            buf[2] = C_SET;
            buf[3] = A_SENDER^C_SET;
            buf[4] = FLAG;

        void alarmHandler(int signal){
            alarmEnabled = FALSE;
            alarmCount++;
        }

        break;
        case RECEIVER:
        break;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    return 1;
}
