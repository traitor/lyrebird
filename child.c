/*
 * child.c
 *
 * A child of the client process. Communicates with the parent, performs 
 * decryption, and sends status updates.
 * 
 * James Shephard
 * CMPT 300 - D100 Burnaby
 * Instructor Brian Booth
 * TA Scott Kristjanson
 */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "child.h"
#include "common.h"
#include "decrypt.h"
#include "memwatch.h"

/*
 * decrypt_file
 *
 * Decrypt the given file and save the decrypted contents to the specified
 * output file.
 *
 * file_in:  Encrypted input file
 * file_out: Decrypted output file
 *
 * returns:
 *         0 - Successfully decrypted file
 *         1 - Unable to open input file
 *         2 - Unable to open output file
 *         3 - Invalid characters in input file
 *         4 - Malloc failure
*/
int decrypt_file(char* file_in, char* file_out)
{
	FILE* encrypted;
	FILE* decrypted;

	encrypted = fopen(file_in, "r");
	if (encrypted == NULL)
		return 1;

	decrypted = fopen(file_out, "w");
	if (decrypted == NULL)
	{
		fclose(encrypted);
		return 2;
	}

	char tweet[MAX_TWEET_LENGTH];
	while (fgets(tweet, MAX_TWEET_LENGTH, encrypted) != NULL)
	{
		//Remove the newline if there is one
		int hasnewline = tweet[strlen(tweet) - 1] == '\n';
		if (hasnewline)
			tweet[strlen(tweet) - 1] = 0;

		int result = decrypt(tweet);
		if (result == -1)
		{
			fclose(encrypted);
			fclose(decrypted);
			return 3;
		}
		else if (result == -2)
		{
			fclose(encrypted);
			fclose(decrypted);
			return 4;
		}

		fwrite(tweet, sizeof(char), strlen(tweet), decrypted);
		if (hasnewline)
			fputc('\n', decrypted); //add our removed newline
	}

	fclose(encrypted);
	fclose(decrypted);

	return 0;
}

/*
 * child_process
 *
 * The 'main' function for the child process. It utilizes the specified pipes to
 * communicate to the parent to receive files to decrypt and send status updates.
 *
 * connection: to communicate with parent
 *
 * returns: result of execution
*/
int child_process(pc_pipe connection)
{
	int result = 0;
	char buffer[65536]; //Size of pipe's buffer, probably will not hit this in practice.
	char wbuffer[MAX_MESSAGE_LENGTH]; //For writing messages
	char input_file[MAX_LOCATION_LENGTH];
	char output_file[MAX_LOCATION_LENGTH];

	//Inform the server we are ready to receive a file
	sendmessage(connection.parent[1], M_READY, "");

	while (1)
	{
		int nbytes = read(connection.child[0], buffer, sizeof(buffer));
		if (nbytes == 0) //Terminating, pipe has been closed.
			break;
		//When no new line is specified, will read past bytes read. Prevent this
		//by setting this byte to 0 (null-term string).
		buffer[nbytes] = 0x00;

		int count = 0;
		int offset = 0;

		//read() reads in the entire buffer, so we may read in multiple lines from
		//the configuration file. We will need to offset for each line.
		while (sscanf(buffer + offset, "%s %s%n", input_file, output_file, &count) == 2)
		{
			//sscanf doesn't include the newline character in its count
			offset += count + 1;

			logmessage(NULL, "Process ID #%i will decrypt %s", getpid(), input_file);
			result = decrypt_file(input_file, output_file);

			switch (result)
			{
				case 0: //Successful decryption
					sendmessage(connection.parent[1], M_SUCCESS, "%s in process %i", input_file, getpid());
					logmessage(NULL, "Process ID #%i decrypted %s successfully.", getpid(), input_file);
					break;
				case 1: //Unable to open input file
					sprintf(wbuffer, "Unable to open file %s in process %i.", input_file, getpid());
					sendmessage(connection.parent[1], M_ERROR, wbuffer);
					logmessage(NULL, "%s", wbuffer);
					break;
				case 2: //Unable to open output file
					sprintf(wbuffer, "Unable to open file %s in process %i.", output_file, getpid());
					sendmessage(connection.parent[1], M_ERROR, wbuffer);
					logmessage(NULL, "%s", wbuffer);
					break;
				case 3: //Invalid file contents
					sprintf(wbuffer, "Invalid characters in %s. Process ID #%i.", input_file, getpid());
					sendmessage(connection.parent[1], M_ERROR, wbuffer);
					logmessage(NULL, "%s", wbuffer);
					break;
				case 4: //Malloc failure
					sprintf(wbuffer, "Malloc failed in process %i, process exiting", getpid());
					sendmessage(connection.parent[1], M_ERROR, wbuffer);
					logmessage(NULL, "%s", wbuffer);
					break;
			}

			if (offset >= nbytes) //Don't go over the number of bytes that've been read
				break;
		}
		if (result == 4)
			break; //Need to break out of this loop too.
	}

	close(connection.parent[1]);

	//Malloc failure is the only reason we terminate early
	return (result == 4) ? EXIT_FAILURE : EXIT_SUCCESS;
}