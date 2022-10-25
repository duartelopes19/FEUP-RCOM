#include "app.h"
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
#define DISC 11
#define BCC A^C_SET
#define BCCUA A^UA
#define BCCRR A^RR
#define BCCREJ A^REJ
#define BCCDISC A^DISC
#define FALSE 0
#define TRUE 1  
#define BUF_SIZE 5

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
        case TRANSMITTER:{
            printf("A");
            FILE *f;
            char buf[MAX_SIZE]; // 256 - 5 ; 5 = FLAG + BCC + FLAG + BCC2 + A
            f = fopen(filename,"r");
            while(!feof(f)){
                int i;
                printf("B");
                for(i = 0 ; i < MAX_SIZE; i++){
                    buf[i]=fgetc(f);
                    printf("C");
                    if(feof(f)){break;}
                }
                llwrite(app.fileDescriptor, buf, i);
            } 
            printf("AQUI`\n");   
            break;
        }
        case RECEIVER:{
            FILE *f;
            char buf[MAX_SIZE]; // 256 - 5 ; 5 = FLAG + BCC + FLAG + BCC2 + A
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
                printf("%d - buf\n",buf[0]);
                fprintf(f,"%s",buf);
            }
            break;
        }   
        default:
            perror("Unable to receive role");
            exit(-1);
    }
    
    llclose(app.fileDescriptor,role);
}

int llopen(char* serialPortName, LinkLayerRole role){

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
            int flag = 0;
            while (alarmCount < 4 && !STOP)
            {   
                if (alarmEnabled == FALSE){
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
                        if(state == 4){
                            printf("Connection established\n");
                            STOP = TRUE;
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
            if(!STOP){return -1;}
            break;

        case RECEIVER:
            STOP = FALSE;
            // Loop for input
            // I removed the declaration of buf here
            state = 0;
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
            unsigned char conf[BUF_SIZE];
            conf[0] = FLAG;
            conf[1] = A;
            conf[2] = UA;
            conf[3] = BCCUA;
            conf[4] = FLAG;
            bytes = write(fd,conf,BUF_SIZE);
            printf("%d - UA\n",bytes );
            break;
    };
    return fd;
}

int llwrite(int fd, char * buffer, int length){

    int bufsize = length*2+6; //most of the time 512
    unsigned char info[bufsize];
    int end = bufsize-1;
    info[0] = FLAG;
    info[1] = A;
    info[2] = 0;
    info[3] = A^0;
   printf("D");
    int record = 0;
    for(int i = 4; i < length+4; i++){
        info[i] = buffer[i-4];
        record = i; // in most cases record +1 = 253; || except when it is the last "trama" and it does not fill the whole buffer
    }
    printf("E");
    int countA = 0;
    info[record +1] = 0; // in most cases, record +1 = 254; || except when it is the last "trama" and it does not fill the whole buffer
    for (int i = 4;i<=record;i++){
      info[record+1] = info[record+1]^info[i]; 
      countA++;
    }
    printf("F");
    printf("%d - countA",countA);
    
    //stuffing
    info[record + 2] = FLAG; // in most cases, the flag will be at info[255] || except when it is the last "trama" and it does not fill the whole buffer
    int flagloc = record +2;
    for(int i = 0; i<=flagloc;i++){
     //printf("%d - info", info[i]);
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
    printf("G");
    for(int i = 0; i<=flagloc;i++){
     printf("%d - info", info[i]);
    }
    printf("H");
    //checking the confirmation
    unsigned char conf[BUF_SIZE]={0};
    int receiv,bytes;
    int alarmEnabled = FALSE;
    int alarmCount = 0;
    int posconf = TRUE;
    int state = 0;
    int STOP = FALSE;
    //flag = 0;
    while (alarmCount < 4 && !STOP)
    {   
        if (alarmEnabled == FALSE)
        { 
             printf("I");
            bytes = write(fd, info, bufsize);
            printf("%d bytes written\n", bytes);
            alarm(3);
            alarmEnabled = TRUE;  
            printf("J"); 
        }
        
        receiv = read(fd,conf,1);
        printf("%d<-RECEIV", receiv);  
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
                    bytes = write(fd, info, bufsize);
                    printf("REJ - %d bytes written\n", bytes);
                    alarm(3);
                    alarmEnabled = TRUE; 
                    state =0;
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
    int STOP = FALSE;
    unsigned char buf[MAX_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
    printf("K");
    while(!STOP){
        int bytes = read(fd, buf, 1);
        printf("L");
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
            ans[1] = A;
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
            printf("M");
            if(state==4){
                while(!STOP){
            	    if(buf[0] == FLAG){
            	        STOP = TRUE;
            	        printf("Aa\n");
            	        break;
            	    }
            	    if(state == 4){
                        info[d] = buf[0];
            	        d++;
            	        //printf("%d\n",d);
                    }
                int bytes = read(fd, buf, 1);
                printf("N");
                }
                printf("O");
            }	
            break;
        };
        count ++;
    }
    printf("P");
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
    printf("Q");
    
    for(int i =0;i<d;i++)
      printf("%d ",info[i]);
    i=0; 
     int countB =0;
    if(d>0){
    	BCC2_received = info[d-1];
    	for (int j = 0 ;j<d-1;j++){
            BCC2 = BCC2^info[j];
            countB++;
        }
    }
    printf("R");
    printf("%d - countB",countB);
    i=0;
    
    unsigned char ans[BUF_SIZE];
    ans[0] = FLAG;
    ans[1] = A;
    ans[4] = FLAG;
    printf("%d - bcc2",BCC2);
    if (BCC2 == BCC2_received){
    
      for (int j = 0 ;j<d-1;j++){
        buffer[j]=info[j];
      }  
      buffer[d-1]='\0';
    	ans[2] = RR;
    	ans[3] = BCCRR;
    	printf("RR sent\n");
    }else{
    	ans[2] = REJ;
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
            ans[1] = A;
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
            int STOP = FALSE;
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
                            STOP = TRUE;
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
            conf[1] = A;
            conf[2] = UA;
            conf[3] = BCCUA;
            conf[4] = FLAG;
            bytes = write(fd,conf,BUF_SIZE);
            printf("FINAL UA SENT\n");
	    break;
	    }
	    
	    case RECEIVER:{
	      int STOP = FALSE;
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
		            STOP = TRUE;
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
