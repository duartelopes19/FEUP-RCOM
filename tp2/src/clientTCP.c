/**      (C)2000-2021 FEUP
 *       tidy up some includes and parameters
 * */

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "../include/info.h"


int getLastLineStatusCode(char *buf){

    int a;
    char *pt;
    pt = strtok (buf,"\n");
    while (pt != NULL) {
        a = 0;
        a = atoi(pt);
        pt = strtok (NULL, "\n");
    }
    return a;
}

int getPortNumber(char* buf){

    int numb[5] = {0};
    int i = 0;
    char *pt;
    pt = strtok (buf,",");
    pt = strtok (NULL,",");
    while (pt != NULL) {
        int a = atoi(pt);
        numb[i] = a;
        pt = strtok (NULL, ",");
        i++;
    }
    return (numb[3]*256 + numb[4]);

    return 0;
}


int CreateSocket(char serverAddress[], int port) {

    int sockfd;
    struct sockaddr_in server_addr;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(serverAddress);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    /*
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }
    */
    connect(sockfd,(struct sockaddr *) &server_addr, sizeof(server_addr));
    return sockfd;
}

int CreateConnection(info *info, char* IPAddress){

    //1º abrir a ligaçao com o IP e a PORT
    //2º fazer login (anonymous ou com os dados fornecidos)
    //3º entrar no passive mode
    //4º receber info e calcular a PORT de leitura
    //5º ler de PATH o ficheiro pretendido

    FILE* fileptr;

    int sizeUser = strlen(info->user), sizePass = strlen(info->password), port = 0, download = 0, sizePath = strlen(info->path);

    char userLogin[sizeUser+7], passwdLogin[sizePass+7], retrvPath[sizePath+7];

    //user string
    userLogin[0] = '\0';
    strcat(userLogin, "user ");
    strcat(userLogin, info->user);
    strcat(userLogin, "\r\n");

    //passwd string
    passwdLogin[0] = '\0';
    strcat(passwdLogin, "pass ");
    strcat(passwdLogin, info->password);
    strcat(passwdLogin, "\r\n");

    //Retrieve string
    retrvPath[0] = '\0';
    strcat(retrvPath, "retr ");
    strcat(retrvPath, info->path);
    strcat(retrvPath, "\r\n");


    char buf[500] = {0};
    char buf2[500]={0};
    int STOP = 0;
    int visited = 0;
    size_t bytes, bytes2;

    int sockfd = CreateSocket(IPAddress, info->port), sockfd2 = 0;
    if(sockfd == -1) return -1;

    //TODO criar state machine
    while (!STOP)
    {  
        memset(buf, 0, 500);
        bytes = read(sockfd, buf, 500);
        
        if(download){
            memset(buf2, 0, 500);
            bytes2 = read(sockfd2, buf2, 500);
            if(bytes2 != -1 && bytes2 != 0) {
                printf("\nbuf2:");
                for(int i=0; i<bytes2; i++){
                    printf("%c", buf2[i]);
                    fputc(buf2[i], fileptr);
                }
            }
        }

        if(bytes == -1 || bytes == 0) {/* printf("\ni got nothing -- buf\n"); */ continue;}
        printf("\n%s\n", buf);
        int sc = getLastLineStatusCode(buf);

        switch(sc){
            case 220: 
                if(visited) break;
                visited = 1;
                write(sockfd, userLogin, strlen(userLogin));
                break;
            case 331:
                write(sockfd, passwdLogin, strlen(passwdLogin));
                break;
            case 230:
                write(sockfd, "pasv\r\n", 6);
                break;
            case 227:
                port = getPortNumber(buf);

                if ((sockfd2 = CreateSocket(IPAddress, port)) == -1) return -1;

                write(sockfd, retrvPath, strlen(retrvPath));


                break;

            case 150: 
                fileptr = fopen(info->filename, "w");
                download = 1;
                break;

            case 226: 
                while(1){
                    memset(buf2, 0, 500);
                    bytes2 = read(sockfd2, buf2, 500);
                    if(bytes2 != -1 && bytes2 != 0) {
                        printf("\nbuf2:");
                        for(int i=0; i<bytes2; i++){
                            printf("%c", buf2[i]);
                            fputc(buf2[i], fileptr);
                        }
                        printf("\n");
                    }
                    else{break;}
                }
                download = 0;
                STOP = 1;
                break;

            default:
                return -1;
                break;
        }
    }

    fclose(fileptr);
    
    if (close(sockfd2)<0) {
        perror("close()");
        return -1;
    }

    if (close(sockfd)<0) {
        perror("close()");
        return -1;
    }
    return 0;


}


