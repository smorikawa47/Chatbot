#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stddef.h>
#include "list.h"
#include "server.h"
#include "client.h"
#define bufferSize 4096

char stat[6] = "stat:";
char *clientStatus;
char *serverStatus;

int isOffline = 0;
int myPort;
int remotePort;
char *remoteHost;

List *receiver_list;
List *sender_list;

pthread_t senderThr;
pthread_t receiverThr;
pthread_t keyboardThr;
pthread_t printerThr;
pthread_cond_t receiveCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t sendCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t receiveMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sendMutex = PTHREAD_MUTEX_INITIALIZER;

void argsCheck(struct sockaddr_in addr, struct in_addr ipAddress) {
    int check = inet_pton(AF_INET, remoteHost, &(addr.sin_addr));
    if(check == 1){
        int check2 = inet_aton(remoteHost, &ipAddress);
        if(check2 == 0) {
            fprintf(stderr,"Usage:\n   ./lets-talk <local port> <remote host> <remote port>\nExamples:\n   ./lets-talk 3000 192.168.0.513 3001   \n./lets-talk 3000 some-computer-name 3001\n");
            exit(EXIT_FAILURE);
        }
    }
    else{
        struct hostent *machine;
        if ((machine = gethostbyname(remoteHost)) == NULL) {
            perror("Usage:\n   ./lets-talk <local port> <remote host> <remote port>\nExamples:\n   ./lets-talk 3000 192.168.0.513 3001\n   ./lets-talk 3000 some-computer-name 3001\n");
            exit(EXIT_FAILURE);
        }
    }
}

int const key = 19691;


void encryption(char *ch){
    int i = 0;
    while(ch[i] != '\0'){
        ch[i] = (char)((ch[i] + key) % 256);
        i++;
    }
}


void decryption(char *ch){
    int i = 0;
    while(ch[i] != '\0'){
        ch[i] = (char)((ch[i] - key) % 256);
        i++;
    }
}


void exiter() {
    pthread_cancel(senderThr);
    pthread_cancel(receiverThr);
    pthread_cancel(keyboardThr);
    pthread_cancel(printerThr);
}

void statusReceived(char * buffer) {
    if(strcmp(buffer,"!status") == 0){
        strcpy(buffer, clientStatus);
        isOffline = 1;
    }
}

void *server_thread() {
    int sockfd = prepareServer(myPort);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    struct in_addr ipAddress;

    argsCheck(server_addr, ipAddress);

    struct hostent *machine;
    machine = gethostbyname(remoteHost);
    ipAddress = *(struct in_addr *) machine->h_addr_list[0];
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr = ipAddress;
    server_addr.sin_port = htons(remotePort);
    
    unsigned int address_len = sizeof(server_addr);

    while (1) {
        char *buffer = malloc(bufferSize * sizeof(char));
        memset(buffer, 0, sizeof(char) * bufferSize);
        int bytes = recvfrom(sockfd, buffer, bufferSize, MSG_WAITALL, (struct sockaddr *) &server_addr, &address_len);

        if (bytes < 0) {
            perror("Gettin message failed\n");
            exit(EXIT_FAILURE);
        } 
        else if (bytes > 0) {
            fflush(stdout);
        }

        buffer[bytes] = '\0';
        
        decryption(buffer);

        if(strcmp(buffer, serverStatus) == 0) {
            int clientSocket = prepareClient(myPort);
            struct sockaddr_in client_addr;
            memset(&client_addr, 0, sizeof(client_addr));

            client_addr.sin_family = AF_INET;
            client_addr.sin_addr = ipAddress;
            client_addr.sin_port = htons(remotePort);

            encryption(buffer);
            
            if (sendto(clientSocket, buffer, strlen(buffer), MSG_CONFIRM, (struct sockaddr *) &client_addr, sizeof(server_addr)) == -1) {
                perror("Sending failed\n");
                exit(EXIT_FAILURE);
            }
        }
        else if(strcmp(buffer, clientStatus) == 0){
            isOffline = 0;
            printf("Online\n");
        } 
        else {
            pthread_mutex_lock(&receiveMutex);

            if (List_append(receiver_list, (void *) buffer) != -1) {
                pthread_cond_signal(&receiveCond);
            }
            else {
                perror("Getting input failed\n");
                exit(EXIT_FAILURE);
            }

            pthread_mutex_unlock(&receiveMutex);
        }
    }
}



void *client_thread() {
    int sockfd = prepareClient(myPort);
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    struct in_addr ipAddress;
    
    argsCheck(client_addr, ipAddress);

    struct hostent *machine;
    machine = gethostbyname(remoteHost);
    ipAddress = *(struct in_addr *) machine->h_addr_list[0];
    
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(remotePort);
    client_addr.sin_addr = ipAddress;

    while (1) {
        pthread_mutex_lock(&sendMutex);
        pthread_cond_wait(&sendCond, &sendMutex);

        int check = List_count(sender_list);
        
        if (check != 0) {
            char *buffer = (char *) List_remove(sender_list);
            
            statusReceived(buffer);
            
            encryption(buffer);
            
            if (sendto(sockfd, buffer, strlen(buffer), MSG_CONFIRM, (struct sockaddr *) &client_addr, sizeof(client_addr)) == -1) {
                perror("Sending failed\n");
                exit(EXIT_FAILURE);
            }
            
            decryption(buffer);
            
            if (strcmp(buffer, "!exit") == 0) {
                exiter();
                return NULL;
            }
        }

        pthread_mutex_unlock(&sendMutex);
        sleep(2);
        
        if(isOffline != 0){
            printf("Offline\n");
        }

    }
}


void *print_thread(){
    while(1) {
        pthread_mutex_lock(&receiveMutex);
        pthread_cond_wait(&receiveCond, &receiveMutex);

        int check = List_count(receiver_list);

        if (check != 0){
            char *buffer = (char*) List_remove(receiver_list);
            
            if(strcmp(buffer,"!exit") == 0){
                exiter();
                return NULL;
            }
            else {
                printf("%s\n",buffer);
            }
            free(buffer);
        }
        pthread_mutex_unlock(&receiveMutex);
    }
}


void getInput(char* input) {
    int pos = 0;
    int buffsize = bufferSize;
    char *buffer = malloc(bufferSize * sizeof(char));

    if(buffer == NULL){
        printf("Allocation failed\n");
        exit(0);
    }

    while (1){
        char ch;
        ch = getc(stdin);
        if (ch == EOF) {
            break;
        }
        if (ch == '\n') {
            sleep(1);
            if(ftell(stdin) < 0){
                buffer[pos] ='\0';
                break;
            }
        }
        if(pos >= bufferSize) {
            buffer = realloc(buffer, buffsize);
        }
        buffer[pos] = ch;
        pos++;
    }
    
    strcpy(input, buffer);
    free(buffer);
}


void *keyboard_thread(){
    while(1) {
        char buffer[bufferSize];

        getInput(buffer);

        pthread_mutex_lock(&sendMutex);

        if (List_append(sender_list, (void *) &buffer) == 0) {
            pthread_cond_signal(&sendCond);
        }
        else {
            perror("Getting input failed\n");
            exit(1);
        }
        pthread_mutex_unlock(&sendMutex);
    }
}


int main (int argc, char *argv[]){
    if (argc != 4){
        printf("Usage:\n   ./lets-talk <local port> <remote host> <remote port>\nExamples:\n   ./lets-talk 3000 192.168.0.513 3001\n   ./lets-talk 3000 some-computer-name 3001\n");
        exit(0);
    }

    myPort = atoi(argv[1]);
    remoteHost = argv[2];
    remotePort = atoi(argv[3]);
    
    char st[10];
    strcpy(st, stat);
    strcat(st,argv[1]);
    clientStatus = st;
    
    char str[10];
    strcpy(str, stat);
    strcat(str,argv[3]);
    serverStatus = str;

    receiver_list = List_create();
    sender_list = List_create();

    pthread_create(&senderThr, NULL, client_thread, NULL);
    pthread_create(&receiverThr, NULL, server_thread, NULL);
    pthread_create(&keyboardThr, NULL, keyboard_thread, NULL);
    pthread_create(&printerThr, NULL, print_thread, NULL);

    printf("Welcome to Lets-Talk! Please type your message now.\n");
    
    pthread_join(senderThr, NULL);
    pthread_join(receiverThr, NULL);
    pthread_join(keyboardThr, NULL);
    pthread_join(printerThr, NULL);

    List_free(sender_list, NULL);
    List_free(receiver_list, NULL);

    printf("bye\n");
    
    return 0;
}