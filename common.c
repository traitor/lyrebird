/*
 * common.c
 *
 * Functions commonly used by the server, client and children.
 * 
 * James Shephard
 * CMPT 300 - D100 Burnaby
 * Instructor Brian Booth
 * TA Scott Kristjanson
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "common.h"

//Stores the datetime when gettime is called
char current_time[30];

/*
 * gettime
 *
 * Retrieves the current time 
 * 
 * returns: Pointer to string containing current time
*/
char* gettime()
{
	time_t currtime;
	time(&currtime);
	strftime(current_time, 30, "%c", localtime(&currtime));
	return current_time;
}

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
void sendmessage(int fd, char status, char* line, ...)
{
	//Format of a message:
	//1 byte - message type (success, error, etc.)
	//n bytes - null terminated string
	char buffer[MAX_MESSAGE_LENGTH];
	buffer[0] = status;

	va_list vl;
	va_start(vl, line);
	int length = vsnprintf(buffer + 1, MAX_MESSAGE_LENGTH - 1, line, vl);
	va_end(vl);
	if (length < 0)
		return;
	
	write(fd, buffer, length + 2);
}

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
int readnullstring(int fd, char* buffer, int max)
{
	int i = 0;
	while (i < max)
	{
		if (read(fd, buffer + i, 1) <= 0)
			return -1;
		i++;
		if (buffer[i - 1] == '\0')
			break;
	}
	return i;
}

/*
 * logmessage
 *
 * Logs a message to both STDOUT and the given file
 * 
 * file:      File to write to
 * line, ...: See sprintf
*/
void logmessage(FILE * file, char* line, ...)
{
	char buf[MAX_CONFIG_FILE_LINE + 255];
	va_list vl;
	va_start(vl, line);
	vsnprintf(buf, sizeof(buf), line, vl);
	va_end(vl);
	printf("[%s] %s\n", gettime(), buf);

	if (file != NULL)
	{
		fprintf(file, "[%s] %s\n", gettime(), buf);
		fflush(file); // Ensure whatever it is gets properly written.
	}
}