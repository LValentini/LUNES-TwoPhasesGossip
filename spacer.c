/*	##############################################################################################
	Advanced RTI System, ARTÌS			http://pads.cs.unibo.it
	Large Unstructured NEtwork Simulator (LUNES)

	Description:
		For a general introduction to LUNES implmentation please see the 
		file: mig-agents.c

		This an external tool used by the performance evaluation scripts.

		TODO		

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

		argv[1]		Filename of the file to process
		argv[2]		Step size
*/
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>
#include <values.h>

/*	File handlers */
FILE*	f_data_file;


int main(int argc, char *argv[]) {

	/*	Temporary variables */
	char 			buffer[1024];
	char* 			filename;
	int 			finished = 0;
	int			first = 1;
	int			emitted = 0;

	char 			string01[1024];
	char 			string02[1024];
	char 			string03[1024];
	char 			string04[1024];
	char 			string05[1024];
	char 			string06[1024];
	char 			string07[1024];
	float			overhead = 0;
	float			previous = 0;
	float			diff = 0;
	float			step_size;

	/*	Command-line parameters */
	filename = argv[1];
	step_size = atof(argv[2]);

	f_data_file = fopen(filename, "r");

	finished = 0;
	while (!finished) {

		if (fgets(buffer, 1024, f_data_file) != NULL ) {

			emitted = 0;

			sscanf(buffer, "%s\t%s\t%s\t%s\t%s\t%s\t%s", string01, string02, string03, string04, string05, string06, string07);

			overhead = atof(string05);

			if (first) {
				printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\n", string01, string02, string03, string04, string05, string06, string07);
				first = 0;
				previous = overhead;
				emitted = 1;
			} else {

				diff = overhead - previous;
				if (diff >= step_size) {
					printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\n", string01, string02, string03, string04, string05, string06, string07);
					previous = overhead;
					emitted = 1;
				}
			}

		} 
		else 	finished = 1;
	}

	if (!emitted)	printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\n", string01, string02, string03, string04, string05, string06, string07);

	fclose(f_data_file);

	fflush(NULL);
	return(0);

}

