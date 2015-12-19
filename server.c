/*
 * server.c
 *
 * Sends files to decrypt to clients. Outputs messages to clients, handles 
 * errors, etc.
 * 
 * James Shephard
 * CMPT 300 - D100 Burnaby
 * Instructor Brian Booth
 * TA Scott Kristjanson
 */

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "common.h"

//The maximum number of clients that can connect
#define MAX_CLIENTS 4096

//Holds all important information about each client connected.
typedef struct {
	int sockfd;
	int ready;
	bool terminated;
	char ip[16];
} client;
int c_current = 0;

//Containts a list of all clients
client clients[MAX_CLIENTS];
//Used for reading/writing to clients
char buffer[MAX_MESSAGE_LENGTH];
FILE * config_file;
FILE * log_file;
//Server socket to accept clients from
int sockfd;

/*
 * getipaddress
 *
 * Determines an IP address that is not associated with a loopback adapter and 
 * is up. If no such address exists, this returns INADDR_ANY.
 * 
 * returns: An avaialble IP address
*/
unsigned long getipaddress()
{
	struct ifaddrs *addrs, *ifa;
    struct sockaddr_in *sockaddr;

    getifaddrs (&addrs);

    for (ifa = addrs; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr->sa_family != AF_INET)
        	continue;

        //Want a connected, non-loopback adapter
        if (!(ifa->ifa_flags & IFF_UP) ||
        	(ifa->ifa_flags & IFF_LOOPBACK))
        	continue;

	    sockaddr = (struct sockaddr_in *) ifa->ifa_addr;
	    if (sockaddr->sin_addr.s_addr == INADDR_ANY)
	    	continue; //Avoid loopback
		
		freeifaddrs(addrs);
		return sockaddr->sin_addr.s_addr;
    }

    freeifaddrs(addrs);

    //If we are unable to locate a proper interface, default to any
    //available interface
    return INADDR_ANY;
}

/*
 * initialize
 *
 * Initializes the server socket, opens the configuration and log files for
 * writing.
 *
 * argv: Parameters passed into the program
 * 
 * returns: False in an error occurs
*/
bool initialize(char* argv[])
{
	config_file = fopen(argv[1], "r");
	if (config_file == NULL)
	{
		logmessage(NULL, "Unable to open configuration file %s. Process ID #%i Exiting.", 
			argv[1], getpid());

		return false;
	}

	log_file = fopen(argv[2], "w");
	if (log_file == NULL)
	{
		logmessage(NULL, "Unable to open log file %s. Process ID #%i Exiting.", 
			argv[2], getpid());

		fclose(config_file);
		return false;
	}
	
	//Initialize server socket
	struct sockaddr_in serv_addr, cli_addr;
	int clilen = sizeof(cli_addr);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		logmessage(NULL, "Unable to create socket. Process ID #%i Exiting.", 
			getpid());

		fclose(config_file);
		fclose(log_file);
		return false;
	}
	

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = 0;
	serv_addr.sin_addr.s_addr = getipaddress();

	if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
	{
		logmessage(NULL, "Unable to bind socket to host %s. Process ID #%i Exiting.", 
			inet_ntoa(serv_addr.sin_addr), getpid());

		fclose(config_file);
		fclose(log_file);
		return false;
	}

	//Begin listening for clients
	if (listen(sockfd, 5) == -1)
	{
		logmessage(NULL, "Unable to listen on socket. Process ID #%i Exiting.", 
			getpid());

		fclose(config_file);
		fclose(log_file);
		return false;
	}

	//Retrieve the port number
	if (getsockname(sockfd, (struct sockaddr*) &cli_addr, &clilen) == -1)
	{
		logmessage(NULL, "Unable to retrieve socket name. Process ID #%i Exiting.", 
			getpid());

		fclose(config_file);
		fclose(log_file);
		return false;
	}

	logmessage(NULL, "lyrebird.server: PID %i on host %s, port %i", 
			getpid(), inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

	return true;
}

/*
 * acceptclient
 *
 * If a client is currently attempting to connect, accept and add to list of 
 * connected client structs.
 *
 * returns: false if an error occurs
*/
bool acceptclient()
{
	fd_set set;
	struct timeval tv;
	int val;

	FD_ZERO(&set);
	FD_SET(sockfd, &set);
	tv.tv_sec = 0;
	tv.tv_usec = 1000;
	val = select(sockfd + 1, &set, NULL, NULL, &tv);

	struct sockaddr_in cli_addr;
	socklen_t clilen = sizeof(cli_addr);

	if (val > 0)
	{
		int clientfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
		if (clientfd < 0)
			return false;

		if (c_current + 1 == MAX_CLIENTS)
		{
			//Cannot accept any more clients
			close(clientfd);
			return true;
		}

		//Create a client struct
		client c;
		c.sockfd = clientfd;
		c.ready = 0; //Client will tell how many are ready
		c.terminated = false;
		strcpy(c.ip, inet_ntoa(cli_addr.sin_addr));

		clients[c_current++] = c;

		logmessage(log_file, "Successfully connected to lyrebird client %s.", 
			inet_ntoa(cli_addr.sin_addr));
	}
	else if (val == -1)
		return false; //Select failed

	return true;
}

/*
 * readmessage
 *
 * Reads and parses a message from a client.
 *
 * i - index into clients array
 *
 * returns:
 *          status code if no errors occur
 *          0 - Socket is closed
 *         -1 - Socket has crashed
*/
int readmessage(int i)
{
	char status = 0;

	//Read in status code
	int nbytes = read(clients[i].sockfd, &status, 1);
	if (nbytes < 0)
		return -1; //Client crashed before finishing
	else if (nbytes == 0)
		return 0; //Socked closed

	nbytes = readnullstring(clients[i].sockfd, buffer, MAX_MESSAGE_LENGTH - 1);
	if (nbytes < 0)
		return -1; //Client crashed before finishing
	else if (nbytes == 0)
		return 0; //Socket closed

	
	if (status == M_SUCCESS)
		logmessage(log_file, "The lyrebird client %s has successfully decrypted %s.",
			clients[i].ip, buffer);
	else if (status == M_ERROR)
		logmessage(log_file, "The lyrebird client %s has encountered an error: %s",
			clients[i].ip, buffer);
	//Also: M_READY, simply for informing server of how many clients are available.
	clients[i].ready++;

	return status;
}

/*
 * updateclients
 *
 * Read any incoming messages from clients, updating the number of available 
 * children in each, outputting any messages received.
 *
 * returns: false if an error has occurred
*/
bool updateclients()
{
	fd_set set;
	struct timeval tv;
	int val;
	//Loop until every awaiting message has been received.
	bool update = true;
	while (update)
	{
		update = false;
		FD_ZERO(&set);

		int maxfd = 0;
		//Select all current clients
		for (int i = 0; i < c_current; i++)
		{
			FD_SET(clients[i].sockfd, &set);
			maxfd = clients[i].sockfd > maxfd ? clients[i].sockfd : maxfd;
		}
		tv.tv_sec = 0;
		tv.tv_usec = 1000;
		val = select(maxfd + 1, &set, NULL, NULL, &tv);

		if (val == -1)
			return false;
		else if (val == 0)
			return true; //No awaiting messages

		for (int i = 0; i < c_current; i++)
		{
			if (!FD_ISSET(clients[i].sockfd, &set))
				continue;

			// Read message from the client
			int status = readmessage(i);
			update = true;
			if (status <= 0)
			{
				logmessage(log_file, "The lyrebird client %s has disconnected unexpectedly.", clients[i].ip);
				clients[i].terminated = true;
				return false;
			}
		}
	}
	return true;
}

/*
 * closeclients
 *
 * Sends termination messages to all clients. Reads any remaining messages, and 
 * checks if they have successfully disconnected.
*/
void closeclients()
{
	//Ensure clients disconnect properly
	for (int i = 0; i < c_current; i++)
	{
		if (clients[i].terminated)
			continue;

		//Send exit message to client
		sendmessage(clients[i].sockfd, M_EXIT, "");

		int status = 0; //Status code of last message received
		while (true)
		{
			//Keep track of the last status sent
			//Last message should be M_EXIT if client exited properly
			int new_status = readmessage(i);
			if (new_status < 0)
			{
				//Socket crashed, thus an unexpected disconnect
				status = -1;
				break;
			}
			else if (new_status == 0)
				break; //Socket closed

			status = new_status;
		}

		if (status == M_EXIT)
			logmessage(log_file, "The lyrebird client %s has disconnected expectedly.",
				clients[i].ip);
		else
			logmessage(log_file, "The lyrebird client %s has disconnected unexpectedly.",
				clients[i].ip);

		close(clients[i].sockfd);
	}
}

int main(int argc, char* argv[])
{
	//Stores the current line read
	char line[MAX_CONFIG_FILE_LINE];
	//For ensuring the line is valid
	char input_file[MAX_LOCATION_LENGTH];
	char output_file[MAX_LOCATION_LENGTH];
	//Current line of the configuration file we are on
	int current_line = 1;
	//Keeps track if we're currently awaiting a client to decrypt a file
	bool line_waiting = false;

	if (argc < 3)
	{
		logmessage(NULL, "Insufficient arguments specified. Please specify the configuration file and log file. Process ID #%i Exiting.", getpid());
		return EXIT_FAILURE;
	}

	//Initialize our server socket and begin listening for connections
	if (!initialize(argv))
		return EXIT_FAILURE;

	while (true)
	{
		if (!acceptclient())
		{
			logmessage(NULL, "Failed to accept client. Process ID#%i Exiting.", getpid());
			break; //Select or accept has failed.
		}

		if (!line_waiting) //Only update if we aren't waiting for an available client
		{
			//Read in the next line
			while (fgets(line, MAX_CONFIG_FILE_LINE, config_file) != NULL)
			{
				if (sscanf(line, "%s %s", input_file, output_file) != 2)
				{
					if (line[0] != '\n')
					{
						//Invalid line, skip.
						logmessage(log_file, "Failed to read line %i in %s, skipping. Process ID #%i.", 
							current_line, argv[1], getpid());
					}
				}
				else
				{
					//Line has been successfully read, now wait for line to be sent to a client
					line_waiting = true;
					current_line++;
					break;
				}
			}
		}

		//No line read, EOF reached
		if (!line_waiting)
			break;

		//Update child readiness
		if (!updateclients())
			break;

		//Attempt to send a client a file to decrypt
		for (int i = 0; i < c_current; i++)
		{
			//Check if a client is available to decrypt a file
			if (clients[i].ready <= 0)
				continue;

			line_waiting = false;
			clients[i].ready--;
			
			//Ensure the line has a null-terminating character
			line[strlen(line) + 1] = 0;

			sendmessage(clients[i].sockfd, M_LINE, line);
			logmessage(log_file, "The lyrebird client %s has been given the task of decrypting %s.",
				clients[i].ip, input_file);

			break;
		}
	}

	//Tell clients to terminate and read any remaining messages
	closeclients();

	close(sockfd);
	fclose(config_file);
	fclose(log_file);

	logmessage(NULL, "lyrebird server: PID %i completed its tasks and is exiting successfully.", getpid());

	return EXIT_SUCCESS;
}