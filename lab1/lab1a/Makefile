# NAME: Collin Prince
# EMAIL: cprince99@g.ucla.edu
# ID: 505091865


all: lab1a

lab1a : lab1a.c
	gcc -o lab1a lab1a.c -Wall -Wextra -g

clean:
	rm -f lab1a-505091865.tar.gz lab1a

dist: lab1a.c README Makefile
	tar -czvf lab1a-505091865.tar.gz lab1a.c README Makefile
