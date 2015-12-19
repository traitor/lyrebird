/*
 * common.h
 *
 * Functions and structs commonly used by the server, client and children.
 * 
 * James Shephard
 * CMPT 300 - D100 Burnaby
 * Instructor Brian Booth
 * TA Scott Kristjanson
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

//Parent-child pipe struct
typedef struct {
	int parent[2];
	int child[2];
	int pid;
	bool ready;
	bool terminated;
} pc_pipe;

//Max length of an encrypted tweet
#define MAX_TWEET_LENGTH 165
//Max length of a specified file location
#define MAX_LOCATION_LENGTH 1024
//Each location + a space delimiter + new line character
#define MAX_CONFIG_FILE_LINE MAX_LOCATION_LENGTH * 2 + 2
//Max total message sent between the client and server
#define MAX_MESSAGE_LENGTH 3000

//Define status codes for messages to be sent between server and client
#define M_EXIT    0x10 //Tells client to exit, or server that client has successfully exited.
#define M_SUCCESS 0x15 //Successful decryption
#define M_ERROR   0x05 //Unsuccessful/error
#define M_READY   0x03 //Indicates a child is ready to receive file
#define M_LINE    0x01 //File to decrypt

/*
 * sendmessage
 *
 * Formats the string, like sprintf, and then sends it to the specified file 
 * descriptor along with the status code.
 * 
 * sockfd:   File descriptor to send to
 * status:   Status code of message
 * fmt, ...: See sprintf
*/
void sendmessage(int sockfd, char status, char* fmt, ...);

/*
 * readnullstring
 *
 * Reads a null-terminated string from a file descriptor and stores in buffer, 
 * limiting itself to read max bytes.
 * 
 * fd:     File descriptor to send to
 * buffer: Location to store string
 * max:    Maximum number of bytes to read
 *
 * returns:
 * 		   Length if no error occurs
 *         -1 - the socket has closed
*/
int readnullstring(int fd, char* buffer, int max);

/*
 * log
 *
 * Logs a message to both STDOUT and the given file
 * 
 * file:      File to write to
 * line, ...: See sprintf
*/
void logmessage(FILE * file, char* line, ...);

/*
 * gettime
 *
 * Retrieves the current time 
 * 
 * returns: Pointer to string containing current time
*/
char* gettime();

#endif