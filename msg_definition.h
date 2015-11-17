/*	##############################################################################################
	Advanced RTI System, ARTÌS			http://pads.cs.unibo.it
	Large Unstructured NEtwork Simulator (LUNES)

	Description:
		-	In this file are defined the message types and their structures

	Authors:
		First version by Gabriele D'Angelo <g.dangelo@unibo.it>

	############################################################################################### */

#ifndef __MESSAGE_DEFINITION_H
#define __MESSAGE_DEFINITION_H

#include "entity_definition.h"

/*---- M E S S A G E S    D E F I N I T I O N ---------------------------------*/

// Model messages definition
typedef struct _ping_msg		PingMsg;		// Interactions among messages
typedef struct _link_msg		LinkMsg;		// Network constructions
#ifdef ADAPTIVE_GOSSIP_SUPPORT
typedef struct _stimulus_msg		StimulusMsg;		// Stimulus message
#endif
typedef struct _migr_msg		MigrMsg;		// Migration message
typedef union   msg			Msg;

// General note:
//	each type of message is composed of a static and a dynamic part
//	-	the static part contains a pre-defined set of variables, and the size
//		of the dynamic part (as number of records)
//	-	a dynamic part that is composed of a sequence of records

// **********************************************
// PING MESSAGES
// **********************************************
// Record definition for dynamic part of ping messages (THIS DYNAMIC PART IS NOT USED IN LUNES)
struct _ping_record {
	unsigned int	key;
	unsigned int	value;
};
//
// Static part of ping messages
struct _ping_static_part {
	char		type;							// Message type
	float		timestamp;						// Timestep of creation (of the message)
	unsigned short	ttl;							// Time-To-Live
	unsigned int	msgvalue;						// Message Identifier
	unsigned int	creator;						// ID of the original sender of the message
	unsigned int	dyn_records;						// Number of records in the dynamic part of the message

	#ifdef DEGREE_DEPENDENT_GOSSIP_SUPPORT
	unsigned int    num_neighbors;  					// Number of neighbors of forwarder
	#endif
};
//
// Dynamic part of ping messages
struct _ping_dynamic_part {
	struct _ping_record	records[MAX_PING_DYNAMIC_RECORDS];		// Adjuntive records
};
//
// Ping message
struct _ping_msg {
	struct	_ping_static_part		ping_static;			// Static part
	struct	_ping_dynamic_part		ping_dynamic;			// Dynamic part
};


// **********************************************
// LINK MESSAGES
// **********************************************
// Record definition for dynamic part of link messages
struct _link_record {
	unsigned int	key;
	unsigned int	value;
};
//
// Static part of link messages
struct _link_static_part {
	char		type;							// Message type
	unsigned int	dyn_records;						// Number of records in the dynamic part of the message
};
//
// Dynamic part of link messages
struct _link_dynamic_part {
	struct _link_record	records[0];					// It is an array of records
};
//
// Link message
struct _link_msg {
	struct	_link_static_part		link_static;			// Static part
	struct	_link_dynamic_part		link_dynamic;			// Dynamic part
};

#ifdef ADAPTIVE_GOSSIP_SUPPORT
// **********************************************
// STIMULI MESSAGES
// **********************************************
//
// Static part of stimuli messages
struct _stimulus_static_part {
	char		type;							// Message type
	int		missing_sender;						// ID of the node that has produced the missing messages
};
//
// Stimulus message
struct _stimulus_msg {
	struct	_stimulus_static_part		stimulus_static;
};
#endif


// **********************************************
// MIGRATION MESSAGES
// **********************************************
//
// Static part of migration messages
struct _migration_static_part {
	char		type;							// Message type
	static_data_t	s_state;						// Static part of the SE state: it is the same
										//	 of the static part of the simulated entities state
	unsigned int	dyn_records;						// Number of records in the dynamic part of the message
};
//
// Dynamic part of ping messages
struct _migration_dynamic_part {
	struct state_element	records[MAX_MIGRATION_DYNAMIC_RECORDS];		// It is an array of records
};
//
// Migration message
struct _migr_msg {
	struct	_migration_static_part		migration_static;		// Static part
	struct	_migration_dynamic_part		migration_dynamic;		// Dynamic part
};

/////////////////////////////////////////////////
// Union structure for all types of messages
union msg {
	char		type;
	LinkMsg		link;
    	PingMsg		ping;
	MigrMsg		migr;
	#ifdef ADAPTIVE_GOSSIP_SUPPORT
	StimulusMsg	stimulus;
	#endif
};
/*---------------------------------------------------------------------------*/

#endif /* __MESSAGE_DEFINITION_H */

