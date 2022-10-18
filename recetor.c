// Read from serial port in non-canonical mode
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

#define ESC 0x7d

#define START 0
#define STOP 1
#define FLAG_RCV 2
#define A_RCV 3
#define C_RCV 4
#define BCC_OK 5
#define BCC_READ 6

// volatile int STOP = FALSE;

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

    // Open serial port device for reading and writing and not as controlling tty
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

    //read SET
    unsigned int state = START;
    unsigned char buf[1] = {0};

    while(state != STOP) {
        if(read(fd,buf,1)==0) continue;
        switch (state)
        {
        case START:
            if (buf[0]==FLAG) state = FLAG_RCV;
            break;
        case FLAG_RCV:
            if (buf[0]==FLAG) break;
            else if (buf[0]==A_SENDER) state = A_RCV;
            else state = START;
            break;
        case A_RCV:
            if (buf[0]==FLAG) state = FLAG_RCV;
            else if (buf[0]==C_SET) state = C_RCV;
            else state = START;
            break;
        case C_RCV:
            if (buf[0]==FLAG) state = FLAG_RCV;
            else if (buf[0]==A_SENDER^C_SET) state = BCC_OK;
            else state = START;
            break;
        case BCC_OK:
            if (buf[0]==FLAG) state = STOP;
            else state = START;
            break;
        default:
            break;
        }
    }

    // write UA
    unsigned char ua[5] = {FLAG,A_RECEIVER,C_UA,A_RECEIVER^C_UA,FLAG};
    write(fd,ua,5);

    // read I(0)
    state = START;
    unsigned char buf1[BUF_SIZE] = {0};
    
    int i = 0, j = 0;

    while(state != STOP) {
        if(read(fd,buf,1)==0) continue;
        switch (state)
        {
        case START:
            if (buf[0]==FLAG) state = FLAG_RCV;
            break;
        case FLAG_RCV:
            if (buf[0]==FLAG) break;
            else if (buf[0]==A_SENDER) state = A_RCV;
            else state = START;
            break;
        case A_RCV:
            if (buf[0]==FLAG) state = FLAG_RCV;
            else if (buf[0]==0x00) state = C_RCV;
            else state = START;
            break;
        case C_RCV:
            if (buf[0]==FLAG) state = FLAG_RCV;
            else if (buf[0]==A_SENDER^0x00) {
                state = BCC_READ;
                i = 0;
            }
            else state = START;
            break;
        case BCC_READ:
            buf1[j]=buf[0];
            j++;
            if (buf[0]==FLAG) state = STOP;
            break;
        default:
            break;
        }
    }
    unsigned char buf2[j] = {0};
    for (int k = 0, x = 0 ; k < j-1 ; k++,x++){
        if (buf1[k] == ESC){
            if(buf1[k+1] == 0x5e){
                buf2[x] = FLAG;
                k++;
            }
            else if(buf1[k+1] == 0x5d){
                buf2[k] = ESC;
                k++;
            }
        }
        else {
            buf2[x] = buf1[k];
        }
        
    }
    if (k == (j-1)){
        buf2[x] = buf1[k];
    }

    for(int y = 0; i <= x; y++){
        bcc2 =bcc2^buf2[y]
    }


    


    // Loop for input
    // unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char

    /* while (STOP == FALSE)
    {
        // Returns after 5 chars have been input
        int bytes = read(fd, buf, BUF_SIZE);
        buf[bytes] = '\0'; // Set end of string to '\0', so we can printf

        printf("%s:%d\n", buf, bytes);
        if (buf[0] == 'z')
            STOP = TRUE;
    } */


   /*  while (STOP == FALSE)
    {
        int bytes = read(fd, buf, BUF_SIZE+1);
        buf[bytes] = '\0';
        printf("%s:%d\n", buf, bytes);
        write(fd, buf, strlen(buf)+1);
    } */

    // The while() cycle should be changed in order to respect the specifications
    // of the protocol indicated in the Lab guide

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}