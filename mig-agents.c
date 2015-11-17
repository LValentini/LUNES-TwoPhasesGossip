/*	##############################################################################################
	Advanced RTI System, ARTÌS			http://pads.cs.unibo.it
	Large Unstructured NEtwork Simulator (LUNES)

	Description:
		This model implements the Large Unstructured NEtwork Simulator (LUNES).

		The part of the simulator that is implemented in this source code file and the
		related ones is the main model characterization and the application level protocol
		implementation (i.e. dissemination protocols based on gossip techniques).
		Other parts such as the network topology builiding (graph definition) and the
		evaluation of the traces obtained by this simulator are demanded to other tools
		that are also provided.

		The LUNES implementation is based on the MIGRATION-AGENTS template provided in the
		ARTÌS/GAIA software distribution. 	

		This means that each node in the network is represented as an Interacting AGent (IAG)
		and that the messages delivered in the network are done by IAGs interactions.

		More in detail, each IAG is implemented as a single Simulated Entity (SE) that are 
		the basic components of the simulation. 

		Please note that this example is based on the GAIA+ framework and therefore can
		exploit all its functionalities such as the adaptive migration of SEs for the
		reduction of the communication overhead and load-balancing.

		Each IAG has a local state that is necessary to implement the network structure and
		for the dissemination protocols implementation. A part of this local state is
		obtained throught statically allocated data structures and the other part is
		obtained by a dynamic hash table. Statically and dynamically data are both 
		automatically migrated when the GAIA functionalites are enabled.


		The source code is modularized in the following way:

		-	mig-agents.c			(this file)
							* simulation related stuffs
							* main simulation cycle
							* global variables

		-	user_event_handlers.c		the template simulation model. A part of LUNES
							is here, but the more specific functions are in
							the lunes.c source file
						
		-	user_event_handlers.h		prototypes

		-	utils.c				utility functions, in particular data
							structures management

		-	utils.h				prototype, constants and macros that are
							used by the whole model

		-	sim-parameters.h		hard coded parameters of the simulation 
							model

		-	msg_definition.h		definition of all messages used in the
							simulation, e.g. ping, migration etc.

		-	entity_definition.h		definition of the state of simulated entities

		-	lunes.c				contains all the specific LUNES functions

		-	lunes.h				prototypes

		-	lunes_constants.h		specific LUNES constansts
							dissemination protocols tuning

	Output:
		the output is placed in standard output and standard error.
		The script "run" will redirect them in the following files:
		<LP_ID>.out	model output
		<LP_ID>.err	middleware output

	Changelog:
		14/01/11
			First public version. This code has been used for the
			performance evaluation of the DISIO 2011 paper and the
			related technical report.

	Authors:
		First version by Gabriele D'Angelo <g.dangelo@unibo.it>

	############################################################################################### */

/*	TODO

	-	Some code polishing
	-	Some data structures could be manager in a more efficient way (i.e. avoid the static
		allocation of some large data structures in adaptive gossip protocols)
*/

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
#include "utils.h"
#include "user_event_handlers.h"


/*-------------------------- D E B U G --------------------------------------*/
// To activate DEBUG see the file:	sim-parameters.h
/*---------------------------------------------------------------------------*/


/*-------- G L O B A L     V A R I A B L E S --------------------------------*/

int      	NSIMULATE, 		// Number of Interacting Agents (Simulated Entities) per LP
		NLP, 			// Number of Logical Processes
		LPID,			// Identification number of the local Logical Process 
		RUN,			// Run number (in the performance evaluation)
		local_pid;		// Process Identifier (PID)

// SImulation MAnager (SIMA) information and localhost identifier
static char	LP_HOST[64];		// Local hostname
static char	SIMA_HOST[64];		// SIMA execution host (fully qualified domain)
static int	SIMA_PORT;		// SIMA execution port number

// Time management variables
double		step,			// Size of each timestep (expressed in time-units)
		simclock = 0.0;		// Simulated time
static int	end_reached = 0;	// Control variable, false if the run is not finished

// A single LP is responsible to show the runtime statistics
//	by default the first started LP is responsible for this task
static int	LP_STAT = 0;

// Seed used for the random generator
TSeed		Seed, *S=&Seed;
/*---------------------------------------------------------------------------*/

// File descriptors: 
//	lcr_fp: (output) -> local communication ratio evaluation
//	finished: (output) -> it is created when the run is finished (used for scripts management)
//
FILE	*lcr_fp, *finished_fp;

// Output directory (for the trace files)
char	*TESTNAME;

// Simulation control (from environment variables, used by batch scripts)
unsigned int	env_migration;				// Migration state
float		env_migration_factor;			// Migration factor
unsigned int	env_load;				// Load balancing
float		env_end_clock = END_CLOCK;		// End clock (simulated time)
unsigned short	env_dissemination_mode;			// Dissemination mode
float		env_broadcast_prob_threshold;		// Dissemination: conditional broadcast, probability threshold
unsigned int	env_cache_size;				// Cache size of each node
float		env_fixed_prob_threshold;		// Dissemination: fixed probability, probability threshold
#ifdef DEGREE_DEPENDENT_GOSSIP_SUPPORT
unsigned int    env_probability_function;   		// Probability function for Degree Dependent Gossip
double          env_function_coefficient;   		// Coefficient of the probability function
#endif


/* ************************************************************************ */
/* 			            Hash Tables		      	            */
/* ************************************************************************ */

hash_t		hash_table,  *table=&hash_table;		/* Global hash table, contains ALL the simulated entities */
hash_t		sim_table,   *stable=&sim_table;		/* Local hash table, contains only the locally managed entities */
/*---------------------------------------------------------------------------*/


/* ************************************************************************ */
/* 			   Migrating Objects List			    */
/* ************************************************************************ */

// List containing the objects (SE) that have to migrate at the end of the 
//	current timestep
static	se_list		migr_list,	
	*mlist 	= 	&migr_list;
/*---------------------------------------------------------------------------*/


/* ************************************************************************* 
 		      M O D E L    D E F I N I T I O N	
	NOTE:	in the following there is only a part of the model definition, the
		most part of it is implemented in the user level,
		see: user_event_handlers.c and lunes.c
**************************************************************************** */


/*
        Computation and Interactions generation: called at each timestep
	it will provide the model behavior
*/
static void     Generate_Computation_and_Interactions (int total_SE) {

        // Call the appropriate user event handler
        user_control_handler();
}


/*
	SEs initial generation: called once when global variables have been 
	initialized.
*/
static void Generate (int count) {

	int	i;

	// The local Simulated Entities are registered using the appropriate GAIA API
	for ( i = 0; i < count; i++ ) {

		// In this case every entity can be migrated
		GAIA_Register ( MIGRABLE );

		// NOTE:	the internal state of entities is initialized in the 
		//		*	register_event_handler()
		//		see in the following of this source file
	}
}


/*
	Performs the migration of the flagged Simulated Entities
*/
static int UNUSED ScanMigrating () {

	// Current entity
	struct hash_node_t 	*se = NULL;

	// Migration message
	MigrMsg     		m;

	// Number of entities migrated in this step, in this LP
	int 			migrated_in_this_step = 0;

	// Cursor used for the local state of entities
	int 			state_position;

	// Total size of the message that will be sent
	unsigned int		message_size;

	// Iterator to scan the whole state hashtable of entities
	GHashTableIter		iter;
	gpointer		key, value;


	// The SEs to migrate have been already identified by GAIA
	//	and placed in the migration list (mlist) when the
	//	related NOTIF_MIGR was received
	while ( ( se = list_del( mlist ) ) ) {

		// Statistics
		migrated_in_this_step++;

		// A new "M" (migration) type message is created
		m.migration_static.type	= 'M';

		// The state of each IA is composed of a set of elements
		//	let's start from the first one
		state_position = 0;

                // The static part of the agents state has to be inserted in 
                //	the migration message  
                m.migration_static.s_state = se->data->s_state;

                // Dynamic part of the agents state
                //
		// The hashtable is empty
		if ( se->data->state == NULL ) {
			
			#ifdef DEBUG
			fprintf(stdout, "ID: %d is empty\n", se->data->key);
			fflush(stdout);
			#endif	

			// No dynamic part in the migration message
			m.migration_static.dyn_records = 0;
		} else {
			// The state of the SE has to be inserted in the migration message as a set of records
			//	number of records in the dynamic part of the migration message
			m.migration_static.dyn_records = g_hash_table_size(se->data->state);
		}

		// Copying the local state of the migrating entity in the payload of the migration message
		//	for each record in the entity state a new record is appended in the dynamic part
		//	of the migration message
		if ( se->data->state != NULL ) {

			#ifdef DEBUG
			int	tmp = 0;
			#endif

			// Hashtable iterator
			g_hash_table_iter_init (&iter, se->data->state);

			while (g_hash_table_iter_next (&iter, &key, &value)) {
			
				m.migration_dynamic.records[state_position].key =		*(unsigned int *)key;
				m.migration_dynamic.records[state_position].elements = 		*((value_element *)value);

				#ifdef DEBUG
				tmp++;

				fprintf(stdout, "%12.2f node: [%5d] migration, copied key: %d, (%4d/%4d)\n", simclock, se->data->key, m.migration_dynamic.records[state_position].key, tmp, g_hash_table_size(se->data->state));

				fflush(stdout);
				#endif

				state_position++;
			}
		}

		// It is time to clean up the hash table of the migrated node
		if ( se->data->state != NULL ) {

			// In the hash table creation it has been provided the cleaning function that is g_free ()		
			g_hash_table_destroy (se->data->state);
		}

		// Calculating the real size of the migration message
		message_size = sizeof( struct _migration_static_part ) + ( m.migration_static.dyn_records * sizeof( struct state_element ) );

		if ( message_size >= BUFFER_SIZE ) {

			// I'm trying to send a message that is larger than the buffer
			fprintf(stdout, "%12.2f node: FATAL ERROR, trying to send a message (migration) that is larger than: %d !\n", simclock, BUFFER_SIZE);
			fflush(stdout);
			exit(-1);
		}

		// The migration is really executed
		GAIA_Migrate ( se->data->key, (void *)&m, message_size );

		// Removing the migrated SE from the local list of migrating nodes
		hash_delete( LSE, stable, se->data->key );
	}

	// Returning the number of migrated SE (for statistics)
	return ( migrated_in_this_step );
}

/*---------------------------------------------------------------------------*/


/* ************************************************************************ */
/* 			 E V E N T   H A N D L E R S			    */
/* ************************************************************************ */


/*
	Upon arrival of a model level event, firstly we have to validate it
	and only in the following the appropriate handler will be called
*/
struct hash_node_t*	validation_model_events (int id, int to, Msg *msg) {

	struct hash_node_t	*node;


	// The receiver has to be a locally manager Simulated Entity, let's check!
	if ( ! (node = hash_lookup ( stable, to ) ) )  {

		// The receiver is not managed by this LP, it is really a fatal error
		fprintf(stdout, "%12.2f node: FATAL ERROR, [%5d] is NOT in this LP!\n", simclock, to);
		fflush(stdout);
		exit(-1);
	}  else	return( node );
}


/*
 	A new SE has been created, we have to insert it into the global 
	and local hashtables, the correct key to use is the sender's ID
*/
static void	register_event_handler (int id, int lp) {

 	hash_node_t 		*node;
	

	// In every case the new node has to be inserted in the global hash table
	//	containing all the Simulated Entities
	node = hash_insert ( GSE, table, NULL, id, lp );
	
	if ( node ) {

		node->data->s_state.changed = YES;

		// If the SMH is local then it has to be inserted also in the local
		//	hashtable and some extra management is required				
		if ( lp == LPID ) {

			// Call the appropriate user event handler
			user_register_event_handler( node, id );

			// Inserting it in the table of local SEs
			if ( ! hash_insert( LSE, stable, node->data, node->data->key, LPID ) ) {
				// Unable to allocate memory for local SEs 
				fprintf(stdout, "%12.2f node: FATAL ERROR, [%5d] impossible to add new elements to the hash table of local entities\n", simclock, id);
				fflush(stdout);
				exit(-1);
			}
		}
	}
	else {
		// The model is unable to add the new SE in the global hash table
		fprintf(stdout, "%12.2f node: FATAL ERROR, [%5d] impossible to add new elements to the global hash table\n", simclock, id);
		fflush(stdout);
		exit(-1);
	}
}

/*
	Manages the "migration notification" of local SEs (i.e. allocated in this LP)
*/
static void	notify_migration_event_handler (int id, int to) {

	hash_node_t *node;


	#ifdef DEBUG
	fprintf(stdout, "%12.2f agent: [%5d] is going to be migrated to LP [%5d]\n", simclock, id, to);
	#endif
	
	// The GAIA framework has decided that a local SE has to be migrated,
	//	the migration can NOT be executed immediately because the SE
	//	could be the destination of some "in flight" messages
	if ( ( node = hash_lookup ( table, id ) ) )  {
		/* Now it is updated the list of SEs that are enabled to migrate (flagged) */
		list_add ( mlist, node );

		node->data->lp			= to;
		node->data->s_state.changed	= YES;

		// Call the appropriate user event handler
		user_notify_migration_event_handler ();
	}

	// Just before the end of the current timestep, the migration list will be emptied
	//	and the pending migrations will be executed
}


/*
	Manages the "migration notification" of external SEs
	(that is, NOT allocated in the local LP)
*/
static void	notify_ext_migration_event_handler (int id, int to) {

	hash_node_t *node;

	
	// A migration that does not directly involve the local LP is going to happen in
	//	the simulation. In some special cases the local LP has to take care of 
	//	this information 
	if ( ( node = hash_lookup ( table, id ) ) )  {
		node->data->lp			= to;		// Destination LP of the migration
		node->data->s_state.changed	= YES;

		// Call the appropriate user event handler
		user_notify_ext_migration_event_handler ();
	}
}


/*
	Migration-event manager (the real migration handler)

	This handler is executed when a migration message is received and
	therefore a new SE has to be accomodated in the local LP
*/
static void	migration_event_handler (int id, Msg *msg) {

	hash_node_t 		*node;


	#ifdef DEBUG
	int	tmp;


	fprintf(stdout, "%12.2f agent: [%5d] has been migrated in this LP\n", simclock, id);

	for (tmp = 0; tmp < msg->migr.migration_static.dyn_records; tmp++) {

		fprintf(stdout, "%12.2f - state position: %d, key %d\n", simclock, tmp, msg->migr.migration_dynamic.records[tmp].key);

		fflush(stdout);
	}
	#endif

	if ( ( node = hash_lookup ( table, id ) ) ) {
		// Inserting the new SE in the local table
		hash_insert(LSE, stable, node->data, node->data->key, LPID);

		// Call the appropriate user event handler
		user_migration_event_handler(node, id, msg);
	}
}
/*---------------------------------------------------------------------------*/


/* ************************************************************************ */
/* 			   	    U T I L S				    */
/* ************************************************************************ */

/*
	Loading the configuration file of the simulator
*/
static void UNUSED LoadINI(char *ini_name) {

	int	ret;	
	char	data[64];
	

	ret = INI_Load(ini_name);
	ASSERT( ret == INI_OK, ("Error loading ini file \"%s\"", ini_name) );
	
	/* SIMA */
        ret = INI_Read( "SIMA", "HOST", data);
	if (ret == INI_OK && strlen(data))
		strcpy(SIMA_HOST, data);

        ret = INI_Read( "SIMA", "PORT", data);
	if (ret == INI_OK && strlen(data))
		SIMA_PORT = atoi(data);

	INI_Free(); 
}
/*---------------------------------------------------------------------------*/


/* ************************************************************************ */
/* 			   	    M A I N				    */
/* ************************************************************************ */

int main(int argc, char* argv[]) {
	char 	msg_type, 		// Type of message
		*data,			// Buffer for incoming messages, dynamic allocation
		*rnd_file="Rand.seed";	// File containing seeds for the random numbers generator

	int	count, 			// Number of SEs to simulate in the local LP
		start,			// First identifier (ID) to be used to tag the locally managed SEs 
		max_data;		// Maximum size of incoming messages

	int	from,			// ID of the message sender 
		to,			// ID of the message receiver
		tot = 0;		// Total number of executed migrations

	int 	loc,			// Number of messages with local destination (intra-LP)
		rem, 			// Number of messages with remote destination (extra-LP)
		migr,			// Number of executed migrations 
		t;			// Total number of messages (local + remote)

	double  Ts;			// Current timestep
	Msg	*msg;			// Generic message

	int	migrated_in_this_step;	// Number of entities migrated in this step, in the local LP

	struct hash_node_t		*tmp_node;			// Tmp variable, a node in the hash table
	char				*dat_filename, *tmp_filename;	// File descriptors for simulation traces

	// Time measurement
	struct timeval 	t1,t2;		

	// Local PID
	local_pid = getpid();

	// Loading the input parameters from the configuration file
	LoadINI( "mig-agents.ini" );

	// Returns the standard host name for the execution host
	gethostname(LP_HOST, 64);

	// Command-line input parameters
	NLP		= atoi(argv[1]);	// Number of LPs
	NSIMULATE	= atoi(argv[2]);	// Number of SEs to simulate
	RUN		= atoi(argv[3]);	// Run number
	TESTNAME	= argv[4];		// Output directory for simulation traces

	/*
		Set-up of the GAIA framework

		Parameters:	
		1. (SIMULATE*NLP) 	Total number of simulated entities
		2. (NLP)	  	Number of LPs in the simulation
		3. (rnd_file)     	Seeds file for the random numbers generator 
		4. (NULL)         	LP canonical name
		5. (SIMA_HOST)	 	Hostname where the SImulation MAnager is running
		6. (SIMA_PORT)	  	SIMA TCP port number
	*/
	LPID = GAIA_Initialize ( NSIMULATE * NLP, NLP, rnd_file, NULL, SIMA_HOST, SIMA_PORT );
	
	// Returns the length of the timestep
	//	this value is defined in the "CHANNELS.TXT" configuration file
	//	given that GAIA is based on the time-stepped synchronization algorithm
	//	it retuns the size of a step
	step = GAIA_GetStep();

	// Due to synchronization constraints The FLIGHT_TIME has to be bigger than the timestep size
	if ( FLIGHT_TIME < step) {

		fprintf(stdout, "FATAL ERROR, the FLIGHT_TIME (%8.2f) is less than the timestep size (%8.2f)\n", FLIGHT_TIME, step);
		fflush(stdout);
		exit(-1);
	}

	// First identifier (ID) of SEs allocated in the local LP
	start	= NSIMULATE * LPID;

	// Number of SEs to allocate in the local LP
	count	= NSIMULATE;

	//  Used to set the ID of the first simulated entity (SE) in the local LP
	GAIA_SetFstID ( start );

	// User level handler to get some configuration parameters from the runtime environment
	// (e.g. the GAIA parameters and many others)
	user_environment_handler();

	// Initialization of the random numbers generator
	RND_Init (S, rnd_file, LPID * RUN);

	// Output file for statistics (communication ratio data)
	dat_filename = malloc(1024);
	snprintf(dat_filename, 1024, "%stmp-evaluation-lcr.dat", TESTNAME);
	lcr_fp = fopen(dat_filename, "w");
	
	// Data structures initialization (hash tables and migration list)
	hash_init ( table,  NSIMULATE * NLP );		// Global hashtable: all the SEs
	hash_init ( stable, NSIMULATE );		// Local hastable: local SEs
	list_init (mlist);				// Migration list (pending migrations in the local LP)

	// Starting the execution timer
	TIMER_NOW(t1);

	fprintf(stdout, "#LP [%d] HOSTNAME [%s]\n", LPID, LP_HOST);
	fprintf(stdout, "#                      LP[%d] STARTED\n#\n", LPID);

	fprintf(stdout, "#          Generating Simulated Entities from %d To %d ... ", ( LPID * NSIMULATE ), ( ( LPID * NSIMULATE ) + NSIMULATE ) - 1 );
	fflush(stdout);

	// Generate all the SEs managed in this LP
	Generate(count);
	fprintf(stdout, " OK\n#\n");

	fprintf(stdout, "# Data format:\n");
	fprintf(stdout, "#\tcolumn 1:	elapsed time (seconds)\n");
	fprintf(stdout, "#\tcolumn 2:	timestep\n");
	fprintf(stdout, "#\tcolumn 3:	number of entities in this LP\n");
	fprintf(stdout, "#\tcolumn 4:	number of migrating entities (from this LP)\n");

	// It is the LP that manages statistics
	if ( LPID == LP_STAT ) {		// Verbose output	
	
		fprintf(stdout, "#\tcolumn 5:	local communication ratio (percentage)\n");
		fprintf(stdout, "#\tcolumn 6:	remote communication ratio (percentage)\n");
		fprintf(stdout, "#\tcolumn 7:	total number of migrations in this timestep\n");
	}

	fprintf(stdout, "#\n");
	
	// Dynamically allocating some space to receive messages
	data = malloc(BUFFER_SIZE);
	ASSERT ((data != NULL), ("simulation main: malloc error, receiving buffer NOT allocated!"));
	
	// Before starting the real simulation tasks, the model level can initialize some
	//	data structures and set parameters
	user_bootstrap_handler();

	/* Main simulation loop, receives messages and calls the handler associated with them */
	while ( ! end_reached ) {
		// Max size of the next message. 
		// 	after the receive the variable will contain the real size of the message
		max_data = BUFFER_SIZE;

		// Looking for a new incoming message
		msg_type = GAIA_Receive( &from, &to,  &Ts, (void *) data, &max_data );
		msg 	 = (Msg *)data;

		// A message has been received, process it (calling appropriate handler)
		// 	message handlers
		switch ( msg_type ) {
			
			// The migration of a locally managed SE has to be done,
			//	calling the appropriate handler to insert the SE identifier
			//	in the list of pending migrations
			case NOTIF_MIGR:
				notify_migration_event_handler ( from, to );
			break;

			// A migration has been executed in the simulation but the local
			//	LP is not directly involved in the migration execution
			case NOTIF_MIGR_EXT:
				notify_ext_migration_event_handler ( from, to );
			break;

			// Registration of a new SE that is manager by another LP
			case REGISTER:
				register_event_handler ( from, to );
			break;

			// The local LP is the receiver of a migration and therefore a new
			//	SE has to be managed in this LP. The handler is responsible
			//	to allocate the necessary space in the LP data structures 
			//	and in the following to copy the SE state that is contained 
			//	in the migration message
			case EXEC_MIGR:
				migration_event_handler ( from, msg );
			break;

			// End Of Step:
			//	the current simulation step is finished, some pending operations
			//	have to be performed
			case EOS:
				// Stopping the execution timer 
				//	(to record the execution time of each timestep)
				TIMER_NOW ( t2 );

				/*  Actions to be done at the end of each simulated timestep  */
				if ( simclock < env_end_clock ) {	// The simulation is not finished

					// Simulating the interactions among SEs
					//
					//	in the last (env_end_clock - FLIGHT_TIME) timesteps
					//	no pings will be sent because we wanna check if all
					//	sent pings are correctly received
					if ( simclock < ( env_end_clock - FLIGHT_TIME ) ) {

						Generate_Computation_and_Interactions( NSIMULATE * NLP );
					}

					// The pending migration of "flagged" SEs has to be executed,
					//	the SE to be migrated were previously inserted in the migration
					//	list due to the receiving of a "NOTIF_MIGR" message sent by 
					//	the GAIA framework
					migrated_in_this_step = ScanMigrating ();

					// The LP that manages statistics prints out them
					if( LPID == LP_STAT ) {		// Verbose output	

						// Some of them are provided by the GAIA framework
						GAIA_GetStatistics( &loc, &rem, &migr);

						// Total number of interactions (in the timestep)
						t 	= loc + rem;

						// Total number of migrations (in the simulation run)
						tot	+= migr;

						// Printed fields:
						// 	elapsed Wall-Clock-Time up to this step
						//	timestep number
						//	number of entities in this LP
						//	percentage of local communications (intra-LP)
						//	percentage of remote communications (inter-LP)
						//	number of migrations in this timestep
						fprintf(stdout, "- [%11.2f]\t[%6.5f]\t%4.0f\t%4d\t%2.2f\t%2.2f\t%d\n", TIMER_DIFF(t2,t1), simclock, 								(float)stable->count, migrated_in_this_step, (float)loc / (float)t * 100.0, (float)rem / (float)t * 100.0, migr );
						if (simclock >= 7)fprintf(lcr_fp, "%f\n", (float)loc / (float)t * 100.0);
					}
					else {	// Reduced output
	
						fprintf(stdout, "[%11.2fs]   %12.2f [%d]\t[%4d]\n", TIMER_DIFF(t2,t1), simclock, stable->count, migrated_in_this_step);
					}

					// Now it is possible to advance to the next timestep
					simclock = GAIA_TimeAdvance();
				}
				else {
					/* End of simulation */
					TIMER_NOW(t2);
	
					fprintf(stdout, "\n\n");
					fprintf(stdout, "### Termination condition reached (%d)\n", tot);
					fprintf(stdout, "### Clock           %12.2f\n", simclock);
					fprintf(stdout, "### Elapsed Time    %11.2fs\n",TIMER_DIFF(t2,t1));
					fprintf(stdout, "### Total sent pings: %10ld; Total received pings: %10ld", get_total_sent_pings(), 							get_total_received_pings());
					fflush(stdout);	
	
					end_reached = 1;
				}
			break;

			// Simulated model events (user level events)
			case UNSET:
				// First some checks for validation
				tmp_node = validation_model_events( from, to, msg );

				// The appropriate handler is defined at model level		
				user_model_events_handler( to, from, msg, tmp_node );
			break;

			default:
				fprintf(stdout, "FATAL ERROR, received an unknown event type: %d\n", msg_type);
				fflush(stdout);
				exit(-1);
		}
	}

	// Finalize the GAIA framework
	GAIA_Finalize();

	// Before shutting down, the model layer is able to deallocate some data structures
	user_shutdown_handler();

	// Closing output file for performance evaluation
	fclose(lcr_fp);
	
	// Freeing of the receiving buffer
	free(data);

	// Creating the "finished file" that is used by some scripts
	tmp_filename = malloc(256);
	snprintf(tmp_filename, 256, "%d.finished", LPID);
	finished_fp = fopen(tmp_filename, "w");
	fclose(finished_fp);

	// That's all folks.
	return 0;
}

