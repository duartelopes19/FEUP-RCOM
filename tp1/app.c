// Main file of the serial port project.
// NOTE: This file must not be changed.

#include <stdio.h>
#include <stdlib.h>

//#include "app.h"
#include "link_layer.c"

#define N_TRIES 3
#define TIMEOUT 4
#define RECEIVER 0
#define TRANSMITTER 1

// Arguments:
//   $1: /dev/ttySxx
//   $2: tx | rx
//   $3: filename

int main(int argc, char *argv[]){

    if (argc < 4)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort> <0/1> <File>\n 0 - TRANSMITTER\n 1 - RECEIVER\n\n"
               "Example: %s /dev/ttyS1 0 <File>\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    char *serialPort = argv[1];
    int rolenum = atoi(argv[2]);
    char *filename = argv[3];
    

    printf("Starting link-layer protocol application...\n"
           "  - Serial port: %s\n"
           "  - Role: %d\n"
           "  - Baudrate: %d\n"
           "  - Number of tries: %d\n"
           "  - Timeout: %d\n"
           "  - Filename: %s\n",
           serialPort,
           rolenum,
           BAUDRATE,
           N_TRIES,
           TIMEOUT,
           filename);

    
    
    
    applicationLayer(serialPort, rolenum, BAUDRATE, N_TRIES, TIMEOUT, filename);

    return 0;
    
}
