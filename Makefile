# Copyright (C) DriveScale, Inc. 2014-2015 - All Rights Reserved.

all:	diop diop_matrix diop_quick

diop:	Makefile diop.h diop_main.c diop_lib.c
	cc -Wall -o diop -O3 diop_main.c diop_lib.c -lm

diop_quick: Makefile diop.h diop_quick.c diop_lib.c
	cc -Wall -o diop_quick diop_quick.c diop_lib.c  -ljson

diop_matrix: Makefile diop_matrix.c diop_lib.c diop.h
	cc -Wall -o diop_matrix -O3 diop_matrix.c diop_lib.c -ljson

diop.tar: Makefile diop.h diop_main.c diop_lib.c diop_quick.c diop_matrix.c 
	tar cf -  Makefile diop.h diop_main.c diop_lib.c diop_quick.c diop_matrix.c  > diop.tar

clean:
	rm -f diop diop_matrix diop_quick
	rm -f *.tar *.o 

install: all
	cp diop diop_quick diop_matrix /usr/local/bin
