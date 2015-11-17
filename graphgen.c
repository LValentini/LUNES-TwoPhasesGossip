/*	##############################################################################################
	Advanced RTI System, ARTÌS			http://pads.cs.unibo.it
	Large Unstructured NEtwork Simulator (LUNES)

	Description:
		For a general introduction to LUNES implmentation please see the 
		file: mig-agents.c

		This an external tool used to build graphs that will be used
		in the simulator.

	Authors:
		First version by Gabriele D'Angelo <g.dangelo@unibo.it>

	############################################################################################### 

	TODO:

		-	Almost everything :-(

*/

#include <stdlib.h>
#include <igraph.h>


int main(int argc, char* argv[]) {
	igraph_t 		graph;
	igraph_integer_t	nodes;
	igraph_integer_t	diameter;
	igraph_bool_t		result;
	FILE			*output_dot;
	FILE			*fstatus;
	igraph_integer_t	max_diameter;
	igraph_integer_t	edges;
	igraph_integer_t	edges_per_node;

	if (argc != 5) {

		fprintf(stdout, "Syntax error:\n");
		fprintf(stdout, "USAGE: graphgen <# nodes> <edges> <output_file_name> <max_diameter>\n");
		fflush(stdout);
		exit (-1);
	}

	nodes = (igraph_integer_t) atoi(argv[1]);
	edges = (igraph_integer_t) atof(argv[2]);
	max_diameter = (igraph_integer_t) atoi(argv[4]);

	edges_per_node = (igraph_integer_t) edges / nodes;

	fprintf(stdout, "Generating a graph with %d vertices and %d edges (%d edges per node)\n", (int)nodes, (int)edges, (int)edges_per_node);
	fflush(stdout);

	while ( (int)result != 1) {

		printf(".");

		igraph_erdos_renyi_game(&graph, IGRAPH_ERDOS_RENYI_GNM, nodes, edges, IGRAPH_UNDIRECTED, IGRAPH_NO_LOOPS);
//		igraph_watts_strogatz_game(&graph, /*dim=*/ 1, /*size=*/ nodes, /*nei=*/ edges_per_node, /*p=*/ 0.1, /*loops=*/ 0, /*multiple=*/ 0);
//		igraph_k_regular_game(&graph, nodes, edges_per_node, 0, 0);

		igraph_is_connected(&graph, &result, 0);

		if ( (int)(result) == 0 ) {	
			// nothing to do
		}
		else {

			igraph_diameter(&graph, &diameter, 0, 0, 0, IGRAPH_UNDIRECTED, 1);

			if ( diameter > max_diameter ) {

				result = (igraph_integer_t) 0;
			}
		}
	}

	printf("\nConnected graph? (0/1): %d\n", (int) result);
	printf("Diameter of the graph: %d\n", (int) diameter);
	printf("Number of vertices in the graph: %d\n", (int) igraph_vcount(&graph));
	printf("Number of edges in the graph: %d\n", (int) igraph_ecount(&graph));


	output_dot = fopen(argv[3], "w");

	igraph_write_graph_dot(&graph, output_dot);

	fclose(output_dot);

	fstatus = fopen("status.txt", "w");

	fprintf(fstatus, "Diameter of the graph: %d\n", (int) diameter);

	fclose(output_dot);

	igraph_destroy(&graph);

	return 0;
}

