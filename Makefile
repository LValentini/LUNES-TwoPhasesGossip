ifndef ROOT
	ROOT = ../..
endif

include         $(ROOT)/config.mak

INCLDIR		= $(ROOT)/INCLUDE
LIBDIR		= $(ROOT)/LIB
BINS		= sima mig-agents graphgen get_ids_next get_coverage_next spacer
HEADERS		= sim-parameters.h utils.h user_event_handlers.h msg_definition.h entity_definition.h lunes.h lunes_constants.h
#------------------------------------------------------------------------------

CFLAGS		+= $(OPTFLAGS) -I. -I$(INCLDIR) `pkg-config --cflags glib-2.0`
LDFLAGS		= -L$(LIBDIR)
LIBS		= -lartis_static -lpthread -lm `pkg-config --libs glib-2.0`
LDFLAGS		+= $(LIBS)
#------------------------------------------------------------------------------

all:	$(BINS) 

mig-agents:	mig-agents.o utils.o user_event_handlers.o lunes.o $(HEADERS)
	$(CC) -o $@ $(CFLAGS) mig-agents.o utils.o user_event_handlers.o lunes.o $(LDFLAGS)

graphgen:	graphgen.c
	$(CC) -o $@ graphgen.c -ligraph -I/usr/include/igraph/

get_ids_next:	get_ids_next.c
	$(CC) -o $@ $(CFLAGS) get_ids_next.c $(LDFLAGS) -D_LARGEFILE64_SOURCE

get_coverage_next:	get_coverage_next.c
	$(CC) -o $@ $(CFLAGS) get_coverage_next.c $(LDFLAGS) -D_LARGEFILE64_SOURCE

.c:
	$(CC) -o $@ $(CFLAGS) $< $(LDFLAGS) 

#------------------------------------------------------------------------------

clean :
	rm -f  $(BINS) *.o *~ 
	rm -f  *.out *.err
	rm -f *.finished	

cleanall : clean 
	rm -f  *.dat *.log *.dot
	rm -f evaluation/*.ps
#------------------------------------------------------------------------------
