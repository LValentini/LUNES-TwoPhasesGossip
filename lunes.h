/*	##############################################################################################
	Advanced RTI System, ARTÌS			http://pads.cs.unibo.it
	Large Unstructured NEtwork Simulator (LUNES)

	Description:
		-	Function prototypes

	Authors:
		First version by Gabriele D'Angelo <g.dangelo@unibo.it>

	############################################################################################### */

#ifndef __LUNES_H
#define __LUNES_H

#include "utils.h"

// LUNES handlers
void	lunes_user_ping_event_handler ( hash_node_t *, int, Msg * );
void	lunes_user_register_event_handler ( hash_node_t * );
void	lunes_user_control_handler ( hash_node_t * );
#ifdef ADAPTIVE_GOSSIP_SUPPORT
void	lunes_user_stimulus_event_handler ( hash_node_t *, int, Msg * );
#endif

// Support functions
void 	lunes_load_graph_topology ();

#endif /* __LUNES_H */

