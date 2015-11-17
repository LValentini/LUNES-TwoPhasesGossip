/*	##############################################################################################
	Advanced RTI System, ARTÌS			http://pads.cs.unibo.it
	Large Unstructured NEtwork Simulator (LUNES)

	Description:
		Model level parameters for LUNES

	Authors:
		First version by Gabriele D'Angelo <g.dangelo@unibo.it>

	############################################################################################### */

#ifndef __LUNES_CONSTANTS_H
#define __LUNES_CONSTANTS_H

// 	General parameters
#define TOPOLOGY_GRAPH_FILE 			"test-graph-cleaned.dot"	// Graph definition to be used for network construction
#define MAX_CACHE_SIZE				512				// MAX cache size (in each node)
#define MAX_TTL					10				// TTL of new messages, standard value
#define	MEAN_NEW_MESSAGE 			30				// Generation of new messages: exponential distribution, mean value

#ifdef ADAPTIVE_GOSSIP_SUPPORT
//	Adaptive dissemination algorithms
#define ADAPTIVE_GOSSIP_EVALUATION_PERIOD	50				// Length of the evaluation period
#define	STIMULUS_PROBABILITY_INCREMENT		10.0				// Dissemination probability increment due to each stimulus
#define	STIMULUS_LENGTH				200				// Length of each stimulus
#endif

//	Dissemination protocols
#define	BROADCAST			0	// Probabilistic broadcast
#define	GOSSIP_FIXED_PROB		1	// Fixed probability
#define GOSSIP_FIXED_FANOUT		2	// Fixed fanout (NOT IMPLEMENTED IN THIS VERSION)
#define UP_AND_DOWN			3	// Up and Down dissemination (NOT IMPLEMENTED IN THIS VERSION)
//
#ifdef ADAPTIVE_GOSSIP_SUPPORT
#define ADAPTIVE_GOSSIP			4	// Adaptive (node) gossip, alg. #1
#define ADAPTIVE_GOSSIP_SENDER		5	// Adaptive sender gossip, alg. #2
#define ADAPTIVE_GOSSIP_SPECIFIC	6	// Adaptive specific gossip, alg. #3
#endif
//
#ifdef DEGREE_DEPENDENT_GOSSIP_SUPPORT
#define DEGREE_DEPENDENT_GOSSIP		7	// Degree Dependent Gossip
#endif

#endif /* __LUNES_CONSTANTS_H */

