#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<netdb.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

int prepareClient(int port){
    int sockfd;
    struct sockaddr_in client;
    memset(&client, 0, sizeof(client));

    client.sin_family = AF_INET;
    client.sin_port = htons(port);
    client.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if((sockfd = socket(PF_INET,SOCK_DGRAM, 0)) < 0){
        perror("Connect failed\n");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}