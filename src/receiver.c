/** @file receiver.c
 *  @brief A UDP receiver program
 *
 *  This contains the code for the receiver
 *  using the UDP file transfer protocol. The
 *  main function parses the command line arguments
 *  and then calls the rrecv function to receive
 *  the file.
 *
 *  @author Vicky Chen (chen-vv)
 *  @author Eric Omielan (eomielan)
 *  @bug No known bugs.
 */

/* -- Includes -- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <pthread.h>
#include <errno.h>
#include "include/udp.h"

/* -- Global Variables -- */

/**
 * @brief The most recent sequence number of a received packet.
 *
 * By default, this is set to 0. This is used to keep track of the
 * most recent sequence number received from the sender. This is
 * used to discard duplicate packets.
 */
uint32_t _latestSequenceNumber = 0;

/**
 * @brief Sends an acknowledgment message to the sender.
 *
 * TODO: add sequence number to ack_msg (depends on how this is implemented in sender)
 * btw this is for sending an acknowledgement that a packet was received
 *
 * @param sockfd The socket file descriptor
 * @param addr The address of the sender
 * @param addrlen The length of the address
 * @param sequenceNumber The sequence number of the packet to acknowledge
 * @return Void
 *
 * Sources:
 * https://www.ibm.com/docs/en/zos/3.1.0?topic=functions-sendto-send-data-socket
 */
void send_packet_ack(int sockfd, struct sockaddr_in *addr, socklen_t addrlen, uint32_t sequenceNumber)
{
    if (sendto(sockfd, &sequenceNumber, sizeof(uint32_t), 0, (struct sockaddr *)addr, addrlen) == -1)
    {
        perror("sendto");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Establishes a connection with the sender using the 3-way handshake process.
 *
 * Receives a SYN packet from the sender and sends a SYN-ACK packet back. Upon receiving
 * the final ACK packet, the connection is established.
 *
 * This function sets a random value for the sequence number that will be sent to the
 * sender. This is used to ensure that the sender and receiver are in sync with each other
 * and that the packets are not duplicated or lost. While the sequence number could always
 * be set to 0, this would make the protocol more susceptible to attacks and would not be
 * as robust as using a random sequence number.
 *
 * If the initial SYN packet or final ACK packet is not received within a certain timeout,
 * this function will initiate an exponential backoff mechanism and double the timeout until a
 * maximum timeout is reached.
 *
 * @param sockfd The socket file descriptor
 * @param addr The address of the sender
 * @param addrlen The length of the address
 * @return Void
 */
void establish_connection(int sockfd, struct sockaddr_in *addr, socklen_t addrlen)
{
    struct timeval tv;
    int timeout = SYN_ACK_DEFAULT_TIMEOUT_MILLISEC;
    tv.tv_sec = 0;

    while (TRUE)
    {
        // Set timeout for SYN and ACK packets
        tv.tv_usec = timeout;

        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        {
            perror("timeout");
            exit(1);
        }

        // Listen for SYN packet
        struct Syn syn;
        ssize_t bytes_received = recvfrom(sockfd, &syn, sizeof(struct Syn), 0, (struct sockaddr *)addr, &addrlen);

        if (bytes_received > 0)
        {
            struct SynAck syn_ack;

            // Initialize sequence number and ack number
            srand(time(NULL));
            syn_ack.sequenceNumber = rand();
            syn_ack.ackNumber = syn.sequenceNumber + 1;

            timeout = SYN_ACK_DEFAULT_TIMEOUT_MILLISEC;
            tv.tv_usec = timeout;

            while (TRUE)
            {
                // Send SYN-ACK packet
                sendto(sockfd, &syn_ack, sizeof(struct SynAck), 0, (struct sockaddr *)addr, addrlen);

                // Listen for ACK packet
                struct Ack ack;
                ssize_t recv_size = recvfrom(sockfd, &ack, sizeof(struct Ack), 0, (struct sockaddr *)addr, &addrlen);

                if (recv_size > 0)
                {
                    if (ack.ackNumber == syn_ack.ackNumber)
                    {
                        return;
                    }
                    else
                    {
                        continue;
                    }
                }
                else if (recv_size == -1)
                {
                    // ACK packet not received, update timeout and resend SYN-ACK
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        if (timeout < SYN_ACK_MAX_TIMEOUT_MILLISEC)
                        {
                            timeout *= 2;
                        }

                        continue;
                    }
                    else
                    {
                        perror("recvfrom");
                        exit(1);
                    }
                }
            }
        }

        else if (bytes_received == -1)
        {
            // SYN packet not received, update timeout and resend SYN
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                if (timeout < SYN_ACK_MAX_TIMEOUT_MILLISEC)
                {
                    timeout *= 2;
                }

                continue;
            }
            else
            {
                perror("recvfrom");
                exit(1);
            }
        }
    }
}

/** @brief Writes the bytes received on port myUDPport to a file
 *         called destinationFile at a rate of writeRate bytes
 *         per second.
 *
 *  If writeRate is 0, then the receiver can write as many bytes
 *  as possible into destinationFile. Otherwise, if writeRate is
 *  non-zero then the receiver writes no more than writeRate bytes
 *  per second to destinationFile. See rsend for the counterpart function.
 *
 *  @param myUDPport The port number to listen on.
 *  @param destinationFile The name of the file to write to.
 *  @param writeRate The maximum number of bytes to write per second.
 *                   Must be an integer greater than or equal to zero.
 *  @return Void.
 *
 *  Sources:
 *  https://www.geeksforgeeks.org/socket-programming-cc/
 *  https://www.gta.ufrj.br/ensino/eel878/sockets/sockaddr_inman.html
 *  https://www.geeksforgeeks.org/time-function-in-c/
 *  https://www.ibm.com/docs/en/zos/3.1.0?topic=functions-recvfrom-receive-messages-socket
 */
void rrecv(unsigned short int myUDPport,
           char *destinationFile,
           unsigned long long int writeRate)
{
    // Initialize socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0)
    {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(myUDPport);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    socklen_t addrlen = sizeof(addr);

    int err = bind(sockfd, (struct sockaddr *)&addr, addrlen);
    if (err < 0)
    {
        perror("bind");
        exit(1);
    }

    // Prepare file for writing
    FILE *file = fopen(destinationFile, "wb");
    if (file == NULL)
    {
        perror("fopen");
        exit(1);
    }

    // Establish connection with sender prior to receiving packets
    establish_connection(sockfd, &addr, addrlen);

    time_t start, end;
    time(&start);

    unsigned long long bytesWritten = 0;

    while (TRUE)
    {
        int packetSize = MAX_BUFFER_SIZE + HEADER_SIZE;
        char packet[packetSize];

        int bytesReceived = recvfrom(sockfd, packet, packetSize, 0, (struct sockaddr *)&addr, &addrlen);
        if (bytesReceived < 0)
        {
            perror("recvfrom");
            exit(1);
        }
        else if (bytesReceived == 0)
        {
            continue;
        }

        struct Header header;
        memcpy(&header, packet, HEADER_SIZE);
        send_packet_ack(sockfd, &addr, addrlen, header.sequenceNumber);

        char packet_data[header.messageLength];
        memcpy(packet_data, packet + HEADER_SIZE, header.messageLength);

        // If packet's sequence number has already been received, discard duplicate
        if (header.sequenceNumber <= _latestSequenceNumber)
        {
            continue;
        }

        fwrite(packet_data, 1, header.messageLength, file);

        if (header.lastPacket == TRUE)
        {
            break;
        }

        bytesWritten += bytesReceived;
        _latestSequenceNumber = header.sequenceNumber;

        time(&end);
        double seconds = difftime(end, start);

        // TODO: implement flow control via sliding window mechanism
        //
        // If writeRate exceeded, signal to sender to slow down
        if (writeRate > 0 && bytesWritten / seconds > writeRate)
        {
            sleep(1);
        }
    }

    fclose(file);
    close(sockfd);
}

/** @brief UDP receiver entrypoint.
 *
 *  Parses the command line arguments and calls the rrecv function
 *  to receive the file. If writeRate is not specified, then the
 *  default value is 0.
 *
 *  @return Should not return
 */
int main(int argc, char **argv)
{

    unsigned short int udpPort;
    char *destinationFile = NULL;
    unsigned long long int writeRate;

    udpPort = (unsigned short int)atoi(argv[1]);
    destinationFile = argv[2];

    if (argc == 4)
    {
        writeRate = (unsigned long long int)atoll(argv[3]);
    }
    else if (argc == 3)
    {
        writeRate = 0;
    }
    else
    {
        fprintf(stderr, "usage: %s UDP_port filename_to_write [writeRate]\n\n", argv[0]);
        exit(1);
    }

    rrecv(udpPort, destinationFile, writeRate);

    return (EXIT_SUCCESS);
}
