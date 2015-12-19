/*
 * decrypt.h
 *
 * Functions for quickly decrypting a given input.
 * 
 * James Shephard
 * CMPT 300 - D100 Burnaby
 * Instructor Brian Booth
 * TA Scott Kristjanson
 */

#ifndef _DECRYPT_H_
#define _DECRYPT_H_

#include <string.h>

/*
 * decrypt
 *
 * Decrypts the given encrypted string, then stores it at same location
 * 
 * encrypted_string: Location of encrypted string. Used for storing decrypted 
 *                   string.
 * 
 * returns:
 * 		   Length if no errors occur
 * 		   -1 if an error occurs
 *         -2 if malloc fails
*/
int decrypt(char* encrypted_string);

#endif 

