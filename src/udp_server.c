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
#include "udp_slw.h"

struct saw_sstate {
    int server_fd;
};

/**
SAW specific application loop
*/
int saw_loop(struct saw_sstate* sstate)
{
    saw_state_t ssstate; saw_initState(&ssstate);
    int listen_r = saw_listen(sstate->server_fd, &ssstate);
    LINFO("listen return code: %d", listen_r);
    // LINFO("DEBUG: { peer address: %d }", ssstate.paddr.sin_addr.s_addr);

    for (;;) {
        char buffer[BUFFER_SIZE];
        //struct sockaddr_in peer;
        //socklen_t peer_length = sizeof(peer);
        /*
        ssize_t length = recvfrom(sstate->server_fd,
                                  buffer,
                                  sizeof(buffer),
                                  0,
                                  (struct sockaddr *)&peer,
                                  &peer_length);
        */
        int length = saw_recv(sstate->server_fd, &ssstate,
            buffer, sizeof(buffer));
        if (length == -1) {
            LWARN("dropped incoming message.");
            continue;
        }
        if (length == 0) {
            LWARN("nothing to read rn");
            continue;
        }

        printf("received: ");
        fwrite(buffer, 1, (size_t)length, stdout);
        if (length == 0 || buffer[length - 1] != '\n') {
            putchar('\n');
        }
        fflush(stdout);

        /*
        if (sendto(sstate->server_fd,
                   buffer,
                   (size_t)length,
                   0,
                   (struct sockaddr *)&peer,
                   peer_length) == -1) {
            perror("sendto");
        }*/
        if (saw_send(sstate->server_fd, &ssstate,
                buffer, length) == -1) {
            LINFO("error sending the echo message");
        }
        continue;
    }
}

int slw_loop(slw_state_t* state, struct sockaddr* addr, socklen_t addrlen)
{
    struct pollfd pfd = {
        .fd         = state->fd,
        .events     = POLLIN,
        .revents    = 0
    };

    for (;;) {
        pfd.revents = 0;

        char buffer[BUFFER_SIZE];
        struct sockaddr_in peer;
        socklen_t peer_length = sizeof(peer);

        int poll_result = poll_s(&pfd, 1, (slw_window_empty(&state->sndwind))
            ? -1 
            : state->timeoutdur);
        if (poll_result == -1) {
            return EXIT_FAILURE;
        }

        // receive data from the client peer
        //      this branch wouldn't exit in the poll timeout case
        //      poll time out (poll_result == 0) would seperately trigger slw_retransmit
        if (pfd.revents & (POLLIN)) {

            // process the sliding windows
            int process_result = slw_process(state, (struct sockaddr*)&peer, &peer_length);
            if (!(process_result == SLW_OK)) {
                if (process_result == SLW_EWOULDBLOCK) {
#ifdef UDP_DEBUG                
                    LWARN("poll returned but there was nothing to process on the socket");            
#endif
                    continue;
                } else if (process_result == SLW_ERROR) {
                    return EXIT_FAILURE;
                }
            }

            // receive the clients incoming message
            ssize_t receive_result = slw_recv(state, buffer, sizeof(buffer));
            if (!(receive_result >= 0)) {
                if (receive_result == SLW_EWOULDBLOCK) {
#ifdef UDP_DEBUG
                    LWARN("rcvwind is empty nothing to receive");
#endif
                    continue;
                } else if (receive_result == SLW_EBUFSIZE) {
#ifdef UDP_DEBUG
                    LWARN("buffer not large enough to contain at least 1 packet\n\trecommend resize the buffer");
#endif
                    // [WO] this is not a great 'solution' to this issue because this will lead to
                    // a lot of retransmission
                    continue;
                }
            }
            /*
            int length = saw_recv(sstate->server_fd, &ssstate,
                buffer, sizeof(buffer));
            */
            /* APPLICATION */
            printf("received: ");
            fwrite(buffer, 1, (size_t)receive_result, stdout);
            if (receive_result == 0 || buffer[receive_result - 1] != '\n') {
                putchar('\n');
            }
            fflush(stdout);
            /*
            if (saw_send(sstate->server_fd, &ssstate,
                    buffer, length) == -1) {
                LINFO("error sending the echo message");
            }
            */

            // transmit the reply message
            ssize_t send_result = slw_send(state, buffer, (size_t)receive_result,
                (struct sockaddr*)&peer, peer_length);
            if (!(send_result >= 0)) {
                if (send_result == SLW_EWOULDBLOCK) {
#ifdef UDP_DEBUG
                    LWARN("new message dropped send window is full");
#endif
                    continue;
                } else if (send_result < 0) {
#ifdef UDP_DEBUG
                    LERROR("send failure");
#endif
                    return EXIT_FAILURE;
                }
            }
        }
    }

    return 0;
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
    /*struct saw_sstate sstate = { 0 };
    sstate.server_fd = server_fd;

    int r = saw_loop(&sstate);
    (void)r;*/


    // [WO] slw server state
    int slots = 5;
    slw_state_t state = { 0 };
    slw_init(&state, server_fd, slots, 1000);

    LINFO("SLW STATE: { fd: %d, slots: %d }", state.fd, state.slots);
    LINFO("SLW SNDWIND STATE: { slots: %d, length: %d }",
        state.sndwind.slots, state.sndwind.length);
    LINFO("SLW RCVWIND STATE: { slots: %d, length: %d }",
        state.rcvwind.slots, state.rcvwind.length);

    int r = slw_loop(&state, (struct sockaddr*)&local, sizeof(local));
    (void)r;
   
    slw_free(&state); 
    close(server_fd);
    
    return 0;
}
