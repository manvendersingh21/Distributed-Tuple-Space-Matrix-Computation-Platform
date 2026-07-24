# # Set up variables
# CC = gcc
# INCS = -I../include
# OBJS = ../obj
# FLAGS = -g

# # Default target
# all: launch copy

# # Compile tshlib.c into object file only
# tshlib.o: tshlib.c tshlib.h
# 	$(CC) $(INCS) $(FLAGS) -c tshlib.c -o tshlib.o

# # Build launch using tshlib.o and -lsng
# launch: launch.c tshlib.o
# 	$(CC) $(INCS) $(FLAGS) -o launch launch.c tshlib.o -L$(OBJS) -lsng

# # Optional: install binaries
# copy: launch
# 	cp launch ../bin



SYS_LINK=-L/usr/lib
INCS = -I../include
OBJS = ../obj 
FLAGS = -g 
cc = gcc

all : tsh matrix  copy 

tsh : tsh.c 
	$(cc) $(EXTRA) $(INCS) $(FLAGS) -o tsh tsh.c -L$(OBJS) -lsng

# Compile tshlib.c into object file only
tshlib.o: tshlib.c tshlib.h
	$(CC) $(INCS) $(FLAGS) -c tshlib.c -o tshlib.o

# Build launch using tshlib.o and -lsng
matrix: matrix.c tshlib.o
	$(CC) $(EXTRA) $(INCS) $(FLAGS) -o matrix matrix.c tshlib.o -L$(OBJS) -lsng

# tshtest : tshtest.c 
# 	$(cc) $(EXTRA) $(INCS) $(FLAGS) -o tshtest tshtest.c -L$(OBJS) -lsng


copy: tsh matrix
	cp tsh ../bin
# cp tshtest ../bin
	cp matrix ../bin