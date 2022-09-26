#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<netdb.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

int prepareServer(int port){
    int sockfd;
    struct sockaddr_in sev;
    memset(&sev,0,sizeof(sev));

    sev.sin_family = AF_INET;
    sev.sin_addr.s_addr = htonl(INADDR_ANY);
    sev.sin_port = htons(port);
    
    if((sockfd = socket(PF_INET,SOCK_DGRAM, 0)) < 0){
        perror("Connect failed\n");
        exit(EXIT_FAILURE);
    }
    
    if(bind(sockfd,(const struct sockaddr*)&sev, sizeof(sev)) < 0){
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}