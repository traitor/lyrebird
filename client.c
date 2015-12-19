/*
 * client.c
 *
 * Connects and communicates with the server to retrieve files to decrypt. 
 * Children of the client perform decryption, sending status updates which are 
 * forwarded to the server.
 * 
 * James Shephard
 * CMPT 300 - D100 Burnaby
 * Instructor Brian Booth
 * TA Scott Kristjanson
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "child.h"
#include "common.h"
#include "memwatch.h"

//Stores the pipes for each child
pc_pipe* children;
//Socket file descriptor of connection with server
int sockfd;
//Total number of children
int number;

/*
 * initialize
 *
 * Initializes the socket connection with the server.
 *
 * argv: Parameters passed into the program
 *
 * returns: False if an error has occurred
*/
bool initialize(char* argv[])
{
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		logmessage(NULL, "Unable to create socket. Process ID #%i Exiting.", 
			getpid());

		return false;
	}

	//Parse host IP address
	struct sockaddr_in serv_addr;
	struct in_addr addr;
	if (inet_pton(AF_INET, argv[1], &addr) == 0)
	{
		logmessage(NULL, "'%s' is not a valid IP address. Process ID #%i Exiting.", 
			argv[1], getpid());

		return false;
	}

	//Parse port number
	char* endptr;
	int port = strtol(argv[2], &endptr, 10);
	if (*endptr != '\0' || port < 1 || port > 65535)
	{
		logmessage(NULL, "'%s' is not a valid port number. Process ID #%i Exiting.", 
			argv[2], getpid());

		return false;
	}

	//Set connection parameters
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr = addr;
	serv_addr.sin_port = htons(port);

	if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
	{
		logmessage(NULL, "Unable to connect to server. Process ID #%i Exiting.", 
			getpid());

		return false;
	}

	//Retrieve IP address & port.
	struct sockaddr_in cli_addr;
	socklen_t clilen = sizeof(cli_addr);
	if (getsockname(sockfd, (struct sockaddr*) &cli_addr, &clilen) == -1)
	{
		logmessage(NULL, "Unable to retrieve socket name. Process ID #%i Exiting.", 
			getpid());

		close(sockfd);
		return false;
	}

	logmessage(NULL, "lyrebird.client: PID %i connected to server %s on port %i.", 
			getpid(), inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

	return true;
}

/*
 * create_children
 *
 * Creates children and pipes to communicate with them. Given that the execution
 * can result in either parent or child returning, the return values are 
 * slightly complicated.
 * 
 * returns: 
 *           -3 - Malloc failure
 *           -2 - Child creation failed
 *           -1 - Parent process exiting function
 * EXIT_SUCCESS - Child process successfully exited
 * EXIT_FAILURE - Child process failed
*/
int create_children()
{
	//Create our child processes
	number = get_nprocs() - 1;
	//Spawning 0 children would make no sense.
	if (number == 0)
		number = 1;

	children = (pc_pipe*)malloc(number * sizeof(pc_pipe));

	if (children == NULL)
	{
		logmessage(NULL, "Memory allocation failed. Process ID #%i Exiting.", 
					getpid());
		return -3;
	}

	for (int i = 0; i < number; i++)
	{
		pc_pipe connection;

		if (pipe(connection.parent) == -1 || pipe(connection.child) == -1)
		{
			logmessage(NULL, "Unable to create pipes. Process ID #%i will exit after existing children terminate.", 
					getpid());

			number = i; //Only i-1 children created.
			return -2;
		}

		int pid = fork();
		if (pid > 0)
		{
			close(connection.parent[1]);
			close(connection.child[0]);

			connection.pid = pid;
			connection.ready = true;
			connection.terminated = false;
			children[i] = connection;
		}
		else if (pid == 0)
		{
			//Close the other pipe's file descriptors (since we have a copy of them)
			for (int j = 0; j < i; j++)
			{
				close(children[j].parent[0]);
				close(children[j].child[1]);
			}

			close(connection.parent[0]);
			close(connection.child[1]);
			close(sockfd);

			free(children);

			//Run the child process 'main' function
			return child_process(connection);
		}
		else
		{
			logmessage(NULL, "Unable to fork process. Process ID #%i will exit after existing children terminate.", 
					getpid());

			number = i; //Only i-1 children created.
			return -2;
		}
	}

	return -1;
}

/*
 * check_children
 *
 * Checks if there is any data on a child pipe, and if so reads and forwards any
 * messages to the server
 *
 * returns: False if a child has terminated or error has occurred
*/
bool check_children()
{
	fd_set set;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 1000;
	FD_ZERO(&set);

	int maxfd = 0;
	for (int i = 0; i < number; i++)
	{
		FD_SET(children[i].parent[0], &set);
		maxfd = children[i].parent[0] > maxfd ? children[i].parent[0] : maxfd;
	}
	int val = select(maxfd + 1, &set, NULL, NULL, &tv); //no timeout curr

	if (val == 0) //No messages
		return true;
	if (val < 0) //select failed
		return false;
	
	for (int i = 0; i < number; i++)
	{
		if (FD_ISSET(children[i].parent[0], &set))
		{
			char buffer[MAX_MESSAGE_LENGTH];
			int nbytes = 0;

			nbytes = read(children[i].parent[0], buffer, MAX_MESSAGE_LENGTH);

			if (nbytes == 0)
			{
				children[i].terminated = true;
				return false;
			}
			else
			{
				//Forward message to server
				write(sockfd, buffer, nbytes);
				children[i].ready = true;
			}
		}
	}

	return true;
}

/*
 * close_children
 *
 * Tell all children to terminate, and read off any remaining messages on the
 * pipe. Ensure all have successfully terminated, outputting messages for 
 * children that did not.
*/
void close_children()
{
	//Close the pipes up and read any remaining messages.
	for (int i = 0; i < number; i++)
	{
		if (children[i].terminated)
			continue;

		//Close our end of child pipe
		close(children[i].child[1]);

		//Read and forward any remaining messages off of pipe
		char buffer[MAX_CONFIG_FILE_LINE];
		int nbytes = 0;
		while ((nbytes = read(children[i].parent[0], buffer, sizeof(buffer))) > 0)
			write(sockfd, buffer, nbytes);
	}

	//Ensure all children have successfully terminated
	for (int i = 0; i < number; i++)
	{
		int status = 0;

		while (0 < waitpid(children[i].pid, &status, 0));

		if (WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS)
			logmessage(NULL, "Child process ID #%i did not terminate successfully.", 
				children[i].pid);
	}
}

/*
 * fcfs_scheduler
 *
 * First Come First Serve scheduler.
 * Will wait until a child is ready to decrypt, then send the file to the first
 * available one.
 *
 * line: Line specifying input and output file
 *
 * returns: False if a child has terminated
*/
bool fcfs_scheduler(char* line)
{
	bool decrypting = false;
	while (!decrypting)
	{
		//Must constantly check if a child is ready.
		if (!check_children())
			return false;

		for (int i = 0; i < number; i++)
		{
			if (children[i].ready)
			{
				write(children[i].child[1], line, strlen(line));
				children[i].ready = false; // Busy!
				decrypting = true;
				break;
			}
		}
	}
	return true;
}

int main(int argc, char **argv)
{
	//True if the socket prematurely closes
	bool socket_error = false;
	//Store messages from server
	char buffer[MAX_MESSAGE_LENGTH];

	if (argc < 3)
	{
		logmessage(NULL, "Insufficient arguments specified. Please specify the IP address and port number. Process ID #%i Exiting.", 
			getpid());

		return EXIT_FAILURE;
	}

	//Initialize our server socket
	if (!initialize(argv))
		return EXIT_FAILURE;

	//Create children. See the function description for full meaning of return
	//values.
 	int result = create_children();
 	if (result == -3)
 		return EXIT_FAILURE; //No children were created, can safely exit.
 	else if (result >= 0)
 		return result; //Child process exiting
	else if (result == -1)
	{
		//Parent process, no critical error has occurred so can continue with
		//decryption of files

		char status = 0;
		fd_set set;
		struct timeval tv;
		int val;

		while (1)
		{
			tv.tv_sec = 0;
			tv.tv_usec = 1000;
			FD_ZERO(&set);

			FD_SET(sockfd, &set);
			val = select(sockfd + 1, &set, NULL, NULL, &tv); //no timeout curr

			if (val > 0 && FD_ISSET(sockfd, &set))
			{
				//Read in the message
				if (read(sockfd, &status, 1) <= 0 || 
					readnullstring(sockfd, buffer, MAX_MESSAGE_LENGTH - 1) <= 0)
				{
					socket_error = true;
					logmessage(NULL, "Socket unexpectedly disconnected. Process ID #%i.", getpid());
					break;
				}
				if (status == M_EXIT)
					break;

				if (status == M_LINE)
				{
					if (!fcfs_scheduler(buffer))
					{
						logmessage(NULL, "Child unexpectedly disconnected. Process ID #%i.", getpid());
						break;
					}
				}
			}
			else if (val < 0)
			{
				socket_error = true;
				logmessage(NULL, "Select failed. Process ID #%i will exit after children terminate.", getpid());
				break;
			}

			//Check, read and forward any messages from children
			if (!check_children())
			{
				logmessage(NULL, "Child unexpectedly disconnected. Process ID #%i.", getpid());
				break;
			}
		}
	}
	//Fourth case: -2, failed to create a child. Simply continue execution here
	//and terminate remaining children.

	//Tell children to terminate
	close_children();

	if (!socket_error) //Send successful exit message
		sendmessage(sockfd, M_EXIT, "");

	logmessage(NULL, "lyrebird.client: PID %i completed its tasks and is exiting successfully.", 
		getpid());

	close(sockfd);
	free(children);

	return socket_error ? EXIT_FAILURE : EXIT_SUCCESS;
} 

