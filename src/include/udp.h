/** @file udp.h
 *  @brief Macro and structure definitions for the UDP implementation
 *         of TCP file transfer.
 *
 *  @author Vicky Chen (chen-vv)
 *  @author Eric Omielan (eomielan)
 *  @bug No known bugs.
 */

#ifndef UDP_H
#define UDP_H

#include <stdint.h> // For uint32_t

/**
 * @brief Represents the boolean value "false".
 *
 * This constant is used to represent the boolean value "false" in C programming.
 * It has a value of 0.
 */
#define FALSE 0

/**
 * @brief Represents the boolean value "true".
 *
 * This constant is used to represent the boolean value "true" in C programming.
 * It has a value of 1.
 */
#define TRUE 1

/**
 * @brief Size of the header structure in bytes.
 *
 * This constant represents the size of the header structure (struct Header) in bytes.
 */
#define HEADER_SIZE sizeof(struct Header)

/**
 * @brief Maximum buffer size in bytes.
 *
 * This constant defines the maximum size of the buffer used for packet data in bytes.
 */
#define MAX_BUFFER_SIZE 8192

/**
 * @brief Default timeout value in milliseconds.
 *
 * This constant represents the default timeout value used in network operations, measured in milliseconds.
 */
#define DEFAULT_TIMEOUT 100000

/**
 * @brief Maximum number of retries.
 *
 * This constant defines the maximum number of retry attempts allowed for network operations before
 * the file transfer is considered a failure.
 */
#define MAX_RETRIES 3

/**
 * @brief Size of the acknowledgment packet in bytes.
 *
 * This constant represents the size of the acknowledgment packet (ACK) in bytes.
 */
#define MAX_ACK_SIZE sizeof(uint32_t)

/**
 * @brief Default timeout value for SYN-ACK packets in milliseconds.
 *
 * This constant represents the default timeout value used for SYN-ACK packets in the three-way handshake process,
 * measured in milliseconds.
 */
#define SYN_ACK_DEFAULT_TIMEOUT_MILLISEC 100000

/**
 * @brief Maximum timeout value for SYN-ACK packets in milliseconds.
 *
 * This constant represents the maximum timeout value allowed for SYN-ACK packets in the three-way handshake process,
 * measured in milliseconds.
 */
#define SYN_ACK_MAX_TIMEOUT_MILLISEC 1600

/**
 * @brief Header structure for packet data.
 *
 * This structure represents the header used for packet data transmission.
 * It contains fields for the sequence number, message length, and a flag indicating if it's the last packet.
 *
 */
struct Header
{
    uint32_t sequenceNumber; /**< Sequence number of the packet. */
    uint32_t messageLength;  /**< Length of the message data in the packet. */
    u_char lastPacket;       /**< Flag indicating if it's the last packet (0 for false, 1 for true). */
};

/**
 * @brief SYN packet structure.
 *
 * This structure represents the SYN packet used in the three-way handshake process.
 * It contains the sequence number.
 */
struct Syn
{
    uint32_t sequenceNumber; /**< Sequence number of the SYN packet. */
};

/**
 * @brief ACK packet structure.
 *
 * This structure represents the ACK packet used in the communication protocol.
 * It contains the acknowledgment number.
 */
struct Ack
{
    uint32_t ackNumber; /**< Acknowledgment number of the ACK packet. */
};

/**
 * @brief SYN-ACK packet structure.
 *
 * This structure represents the SYN-ACK packet used in the three-way handshake process.
 * It contains both the sequence number and acknowledgment number.
 */
struct SynAck
{
    uint32_t sequenceNumber; /**< Sequence number of the SYN-ACK packet. */
    uint32_t ackNumber;      /**< Acknowledgment number of the SYN-ACK packet. */
};

#endif // UDP_H
