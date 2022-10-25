// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source
#define FLAG 126
#define A 3
#define C_SET 3
#define UA 7
#define RR 5 //just to test
#define REJ 1
#define BCC A^C_SET
#define BCCUA A^UA
#define BCCRR A^RR
#define BCCREJ A^REJ
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
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Create string to send
    unsigned char buf[BUF_SIZE];
    buf[0] = FLAG;
    buf[1] = A;
    buf[2] = C_SET;
    buf[3] = BCC;
    buf[4] = FLAG;



    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.
    unsigned char ans[BUF_SIZE];
    int receiv,bytes;
    int alarmEnabled = FALSE;
    int alarmCount = 0;
    
  
    void alarmHandler(int signal)
    {
        alarmEnabled = FALSE;
        alarmCount++;
    }

    // Set alarm function handler
    (void)signal(SIGALRM, alarmHandler);
    int state = 0;
    int STOP = FALSE;
    //int flag = 0;
    while (alarmCount < 4 && !STOP)
    {   
        if (alarmEnabled == FALSE)
        {
            bytes = write(fd, buf, BUF_SIZE);
            printf("%d bytes written\n", bytes);
            alarm(3);
            alarmEnabled = TRUE;
            
        }
        receiv = read(fd,ans,1);
        if(receiv == 0){
         continue;
        }
        switch(ans[0]){
        case FLAG:
            if(state == 4)
                STOP = TRUE;
            else
                state = 1;
            break;
        case A:
            if(state == 1)
                state = 2;
            else
                state = 0;
            break;
        case UA:
            if(state == 2)
                state = 3;
            else
                state = 0;
            break;
        case BCCUA:
            if(state ==3)
                state =4;
            else 
                state = 0;
            break;
        };
        
    }



    unsigned char info[30]={0};
    int end = 14;
    info[0] = FLAG;
    info[1] = A;
    info[2] = 0;
    info[3] = A^0;
    info[4] = 5;
    info[5] = 4;
    info[6] = 4;
    info[7] = 5;
    info[8] = 6;
    info[9] = 7;
    info[10] = 8;
    info[11] = 126;
    info[12] = 10;
    info[13] = 0;
    for (int i = 4;i<13;i++){
      info[13] = info[13]^info[i]; 
    }
    info[14] = FLAG;
    for (int i = 4;i<end;i++){
        if(info[i]==0x7e){
            info[i]=0x7d;
            for(int x = end;x>i;x--){
                printf("%d\n",x);
                info[x+1] = info[x];                           
            }
            info[i+1]=0x5e;
            end++;
        }
        else if(info[i] == 0x7d){
            info[i]=0x7d;
            for(int x = end;x>i;x--){
                info[x+1] = info[x];                           
            }
            info[i+1]=0x5d;
            end++;
        }
    }

    
    unsigned char conf[BUF_SIZE]={0};
    alarmEnabled = FALSE;
    alarmCount = 0;
    int posconf = TRUE;
    state = 0;
    STOP = FALSE;
    //flag = 0;
    while (alarmCount < 4 && !STOP)
    {   
        if (alarmEnabled == FALSE)
        {
            bytes = write(fd, info, 30);
            printf("%d bytes written\n", bytes);
            alarm(3);
            alarmEnabled = TRUE;   
        }
        receiv = read(fd,conf,1);
        if(receiv == 0){
         continue;
        }
        printf("%d-in state %d\n",conf[0],state);
        switch(conf[0]){
        case FLAG:
            if(state == 6)
               if(posconf)
                   STOP = TRUE;
               else{
                    bytes = write(fd, info, 30);
                    printf("REJ - %d bytes written\n", bytes);
                    alarm(3);
                    alarmEnabled = TRUE; 
                }  
            else
                state = 1;
            break;
        case A:
            if(state == 1)
                state = 2;
            else
                state = 0;
            break;
        case RR:
            if(state == 2)
                state = 3;
            else
                state = 0;
            break;
        case REJ:
           if(state == 2)
                state = 4;
            else
                state = 0;
            break;
        case BCCRR:
            if(state == 3){
                state = 6;
                posconf = TRUE;
            }else 
                state = 0;
            break;
        case BCCREJ:
            if(state == 4){
                state = 6;
                posconf = FALSE;
            }else 
                state = 0;
            break;
        };
        
    }
    
    
    
    
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}

