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

#define FLAG 126
#define A 3
#define C_SET 3
#define UA 7
#define BCC A ^ C_SET
#define BCCUA A ^ UA
#define C_RR 5
#define C_REJ 1
#define BCC_RR A ^ C_RR
#define BCC_REJ A ^ C_REJ
 

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 5

volatile int STOP = FALSE;

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

    // Loop for input
    unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
    int state = 0;
    while(!STOP)
    {
        // Returns after 5 chars have been input
        int bytes = read(fd, buf, 1);
        //buf[bytes] = '\0'; // Set end of string to '\0', so we can printf
        //n--;
        switch (buf[0])
        {
        case FLAG:
            if (state == 4)
                STOP = TRUE;
            else
                state = 1;
            break;
        case A:
            if(state == 1)
                state = 2;
            else
                if(state == 2) //C_SET  
                    state = 3; 
                else
                    state = 0;
            break;
        case BCC:
            if(state == 3)
                state = 4;
            else 
                state = 0;
            break;
        };
    }
    unsigned char conf[BUF_SIZE] = {0};
    conf[0] = FLAG;
    conf[1] = A;
    conf[2] = UA;
    conf[3] = BCCUA;
    conf[4] = FLAG;
    int bytes = write(fd,conf,BUF_SIZE);
    printf("message sent\n");

    state=0;
    int BCC2 = 0;
    int BCC2_received = 0;
    unsigned char info[256];
    int d = 0;
    int i = 0;
    STOP = FALSE;
    
    while(!STOP){
        int bytes = read(fd, buf, 1);
        printf("%dth state with value-%d\n",state,buf[0]);
        switch (buf[0])
        {
        case FLAG:
            state = 1;
            break;
        case A:
            if(state == 1)
                state = 2;
            else
                if(state==3)    //BCC1 tem de ser esclarecido
                    state = 4;
                else
                    state = 0;
            break;
        case 0:
            if(state == 2) //C_SET  
                state = 3; 
            else
                state = 0;
            break;
        default:
            if(state==4){
                while(!STOP){
                    printf("%dth state with value - %d\n",state,buf[0]);
            	    if(buf[0] == FLAG){
            	        STOP = TRUE;
            	        printf("Aa\n");
            	    }else{
                        info[d] = buf[0];
            	        d++;
            	        printf("%d\n",d);
                        int bytes = read(fd, buf, 1);
                    }                    
                }
            }
            else{
                state=0;
            }
            break;
        };
    }
    for (int i=0;i<=d;i++){
        if(info[i]==0x7d){
            if(info[i+1]==0x5e){
                info[i] = 0x7e;
            }
            else if(info[i+1]==0x5d){
                info[i] = 0x7d;
            }else{
                continue;
            }
            for (int j=i+2;j<=d;j++){
                info[j-1]=info[j];
            }
            d--;
        }
    }
    if(d>0){
    	BCC2_received = info[d-1];
    	info[d-1] = '\0';
    	while(info[i]!='\0'){
    	    printf("%d unstuffed\n",info[i]);
            BCC2^= info[i];
            i++;
        }
    }
    unsigned char ans[BUF_SIZE]={0};
    ans[0] = FLAG;
    ans[1] = A;
    ans[4] = FLAG;
    if (BCC2 == BCC2_received){
    	ans[2] = C_RR;
    	ans[3] = BCC_RR;
    }else{
    	ans[2] = C_REJ;
    	ans[3] = BCC_REJ;
    }
    printf("%d %d %d %d %d", ans[0], ans[1], ans[2], ans[3], ans[4]);
    bytes = write(fd,ans,BUF_SIZE);
    printf("message sent\n");
	
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
