#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "src/info.c"
#include "src/getip.c"
#include "src/clientTCP.c"


int main(int argc, char *argv[]){

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <URL to deconstruct>\nURL should be as follows: ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        exit(-1);
    }

    struct info input_info;
	resetinfo(&input_info);

    parseinfo(&input_info,argv[1]);

    printf("Username: %s\n", input_info.user);
	printf("Password: %s\n", input_info.password);
	printf("Host: %s\n", input_info.host);
	printf("Path: %s\n", input_info.path);
	printf("Filename: %s\n", input_info.filename);


    char IPAddress[99] = "";

    if(getIP(input_info.host, IPAddress) != 0){
        printf("Error occurred in fuction getIP (main.c -- line 31)\n");
        exit(-1);
    }
    if(CreateConnection(&input_info, IPAddress) != 0){
        printf("Error in the connection\n");
        exit(-1);
    }

    return 0;
}