#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "cdata.h"

pthread_t ui_mngr_thread, recv_mngr_thread;
uint64_t ui_mngr_ticks;
int server_sd;

void *ui_mngr_func(void *arg)
{

}

void *recv_mngr_func(void *arg)
{
    uint8_t buf[sizeof(struct msg_batch)];
    ssize_t recvbytes;
    
    while("zombies walk") {
        if((recvbytes = recvfrom(server_sd, buf, sizeof(struct msg_batch),
                                 0, NULL, NULL)) < 0) {
            perror("client: recvfrom");
        }
    }

    close(sd);
}

int main(int argc, char **argv)
{
    pthread_attr_t common_attr;

    ui_mngr_ticks = ticks_start();
    
    // TODO: get the address from the argv
    struct hosten *host = gethostbyname((char *) "127.0.0.1");
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(struct server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(6006);
    server_addr.sin_addr = *((struct in_addr *) host->h_addr);

    server_sd = socket(server_addr.sin_family, SOCK_DGRAM, 0);
    if(sd < 0) {
        perror("client: socket");
        exit(EXIT_FAILURE);
    }
    
    pthread_attr_init(&common_attr);
    pthread_attr_setdetachstate(&common_attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&recv_mngr_thread, &common_attr, recv_mngr_func, NULL);
    pthread_create(&ui_mngr_thread, &common_attr, ui_mngr_func, NULL);

    pthread_join(recv_mngr_thread);
    pthread_join(ui_mngr_thread);
    pthread_attr_destroy(&common_attr);
    pthread_exit(NULL);
    
    return 0;
}
