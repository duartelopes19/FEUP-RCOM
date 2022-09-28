// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

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

//volatile int STOP = FALSE;

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 1;  // Blocking read until 5 chars received

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
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    //write SET
    unsigned char set[5] = {FLAG,A_SENDER,C_SET,A_SENDER^C_SET,FLAG};
    write(fd,set,5);

    //read UA
    unsigned int state = START;
    unsigned char bcc = 0;
    unsigned char buf[1] = {0};

    while(state != STOP) {
        read(fd,buf,1);
        switch (buf[0])
        {
        case FLAG:
            if(state==BCC_OK) {
                state = STOP;
                break;
            }
            state = FLAG_RCV;
            break;
        case A_RECEIVER:
            if(state==FLAG_RCV) {
                state = A_RCV;
                break;
            }
            state = START;
            break;
        case C_UA:
            if(state==A_RCV) {
                state = C_RCV;
                break;
            }
            state = START;
            break;
        case A_RECEIVER^C_UA:
            if(state==C_RCV) {
                state = BCC_OK;
                break;
            }
            state = START;
            break;
        default:
            state = START;
            break;
        }
    }


    // Create string to send
    // unsigned char buf[BUF_SIZE] = {0};

    /* for (int i = 0; i < BUF_SIZE; i++)
    {
        buf[i] = 'a' + i % 26;
    } */

    // gets(buf);

    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.
    // buf[5] = '\n';

    // int bytes = write(fd, buf, strlen(buf)+1);
    // printf("% */d bytes written\n", bytes);

    // Wait until all bytes have been written to the serial port
    // sleep(1);

    // unsigned char received[BUF_SIZE + 1] = {0};

    // bytes = read(fd, received, BUF_SIZE);
    // received[bytes] = '\0'; // Set end of string to '\0', so we can printf

    /* if(strcmp(buf,received)) {
        printf("%s\n","Error");
    } else {
        printf("%s\n","Check");
    } */

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
