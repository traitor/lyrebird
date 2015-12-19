/*
 * child.h
 *
 * A child of the client process. Communicates with the parent, performs 
 * decryption, and sends status updates.
 * 
 * James Shephard
 * CMPT 300 - D100 Burnaby
 * Instructor Brian Booth
 * TA Scott Kristjanson
 */

#ifndef _CHILD_H_
#define _CHILD_H_

#include "common.h"

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
int child_process(pc_pipe connection);

#endif