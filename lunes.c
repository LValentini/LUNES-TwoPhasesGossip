/*	##############################################################################################
	Advanced RTI System, ARTÌS			http://pads.cs.unibo.it
	Large Unstructured NEtwork Simulator (LUNES)

	Description:
		For a general introduction to LUNES implmentation please see the 
		file: mig-agents.c

	Authors:
		First version by Gabriele D'Angelo <g.dangelo@unibo.it>

	############################################################################################### */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <ctype.h>
#include <assert.h>
#include <ini.h>
#include <ts.h>
#include <rnd.h>
#include <gaia.h>
#include <rnd.h>
#include <values.h>
#include "utils.h"
#include "user_event_handlers.h"
#include "lunes.h"
#include "lunes_constants.h"
#include "entity_definition.h"


/* ************************************************************************ */
/* 		 L O C A L	V A R I A B L E S			    */
/* ************************************************************************ */

FILE		*fp_print_trace;		// File descriptor for simulation trace file
unsigned short	env_max_ttl = MAX_TTL;		// TTL of newly created messages


/* ************************************************************************ */
/* 			E X T E R N A L     V A R I A B L E S 	            */
/* ************************************************************************ */

extern hash_t		hash_table, *table;		/* Global hash table of simulated entities */
extern hash_t		sim_table, *stable;		/* Hash table of locally simulated entities */
extern double		simclock;			/* Time management, simulated time */
extern TSeed		Seed, *S;			/* Seed used for the random generator */
extern char		*TESTNAME;			/* Test name */
extern int	     	NSIMULATE;	 		/* Number of Interacting Agents (Simulated Entities) per LP */
extern int		NLP; 				/* Number of Logical Processes */
// Simulation control
extern unsigned short	env_dissemination_mode;		/* Dissemination mode */
extern float 		env_broadcast_prob_threshold;	/* Dissemination: conditional broadcast, probability threshold */
extern unsigned int	env_cache_size;			/* Cache size of each node */
extern float		env_fixed_prob_threshold;	/* Dissemination: fixed probability, probability threshold */
#ifdef DEGREE_DEPENDENT_GOSSIP_SUPPORT
extern unsigned int	env_probability_function;   	/* Probability function for Degree Dependent Gossip */
extern double		env_function_coefficient;   	/* Coefficient of the probability function */
#endif


/* ************************************************************************ */
/* 			S U P P O R T      F U N C T I O N S		    */
/* ************************************************************************ */

#ifdef ADAPTIVE_GOSSIP_SUPPORT
/*
  	Sends a stimulus to another SE, creating and sending a 'S' type message
	Usually called by lunes_stimuli()
 */
void	lunes_execute_stimulus (double ts, hash_node_t *src, hash_node_t *dest, int sender) {

	StimulusMsg		msg;			// Message
	unsigned int		message_size;		// Size


	// Defining the message type
	msg.stimulus_static.type = 'S';

	// Inserting the ID of the sender of which this node is missing messages
	msg.stimulus_static.missing_sender = sender;

	// To reduce the network overhead, only the used part of the message is really sent
	message_size = sizeof(struct _stimulus_static_part);

	// Buffer check
	if (message_size > BUFFER_SIZE) {

		fprintf(stdout, "%12.2f FATAL ERROR, the outgoing BUFFER_SIZE is not sufficient!\n", simclock);
		fflush(stdout);
		exit(-1);
	}

	// Real send
	GAIA_Send (src->data->key, dest->data->key, ts, (void *)&msg, message_size);
}
#endif


#ifdef ADAPTIVE_GOSSIP_SUPPORT
/*
	Verifies the reception rate of a given node with respect to all other nodes in the network.
	If this rate is below threshold then a stimuli is sent
 */
void lunes_stimuli (hash_node_t *node) {
	int		sender = 0;		// Checked node (sender)
	int		forwarder;		// Tmp, checked forwarder
	int 		max_forwarder;		// Best forwarder of the messages produced by a given node
	int		max_value;		// Max number of messages forwarded by a given forwarder
	int		tot_received;		// Total number of received messages (from all forwarders)
	float		theoretical_rate;	// Theoretical rate of reception of each message
	//
	unsigned int 	destination;		// Stimulus destination (node identifier)
	hash_node_t	*receiver;		// Stimulus destination (node in the hash table)
	//
	char		*dest_table;		// Table of all the possible receivers of stimuli

	
	// Table used to limit the number of stimuli sent to a given node
	dest_table = calloc(sizeof(char), table->size);

	// I've to check the reception rate from all nodes in the network
	while ( sender < table->size ) {

		// This node should not check for the receiving of messages produced by itself
		if ( sender != node->data->key) {

			// Looking for the "max forwarder" of a given message (identified by the producer ID)
			max_forwarder	= -1;
			max_value	= 0;
			tot_received	= 0;	// I've to calculate this value for each sender (producer ID)

			// Checking all forwarders, theoretically all the other nodes could be forwarders
			for ( forwarder = 0; forwarder < table->size; forwarder++) {

				// Updating the total number of received messages from a given sender
				tot_received += node->data->s_state.histable[sender][forwarder];

				// If there's a new max then update some cursors
				if ( node->data->s_state.histable[sender][forwarder] > max_value ) {

					max_value = node->data->s_state.histable[sender][forwarder];
					max_forwarder = forwarder;
				}
			}

			// What's the theoretical rate of reception of each produced message?
			theoretical_rate = (float)( ADAPTIVE_GOSSIP_EVALUATION_PERIOD / MEAN_NEW_MESSAGE );

			// I've to check if the reception rate is lower than the threshold
			if ( tot_received < theoretical_rate ) {

				#ifdef AG_DEBUG
				fprintf(stdout, "%12.2f node: [%5d], sender: [%5d], tot received [%5d] / theoretical_rate [%3.2f]\n", simclock, node->data->key, sender, tot_received, theoretical_rate);
				#endif

				if ( max_forwarder == -1 ) {
					// This node has missed all the messages from a given source and therefore
					// the stimulus destination is chosen at random from the neighbors
					destination = *(unsigned int *)hash_table_random_key(node->data->state);
				} else {
					// Some messages has been received and therefore the best forwarder is
					// destination of the stimulus
					destination = max_forwarder;
				}

				// In the ADAPTIVE_GOSSIP (alg. #1) dissemination it's possible to send only
				// one stimulus to a sender in evaluation session
				if ( dest_table[destination] == 0 ) {

					// No stimuli sent or another adaptive algorithm is used
					if ( env_dissemination_mode == ADAPTIVE_GOSSIP )	dest_table[destination] = 1;	// no more stimuli

					#ifdef AG_DEBUG
					fprintf(stdout, "%12.2f node: [%5d], the stimulus destination is [%5d]\n", simclock, node->data->key, destination);
					#endif

					// Validity check, the destination must be a neighbor!
					if (g_hash_table_lookup (node->data->state, &destination) == NULL) {

						fprintf(stdout, "%12.2f FATAL ERROR, the chosen destination node [%5d] for the stimulus is not a neighbor!!!\n", simclock, destination);
						fflush(stdout);
						exit(-1);
					}

					// Looking up the destination node in the global hashtable
					receiver = hash_lookup(table, destination);

					// Validity check, the destination must exist in the general hash table
					if ( receiver == NULL ) {

						fprintf(stdout, "%12.2f FATAL ERROR, the chosen destination node [%5d] does not exist in the global hashtable of simulated entities!!!\n", simclock, destination);
						fflush(stdout);
						exit(-1);
					}

					// Sending the real stimulus message
					lunes_execute_stimulus (simclock + FLIGHT_TIME, node, receiver, sender);
				}
			}
		}

		// Next sender to be checked
		sender++;
	}
}
#endif


#ifdef ADAPTIVE_GOSSIP_SUPPORT
/*
	Updates the table of received messages, used to check the reception rate
*/
void lunes_histable_update (hash_node_t *node, unsigned int sender_id, unsigned int forwarder_id) {

	// Overflow hack
	if ( node->data->s_state.histable[sender_id][forwarder_id] < 254 )
		node->data->s_state.histable[sender_id][forwarder_id]++;

	#ifdef DEBUG
	fprintf(stdout, "%12.2f node: [%5d] updating node adaptive gossip statistics, sender: [%5d], forwarder: [%5d]\n", simclock, node->data->key, sender_id, forwarder_id);
	#endif
}
#endif


/*
	Finds the oldest element in the cache, that is the one with the lowest value
	in the age field
*/
int	lunes_cache_find_oldest (CacheElement *cache) {

	int	position = -1;
	int	minimum = MAXINT;
	int	tmp;

	for (tmp = 0; tmp < env_cache_size; tmp++)
		if (cache[tmp].age < minimum) {
			minimum = cache[tmp].age;
			position = tmp;
		}

	return(position);
}


/*
	Inserts a new value in the local cache of a given node
*/
void lunes_cache_insert(CacheElement *cache, unsigned long value) {

	// Finds the oldest message in the cache
	int tmp = lunes_cache_find_oldest(cache);

	// Inserts the new message in the cache
	cache[tmp].element = value;
	cache[tmp].age = simclock;
}


/*
	Boolean, verifies if the received message is already in the local cache of the node
*/
int lunes_cache_verify(CacheElement *cache, unsigned long value) {

	int retvalue = 0;
	int tmp;


	for ( tmp = 0; tmp < env_cache_size; tmp++ )
		if ( cache[tmp].element == value ) {

			retvalue = 1;
			cache[tmp].age = simclock;

			break;
		}

	return(retvalue);
}


#ifdef DEGREE_DEPENDENT_GOSSIP_SUPPORT
/*
	Used to calculate the forwarding probability value for a given node
*/
double lunes_degdependent_prob(unsigned int deg)
{
	double prob = 0.0;
		
	switch (env_probability_function)
	{
		// Function 2
		case 2:
			prob = 1.0/log(env_function_coefficient * deg);
		break;

		// Function 1
		case 1:
			prob = 1.0/pow(deg, env_function_coefficient);
		break;

		default:
			fprintf(stdout, "%12.2f FATAL ERROR, function: %d does NOT exist!\n", simclock, env_probability_function);
			fflush(stdout);
			exit(-1);
		break;
	}

	/*
		The probability is defined in the range [0, 1], to get full dissemination some functions require that the negative
		results are treated as "true" (i.e. 1)
	*/
	if ( (prob < 0) || (prob > 1) )
		prob = 1;

	return prob;
}
#endif

/*
	Used to forward a received message to (some of) the neighbors of a given node
*/
void lunes_real_forward (hash_node_t *node, long value_to_send, unsigned short ttl, double timestamp, unsigned int creator, unsigned int forwarder) {

	// Iterator to scan the whole state hashtable of neighbors
	GHashTableIter		iter;
	gpointer		key, destination;
	//
	float			threshold;		// Tmp, used for probabilistic-based dissemination algorithms
	//
	hash_node_t		*sender, *receiver;	// Sender and receiver nodes in the global hashtable


	// Dissemination mode for the forwarded messages (dissemination algorithm)
	switch ( env_dissemination_mode ) {

		case BROADCAST:			// Probabilistic broadcast dissemination

			// The message is forwarded to ALL neighbors of this node
			// NOTE:	in case of probabilistic broadcast dissemination this function is called
			//		only if the probabilities evaluation was positive
			
			g_hash_table_iter_init (&iter, node->data->state);

			// All neighbors
			while (g_hash_table_iter_next (&iter, &key, &destination)) {

					sender = hash_lookup(stable, node->data->key);				// This node
					receiver = hash_lookup(table, *(unsigned int *)destination);		// The neighbor

					// The original forwarder of this message and its creator are exclueded 
					// from this dissemination
					if ( ( receiver->data->key != forwarder ) && ( receiver->data->key != creator) )
						execute_ping (simclock + FLIGHT_TIME, sender, receiver, ttl, value_to_send, timestamp, creator);
			}
		break;

		case GOSSIP_FIXED_PROB:		// Fixed probability dissemination

			// In this case, all neighbors will be analyzed but the message will be
			// forwarded only to some of them			

			g_hash_table_iter_init (&iter, node->data->state);

			// All neighbors
			while (g_hash_table_iter_next (&iter, &key, &destination)) {

				// Probabilistic evaluation
				threshold = RND_Interval (S, (double)0, (double)100);

				if ( threshold <= env_fixed_prob_threshold ) {

					sender = hash_lookup(stable, node->data->key);				// This node
					receiver = hash_lookup(table, *(unsigned int *)destination);		// The neighbor

					// The original forwarder of this message and its creator are exclueded 
					// from this dissemination
					if ( ( receiver->data->key != forwarder ) && ( receiver->data->key != creator) )
						execute_ping (simclock + FLIGHT_TIME, sender, receiver, ttl, value_to_send, timestamp, creator);
				}
			}
		break;

		#ifdef ADAPTIVE_GOSSIP_SUPPORT
		// Adaptive protocols
		case ADAPTIVE_GOSSIP:		// Adaptive dissemination, alg. #1 (adaptive node)
		case ADAPTIVE_GOSSIP_SENDER:	// Adaptive dissemination, alg. #2 (adaptive sender)
		case ADAPTIVE_GOSSIP_SPECIFIC:	// Adaptive dissemination, alg. #3 (adaptive specific)

			// In this case, all neighbors will be analyzed but the message will be
			// forwarded only to some of them

			g_hash_table_iter_init (&iter, node->data->state);

			// All neighbors
			while (g_hash_table_iter_next (&iter, &key, &destination)) {

				value_element	*value;
				float		adaptive_prob_threshold;
				unsigned int	keyval;
				int		residual_window;
				float		residual_stimulus;
				int		cursor;


				// In the adaptive gossip (alg. #1) the stimulus is associated to
				// the node and therefore only one stimulus can be concurrently
				// active in a given node.
				//
				// This is not true in the other gossip protocols (algs. #2 and #3),
				// in this case each node have to take care of many concurrent stimuli
				if ( env_dissemination_mode == ADAPTIVE_GOSSIP )
					cursor = 0;
				else	cursor = creator;


				// Numeric identificator of the (possible) destination node
				keyval = *(unsigned int *)destination;
				// Extra information linked to the (possible) destination node
				value = (value_element *) g_hash_table_lookup (node->data->state, &keyval);

				// If the stimulus timeout has expired (there is no more stimulus residual) or
				// if there has never been a stimulus before then the new dissemination probability
				// is the standard one (that is, the same of the fixed probability dissemination)
				if ( ( value->stim_timeout[cursor] <= simclock ) || ( value->stim_timeout[cursor] == 0 ) ) {

					adaptive_prob_threshold =  env_fixed_prob_threshold;

				} else {

					// If a stimulus is already active then I've to calculate what is the
					// residual stimulus and add it to the baseline dissemination probability

					residual_window = (int) ( value->stim_timeout[cursor] - simclock ); 

					residual_stimulus = ( value->stim_increment[cursor] * residual_window ) / STIMULUS_LENGTH;

					adaptive_prob_threshold =  env_fixed_prob_threshold + residual_stimulus;
				}

				#ifdef AG_DEBUG
				if ( adaptive_prob_threshold > env_fixed_prob_threshold )
					fprintf(stdout, "%12.2f node: [%5d] neighbor [%5d] with adaptive threshold: %3.2f, increment %3.2f, residual %3.2f\n", simclock, node->data->key, *(unsigned int *)destination, adaptive_prob_threshold, value->stim_increment[cursor], residual_stimulus);
				#endif

				// Probabilistic evaluation
				threshold = RND_Interval (S, (double)0, (double)100);

				if ( threshold <= adaptive_prob_threshold ) {

					sender = hash_lookup(stable, node->data->key);				// This node
					receiver = hash_lookup(table, *(unsigned int *)destination);		// The neighbor

					// The original forwarder of this message and its creator are exclueded 
					// from this dissemination
					if ( ( receiver->data->key != forwarder ) && ( receiver->data->key != creator) )
						execute_ping (simclock + FLIGHT_TIME, sender, receiver, ttl, value_to_send, timestamp, creator);
				}
			}
		break;
		#endif

		#ifdef DEGREE_DEPENDENT_GOSSIP_SUPPORT
		// Degree Dependent dissemination algorithm
		case DEGREE_DEPENDENT_GOSSIP: 

			g_hash_table_iter_init (&iter, node->data->state);

			// All neighbors
			while (g_hash_table_iter_next (&iter, &key, &destination)) 
			{
				sender = hash_lookup(stable, node->data->key);				// This node
				receiver = hash_lookup(table, *(unsigned int *)destination);		// The neighbor

				// The original forwarder of this message and its creator are excluded 
				// from this dissemination
				if ( ( receiver->data->key != forwarder ) && ( receiver->data->key != creator) )
				{
					// Probabilistic evaluation
					threshold = (RND_Interval (S, (double)0, (double)100)) / 100;

					// If the eligible recipient has less than 3 neighbors, its reception probability is 1. However,
					// if its value of num_neighbors is 0, it means that I don't know the dimension of 
					// that node's neighborhood, so the threshold is set to 1/n, being n 
					// the dimension of my neighborhood

					if (((value_element *)destination)->num_neighbors < 3)
					{
						// Note that, the startup phase (when the number of neighbors is not known) falls in
						// this case (num_neighbors = 0)
						// -> full dissemination
						execute_ping (simclock + FLIGHT_TIME, sender, receiver, ttl, value_to_send, timestamp, creator);
					}
					// Otherwise, the probability is evaluated according to the function defined by the
					// environment variable env_probability_function
					else
					{
						if (threshold <= lunes_degdependent_prob(((value_element *)destination)->num_neighbors))
							execute_ping (simclock + FLIGHT_TIME, sender, receiver, ttl, value_to_send, timestamp, creator);
					}
				}
			}

		break;
		#endif

		default:

			fprintf(stdout, "%12.2f FATAL ERROR, the dissemination mode [%2d] is NOT implemented in this version of LUNES!!!\n", simclock, env_dissemination_mode);
			fprintf(stdout, "%12.2f NOTE: all the adaptive protocols require compile time support: see the ADAPTIVE_GOSSIP_SUPPORT define in sim-parameters.h\n", simclock);
			fflush(stdout);
			exit(-1);
		break;	
	}
}


/*
	Dissemination protocol implementation:
	a new message has been received from a neighbor, it is necessary to forward it in some way
*/
void lunes_forward_to_neighbors (hash_node_t *node, long value_to_send, unsigned short ttl, double timestamp, unsigned int creator, unsigned int forwarder) {

	float	threshold;	// Tmp, probabilistic evaluation


	// Dissemination mode for the forwarded messages
	switch (env_dissemination_mode) {

		case BROADCAST:			// Probabilistic broadcast

			// Probabilistic evaluation
			threshold = RND_Interval (S, (double)0, (double)100);

			if ( threshold <= env_broadcast_prob_threshold )
				lunes_real_forward (node, value_to_send, ttl, timestamp, creator, forwarder);
		break;

		case GOSSIP_FIXED_PROB:		// Fixed probability dissemination

			lunes_real_forward (node, value_to_send, ttl, timestamp, creator, forwarder);
		break;

		#ifdef ADAPTIVE_GOSSIP_SUPPORT
		case ADAPTIVE_GOSSIP:		// Adaptive dissemination, alg. #1 (adaptive node)
		case ADAPTIVE_GOSSIP_SENDER:	// Adaptive dissemination, alg. #2 (adaptive sender)
		case ADAPTIVE_GOSSIP_SPECIFIC:	// Adaptive dissemination, alg. #3 (adaptive specific)

			lunes_real_forward (node, value_to_send, ttl, timestamp, creator, forwarder);
		break;
		#endif

		#ifdef DEGREE_DEPENDENT_GOSSIP_SUPPORT
		case DEGREE_DEPENDENT_GOSSIP:

			lunes_real_forward (node, value_to_send, ttl, timestamp, creator, forwarder);
		break;
		#endif

		default:

			fprintf(stdout, "%12.2f FATAL ERROR, the dissemination mode [%2d] is NOT implemented in this version of LUNES!!!\n", simclock, env_dissemination_mode);
			fprintf(stdout, "%12.2f NOTE: all the adaptive protocols require compile time support: see the ADAPTIVE_GOSSIP_SUPPORT define in sim-parameters.h\n", simclock);
			fflush(stdout);
			exit(-1);
		break;
	}
}


/*
	Dissemination protocol implementation:
	a new message has been generated in this node and now it is propagated to (some) neighbors
*/
void lunes_send_to_neighbors (hash_node_t *node, long value_to_send) {

	// Iterator to scan the whole state hashtable of neighbors
	GHashTableIter		iter;
	gpointer		key, destination;

	// All neighbors
	g_hash_table_iter_init (&iter, node->data->state);

	while (g_hash_table_iter_next (&iter, &key, &destination)) {

		// It's a standard ping message
		execute_ping (simclock + FLIGHT_TIME, hash_lookup(stable, node->data->key), hash_lookup(table, *(unsigned int *)destination), env_max_ttl, value_to_send, simclock, node->data->key);
	}
}


#ifdef ADAPTIVE_GOSSIP_SUPPORT
/*
	Adaptive sender dissemination (alg. #2) : each node has a single table for increments and 
	timeouts, this is implemented using the neighbor-based table (of the adaptive specific, alg #3) 
	and copying the same values to all neighbors of a given node.
	Note: this task is done at each update!
*/
void lunes_update_neighbors_table (hash_node_t *node, int cursor, double timeout, float increment) {

	// Iterator to scan the whole state hashtable of neighbors
	GHashTableIter		iter;
	gpointer		key, neighbor;


	g_hash_table_iter_init (&iter, node->data->state);

	// Scanning the whole list of neighbor nodes
	while (g_hash_table_iter_next (&iter, &key, &neighbor)) {

		value_element	*value;
		unsigned int	keyval;

		keyval = *(unsigned int *)neighbor;
		value = (value_element *) g_hash_table_lookup (node->data->state, &keyval);

		// Updating the table values
		value->stim_timeout[cursor] = timeout;
		value->stim_increment[cursor] = increment;
	}
}
#endif


/* -----------------------   GRAPHVIZ DOT FILES SUPPORT --------------------- */


/*
	Support function for the parsing of graphviz dot files,
	used for loading the graphs (i.e. network topology)
*/
void lunes_dot_tokenizer (char *buffer, int *source, int *destination) {
	char *token;
	int i = 0;

	
	token = strtok(buffer, "--");
	do {
		i++;

		if ( i == 1 ) {

			*source = atoi(token);
		}

		if ( i == 2 ) {

			token[strlen(token) - 1] = '\0';
			*destination = atoi(token);
		}
	}
	while (( token = strtok(NULL, "--") ));
}


/*
	Parsing of graphviz dot files,
	used for loading the graphs (i.e. network topology)
*/
void lunes_load_graph_topology () { 
	FILE 		*dot_file;
	char		buffer[1024];
	int		source = 0, 
			destination = 0;
	hash_node_t	*source_node, 
			*destination_node;
	value_element	val;
	#ifdef ADAPTIVE_GOSSIP_SUPPORT	
	int		i;
	#endif


	// What's the file to read?
	sprintf(buffer, "%s%s", TESTNAME, TOPOLOGY_GRAPH_FILE);
	dot_file = fopen(buffer, "r");

	// Reading all of it
	while ( fgets(buffer, 1024, dot_file) != NULL) {

		// Parsing line by line
		lunes_dot_tokenizer(buffer, &source, &destination);

		// I check all the edges defined in the dot file to build up "link messages" 
		// between simulated entities in the simulated network model

		// Is the source node a valid simulated entity?
		if (( source_node = hash_lookup (stable, source) ))  {

			// Is destination vertex a valid simulated entity?
			if (( destination_node = hash_lookup (table, destination) )) {

				#ifdef AG_DEBUG
				fprintf(stdout, "%12.2f node: [%5d] adding link to [%5d]\n", simclock, source_node->data->key, destination_node->data->key);
				#endif

				// Creating a link between simulated entities (i.e. sending a "link message" between them)
				execute_link (simclock + FLIGHT_TIME, source_node, destination_node);

				// Initializing the extra data for the new neighbor
				val.value = destination;

				#ifdef ADAPTIVE_GOSSIP_SUPPORT	
				for ( i = 0; i < NSIMULATE * NLP; i++)	val.stim_timeout[i] = 0;	// Stimuli timeouts
				for ( i = 0; i < NSIMULATE * NLP; i++)	val.stim_increment[i] = 0;	// Stimuli values
				#endif

				// I've to insert the new link (and its extra data) in the neighbor table of this sender,
				// the receiver will do the same when receiving the "link request" message

				// Adding a new entry in the local state of the sender
				//	first entry	= key
				//	second entry	= value
				//	note: no duplicates are allowed
				if ( add_entity_state_entry( destination, &val, source, source_node ) == -1) {
					// Insertion aborted, the key is already in the hash table
					fprintf(stdout, "%12.2f node: FATAL ERROR, [%5d] key %d (value %d) is a duplicate and can not be inserted in the hash table of local state\n", simclock, source, destination, destination);
					fflush(stdout);
					exit(-1);
				}

			} else {
				fprintf(stdout, "%12.2f FATAL ERROR, destination: %d does NOT exist!\n", simclock, destination);
				fflush(stdout);
				exit(-1);
			}
		} 
	}

	fclose(dot_file);
}



/* ************************************************************************ */
/* 	L U N E S     U S E R    L E V E L     H A N D L E R S		    */
/* ************************************************************************ */


/****************************************************************************
	LUNES_CONTROL: node activity for the current timestep
*/
void lunes_user_control_handler (hash_node_t *node) {

	unsigned int	value;

	
	// If the timer expires we can proceed to the generation of the new message
	if ( node->data->s_state.time_of_next_message <= simclock ) {

		// Reset of the timer, it is the time of the next sending
		node->data->s_state.time_of_next_message = simclock + (RND_Exponential(S, 1) * MEAN_NEW_MESSAGE);

		// Creating a (maybe) unique identifier for the new message
		value = RND_Interval(S, (double) 0, (double)MAXINT);

		// The newly generated message has to be inserted in the local cache
		lunes_cache_insert(node->data->s_state.cache, value);

		// Statistics: print in the trace file all the necessary information
		//		a new message has been generated
		#ifdef TRACE_DISSEMINATION
		fprintf(fp_print_trace, "G %010u\n", value);
		#endif
		//		obviously the generating node has "seen" (received) 
		//		the locally generated message
		#ifdef TRACE_DISSEMINATION
		fprintf(fp_print_trace, "R %010u %010u %010u\n", node->data->key, value, 0);
		#endif

		#ifdef DEGREE_DEPENDENT_GOSSIP_SUPPORT
		// Updating (or initializing) the number of my neighbors
		node->data->num_neighbors = g_hash_table_size(node->data->state);
		#endif

		// Broadcasting the new message to all neighbors
		lunes_send_to_neighbors(node, value);
	}

	#ifdef ADAPTIVE_GOSSIP_SUPPORT
	// Adaptive gossip, all variants (algs. #1, #2, #3)
	if ( 	( env_dissemination_mode == ADAPTIVE_GOSSIP ) || 
		( env_dissemination_mode == ADAPTIVE_GOSSIP_SENDER ) ||
		( env_dissemination_mode == ADAPTIVE_GOSSIP_SPECIFIC )
	) {
		// All the adaptive gossip algorithms need evaluation points in which
		// is checked if the reception ratio is under threshold or not

		if ( simclock >= node->data->s_state.histable_cleanup ) {
			// It's time for an evaluation

			// First of all, preparing for the next evaluation point
			node->data->s_state.histable_cleanup = simclock + ADAPTIVE_GOSSIP_EVALUATION_PERIOD;

			// Evaluation of the node's history and sending of stimuli
			lunes_stimuli(node);

			// Cleanup the local statistics about received messages
			memset(node->data->s_state.histable, 0, sizeof(node->data->s_state.histable));			
		}
	}
	#endif
}


/****************************************************************************
	LUNES_REGISTER: a new SE (in this LP) has been created, LUNES needs to
		initialize some data structures
*/
void lunes_user_register_event_handler (hash_node_t *node) {

	// Initialization of the time for the generation of new messages
	node->data->s_state.time_of_next_message = simclock + (RND_Exponential(S, 1) * MEAN_NEW_MESSAGE);

	#ifdef ADAPTIVE_GOSSIP_SUPPORT
	// Adaptive gossip all variants (algs. #1, #2, #3)
	if ( 	( env_dissemination_mode == ADAPTIVE_GOSSIP ) || 
		( env_dissemination_mode == ADAPTIVE_GOSSIP_SENDER ) ||
		( env_dissemination_mode == ADAPTIVE_GOSSIP_SPECIFIC )
	) {
		// Scheduling a new evaluation point in future, please note that the first
		// evaluation point is in partially randomized to avoid crowd effects in the system
		node->data->s_state.histable_cleanup = simclock + ADAPTIVE_GOSSIP_EVALUATION_PERIOD + RND_Interval (S, (double)0, (double)ADAPTIVE_GOSSIP_EVALUATION_PERIOD );
	}
	#endif
}


#ifdef ADAPTIVE_GOSSIP_SUPPORT
/****************************************************************************
	LUNES_STIMULUS: upon arrival of a stimulus modify the probability 
		that is associated to the sender of the stimulus
*/
void	lunes_user_stimulus_event_handler (hash_node_t *node, int from, Msg *msg) {

	unsigned int	key;			// Key to access the hashtable
	value_element	*value;			// Extra data associated to the key

	int		missing_sender;		// Node that has sent the stimulus
	int		cursor;


	// Why this stimulus has been sent? Some node is receiving less messages 
	// than the theoretical rate. The missing messages are from "missing_sender"
	missing_sender = msg->stimulus.stimulus_static.missing_sender;

	#ifdef AG_DEBUG
	fprintf(stdout, "%12.2f node: [%5d] received a stimulus message from agent [%5d], missing sender [%5d]\n", simclock, node->data->key, from, missing_sender);
	#endif

	// I've to access the information about the neighbor that has sent the stimulus
	key = from;
	value = (value_element *) g_hash_table_lookup (node->data->state, &key);

	// Validity check
	if (value == NULL) {
		fprintf(stdout, "%12.2f node: FATAL ERROR, node [%5d] has received a stimulus from node [%5d], thas it NOT a neighbor\n", simclock, node->data->key, from);
		fflush(stdout);
		exit(-1);
	}

	// If the adaptive gossip (alg. #1) is used then the stimulus is associated to the node
	// in the other cases the stimuli have to be associated to each entry in the neighbors data structure
	if ( env_dissemination_mode == ADAPTIVE_GOSSIP )
		cursor = 0;
	else	cursor = missing_sender;

	// There is no valid stimulus active: the period in which the stimulus was active is finished 
	//	or there has been no stimuli up to now
	if ( ( value->stim_timeout[cursor] <= simclock ) || ( value->stim_timeout[cursor] == 0 ) ) {

		// Updating the stimulus probability and its timeout
		value->stim_increment[cursor] = STIMULUS_PROBABILITY_INCREMENT;
		value->stim_timeout[cursor] = simclock + STIMULUS_LENGTH;

		#ifdef AG_DEBUG
		fprintf(stdout, "%12.2f node: [%5d] stimulus to neighbor [%5d] will be %3.2f with timeout %12.2f\n", simclock, node->data->key, from, value->stim_increment[cursor], value->stim_timeout[cursor]);
		#endif

	} else {
		// A stimulus is already active, I have to combine it with the new one

		int 	residual_window;
		float	residual_stimulus;

		// First of all it is necessary to calculate how much is the stimulus residual
		residual_window = (int) ( value->stim_timeout[cursor] - simclock ); 
		residual_stimulus = ( STIMULUS_PROBABILITY_INCREMENT * residual_window ) / STIMULUS_LENGTH;

		#ifdef AG_DEBUG
		fprintf(stdout, "%12.2f node: [%5d] stimulus from node [%5d], residual window %d, residual stimulus %f\n", simclock, node->data->key, from, residual_window, residual_stimulus);
		#endif

		// The new stimulus is added to the residual stimulus
		value->stim_increment[cursor] = STIMULUS_PROBABILITY_INCREMENT + residual_stimulus;

		// Validity check: the combined stimulus can not be larger than 100%
		if  ( ( env_fixed_prob_threshold + value->stim_increment[cursor] ) > 100 )
			value->stim_increment[cursor] = 100 - env_fixed_prob_threshold;

		// There's also a new timeout
		value->stim_timeout[cursor] = simclock + STIMULUS_LENGTH;

		#ifdef AG_DEBUG
		fprintf(stdout, "%12.2f node: [%5d] composed stimulus to neighbor [%5d] will be %3.2f with timeout %12.2f\n", simclock, node->data->key, from, value->stim_increment[cursor], value->stim_timeout[cursor]);
		#endif

		// In the "adaptive sender" dissemintaion (alg. #2) each node has a single table that is used for 
		// the dissemination probabilites of each sender. This is implemented using the table of 
		// "adaptive specific" alg and updating the values associated to all neighbors
		if ( env_dissemination_mode == ADAPTIVE_GOSSIP_SENDER )
			lunes_update_neighbors_table (node, cursor, value->stim_timeout[cursor], value->stim_increment[cursor]);
	}
}
#endif


/****************************************************************************
	LUNES_PING: what happens in LUNES when a node receives a PING message?
*/
void	lunes_user_ping_event_handler (hash_node_t *node, int forwarder, Msg *msg) {

	#ifdef DEGREE_DEPENDENT_GOSSIP_SUPPORT
	gpointer neighbor;
	#endif

	#ifdef ADAPTIVE_GOSSIP_SUPPORT
	// Adaptive gossip, all variants (algs. #1, #2, #3)
	if ( 	( env_dissemination_mode == ADAPTIVE_GOSSIP ) || 
		( env_dissemination_mode == ADAPTIVE_GOSSIP_SENDER ) ||
		( env_dissemination_mode == ADAPTIVE_GOSSIP_SPECIFIC )
	) {
		// The table that reports the received messages has to be updated,
		// it will be used to verify if some node receives less messages
		// than expected

		lunes_histable_update (node, msg->ping.ping_static.creator, forwarder);
	}
	#endif

	// Time-To-Live check
	if (msg->ping.ping_static.ttl == 0 ) {

		// It's time to drop the message

		#ifdef TTLDEBUG
		fprintf(stdout, "%12.2f node: [%5d] message [%5d] TTL=0, dropping\n", simclock, node->data->key, msg->ping.ping_static.msgvalue);
		#endif
	} else {

		// The TTL is still OK

		// Verifies (using the local cache) if the message has been already received
		if ( lunes_cache_verify ( node->data->s_state.cache, msg->ping.ping_static.msgvalue ) == 0 )  {

			// It has not been received
			lunes_cache_insert (node->data->s_state.cache, msg->ping.ping_static.msgvalue);

			#ifdef DEGREE_DEPENDENT_GOSSIP_SUPPORT
			// Updating (or initializing) the number of my neighbors
			node->data->num_neighbors = g_hash_table_size(node->data->state);

			// Updating the number of neighbors of forwarder's neighbors
			neighbor = g_hash_table_lookup (node->data->state, &forwarder);
			((value_element *)neighbor)->num_neighbors = msg->ping.ping_static.num_neighbors;
			#endif

			// Dissemination (to some of) the neighbors
			// NOTE: the TTL is being decremented here!
			lunes_forward_to_neighbors (node, msg->ping.ping_static.msgvalue, --(msg->ping.ping_static.ttl), msg->ping.ping_static.timestamp, msg->ping.ping_static.creator, forwarder);
		} else {

			// The message is already in the cache -> it is dropped

			#ifdef CACHEDEBUG
			fprintf(stdout, "%12.2f node: [%5d] message [%5d] is already in cache, dropping\n", simclock, node->data->key, msg->ping.ping_static.msgvalue);
			#endif
		}
	}
}

