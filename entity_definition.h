/*	##############################################################################################
	Advanced RTI System, ARTÌS			http://pads.cs.unibo.it
	Large Unstructured NEtwork Simulator (LUNES)

	Description:
		-	In this file is defined the state of the simulated entities

	Authors:
		First version by Gabriele D'Angelo <g.dangelo@unibo.it>

	############################################################################################### */

#ifndef __ENTITY_DEFINITION_H
#define __ENTITY_DEFINITION_H

#include "lunes_constants.h"

/*---- E N T I T I E S    D E F I N I T I O N ---------------------------------*/

// Structure of "value" in the hash table of each node
//	in LUNES used to implement neighbors and its properties
typedef struct v_e {
	unsigned int	value;						// Value
	#ifdef ADAPTIVE_GOSSIP_SUPPORT
	double		stim_timeout[ADAPTIVE_GOSSIP_MAX_NODES];	// Table of stimuli timeouts
	float		stim_increment[ADAPTIVE_GOSSIP_MAX_NODES];	// Table of stimuli increments
	#endif

	#ifdef DEGREE_DEPENDENT_GOSSIP_SUPPORT
	unsigned int 	num_neighbors; 					// Number of neighbors of each neighbor of a given node
	#endif
} value_element;

// Records composing the local state (dynamic part) of each SE
// NOTE: no duplicated keys are allowed
struct state_element {
	unsigned int	key;					// Key
	value_element	elements;				// Value
};

// Structure of the cache element
typedef struct cache_element {
	long		element;				// Cached element ID
	float		age;					// Time of insertion
} CacheElement;

// Static part of the SE state
typedef struct static_data_t {
	char			changed;			// ON if there has been a state change in the last timestep
	float			time_of_next_message;		// Timestep in which the next new message will be created and sent
	CacheElement		cache[MAX_CACHE_SIZE];		// Cache local to each node, used to suppress duplicate messages
	#ifdef ADAPTIVE_GOSSIP_SUPPORT
	unsigned char		histable[ADAPTIVE_GOSSIP_MAX_NODES][ADAPTIVE_GOSSIP_MAX_NODES];		
								// Main table of received events, used for adaptive gossip algorithms
	unsigned int		histable_cleanup;		// Last timestep in which the histable has been cleaned up
	#endif
} static_data_t;

// SE state definition
typedef struct hash_data_t {
	int			key;				// SE identifier
	int 			lp;				// Logical Process ID (that is the SE container)
	static_data_t		s_state;			// Static part of the SE local state
	GHashTable*		state;				// Local state as an hash table (glib) (dynamic part)

	#ifdef DEGREE_DEPENDENT_GOSSIP_SUPPORT
	unsigned int 		num_neighbors;			// Number of SE's neighbors (dynamically updated)
	#endif

} hash_data_t;

#endif /* __ENTITY_DEFINITION_H */

