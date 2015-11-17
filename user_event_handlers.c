/*	##############################################################################################
	Advanced RTI System, ARTÌS			http://pads.cs.unibo.it
	Large Unstructured NEtwork Simulator (LUNES)

	Description:
		-	In this file you find all the user event handlers to be used to implement a 
			discrete event simulation. Only the modelling part is to be inserted in this 
			file, other tasks such as GAIA-related data structure management are 
			implemented in other parts of the code.
		-	Some "support" functions are also present.
		-	This file is part of the MIGRATION-AGENTS template provided in the
			ARTÌS/GAIA software distribution but some modifications have been done to
			include the LUNES features.

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
#include "utils.h"
#include "msg_definition.h"
#include "lunes.h"
#include "lunes_constants.h"
#include "user_event_handlers.h"

/* ************************************************************************ */
/* 			E X T E R N A L     V A R I A B L E S 	            */
/* ************************************************************************ */

extern hash_t		hash_table, *table;		/* Global hash table of simulated entities */
extern hash_t		sim_table, *stable;		/* Hash table of locally simulated entities */
extern double		simclock;			/* Time management, simulated time */
extern TSeed		Seed, *S;			/* Seed used for the random generator */
extern FILE		*fp_print_trace;		/* File descriptor for simulation trace file */
extern char		*TESTNAME;			/* Test name */
extern int		LPID;				/* Identification number of the local Logical Process */
extern int		local_pid;			/* Process identifier */
extern int	     	NSIMULATE;	 		/* Number of Interacting Agents (Simulated Entities) per LP */
extern int		NLP; 				/* Number of Logical Processes */
// Simulation control
extern unsigned int	env_migration;			/* Migration state */
extern float		env_migration_factor;		/* Migration factor */
extern unsigned int	env_load;			/* Load balancing */
extern float		env_end_clock;			/* End clock (simulated time) */
extern unsigned short	env_max_ttl;			/* TTL of new messages */
extern unsigned short	env_dissemination_mode;		/* Dissemination mode */
extern float 		env_broadcast_prob_threshold;	/* Dissemination: conditional broadcast, probability threshold */
extern unsigned int	env_cache_size;			/* Cache size of each node */
extern float		env_fixed_prob_threshold;	/* Dissemination: fixed probability, probability threshold */
#ifdef DEGREE_DEPENDENT_GOSSIP_SUPPORT
extern unsigned int env_probability_function;		/* Probability function for Degree Dependent Gossip */
extern double       env_function_coefficient;		/* Coefficient of probability function */
#endif


/* ************************************************************************ */
/* 		 L O C A L	V A R I A B L E S			    */
/* ************************************************************************ */

// Total number of sent and received pings in this LP, for statistics
unsigned long	lp_total_sent_pings 	= 0;
unsigned long	lp_total_received_pings = 0;


/* ************************************************************************ */
/* 		 S U P P O R T     F U N C T I O N S			    */
/* ************************************************************************ */

/* ***************************** D E B U G **********************************/

/*
	Prints out the whole content of a glib hashtable data structure
*/
void	UNUSED hash_table_print (GHashTable* ht) {

	// Iterator to scan the whole state hashtable of entities
	GHashTableIter		iter;
	gpointer		key, value;


	g_hash_table_iter_init (&iter, ht);

	while (g_hash_table_iter_next (&iter, &key, &value)) {

		fprintf(stdout, "DEBUG: %d:%d\n", *(unsigned int *)key, *(unsigned int *)value);
		fflush(stdout);
	}	
}


/*
	Returns a random key from a hash table
*/
gpointer UNUSED hash_table_random_key (GHashTable* ht) {

	// Iterator to scan the (whole) state hashtable of entities
	GHashTableIter		iter;
	gpointer		key, value;
	//
	guint			size;
	unsigned int		position;


	size = g_hash_table_size( ht );
	position = RND_Integer( S, (double) 1, (double) size );
	
	g_hash_table_iter_init ( &iter, ht );

	while ( position ) {

		g_hash_table_iter_next (&iter, &key, &value);
		position--;
	}	

	return(key);	
}


/*
	Statistics: exports the total number of SENT ping messages
*/
unsigned long	get_total_sent_pings () {

	return(lp_total_sent_pings);
}

/*
	Statistics: exports the total number of RECEIVED ping messages
*/
unsigned long	get_total_received_pings () {

	return(lp_total_received_pings);
}


/*
	Utility to check environment variables, if the variable is not defined then the run is aborted
*/
char *	check_and_getenv( char *variable ) {

	char *value;


	value = getenv(variable);

	if (value == NULL) {

		fprintf(stdout, "The environment variable %s is not defined\n", variable);

		fflush(stdout);
		exit(1);
	}
	else	return(value);
}


/* *********** E N T I T Y    S T A T E    M A N A G E M E N T **************/

/*
	Adds a new entry in the hash table that implements the SE's local state
	Note: it is used both from the register and the migration handles
*/
int	add_entity_state_entry (unsigned int key, value_element *val, int id, hash_node_t *node) {

	struct	state_element	*state_e;


	// First of all, it is necessary to check if the used key is already in the hash table
	if ( g_hash_table_lookup ( node->data->state, &key ) != NULL )	return(-1);

	// The number of state records is limited by the MAX_MIGRATION_DYNAMIC_RECORDS constant,
	//	that is the max number of records that can be inserted in a migration message
	if ( g_hash_table_size(node->data->state) > MAX_MIGRATION_DYNAMIC_RECORDS ) {

		// No more entries can be added, the resulting state would be impossible to migrate
		fprintf(stdout, "%12.2f node: FATAL ERROR, [%5d] impossible to add new elements to the state hash table of this node, see constant MAX_MIGRATION_DYNAMIC_RECORDS in file: sim-parameters.h\n", simclock, id);
		fflush(stdout);
		exit(-1);
	}

	// Dynamic allocation of memory and initialization of values
	// Note: this memory will be automatically freed in case of SE migration
	state_e = g_malloc( sizeof( struct state_element ) );
	if ( state_e ) {

		state_e->key = key;
		state_e->elements = *val;

		#ifdef DEGREE_DEPENDENT_GOSSIP_SUPPORT
		// Initialization of the number of neighbors of current node's neighbor
		(state_e->elements).num_neighbors = 0;
		#endif

		g_hash_table_insert (node->data->state, &(state_e->key), &(state_e->elements));

		#ifdef DEBUG
		fprintf(stdout, "%12.2f node: [%5d] local state key: %d, local hash_size: %d\n", simclock, id, state_e->key, g_hash_table_size (node->data->state));

		fflush(stdout);
		#endif

		return (1);
	}
	else {
		// Unable to allocate memory for state elements
		fprintf(stdout, "%12.2f node: FATAL ERROR, [%5d], memory allocation, impossible to add new elements to the state hash table of this node\n", simclock, id);
		fflush(stdout);
		exit(-1);
	}
}


/*
	Deletes an entry in the hash table that implements the SE's local state
	Note: the freeing of the associated memory is automatic
*/
int	delete_entity_state_entry (unsigned int key, hash_node_t *node) {

	if ( g_hash_table_remove ( node->data->state, &key ) == TRUE)	return (0);
	else								return (-1);
}


/*
	Modifies the value of an entry in the SE's local state
*/
int	modify_entity_state_entry (unsigned int key, unsigned int new_value, hash_node_t *node) {

	unsigned int	*value;


	value = g_hash_table_lookup ( node->data->state, &key );

	if ( value ) {
		*(value) = new_value;
		return (0);
	} else	return (-1);
}


/* ********************** I N T E R A C T I O N S ***************************/


/*
  	Ping another SE, creating and sending a 'P' type message
	Usually called by user_generate_interactions_handler ()
 */
void	execute_ping (double ts, hash_node_t *src, hash_node_t *dest, unsigned short ttl, unsigned int value_to_send, double timestamp, unsigned int creator) {

	PingMsg		msg;
	unsigned int	message_size;


	// Defining the message type
	msg.ping_static.type = 'P';

	msg.ping_static.timestamp = timestamp;
	msg.ping_static.ttl = ttl;
	msg.ping_static.msgvalue = value_to_send;
	msg.ping_static.creator = creator;

	#ifdef DEGREE_DEPENDENT_GOSSIP_SUPPORT
	// The number of sender's neighbors is transported by the ping message
	msg.ping_static.num_neighbors = src->data->num_neighbors;
	#endif

	// In this specific case, the number of records in the ping message is set to zero
	msg.ping_static.dyn_records = 0;

	// To reduce the network overhead, only the used part of the message is really sent
	message_size = sizeof(struct _ping_static_part) + (msg.ping_static.dyn_records * sizeof(struct _ping_record));

	// Dynamic boudaries check
	if (msg.ping_static.dyn_records > MAX_PING_DYNAMIC_RECORDS) {

		fprintf(stdout, "%12.2f FATAL ERROR, the number of entries in the dynamic part of the ping message is bigger than MAX_PING_DYNAMIC_RECORDS!\n", simclock);
		fflush(stdout);
		exit(-1);
	}

	// Buffer check
	if (message_size > BUFFER_SIZE) {

		fprintf(stdout, "%12.2f FATAL ERROR, the outgoing BUFFER_SIZE is not sufficient!\n", simclock);
		fflush(stdout);
		exit(-1);
	}

	// Real send
	GAIA_Send (src->data->key, dest->data->key, ts, (void *)&msg, message_size);

	// Statistics
	lp_total_sent_pings++;
}


/*
  	Links another SE, creating and sending a 'L' type message
	In LUNES it is used to build up the graph structure that has been read
	from the input graph definition file (in dot format).
 */
void	execute_link (double ts, hash_node_t *src, hash_node_t *dest) {
	LinkMsg		msg;
	unsigned int	message_size;


	// Defining the message type
	msg.link_static.type = 'L';

	// The number of records in the ping message is chosen at random
	msg.link_static.dyn_records = 0;

	// To reduce the network overhead, only the used part of the message is really sent
	message_size = sizeof(struct _link_static_part);

	// Buffer check
	if (message_size > BUFFER_SIZE) {

		fprintf(stdout, "%12.2f FATAL ERROR, the outgoing BUFFER_SIZE is not sufficient!\n", simclock);
		fflush(stdout);
		exit(-1);
	}

	// Real send
	GAIA_Send (src->data->key, dest->data->key, ts, (void *)&msg, message_size);
}


/* ************************************************************************ */
/* 		U S E R   E V E N T   H A N D L E R S			    */
/*									    */
/*	NOTE: when a handler required extensive modifications for LUNES	    */
/*		then it calls another user level handerl called		    */
/*		lunes_<handler_name> and placed in the file lunes.c	    */
/* ************************************************************************ */


/****************************************************************************
	PING: Upon arrival of an echo request do "nothing" or print some log
*/
void	user_ping_event_handler (hash_node_t *node, int forwarder, Msg *msg) {
	float difference;


	// Statistics	
	lp_total_received_pings++;

	#ifdef PINGDEBUG
	fprintf(stdout, "%12.2f node: [%5d] received a ping from agent [%5d] original sender [%5d], timestamp [%5f], ttl [%5d], value [%10d]\n", simclock, node->data->key, forwarder, msg->ping.ping_static.creator, msg->ping.ping_static.timestamp, msg->ping.ping_static.ttl, msg->ping.ping_static.msgvalue);
	#endif

	difference = simclock - msg->ping.ping_static.timestamp;

	#ifdef TRACE_DISSEMINATION
	fprintf(fp_print_trace, "R %010u %010u %03u\n", node->data->key, msg->ping.ping_static.msgvalue, (int)difference);
	#endif

	// Calling the appropriate LUNES user level handler
	lunes_user_ping_event_handler (node, forwarder, msg);
}


/****************************************************************************
	LINK: upon arrival of a link request some tasks have to be executed
*/
void	user_link_event_handler (hash_node_t *node, int id, Msg *msg) {

	value_element	val;
	#ifdef ADAPTIVE_GOSSIP_SUPPORT
	int 		i;
	#endif

	val.value = id;

	#ifdef ADAPTIVE_GOSSIP_SUPPORT	
	for ( i = 0; i < NSIMULATE * NLP; i++)	val.stim_timeout[i] = 0;
	for ( i = 0; i < NSIMULATE * NLP; i++)	val.stim_increment[i] = 0;
	#endif

	// Adding a new entry in the local state of the registering node
	//	first entry	= key
	//	second entry	= value
	//	note: no duplicates are allowed
	if ( add_entity_state_entry(id, &val, node->data->key, node) == -1 ) {
		// Insertion aborted, the key is already in the hash table
		fprintf(stdout, "%12.2f node: FATAL ERROR, [%5d] key %d (value %d) is a duplicate and can not be inserted in the hash table of local state\n", simclock, node->data->key, id, id);
		fflush(stdout);
		exit(-1);
	}

	#ifdef AG_DEBUG
	fprintf(stdout, "%12.2f node: [%5d] received a link request from agent [%5d], total received requests: %d\n", simclock, node->data->key, id, g_hash_table_size(node->data->state));
	#endif
}


/*****************************************************************************
 	REGISTER: a new SE (in this LP) has been created, now it is possibile to
	initialize its data structures (es. local state)
*/
void	user_register_event_handler (hash_node_t *node, int id) {


	// Initializing the local data structures of the node		
	node->data->state = g_hash_table_new_full( g_int_hash, g_int_equal, g_free, NULL );

	// Calling the appropriate LUNES user level handler
	lunes_user_register_event_handler ( node );
}


/*****************************************************************************
	NOTIFY MIGRATION: a local SE will be migrated in another LP.
	This notification is reported to the user level but usually nothing 
	has to be done
*/
void	user_notify_migration_event_handler () {

	// Nothing to do
}


/*****************************************************************************
	NOTIFY EXTERNAL MIGRATION: SEs that are allocated in other LPs are going 
	to be migrated, this LP is notified of this update but the user level 
	usually does not care of it
*/
void	user_notify_ext_migration_event_handler () {

	// Nothing to do	
}


/*****************************************************************************
	MIGRATION: migration-event manager (the real migration handler)

	A new migration message for this LP has been received, the trasported SE has 
	been created and inserted in the data structures. Now it is necessary to 
	perform some user level tasks such as taking care of de-serializing the 
	SE's local state
*/
void	user_migration_event_handler (hash_node_t *node, int id, Msg *msg) {
	int		tmp;

	// Initializing the local data structures of the node		
	node->data->state = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, NULL);

	// The migration message contains the state of the migrating SE,
	//	after allocating space to locally manage the node, I've
	//	now to update the state of the SE using the state 
	//	information contained in the migration message
	//
	// Static part
	node->data->s_state = msg->migr.migration_static.s_state;
	// Dynamic part
	for ( tmp = 0; tmp < msg->migr.migration_static.dyn_records; tmp++ ) {

		// Adding a new entry in the local state of the migrated node (rebuilds its local state)
		//	first entry	= key
		//	second entry	= value
		//	note: no duplicates are allowed
		if ( add_entity_state_entry( msg->migr.migration_dynamic.records[tmp].key, 
			&(msg->migr.migration_dynamic.records[tmp].elements), id, node ) == -1 ) {

			// Insertion aborted, the key is already in the hash table
			fprintf(stdout, "%12.2f node: FATAL ERROR, [%5d] key %d (value %d) is a duplicate and can not be inserted in the hash table of local state\n", simclock, id, msg->migr.migration_dynamic.records[tmp].key, msg->migr.migration_dynamic.records[tmp].elements.value);
			fflush(stdout);
			exit(-1);
		}
	}
}


/*****************************************************************************
	CONTROL: at each timestep, the LP calls this handler to permit the execution 
	of model level interactions, for performance reasons the handler is called once 
	for all the SE that allocated in the LP	
*/
void	user_control_handler () {

	int		h;
	hash_node_t	*node;

	// Only if in the BUILDING_STEP
	if ( simclock == (float) BUILDING_STEP ) {
		// Loading the graph topology that was previously generated
		lunes_load_graph_topology();
	}

	// Only if in the aggregation phase is finished &&
	// if it is possible to send messages up to the last simulated timestep then the statistics will be
	// affected by some messages that have been sent but with no time to be received
	if ( ( simclock >= (float) EXECUTION_STEP) && ( simclock <  ( env_end_clock - MAX_TTL ) ) ) {

		// For each local SE
	   	for ( h = 0; h < stable->size; h++ ) {
			for ( node = stable->bucket[h]; node; node = node->next) {	

				// Calling the appropriate LUNES user level handler			
				lunes_user_control_handler ( node );
			}
		}
	}
}


/*****************************************************************************
	USER MODEL: when it is received a model level interaction, after some 
	validation this generic handler is called. The specific user level 
	handler will complete its processing
*/
void	user_model_events_handler (int to, int from, Msg *msg, hash_node_t *node) {

	// A model event has been received, now calling appropriate user level handler
	switch ( msg->type ) {

		case 'P':	// Ping message 
			user_ping_event_handler(node, from, msg);
		break;

		case 'L':	// Link message
			user_link_event_handler(node, from, msg);
		break;

		#ifdef ADAPTIVE_GOSSIP_SUPPORT
		case 'S':	// Stimulus message
			lunes_user_stimulus_event_handler(node, from, msg);
		break;
		#endif

		default:
			fprintf(stdout, "FATAL ERROR, received an unknown user model event type: %d\n", msg->type);
			fflush(stdout);
			exit(-1);
	}
}


void	user_environment_handler () {

	// ######################## RUNTIME CONFIGURATION SECTION ####################################
	//	Runtime configuration:	migration type configuration
	env_migration = atoi(check_and_getenv("MIGRATION"));
        fprintf(stdout, "LUNES____[%10d]: MIGRATION, migration variable set to %d\n", local_pid, env_migration);
	if ((env_migration > 0) && (env_migration < 4)) {
	        fprintf(stdout, "LUNES____[%10d]: MIGRATION is ON, migration type is set to %d\n", local_pid, env_migration);
		GAIA_SetMigration(env_migration);
	} else {
	        fprintf(stdout, "LUNES____[%10d]: MIGRATION is OFF\n", local_pid);
		GAIA_SetMigration(MIGR_OFF);
	}

	//	Runtime configuration:	migration factor (GAIA)
	env_migration_factor = atof(check_and_getenv("MFACTOR"));
        fprintf(stdout, "LUNES____[%10d]: MFACTOR, migration factor: %f\n", local_pid, env_migration_factor);
	GAIA_SetMF(env_migration_factor);

	//	Runtime configuration:	turning on/off the load balancing (GAIA)
	env_load = atoi(check_and_getenv("LOAD"));
        fprintf(stdout, "LUNES____[%10d]: LOAD, load balancing: %d\n", local_pid, env_load);
	if (env_load == 1) {
	        fprintf(stdout, "LUNES____[%10d]: LOAD, load balancing is ON\n", local_pid);
		GAIA_SetLoadBalancing(LOAD_ON);
	} else {
	        fprintf(stdout, "LUNES____[%10d]: LOAD, load balancing is OFF\n", local_pid);
		GAIA_SetLoadBalancing(LOAD_OFF);
	}

	//	Runtime configuration:	number of steps in the simulation run
	env_end_clock = atof(check_and_getenv("END_CLOCK"));

        fprintf(stdout, "LUNES____[%10d]: END_CLOCK, number of steps in the simulation run -> %f\n", local_pid, env_end_clock);
	if (env_end_clock == 0) {

		fprintf(stdout, "LUNES____[%10d]:  END_CLOCK is 0, no timesteps are defined for this run!!!\n", local_pid);
	}

	//	Runtime configuration:	time-to-live for new messages in the network
	env_max_ttl = atoi(check_and_getenv("MAX_TTL"));
        fprintf(stdout, "LUNES____[%10d]: MAX_TTL, maximum time-to-live for messages in the network -> %d\n", local_pid, env_max_ttl);

	if (env_max_ttl == 0) {

		fprintf(stdout, "LUNES____[%10d]: MAX_TTL is 0, no TTL is defined for this run!\n", local_pid);
	}

	//	Runtime configuration:	dissemination mode (gossip protocol)
	env_dissemination_mode = atoi(check_and_getenv("DISSEMINATION"));
	fprintf(stdout,"LUNES____[%10d]: DISSEMINATION, dissemination mode -> %d\n", local_pid, env_dissemination_mode);
	//
	switch ( env_dissemination_mode ) {

		case BROADCAST:			//	probabilistic broadcast dissemination

			//	Runtime configuration:	probability threshold of the broadcast dissemination
			env_broadcast_prob_threshold = atof(check_and_getenv("BROADCAST_PROB_THRESHOLD"));
			fprintf(stdout, "LUNES____[%10d]: BROADCAST_PROB_THRESHOLD, probability of the broadcast dissemination -> %f\n", local_pid, env_broadcast_prob_threshold);
			if ( ( env_broadcast_prob_threshold < 0 ) || ( env_broadcast_prob_threshold > 100 ) ) {

				fprintf(stdout, "LUNES____[%10d]: BROADCAST_PROB_THRESHOLD is out of the boundaries!!!\n", local_pid);
			}		
		break;
	
		case GOSSIP_FIXED_PROB:		//	gossip with fixed probability

			//	Runtime configuration:	probability threshold of the fixed probability dissemination
			env_fixed_prob_threshold = atof(check_and_getenv("FIXED_PROB_THRESHOLD"));
			fprintf(stdout, "LUNES____[%10d]: FIXED_PROB_THRESHOLD, probability of the fixed probability dissemination -> %f\n", local_pid, env_fixed_prob_threshold);
			if ( ( env_fixed_prob_threshold < 0 ) || ( env_fixed_prob_threshold > 100 ) ) {

				fprintf(stdout, "LUNES____[%10d]:  FIXED_PROB_THRESHOLD is out of the boundaries!!!\n", local_pid);
			}		
		break;

		#ifdef ADAPTIVE_GOSSIP_SUPPORT
		case ADAPTIVE_GOSSIP:		//	adaptive gossip, alg. #1
		case ADAPTIVE_GOSSIP_SENDER:	//	adaptive gossip, alg. #2
		case ADAPTIVE_GOSSIP_SPECIFIC:	//	adaptive gossip, alg. #3

			//	Runtime configuration:	baseline probability threshold of the adaptive gossip
			env_fixed_prob_threshold = atof(check_and_getenv("FIXED_PROB_THRESHOLD"));
			fprintf(stdout, "LUNES____[%10d]: FIXED_PROB_THRESHOLD, baseline probability of the adaptive gossip dissemination -> %f\n", local_pid, env_fixed_prob_threshold);
			if ( ( env_fixed_prob_threshold < 0 ) || ( env_fixed_prob_threshold > 100 ) ) {

				fprintf(stdout, "LUNES____[%10d]:  FIXED_PROB_THRESHOLD is out of the boundaries!!!\n", local_pid);
			}		
		break;
		#endif

		#ifdef DEGREE_DEPENDENT_GOSSIP_SUPPORT
		case DEGREE_DEPENDENT_GOSSIP:

			// Runtime configuration: probability function to be applied to Degree Dependent Gossip
			env_probability_function = atoi(check_and_getenv("PROBABILITY_FUNCTION"));
			fprintf(stdout, "LUNES____[%10d]: PROBABILITY_FUNCTION, probability function -> %u\n", local_pid, env_probability_function);

			// Coefficient of probability function
			env_function_coefficient = atof(check_and_getenv("FUNCTION_COEFFICIENT"));      
			fprintf(stdout, "LUNES____[%10d]: FUNCTION_COEFFICIENT, function coefficient -> %f\n", local_pid, env_function_coefficient);

		break;
		#endif

		default:

			fprintf(stdout, "LUNES____[%10d]: FATAL ERROR, the dissemination mode [%2d] is NOT implemented in this version of LUNES!!!\n", local_pid, env_dissemination_mode);
			fprintf(stdout, "LUNES____[%10d]: NOTE, all the adaptive protocols require compile time support, see the ADAPTIVE_GOSSIP_SUPPORT define in sim-parameters.h\n", local_pid);
			fflush(stdout);
			exit(-1);
		break;
	}

	//	Runtime configuration:	cache size
	//
	env_cache_size = atoi(check_and_getenv("CACHE_SIZE"));
	fprintf(stdout,"LUNES____[%10d]: CACHE_SIZE, cache size: %d (max: %d)\n", local_pid, env_cache_size, MAX_CACHE_SIZE);
	if (env_cache_size < 0)	fprintf(stdout, "LUNES____[%10d]: CACHE_SIZE is out of the boundaries!!!\n", local_pid);
	if (env_cache_size == 0)	fprintf(stdout, "LUNES____[%10d]: CACHE_SIZE is 0 and therefore the cache is disabled\n", local_pid);
	if (env_cache_size > MAX_CACHE_SIZE) {

		fprintf(stdout, "LUNES____[%10d]:  CACHE_SIZE is bigger that MAX_CACHE_SIZE (%d) and therefore the cache is set to the maximum\n", local_pid, MAX_CACHE_SIZE);
		env_cache_size = MAX_CACHE_SIZE;
	}

	#ifdef ADAPTIVE_GOSSIP_SUPPORT
	// Checking some constraints
	
	// If an adaptive protocol is used then I've to check that the number of nodes in the graph
	// is within the static limit that has been defined at compile time for the support data
	// structure used in this dissemination protocol
	if ( ( NSIMULATE * NLP ) > ADAPTIVE_GOSSIP_MAX_NODES) {

		fprintf(stdout, "LUNES____[%10d]: FATAL ERROR, the adaptive dissemination protocol [%2d] is used and the numer of nodes in the graph (%5d) is too high!\n", local_pid, env_dissemination_mode, NSIMULATE * NLP);
		fprintf(stdout, "LUNES____[%10d]: the compile time limit ADAPTIVE_GOSSIP_MAX_NODES is now set to (%5d) and is defined in sim-parameters.h\n", local_pid, ADAPTIVE_GOSSIP_MAX_NODES);
		fflush(stdout);
		exit(-1);
	}
	#endif
}


/*****************************************************************************
	BOOTSTRAP: before starting the real simulation tasks, the model level 
	can initialize some data structures and set parameters
*/
void	user_bootstrap_handler () {

	#ifdef TRACE_DISSEMINATION
	char buffer[1024];


	// Preparing the simulation trace file
	sprintf (buffer, "%sSIM_TRACE_%03d.log", TESTNAME, LPID);

	fp_print_trace = fopen(buffer, "w");
	#endif
}


/*****************************************************************************
	SHUTDOWN: Before shutting down, the model layer is able to 
	deallocate some data structures
*/
void	user_shutdown_handler () {

	#ifdef TRACE_DISSEMINATION
	char	buffer[1024];
	FILE	*fp_print_messages_trace;

	
	sprintf(buffer, "%stracefile-messages-%d.trace", TESTNAME, LPID);

	fp_print_messages_trace = fopen(buffer, "w");

	//	statistics
	//		total number of pings on the network
	fprintf(fp_print_messages_trace, "M %010lu\n", get_total_sent_pings ());

	fclose(fp_print_messages_trace);

	fclose(fp_print_trace);
	#endif
}

