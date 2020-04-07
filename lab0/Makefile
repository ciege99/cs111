# Name: Collin Prince
# Email: cprince99@g.ucla.edu
# ID: 505091865

all: lab0

lab0: lab0.c 
	gcc -g -Wall -Wextra -o lab0 lab0.c

check: lab0
	./check_tests

dist: lab0.c Makefile backtrace.png breakpoint.png README check_tests
	tar -czvf lab0-505091865.tar.gz lab0.c Makefile backtrace.png breakpoint.png README check_tests

clean: 
	rm lab0 lab0-505091865.tar.gz 
