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
#define DATA 1
#define FLAG 126
#define A 3
#define C_SET 3
#define C_INFO 0
#define UA 7
#define RR 5 //just to test
#define REJ 1
#define DISC 11
#define BCC A^C_SET
#define BCC_INFO A^C_INFO
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
    int seqN = 0;
    struct applicationLayer app;
    app.fileDescriptor = llopen(serialPort,role); // opens the serial Port and starts comunication || Transmitter send SET receiver responds with UA
    if(app.fileDescriptor<0){
        perror("Could not open communication");
        exit(-1);
    }
    switch(role){
        case TRANSMITTER:{
            int bytes;
            int numSeq = 0;
            FILE *f;
            char info_packet[MAX_SIZE];
            f = fopen(filename,"r");
            
            while(!feof(f)){
                int i;
                info_packet[0] = DATA;
                info_packet[1] = numSeq;  
                numSeq=(numSeq+1)%255;
                for(i = 4 ; i < MAX_SIZE; i++){                  
                    info_packet[i]=fgetc(f);
                    if(feof(f)){break;}   
                }
                unsigned int num_bytes = i-4;
                info_packet[2] = num_bytes / 256;
                info_packet[3] = num_bytes % 256; 
                if(llwrite(app.fileDescriptor, info_packet, num_bytes+4)<0){
                  exit(-1);
                } 
            } 
            
            break;
        }
        case RECEIVER:{
            FILE *f;
            //int aa = (128+1)%255;
            //int bb = (130+1)%255;
            //printf("%d .=. %d" , aa,bb);
            char buf[MAX_SIZE]; // 256 - 5 ; 5 = FLAG + BCC + FLAG + BCC2 + A
            f = fopen(filename,"w");
            long current_size = 0;
            int c = 1;
            while(c != 0){ //this only is equal to 0 when it receives a DISC message

                if((c = llread(app.fileDescriptor, buf))<0){
                    fprintf(stderr,"%s:error reading from serial Port %s\n",strerror(-2),serialPort);
                    perror("error reading from serial Port");
                    exit(-1);
                }
                
                if(c==0) {break;}
                
                if(buf[0]==1){
                    int bufsn = buf[1];
                    if(bufsn<0){
                     bufsn+=256;
                    }
                    if(bufsn==seqN){
                      int L1 = buf[3];
                      int L2 = buf[2];
                      if(L1<0){L1+=256;}
                      unsigned int data_size = L2*256+L1;
                      current_size+=data_size;
	              for(int x=0;x<data_size;x++){
	                fprintf(f,"%c",buf[x+4]);
	              }
	              seqN=(seqN+1)%255;
                    }
                   
                    else{exit(-1);} 
                }
                else if(buf[0]==2){}
                else if(buf[0]==3){}
                else{
                  exit(-1);
                }
                    
            }
            printf("%ld\n",current_size);
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
    unsigned char buf[BUF_SIZE +1];
    switch(role){
        
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
            int state_t = 0;
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
                switch(state_t){
                 case 0:
                  if(ans[0]==FLAG){
                    state_t = 1;
                  }
                  else {state_t = 0;}         
                  break;
                  
                 case 1:
                  if(ans[0]==A){
                    state_t = 2;
                  }
                  else if(ans[0]==FLAG){
                    state_t = 1;
                  } 
                  else {state_t = 0;}
                  break;
                  
                 case 2:
                  if(ans[0]==UA){
                    state_t = 3;
                  }
                  else if(ans[0]==FLAG){
                    state_t = 1;
                  } 
                  else {state_t = 0;} 
                  break;
                  
                 case 3:
                  if(ans[0]==BCCUA){
                    state_t = 4;
                  }
                  else if(ans[0]==FLAG){
                    state_t = 1;
                  } 
                  else {state_t = 0;}
                  break;
                  
                 case 4:
                  if(ans[0]==FLAG){
                    printf("Connection established\n");
                    STOP = TRUE;
                  }
                  else {state_t = 0;}
                  break; 
                };
                
            }
            if(!STOP){return -1;}
            break;

        case RECEIVER:
            STOP = FALSE;
            // Loop for input
            // I removed the declaration of buf here
            int state_r = 0;
            while(!STOP)
            {
                // Returns after 5 chars have been input
                int bytes = read(fd, buf, 1);
                //buf[bytes] = '\0'; // Set end of string to '\0', so we can printf
                //n--;
                switch (state_r)
                {
                case 0:
                  if(buf[0]==FLAG){
                    state_r = 1;
                  }
                  else {state_r = 0;}         
                  break;
                  
                 case 1:
                  if(buf[0]==A){
                    state_r = 2;
                  }
                  else if(buf[0]==FLAG){
                    state_r = 1;
                  } 
                  else {state_r = 0;}
                  break;
                  
                 case 2:
                  if(buf[0]==C_SET){
                    state_r = 3;
                  }
                  else if(buf[0]==FLAG){
                    state_r = 1;
                  } 
                  else {state_r = 0;} 
                  break;
                  
                 case 3:
                  if(buf[0]==BCC){
                    state_r = 4;
                  }
                  else if(buf[0]==FLAG){
                    state_r = 1;
                  } 
                  else {state_r = 0;}
                  break;
                  
                 case 4:
                  if(buf[0]==FLAG){  
                    STOP = TRUE;
                  }
                  else {state_r = 0;}
                  break; 
               };
            }
            unsigned char unum_ack[BUF_SIZE];
            unum_ack[0] = FLAG;
            unum_ack[1] = A;
            unum_ack[2] = UA;
            unum_ack[3] = BCCUA;
            unum_ack[4] = FLAG;
            bytes = write(fd,unum_ack,BUF_SIZE);
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
  
    int record = 0;
    for(int i = 4; i < length + 4; i++){
        info[i] = buffer[i-4];
        record = i; // in most cases record +1 = 253; || except when it is the last "trama" and it does not fill the whole buffer
    }

    
    info[record +1] = 0; // in most cases, record +1 = 254; || except when it is the last "trama" and it does not fill the whole buffer
    for (int i = 4;i<=record;i++){
      info[record+1] = info[record+1]^info[i]; 
    }
   
    
    //stuffing
    info[record + 2] = FLAG; // in most cases, the flag will be at info[255] || except when it is the last "trama" and it does not fill the whole buffer
    int flagloc = record +2;
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
   
   
    //checking the confirmation
    unsigned char conf[BUF_SIZE]={0};
    int receiv,bytes;
    int alarmEnabled = FALSE;
    int alarmCount = 0;
    int rrconfirm = TRUE;
    int state_t = 0;
    int STOP = FALSE;
    int numBytes = 0;
    void alarmHandler(int signal){
	alarmEnabled = FALSE;
	alarmCount++;
    }
    (void)signal(SIGALRM, alarmHandler);
    while (alarmCount < 4 && !STOP)
    {   
        
        if (alarmEnabled == FALSE)
        { 
           
            bytes = write(fd, info, flagloc+1);
            printf("%d bytes written\n", bytes);
            alarm(3);
            alarmEnabled = TRUE;  
           
        }
        
        receiv = read(fd,conf,1);
        if(receiv == 0){
         continue;
        }
        
        
        switch(state_t){
        
        case 0:
         if(conf[0]==FLAG){
          state_t = 1;
         }
         else{state_t = 0;}
         break;
         
        case 1:
         if(conf[0]==A){
          state_t = 2;
         }
         else if(conf[0]==FLAG){
          state_t = 1;
         }
         else{state_t = 0;}
         break;
         
        case 2:
         if(conf[0]==RR){
          state_t = 3;
         }
         else if(conf[0]==REJ){
          state_t = 4;
         }
         else if(conf[0]==FLAG){
          state_t = 1;
         }
         else{state_t = 0;}
         break;
         
        case 3:
         if(conf[0]==BCCRR){
          state_t = 5;
          rrconfirm = TRUE;
         }
         else if(conf[0]==FLAG){
          state_t = 1;
         }
         else{state_t = 0;}
         break;
         
        case 4:
         if(conf[0]==BCCREJ){
          state_t = 5;
          rrconfirm = FALSE;
         }
         else if(conf[0]==FLAG){
          state_t = 1;
         }
         else{state_t = 0;}
         break;
         
        case 5:
         if(conf[0]==FLAG){
          if(rrconfirm)
            STOP = TRUE;
          else{
            bytes = write(fd, info, flagloc+1);
            printf("REJ - %d bytes written\n", bytes);
            alarm(3);
            alarmEnabled = TRUE; 
            state_t =0;
          } 
         }
         else{state_t = 0;}
         break;
         
        };     
    }
    if(!STOP){return -1;}
    return 0;
}

int llread(int fd, char * buffer){
    
    int state_r = 0;
    int BCC2 = 0;
    int BCC2_received = 0;
    unsigned char info[(MAX_SIZE)*2];
    int d = 0;
    int i = 0;
    int STOP = FALSE;
    unsigned char buf[MAX_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
   
    while(!STOP){
        int bytes = read(fd, buf, 1);      
        switch (state_r){
        
        case 0:
         if(buf[0]==FLAG){
          state_r = 1;
         }
         else{state_r = 0;}
         break;
         
        case 1:
         if(buf[0]==A){
          state_r = 2;
         }
         else if(buf[0]==FLAG){
          state_r = 1;
         }
         else{state_r = 0;}
         break;
         
        case 2:
         if(buf[0]==C_INFO){
          state_r = 3;
         }
         else if(buf[0]==DISC){
          state_r = 5;
         }
         else if(buf[0]==FLAG){
          state_r = 1;
         }
         else{state_r = 0;}
         break;
         
        case 3:
         if(buf[0]==BCC_INFO){
          state_r = 4;
         }
         else if(buf[0]==FLAG){
          state_r = 1;
         }
         else{state_r = 0;}
         break;
           
        case 4:
         while(!STOP){
            if(buf[0] == FLAG){
            	STOP = TRUE; 
            	break;
            }
            info[d] = buf[0];
            d++;
            int bytes = read(fd, buf, 1);    
         }
         break; 
         
        case 5:
         if(buf[0]==BCCDISC){
          state_r = 6;
         }
         else if(buf[0]==FLAG){
          state_r = 1;
         }
         else{state_r = 0;}
         break;  
         
        case 6:
         if(buf[0]==FLAG){
            unsigned char ans[BUF_SIZE];
            ans[0] = FLAG;
            ans[1] = A;
            ans[2] = DISC;
            ans[3] = BCCDISC;
            ans[4] = FLAG;
            int bytes = write(fd,ans,BUF_SIZE);
            buffer[0] = '\0';
            return 0; 
         }
         else{state_r = 0;}
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
   
    
  
    i=0; 
     
    if(d>0){
    	BCC2_received = info[d-1];
    	for (int j = 0 ;j<d-1;j++){
            BCC2 = BCC2^info[j];
          
        }
    }
    printf("%d\n",d-1);
    i=0;
    
    unsigned char ans[BUF_SIZE];
    ans[0] = FLAG;
    ans[1] = A;
    ans[4] = FLAG;
    
    if (BCC2 == BCC2_received){
    
      for (int j = 0 ;j<d-1;j++){
        buffer[j]=info[j];
      }  
      //buffer[d-1]='\0';
    	ans[2] = RR;
    	ans[3] = BCCRR;
    	//printf("RR sent\n");
    }else{
    	ans[2] = REJ;
    	ans[3] = BCCREJ;
    	printf("REJ sent\n");
    }
    int bytes = write(fd,ans,BUF_SIZE);
    return d-4;
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
            int state_t = 0;
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
 
                switch(state_t){
                 case 0:
                  if(received[0]==FLAG){
                    state_t = 1;
                  }
                  else {state_t = 0;}         
                  break;
                  
                 case 1:
                  if(received[0]==A){
                    state_t = 2;
                  }
                  else if(received[0]==FLAG){
                    state_t = 1;
                  } 
                  else {state_t = 0;}
                  break;
                  
                 case 2:
                  if(received[0]==DISC){
                    state_t = 3;
                  }
                  else if(received[0]==FLAG){
                    state_t = 1;
                  } 
                  else {state_t = 0;} 
                  break;
                  
                 case 3:
                  if(received[0]==BCCDISC){
                    state_t = 4;
                  }
                  else if(received[0]==FLAG){
                    state_t = 1;
                  } 
                  else {state_t = 0;}
                  break;
                  
                 case 4:
                  if(received[0]==FLAG){
                    printf("DISC RECEIVED\n");
		    STOP = TRUE;        
                  }
                  else {state_t = 0;}
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
             int state_r = 0;
             while(!STOP)
             {   
                unsigned char buf[BUF_SIZE];
                int bytes = read(fd, buf, 1);
                switch(state_r){
                 case 0:
                  if(buf[0]==FLAG){
                    state_r = 1;
                  }
                  else {state_r = 0;}         
                  break;
                  
                 case 1:
                  if(buf[0]==A){
                    state_r = 2;
                  }
                  else if(buf[0]==FLAG){
                    state_r = 1;
                  } 
                  else {state_r = 0;}
                  break;
                  
                 case 2:
                  if(buf[0]==UA){
                    state_r = 3;
                  }
                  else if(buf[0]==FLAG){
                    state_r = 1;
                  } 
                  else {state_r = 0;} 
                  break;
                  
                 case 3:
                  if(buf[0]==BCCUA){
                    state_r = 4;
                  }
                  else if(buf[0]==FLAG){
                    state_r = 1;
                  } 
                  else {state_r = 0;}
                  break;
                  
                 case 4:
                  if(buf[0]==FLAG){
                    printf("Final UA RECEIVED\n");
		    STOP = TRUE;        
                  }
                  else {state_r = 0;}
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
