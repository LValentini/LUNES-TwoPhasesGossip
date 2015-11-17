/*	##############################################################################################
	Advanced RTI System, ARTÌS			http://pads.cs.unibo.it
	Large Unstructured NEtwork Simulator (LUNES)

	Description:
		For a general introduction to LUNES implmentation please see the 
		file: mig-agents.c

		This an external tool used by the performance evaluation scripts.

		The goal of this tool is to analyze the collected data and to find
		the coverage and the delay of each delivered message with respect to
		each node in the simulated graph.

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

		argv[1]		Total number of nodes
		argv[2]		Total number of messages
		argv[3]		Filename of the messages file (input)
		argv[4]		Directory of the log (trace) files (input)
		argv[5]		File name of the coverage file (output)
		argv[6]		File name of the delay file (output)
		argv[7]		File name of the nodes and degree file (input)
		argv[8]		File name of the degree distribution file (input)
		argv[9]		File name of the average missing messages per degree file (output)
		argv[10]	Total number of LPs in this run (input)
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
GHashTable*		hash_table;

/*	Total number of nodes and messages */
int			nodes;
int			messages;

char			c_node[11];
char			c_message[11];
char			c_delay[4];

unsigned short		i_delay;

/*	File handlers */
char*			messages_file;
FILE*			f_messages_file;
char*			log_dir;
char			log_file[1024];
FILE*			f_log_file;
char*			coverage_file;
FILE*			f_coverage_file;
char*			delay_file;
FILE*			f_delay_file;
char*			average_missing_file;
FILE*			f_average_missing_file;

/*	Tables used for statistics */
unsigned char*		bigtable;		/* Coverage and delay calculation */

/*	Variables used for statistics */
float			coverage;
float			delay;

int			LPs;

/*
	TODO
*/
void compute_coverage_and_delay() {

	unsigned long	tmp;
	unsigned long	reached_pairs = 0;
	unsigned long	total_pairs;
	unsigned long	sum_delays = 0;


	total_pairs = nodes * messages;
	for ( tmp = 0; tmp < total_pairs; tmp++ ) {

		if ( bigtable[tmp] < UCHAR_MAX ) {
	
			sum_delays += bigtable[tmp];
			reached_pairs++;
		}
	}

	coverage = (float) (reached_pairs * 100) / total_pairs;
	delay = (float) sum_delays / reached_pairs;
}


/*
	TODO
*/
int main(int argc, char *argv[]) {

	/*	Temporary variables */
	char* key;
	int* value;
	char buffer[1024];
	int finished = 0;
	int counter = 0;
	int tmp;
	int index;

        char command[10];

	int current_lp = 0;


	/*	Command-line parameters */
	nodes = atoi(argv[1]);
	messages = atoi(argv[2]);
	messages_file = argv[3];
	log_dir = argv[4];
	coverage_file = argv[5];
	delay_file = argv[6];
	average_missing_file = argv[7];
	LPs = atoi(argv[8]);

//	printf("GET_COVERAGE_NEXT: starting\n");
//	printf("Number of nodes: %d\n", nodes);
//	printf("Number of messages: %d\n", messages);
//	printf("Messages file: %s\n", messages_file);
//	printf("Log file: %s\n", log_file);
//	printf("Coverage file (output): %s\n", coverage_file);
//	printf("Delay file (output): %s\n", delay_file);


	/*	Hash table initialization */
//	printf("\nCreating and populating the hash table\n");
	hash_table = g_hash_table_new(g_str_hash, g_str_equal);

	f_messages_file = fopen(messages_file, "r");

	while (!finished) {

		if (fgets(buffer, 1024, f_messages_file) != NULL ) {

			key = calloc(10, sizeof(char));
			memcpy(key, buffer, 10);

			value = malloc(sizeof(int));
			*value = counter;

			g_hash_table_insert (hash_table, key, value);

			counter++;
		}
		else 
			finished = 1;
	}

	fclose(f_messages_file);

	bigtable = calloc((messages)*(nodes), sizeof(int));

	if (bigtable == NULL) {
		
		printf("GET_COVERAGE_NEXT: not enough free memory\n");
		exit(0);
	}

	for (tmp = 0; tmp < (messages)*(nodes); tmp++) {

		bigtable[tmp] = UCHAR_MAX;
	}
	
	while (current_lp < LPs) {

		sprintf(log_file, "%s/SIM_TRACE_%03d.log", log_dir, current_lp);
		printf("get_coverage_next: processing... %s\n", log_file);

		f_log_file = fopen64(log_file, "r");

		finished = 0;
		while (!finished) {

			if (fgets(buffer, 1024, f_log_file) != NULL ) {

				memcpy(command, buffer, 1);
				command[1]='\0';
                                          
				if (strncmp(command, "R", 1) == 0) {

					memcpy(c_node, &(buffer[2]), 10);
					c_node[10]='\0';

					memcpy(c_message, &(buffer[13]), 10);
					c_message[10]='\0';

					value = g_hash_table_lookup (hash_table, c_message);

					if (value == NULL) {
						printf("GET_COVERAGE_NEXT: message identifier: %s NOT FOUND in the hash table\n", c_message);
						exit(0);
					}

					memcpy(c_delay, &(buffer[24]), 3);
					c_delay[3]='\0';
					i_delay = atoi(c_delay);

					if (i_delay >= UCHAR_MAX) {
						printf("GET_COVERAGE_NEXT: message TTL: %d >= %d\n", i_delay, UCHAR_MAX);
						exit(0);
					}

					index = (*value) * nodes + atoi(c_node);

					if (i_delay < bigtable[index]) {

						bigtable[index] = i_delay;
					}
				}	
			}
			else	{

				finished = 1;
			}
		}

		current_lp++;
		fclose(f_log_file);
	}

	compute_coverage_and_delay();

	f_coverage_file = fopen(coverage_file, "a");
	fprintf(f_coverage_file, "%.2f\n", coverage);
	fclose(f_coverage_file);

	f_delay_file = fopen(delay_file, "a");
	fprintf(f_delay_file, "%.2f\n", delay);
	fclose(f_delay_file);

	return(0);

}

