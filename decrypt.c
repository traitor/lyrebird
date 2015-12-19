/*
 * decrypt.c
 *
 * Functions for quickly decrypting a given input.
 * 
 * James Shephard
 * CMPT 300 - D100 Burnaby
 * Instructor Brian Booth
 * TA Scott Kristjanson
 */

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include "decrypt.h"
#include "memwatch.h"

//this table will map ascii values to the corresponding 'numeric' values
//as defined in the assignment's table
char conversion_table[255];

//Performs the inverse of the above, taking one of the 41 given characters 
//and converting it into corresponding ascii
char inversion_table[255];

//Don't reinitialize tables every time
bool tables_initialized = false;

/*
 * initialize_table
 *
 * Initializes our lookup and inverse lookup tables
*/
void initialize_table()
{
	int i;
	for (i = 0; i < 255; i++)
		conversion_table[i] = -1; //default 'error' value.

	//ASCII characters to our base 41 encoding
	conversion_table[' '] = 0;
	for (i = 'a'; i <= 'z'; i++)
		conversion_table[i] = i - 'a' + 1; //goes 1 thru 26
	conversion_table['#'] = 27;
	conversion_table['.'] = 28;
	conversion_table[','] = 29;
	conversion_table['\''] = 30;
	conversion_table['!'] = 31;
	conversion_table['?'] = 32;
	conversion_table['('] = 33;
	conversion_table[')'] = 34;
	conversion_table['-'] = 35;
	conversion_table[':'] = 36;
	conversion_table['$'] = 37;
	conversion_table['/'] = 38;
	conversion_table['&'] = 39;
	conversion_table['\\'] = 40;

	//Set up the inverse table
	for (i = 0; i < 255; i++)
		if (conversion_table[i] != -1)
			inversion_table[conversion_table[i]] = i;
}

/*
 * modular_exponentiation
 *
 * Quickly calculates num^n % mod
 * Source: http://homepages.math.uic.edu/~leon/cs-mcs401-s08/handouts/fastexp.pdf
 * 
 * returns: num^n % mod
*/
unsigned long long modular_exponentiation(unsigned int num, unsigned int n, unsigned int mod)
{
	unsigned long long x = num;
	unsigned long long  y = num;
	if (n & 1 == 0)
		y = 1;
	int np = n/2;
	while (np > 0)
	{
		x = (x * x) % mod;
		if (np & 1 == 1)
		{
			if (y == 1)
				y = x;
			else
				y= (y * x) % mod;
		}
		np /= 2;
	}
	return y;
}

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
int decrypt(char* encrypted_string)
{
	//Initialize our conversion arrays
	if (!tables_initialized)
	{
		initialize_table();
		tables_initialized = true;
	}

	int length = strlen(encrypted_string);
	//length when we factor out the excess 8th characters
	int true_length = strlen(encrypted_string) - (strlen(encrypted_string) / 8);

	//we use an array to temporarily store the decrypted string
	//in the event that we are unable to successfuly decrypt it
	//so that we don't destroy the original data
	char* intermediary = (char*)malloc(length);
	if (intermediary == NULL)
	{
		return -2;
	}

	for (int i = 0, j = 0; i < true_length; i++, j++)
	{
		//Skip every 8th character in encrypted_string
		if ((j + 1) % 8 == 0)
			j++;

		intermediary[i] = encrypted_string[j];
	}
	//Make sure remainder is clear.
	for (int i = true_length; i < length; i++)
		intermediary[i] = 0;

	//Loop through in groups of 6
	for (int i = 0; i < true_length; i += 6)
	{
		unsigned long long temp = 0;
		for (int k = 0; k < 6 && i + k < true_length; k++)
		{
			int value = conversion_table[intermediary[i + k]];
			if (value == -1)
			{
				free(intermediary);
				return -1; //Undefined character in the encrypted text
			}

			temp += value * pow (41, 5 - k);
		}

		//Step 3
		//M=C^d % n
		//d=1921821779
		//n=4294434817
		temp = modular_exponentiation(temp, 1921821779, 4294434817);

		for (int k = 0; k < 6 && i + k < true_length; k++)
		{

			int result = (temp / (long)pow(41, 5 - k)) % 41;
			intermediary[i + k] = inversion_table[result];
		}
	}

	//Copy our decrypted string to the given pointer
	memcpy(encrypted_string, intermediary, length);

	free(intermediary);

 	return strlen(encrypted_string);
} 
