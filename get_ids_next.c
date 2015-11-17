/*	##############################################################################################
	Advanced RTI System, ARTÌS			http://pads.cs.unibo.it
	Large Unstructured NEtwork Simulator (LUNES)

	Description:
		For a general introduction to LUNES implmentation please see the 
		file: mig-agents.c

		This an external tool used by the performance evaluation scripts.

		The goal of this tool is to collect all the message IDs that are
		in the trace files produced by the simulator.

	Authors:
		First version by Gabriele D'Angelo <g.dangelo@unibo.it>

	############################################################################################### 

	TODO:

		-	This version is preliminary, it requires an extensive amount of 
			polishing and much more comments
		-	Some parts can be highly optimized

*/

/*
	Input arguments and their semantic:

		argv[1]		Directory where to find the trace files (input)
		argv[2]		Filename of the message IDs file (output)
		argv[3]		Filename of the senders file (output)
		argv[4]		Total number of LPs in each simulaton run (input)
*/
#include <assert.h>
#include <glib.h>
#include <ini.h>
#include <math.h>
#include <rnd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <ts.h>
#include <unistd.h>
#include <glib.h>
#include <values.h>

/*	Hash table used for fast indexing of messages */
GHashTable*	hash_table_senders;
GHashTable*	hash_table_ids;

/*	File handlers */
char*	messages_dir;
FILE*	f_messages_file;

char*	ids_file;
FILE*	f_ids_file;

char*	senders_file;
FILE*	f_senders_file;

int	LPs;


int main(int argc, char *argv[]) {

	/*	Temporary variables */
	char* key;
	int* return_value;
	char buffer[1024];
	int finished = 0;

	char *value;

	char command[10];
	char sender[11];
	char messageid[11];

	int current_lp = 0;


	char messages_file[1024];
	
	/*	Command-line parameters */
	messages_dir = argv[1];
	ids_file = argv[2];
	senders_file = argv[3];
	LPs = atoi(argv[4]);

	/*	Hash table initialization */
//	printf("\nCreating and populating the hash table\n");
	hash_table_ids = g_hash_table_new(g_str_hash, g_str_equal);
	hash_table_senders = g_hash_table_new(g_str_hash, g_str_equal);

	f_ids_file = fopen(ids_file, "w");
	f_senders_file = fopen(senders_file, "w");


	while (current_lp < LPs) {

		sprintf(messages_file, "%s/SIM_TRACE_%03d.log", messages_dir, current_lp);
		printf("get_ids_next: processing... %s\n", messages_file);
		
		f_messages_file = fopen64(messages_file, "r");

		finished = 0;
		while (!finished) {

			if (fgets(buffer, 1024, f_messages_file) != NULL ) {

				memcpy(command, buffer, 1);
				command[1]='\0';

				if (strncmp(command, "G", 1) == 0) {

					memcpy(messageid, buffer+2, 10);
					messageid[10]='\0';
						
					key = calloc(10, sizeof(char));
					memcpy(key, messageid, 10);

		                        return_value = g_hash_table_lookup (hash_table_ids, key);
				                                
	                                if (return_value == NULL) {

						value = malloc(sizeof(char));
						*value = 0;

	//					printf("...inserting value %010d with key %s\n", *value, key);
						g_hash_table_insert (hash_table_ids, key, value);
						fprintf(f_ids_file, "%s\n", key);
					}
				}
						
				if (strncmp(command, "R", 1) == 0) {

					memcpy(sender, buffer+2, 10);
					sender[10]='\0';

					key = calloc(10, sizeof(char));
					memcpy(key, sender, 10);

		                        return_value = g_hash_table_lookup (hash_table_senders, key);
				                                
	                                if (return_value == NULL) {

						value = malloc(sizeof(char));
						*value = 0;

	//					printf("...inserting value %010d with key %s\n", *value, key);
						g_hash_table_insert (hash_table_senders, key, value);
						fprintf(f_senders_file, "%s\n", key);
					}
				}		
			}
			else 
				finished = 1;
		}

		current_lp++;
		fclose(f_messages_file);
	}

	fclose(f_ids_file);
	fclose(f_senders_file);

	fflush(NULL);
	return(0);

}

