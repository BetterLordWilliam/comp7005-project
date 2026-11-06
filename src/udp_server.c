#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

/*      SERVER STARTER CODE LOOP
for (;;) {
    unsigned char buffer[BUFFER_SIZE];
    struct sockaddr_in peer;
    socklen_t peer_length = sizeof(peer);
    ssize_t length = recvfrom(sstate->server_fd,
                              buffer,
                              sizeof(buffer),
                              0,
                              (struct sockaddr *)&peer,
                              &peer_length);
    if (length == -1) {
        perror("recvfrom");
        continue;
    }

    printf("received: ");
    fwrite(buffer, 1, (size_t)length, stdout);
    if (length == 0 || buffer[length - 1] != '\n') {
        putchar('\n');
    }
    fflush(stdout);

    if (sendto(sstate->server_fd,
               buffer,
               (size_t)length,
               0,
               (struct sockaddr *)&peer,
               peer_length) == -1) {
        perror("sendto");
    }
    continue;
}
*/

#include "udp_saw.h"

struct saw_sstate {
    int server_fd;
};

enum saw_loop_codes {
    CONTINUE
};

/**
SAW specific application loop
*/
int saw_loop(struct saw_sstate* sstate)
{
    LINFO("fd %d", sstate->server_fd);

    saw_state_t ssstate; saw_initState(&ssstate);
    int listen_r = saw_listen(sstate->server_fd, &ssstate);
    LINFO("listen return code: %d", listen_r);

    for (;;) {
        unsigned char buffer[BUFFER_SIZE];
        struct sockaddr_in peer;
        socklen_t peer_length = sizeof(peer);
        ssize_t length = recvfrom(sstate->server_fd,
                                  buffer,
                                  sizeof(buffer),
                                  0,
                                  (struct sockaddr *)&peer,
                                  &peer_length);
        if (length == -1) {
            perror("recvfrom");
            continue;
        }

        printf("received: ");
        fwrite(buffer, 1, (size_t)length, stdout);
        if (length == 0 || buffer[length - 1] != '\n') {
            putchar('\n');
        }
        fflush(stdout);

        if (sendto(sstate->server_fd,
                   buffer,
                   (size_t)length,
                   0,
                   (struct sockaddr *)&peer,
                   peer_length) == -1) {
            perror("sendto");
        }
        continue;
    }
}


int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <listen-port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    unsigned short port = (unsigned short)strtoul(argv[1], NULL, 10);
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in local = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
    };
    if (bind(server_fd, (struct sockaddr *)&local, sizeof(local)) == -1) {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("UDP server listening on 127.0.0.1:%u\n", port);
    fflush(stdout);

   
    // [WO] saw server state
    struct saw_sstate sstate = { 0 };
    sstate.server_fd = server_fd;

    int r = saw_loop(&sstate);
    (void)r;
    
    close(server_fd);
    
    return 0;
}
