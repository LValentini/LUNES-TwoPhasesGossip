/*	##############################################################################################
	Advanced RTI System, ARTÌS			http://pads.cs.unibo.it
	Large Unstructured NEtwork Simulator (LUNES)

	Description:
		-	See "user_event_handlers.c" description
		-	Function prototypes

	Authors:
		First version by Gabriele D'Angelo <g.dangelo@unibo.it>

	############################################################################################### */

#ifndef __USER_EVENT_HANDLERS_H
#define __USER_EVENT_HANDLERS_H

#include "msg_definition.h"
#include <rnd.h>


/* ************************************************************************ */
/* 		U S E R   E V E N T   H A N D L E R S			    */
/* ************************************************************************ */

//	Event handlers
void		user_register_event_handler (hash_node_t *, int);
void		user_notify_migration_event_handler ();
void		user_notify_ext_migration_event_handler ();
void		user_migration_event_handler (hash_node_t *, int, Msg *);
void		user_model_events_handler (int, int, Msg *, hash_node_t *);
//	Other handlers
void		user_control_handler ();
void		user_bootstrap_handler ();
void		user_environment_handler ();
void		user_shutdown_handler ();
//	Statistics
unsigned long	get_total_sent_pings ();
unsigned long	get_total_received_pings ();

/* ************************************************************************ */
/* 		S U P P O R T     F U N C T I O N S			    */
/* ************************************************************************ */

int		add_entity_state_entry (unsigned int, value_element *, int, hash_node_t *);
gpointer	hash_table_random_key (GHashTable* );
void		execute_link (double, hash_node_t *, hash_node_t *);
void		execute_ping (double, hash_node_t *, hash_node_t *, unsigned short, unsigned int, double, unsigned int);

#endif /* __USER_EVENT_HANDLERS_H */

