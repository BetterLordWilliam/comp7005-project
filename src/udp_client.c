#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>


/*      CLIENT STARTER CODE LOOP
char buffer[BUFFER_SIZE];

for (;;) {
    cstate->watched[0].revents = 0;
    cstate->watched[1].revents = 0;

    int ready = poll(cstate->watched, 2, -1);
    if (ready == -1) {
        if (errno == EINTR) {
            continue;
        }
        perror("poll");
        return EXIT_FAILURE;
    }

    if (cstate->watched[0].revents & (POLLIN | POLLHUP)) {
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }

        size_t length = strlen(buffer);
        int send_result = sendto(cstate->client_fd, buffer, length, 0,
            (struct sockaddr*)&cstate->network, sizeof(cstate->network));
        if (send_result == -1) {
            return EXIT_FAILURE;
        }
    }

    if (cstate->watched[1].revents & POLLIN) {
        ssize_t received = recvfrom(cstate->client_fd,
                                    buffer,
                                    sizeof(buffer),
                                    0,
                                    NULL,
                                    NULL);
        if (received == -1) {
            if (errno == EINTR) {
                return CONTINUE;
            }
            perror("recvfrom");
            return EXIT_FAILURE;
        }

        printf("echo: ");
        fwrite(buffer, 1, (size_t)received, stdout);
        if (received == 0 || buffer[received - 1] != '\n') {
            putchar('\n');
        }
        fflush(stdout);
    }
}
*/


#include "common.h"
#include "udp_saw.h"
#include "udp_slw.h"


static void usage(const char *program)
{
    fprintf(stderr, "Usage: %s <network-ip> <network-port>\n", program);
}

struct saw_cstate {
    int client_fd;
    struct pollfd* watched;
    struct sockaddr_in network;
};


int saw_loop(struct saw_cstate* cstate)
{
    char buffer[BUFFER_SIZE];
    saw_state_t scstate; saw_initState(&scstate);
    scstate.paddr       = cstate->network;
    scstate.paddrLen    = sizeof(cstate->network);
    int conn_r = saw_connect(cstate->client_fd, &scstate);
    LINFO("connect return code: %d", conn_r);

    for (;;) {
        cstate->watched[0].revents = 0;
        cstate->watched[1].revents = 0;

        int ready = poll(cstate->watched, 2, -1);
        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            return EXIT_FAILURE;
        }

        /* stdin is independent: send each available line immediately. */
        if (cstate->watched[0].revents & (POLLIN | POLLHUP)) {
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                break;
            }
            size_t length = strlen(buffer);
            int send_result = saw_send(cstate->client_fd, &scstate,
                buffer, length);
            if (send_result == -1) {
                LERROR("failed to send message.");
                continue;
                // return EXIT_FAILURE;
            }
        }

        /* Echo reception is independent and never gates the next send. */
        if (cstate->watched[1].revents & POLLIN) {
            /*
            ssize_t received = recvfrom(cstate->client_fd,
                                        buffer,
                                        sizeof(buffer),
                                        0,
                                        NULL,
                                        NULL);
            */
            int received = saw_recv(cstate->client_fd, &scstate,
                buffer, sizeof(buffer));
            if (received == -1) {
                if (errno == EINTR) {
                    return 0;
                }
                return EXIT_FAILURE;
            }
            if (received == 0) {
                LWARN("nothing to read now");
                continue;
            }

            printf("echo: ");
            fwrite(buffer, 1, (size_t)received, stdout);
            if (received == 0 || buffer[received - 1] != '\n') {
                putchar('\n');
            }
            fflush(stdout);
        }
    }
    return 0;
}


int slw_loop(slw_state_t* state, struct sockaddr* addr, socklen_t addrlen) {
    struct pollfd watched[2] = {
        {
            .fd = STDIN_FILENO,
            .events = POLLIN,
            .revents = 0,
        },
        {
            .fd = state->fd,
            .events = POLLIN,
            .revents = 0,
        },
    };

    char buffer[BUFFER_SIZE];
    for (;;) {
        watched[0].revents = 0;
        watched[1].revents = 0;

        // if sndwind is not empty
        //      infinite poll waiting for data from stdin
        // otherwise
        //      poll duration should be the timeout duration
        int ready = poll_s(watched, 2, (slw_window_empty(&state->sndwind))
            ? -1
            : state->timeoutdur);
        if (ready == -1) {
            return EXIT_FAILURE;
        }
        if (ready == 0) {
#ifdef UDP_DEBUG
            LWARN("poll timeout");
#endif
        }

        /* stdin is independent: send each available line immediately. */
        if (watched[0].revents & (POLLIN | POLLHUP)) {
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                break;
            }
            size_t length = strlen(buffer);
            /*
            int send_result = saw_send(cstate->client_fd, &scstate,
                buffer, length);
            if (send_result == -1) {
                LERROR("failed to send message.");
                continue;
                // return EXIT_FAILURE;
            }
            */

            // submit new messages to the server
            ssize_t send_result = slw_send(state, buffer, length, addr, addrlen);
            if (send_result == SLW_EWOULDBLOCK) {
#ifdef UDP_DEBUG
                LWARN("new message dropped send window is full");
#endif
                continue;
            } else if (send_result < 0) {
#ifdef UDP_DEBUG
                LERROR("send failed");
#endif
                return EXIT_FAILURE;
            }
#ifdef UDP_DEBUG
            LINFO("sent %ld bytes", send_result);
#endif
        }

        /* Echo reception is independent and never gates the next send. */
        if (watched[1].revents & POLLIN) {
            // sliding window processing
            int process_result = slw_process(state, addr, &addrlen);
            if (process_result != SLW_OK) {
                if (process_result == SLW_EWOULDBLOCK) {
#ifdef UDP_DEBUG
                    LWARN("poll returned but there was nothing to process on the socket");
#endif
                    continue;
                } else if (process_result == SLW_ERROR) {
                    break;
                }
            }

            // receive any server replies
            ssize_t receive_result = slw_recv(state, buffer, sizeof(buffer));
            if (!(receive_result > 0)) {
                if (receive_result == SLW_EWOULDBLOCK) {
#ifdef UDP_DEBUG
                    LWARN("rcvwind is empty nothing to receive");
#endif
                    continue;
                } else if (receive_result == SLW_EBUFSIZE) {
#ifdef UDP_DEBUG
                    LWARN("buffer not large enough to contain at least 1 packet data\n\trecommend resize the buffer");
#endif
                    // [WO] this is not a great 'solution' to this issue because this will lead to
                    // a lot of retransmissions
                    continue;
                }
            }

            /*
            int received = saw_recv(cstate->client_fd, &scstate,
                buffer, sizeof(buffer));
            if (received == -1) {
                if (errno == EINTR) {
                    return 0;
                }
                return EXIT_FAILURE;
            }
            if (received == 0) {
                LWARN("nothing to read now");
                continue;
            }
            */

        
            /* APPLICATION */
            printf("echo: ");
            fwrite(buffer, 1, (size_t)receive_result, stdout);
            if (receive_result == 0 || buffer[receive_result - 1] != '\n') {
                putchar('\n');
            }
            fflush(stdout);
        }
    }
    return 0;
}


int main(int argc, char **argv)
{
    if (argc != 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    unsigned short port = (unsigned short)strtoul(argv[2], NULL, 10);
    struct sockaddr_in network = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };
    if (inet_pton(AF_INET, argv[1], &network.sin_addr) != 1) {
        fprintf(stderr, "Invalid IPv4 address: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    int client_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    setvbuf(stdin, NULL, _IONBF, 0);
    printf("Type lines quickly, or paste several lines. Echoes print as they arrive.\n");
    printf("Ctrl+D exits.\n");

    struct pollfd watched[2] = {
        {
            .fd = STDIN_FILENO,
            .events = POLLIN,
            .revents = 0,
        },
        {
            .fd = client_fd,
            .events = POLLIN,
            .revents = 0,
        },
    };

    // [WO] saw client state
    /*
    struct saw_cstate cstate = { 0 };
    cstate.client_fd = client_fd;
    cstate.watched   = watched;
    cstate.network   = network;


    int r = saw_loop(&cstate);

    if (r == EXIT_FAILURE) {
        close(client_fd);
        return EXIT_FAILURE;
    }*/


    // [WO] slw client state
    int slots = 5;
    slw_state_t state = { 0 };
    slw_init(&state, client_fd, slots, 1000);

    LINFO("SLW STATE: { fd: %d, slots: %d }", state.fd, state.slots);
    LINFO("SLW SNDWIND STATE: { slots: %d, length: %d }",
        state.sndwind.slots, state.sndwind.length);
    LINFO("SLW RCVWIND STATE: { slots: %d, length: %d }",
        state.rcvwind.slots, state.rcvwind.length);

    int r = slw_loop(&state, (struct sockaddr*)&network, sizeof(network));
    (void)r;

    slw_free(&state);
    close(client_fd);

    return EXIT_SUCCESS;
}
