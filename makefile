###################################################################
#
# makefile
# For compiling Lyrebird client and server
#
# James Shephard
# CMPT 300 - D100 Burnaby
# Instructor Brian Booth
# TA Scott Kristjanson
#
###################################################################

.SUFFIXES: .h .o .c

# Options same for both client and server
CC = gcc
CCOPTS = -g -DMW_STDIO -std=c99
LIBS = -lm

# Client
CCMAIN1 = client.c
OBJS1 = client.o decrypt.o child.o common.o
CCEXEC1 = lyrebird.client
# Server
CCMAIN2 = parent.c memwatch.c
OBJS2 = common.o server.o
CCEXEC2 = lyrebird.server

all:	$(CCEXEC1) $(CCEXEC2)

$(CCEXEC1):	$(OBJS1) makefile 
	@echo Linking $@ . . .
	$(CC) $(CCOPTS) $(OBJS1) -o $@ $(LIBS)

$(CCEXEC2):	$(OBJS2) makefile 
	@echo Linking $@ . . .
	$(CC) $(CCOPTS) $(OBJS2) -o $@ $(LIBS)

%.o:	%.c
	@echo Compiling $< . . .
	$(CC) -c $(CCOPTS) $<

run:	all
	./$(CCEXEC)

clean:
	rm -f $(OBJS1)
	rm -f $(OBJS2)
	rm -f $(CCEXEC1)
	rm -f $(CCEXEC2)
	rm -f core
	rm -f memwatch.log