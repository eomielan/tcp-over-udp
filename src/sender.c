/** @file sender.c
 *  @brief A UDP sender program
 *
 *  This contains the code for the sender
 *  using the UDP file transfer protocol. The
 *  main function parses the command line arguments
 *  and then calls the rsend function to send
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
#include <netdb.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include <pthread.h>
#include <errno.h>
#include "include/udp.h"

/* -- Global Variables -- */

/**
 * @brief The sequence number used to keep track of the packets sent.
 *
 * This variable is used to keep track of the sequence number of the packets
 * that are sent to the receiver. The sequence number is used to ensure that
 * the packets are not duplicated or lost.
 */
int _sequenceNumber = 0;

/**
 * @brief Gets the size of a file.
 *
 * This function retrieves the size of the file specified by the given filename.
 *
 * @param filename The path to the file whose size is to be retrieved.
 * @return The size of the file in bytes if successful, or -1 if an error occurs.
 *
 * @note If the file size exceeds the maximum value representable by a long long,
 *       the behavior of this function is undefined.
 */
long long getFileSize(const char *filename)
{
    struct stat st;
    if (stat(filename, &st) == 0)
    {
        return st.st_size;
    }
    else
    {
        return -1;
    }
}

/**
 * @brief Establishes a connection with the receiver using the 3-way handshake process.
 *
 * Sends a SYN packet to the receiver and waits for a SYN-ACK packet. Upon receiving
 * the SYN-ACK packet, the function sends a single ACK packet to the receiver and
 * assumes the connection is established.
 *
 * This function sets a random value for the sequence number that will be sent to the
 * receiver. This is used to ensure that the sender and receiver are in sync with each other
 * and that the packets are not duplicated or lost. While the sequence number could always
 * be set to 0, this would make the protocol more susceptible to attacks and would not be
 * as robust as using a random sequence number.
 *
 * If a SYN-ACK packet is not received within a certain timeout, the SYN packet is resent.
 * Additionally, this function will initiate an exponential backoff mechanism and double
 * the timeout until a maximum threshold is reached.
 *
 * @param sockfd The socket file descriptor
 * @param addr The address of the receiver
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
        // Set timeout for SYN-ACK packet
        tv.tv_usec = timeout;

        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        {
            perror("timeout");
            exit(1);
        }

        struct Syn syn;

        // Initialize sequence number
        srand(time(NULL));
        syn.sequenceNumber = rand();
        _sequenceNumber = syn.sequenceNumber;

        // Send SYN packet and delay before checking for SYN-ACK
        sendto(sockfd, &syn, sizeof(struct Syn), 0, (struct sockaddr *)addr, addrlen);
        usleep(timeout);

        struct SynAck syn_ack;
        ssize_t recv_size = recvfrom(sockfd, &syn_ack, sizeof(struct SynAck), 0, (struct sockaddr *)addr, &addrlen);

        if (recv_size > 0 && recv_size == sizeof(struct SynAck))
        {
            // Send ACK packet
            struct Ack ack;
            ack.ackNumber = syn_ack.sequenceNumber + 1;

            sendto(sockfd, &ack, sizeof(struct Ack), 0, (struct sockaddr *)addr, addrlen);

            return;
        }

        else if (recv_size == -1)
        {
            // No response from receiver. Update timeout before resending SYN packet.
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

/**
 * @brief Checks for an ACK packet from the receiver.
 *
 * This function checks for an ACK packet from the receiver. If an ACK packet is received,
 * the function checks if the sequence number of the ACK packet matches the expected sequence
 * number. If the sequence number matches, the function updates the sequence number. If the
 * sequence number does not match, the function updates the timeout and retries the packet.
 *
 * @param addr The address of the receiver
 * @param sockfd The socket file descriptor
 * @param sequenceNumber The sequence number of the most recent packet that was sent
 * @param totalBytesSent The total number of bytes sent to the receiver
 * @param bytesSentThisTime The number of bytes most recently sent to the receiver
 * @param timeout The current timeout value, in milliseconds
 * @param retries The current number of retries attempted
 * @param tv The timeval struct that specifies the socket timeout
 * @return Void
 */
void checkAck(struct sockaddr_in addr, int sockfd, int *sequenceNumber,
              unsigned long long *totalBytesSent, int bytesSentThisTime, int *timeout,
              int *retries, struct timeval *tv)
{
    socklen_t addrlen = sizeof(addr);
    char ack[MAX_ACK_SIZE];

    int bytesReceived = recvfrom(sockfd, ack, MAX_ACK_SIZE, 0, (struct sockaddr *)&addr, &addrlen);
    if (bytesReceived < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            if (*retries < MAX_RETRIES)
            {
                *timeout *= 2;
                (*retries)++;

                return;
            }
            else
            {
                perror("max timeout reached");
                exit(1);
            }
        }
        else
        {
            perror("recvfrom");
            exit(1);
        }
    }

    uint32_t receivedSequenceNumber;
    memcpy(&receivedSequenceNumber, ack, MAX_ACK_SIZE);

    if (receivedSequenceNumber != *sequenceNumber)
    {
        *timeout *= 2;
        (*retries)++;
        return;
    }

    *totalBytesSent += (bytesSentThisTime - HEADER_SIZE);
    (*sequenceNumber)++;
    *retries = 0;
    *timeout = DEFAULT_TIMEOUT;
    tv->tv_usec = *timeout;
}

/** @brief Sends the first bytesToTransfer bytes of the file indicated by
 *         filename to the receiver at hostname:hostUDPport.
 *
 *  This function sends the file using the UDP (SOCK_DGRAM) file transfer protocol.
 *  The bytes are transferred correctly and efficiently, even if the network drops,
 *  duplicates, or reorders packets. See rrecv for the counterpart function.
 *
 *  @param hostname The name of the receiver host.
 *  @param hostUDPport The port number on the receiver host.
 *  @param filename The name of the file to transfer.
 *  @param bytesToTransfer The number of bytes to transfer.
 *  @return Void.
 *
 *  Sources:
 *  https://www.geeksforgeeks.org/socket-programming-cc/
 *  https://www.cs.cmu.edu/~srini/15-441/S10/lectures/r01-sockets.pdf
 *  https://stackoverflow.com/questions/13547721/udp-socket-set-timeout
 *  https://www.ibm.com/docs/en/zos/3.1.0?topic=functions-sendto-send-data-socket
 */
void rsend(char *hostname,
           unsigned short int hostUDPport,
           char *filename,
           unsigned long long int bytesToTransfer)
{
    // Initialize socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0)
    {
        perror("socket");
        exit(1);
    }

    struct hostent *host = gethostbyname(hostname);
    if (host == NULL)
    {
        perror("gethostbyname");
        exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(hostUDPport);
    memcpy(&addr.sin_addr.s_addr, host->h_addr, host->h_length);

    // Prepare file for reading
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        perror("fopen");
        exit(1);
    }

    if (getFileSize(filename) < bytesToTransfer)
    {
        bytesToTransfer = getFileSize(filename);
    }

    // Establish connection with receiver prior to sending packets
    establish_connection(sockfd, &addr, sizeof(addr));

    unsigned long long totalBytesSent = 0;
    char packet_data[MAX_BUFFER_SIZE];
    char packet[MAX_BUFFER_SIZE + HEADER_SIZE];
    struct Header header;

    struct timeval tv;
    int timeout = DEFAULT_TIMEOUT;
    int retries = 0;
    tv.tv_sec = 0;
    tv.tv_usec = timeout;

    while (totalBytesSent < bytesToTransfer)
    {
        tv.tv_usec = timeout;

        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        {
            perror("timeout");
            exit(1);
        }

        if (retries == 0)
        {
            int bytesRead = fread(packet_data, 1, sizeof(packet_data), file);
            if (bytesRead < 0)
            {
                perror("fread");
                exit(1);
            }

            memcpy(packet + HEADER_SIZE, packet_data, bytesRead);

            header.sequenceNumber = _sequenceNumber;
            header.messageLength = bytesRead;
            if (totalBytesSent + MAX_BUFFER_SIZE > bytesToTransfer)
            {
                header.lastPacket = TRUE;
            }
            else
            {
                header.lastPacket = FALSE;
            }

            memcpy(packet, &header, HEADER_SIZE);
        }

        int bytesSentThisTime = sendto(sockfd, packet, HEADER_SIZE + MAX_BUFFER_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr));
        if (bytesSentThisTime < 0)
        {
            perror("sendto");
            exit(1);
        }

        checkAck(addr, sockfd, &_sequenceNumber, &totalBytesSent, bytesSentThisTime, &timeout, &retries, &tv);
    }

    fclose(file);
    close(sockfd);
}

/** @brief UDP sender entrypoint.
 *
 *  Parses the command line arguments and calls the rsend function to send
 *  the file.
 *
 * @return Should not return
 */
int main(int argc, char **argv)
{
    int hostUDPport;
    unsigned long long int bytesToTransfer;
    char *hostname = NULL;
    char *filename = NULL;

    if (argc != 5)
    {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }

    hostname = argv[1];
    hostUDPport = (unsigned short int)atoi(argv[2]);
    filename = argv[3];
    bytesToTransfer = atoll(argv[4]);

    rsend(hostname, hostUDPport, filename, bytesToTransfer);

    return (EXIT_SUCCESS);
}