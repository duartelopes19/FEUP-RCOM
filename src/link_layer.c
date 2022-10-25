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
#define DISC 11
#define FLAG_RCV 2
#define A_RCV 3
#define C_RCV 4
#define BCC_OK 5
#define BCCUA A_RECEIVER^C_UA
#define BCCRR A_RECEIVER^BCC_OK
#define BCCREJ A_RECEIVER^STOP
#define BCCDISC A_RECEIVER^DISC

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
    llclose(app.fileDescriptor, role);
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

            int alarmEnabled = FALSE;
            int alarmCount = 0;

            void alarmHandler(int signal){
                alarmEnabled = FALSE;
                alarmCount++;
            }

            (void)signal(SIGALRM, alarmHandler);
            unsigned int state = START;
            unsigned char buf[1] = {0};

            while(state != STOP && alarmCount < 4) {
            if (alarmEnabled == FALSE)
            {
                write(fd,buf,5);
                alarm(3); // Set alarm to be triggered in 3s
                alarmEnabled = TRUE;
            }
            while (alarmEnabled == TRUE) {
                if(read(fd,buf,1)==0) continue;
                switch (state)
                {
                case START:
                    if (buf[0]==FLAG) state = FLAG_RCV;
                    break;
                case FLAG_RCV:
                    if (buf[0]==FLAG) break;
                    else if (buf[0]==A_RECEIVER) state = A_RCV;
                    else state = START;
                    break;
                case A_RCV:
                    if (buf[0]==FLAG) state = FLAG_RCV;
                    else if (buf[0]==C_UA) state = C_RCV;
                    else state = START;
                    break;
                case C_RCV:
                    if (buf[0]==FLAG) state = FLAG_RCV;
                    else if (buf[0]==A_RECEIVER^C_UA) state = BCC_OK;
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
        }
            alarm(0);
            alarmCount = 0;
            alarmEnabled = FALSE;
            break;
        case RECEIVER:
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
                break;
        }
    return fd;
}

int llwrite(int fd, char * buffer, int length)
{
    int bufsize = length*2+6; //most of the time 512
    unsigned char info[bufsize];
    int end = bufsize-1;
    info[0] = FLAG;
    info[1] = A_RECEIVER;
    info[2] = 0;
    info[3] = A_RECEIVER^0;
  
    int record = 0;
    for(int i = 4; i < length+4; i++){
        info[i] = buffer[i-4];
        record = i; // in most cases record +1 = 253; || except when it is the last "trama" and it does not fill the whole buffer
    }

    int countA = 0;
    info[record +1] = 0; // in most cases, record +1 = 254; || except when it is the last "trama" and it does not fill the whole buffer
    for (int i = 4;i<=record;i++){
      info[record+1] = info[record+1]^info[i]; 
      countA++;
    }

    //stuffing
    info[record + 2] = FLAG; // in most cases, the flag will be at info[255] || except when it is the last "trama" and it does not fill the whole buffer
    int flagloc = record +2;
    for(int i = 0; i<=flagloc;i++){
    
    }
    for (int i = 4;i<flagloc;i++){
        if(info[i]==0x7e){
            info[i]=0x7d;
            for(int x = flagloc;x>i;x--){
                info[x+1] = info[x];                           
            }
            info[i+1]=0x5e;
            flagloc++;
        }
        else if(info[i] == 0x7d){
            info[i]=0x7d;
            for(int x = flagloc;x>i;x--){
                info[x+1] = info[x];                           
            }
            info[i+1]=0x5d;
            flagloc++;
        }
    }
    unsigned char conf[BUF_SIZE]={0};
    int receiv;
    int alarmEnabled = FALSE;
    int alarmCount = 0;
    unsigned int state = START;
    unsigned char buf[1] = {0};
    int posconf = TRUE;
    int STOP1 = FALSE;

    while (alarmCount < 4 && !STOP)
    {   
        if (alarmEnabled == FALSE)
        { 
             
            write(fd, info, bufsize);
            alarm(3);
            alarmEnabled = TRUE;  
           
        }
        
        receiv = read(fd,conf,1);
        
        if(receiv == 0){
         continue;
        }
        
        switch(conf[0]){
        case FLAG:
            if(state == 6)
               if(posconf)
                   STOP1 = TRUE;
               else{
                    write(fd, info, bufsize);
                    alarm(3);
                    alarmEnabled = TRUE; 
                    state =0;
                }  
            else
                state = 1;
            break;
        case A_RECEIVER:
            if(state == 1)
                state = 2;
            else
                state = 0;
            break;
        case BCC_OK:
            if(state == 2)
                state = 3;
            else
                state = 0;
            break;
        case STOP:
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

    return 0;
}

int llread(int fd, char * buffer){
    
    int count = 0;
    int state=0;
    int BCC2 = 0;
    int BCC2_received = 0;
    unsigned char info[256];
    int d = 0;
    int i = 0;
    int STOP1 = FALSE;
    unsigned char buf[MAX_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
   
    while(!STOP){
        int bytes = read(fd, buf, 1);
        
        switch (buf[0])
        {
        case FLAG:
            if (state == 4)
                STOP1 = TRUE;
            else
                state = 1;
            break;
        case A_RECEIVER:
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
        case DISC:{
           if(state == 2){
            /* At this stage it should recognise a DISCONNECT message and should reply with the same massage*/
            unsigned char ans[BUF_SIZE];
            ans[0] = FLAG;
            ans[1] = A_RECEIVER;
            ans[2] = DISC;
            ans[3] = BCCDISC;
            ans[4] = FLAG;
            bytes = write(fd,ans,BUF_SIZE);
            buffer[0] = '\0';
            return 0; // telling application layer that it no longer needs to read from this serial port and it should now proceed to starting the llclose function
            }
            else{
             state = 0;
            }
            break;
         }
        default:
            
            if(state==4){
                while(!STOP){
            	    if(buf[0] == FLAG){
            	        STOP1 = TRUE;
            	       
            	        break;
            	    }
            	    if(state == 4){
                        info[d] = buf[0];
            	        d++;
            	        
                    }
                int bytes = read(fd, buf, 1);
                
                }
               
            }	
            break;
        };
        count ++;
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
   
    
  
    i=0; 
     int countB =0;
    if(d>0){
    	BCC2_received = info[d-1];
    	for (int j = 0 ;j<d-1;j++){
            BCC2 = BCC2^info[j];
            countB++;
        }
    }
   
    i=0;
    
    unsigned char ans[BUF_SIZE];
    ans[0] = FLAG;
    ans[1] = A_RECEIVER;
    ans[4] = FLAG;
    
    if (BCC2 == BCC2_received){
    
      for (int j = 0 ;j<d-1;j++){
        buffer[j]=info[j];
      }  
      buffer[d-1]='\0';
    	ans[2] = BCC_OK;
    	ans[3] = BCCRR;
    	printf("RR sent\n");
    }else{
    	ans[2] = STOP;
    	ans[3] = BCCREJ;
    	printf("REJ sent\n");
    }
    int bytes = write(fd,ans,BUF_SIZE);
    return d;
}

int llclose(int fd,LinkLayerRole role){
    switch(role){
	    case TRANSMITTER:{
	    unsigned char ans[BUF_SIZE];
            ans[0] = FLAG;
            ans[1] = A_RECEIVER;
            ans[2] = DISC;
            ans[3] = BCCDISC;
            ans[4] = FLAG;
            
            unsigned char received[BUF_SIZE];
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
            int STOP1 = FALSE;
            int flag = 0;
            while (alarmCount < 4 && !STOP)
            {   
                if (alarmEnabled == FALSE){
                    bytes = write(fd,ans,BUF_SIZE);
                    printf("DISC SENT\n");
                    alarm(3);
                    alarmEnabled = TRUE;   

                }
                receiv = read(fd,received,1);
                if(receiv == 0){
                    continue;
                }
                switch(received[0]){
                    case FLAG:
                        if(state == 4){
                            printf("DISC RECEIVED\n");
                            STOP1 = TRUE;
                        }
                        else
                            state = 1;
                        break;
                    case A_RECEIVER:
                        if(state == 1)
                            state = 2;
                        else
                            state = 0;
                        break;
                    case DISC:
                        if(state == 2)
                            state = 3;
                        else
                            state = 0;
                        break;
                    case BCCDISC:
                        if(state ==3)
                            state =4;
                        else 
                            state = 0;
                        break;
                };
                
            }
            if(!STOP){return -1;}
            unsigned char conf[BUF_SIZE];
            conf[0] = FLAG;
            conf[1] = A_RECEIVER;
            conf[2] = C_UA;
            conf[3] = BCCUA;
            conf[4] = FLAG;
            bytes = write(fd,conf,BUF_SIZE);
            printf("FINAL UA SENT\n");
	    break;
	    }
	    
	    case RECEIVER:{
	      int STOP1 = FALSE;
             int state = 0;
             while(!STOP)
             {   
                unsigned char buf[BUF_SIZE];
                int bytes = read(fd, buf, 1);
                switch (buf[0])
                {
                case FLAG:
		        if(state == 4){
		            printf("Final UA RECEIVED\n");
		            STOP1 = TRUE;
		        }
		        else
		            state = 1;
		        break;
		case A_RECEIVER:
		        if(state == 1)
		            state = 2;
		        else
		            state = 0;
		        break;
		case C_UA:
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
	     break;
	    }
    };
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
    return 0;
}
