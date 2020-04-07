# NAME: Collin Prince
# UID: 505091865
# EMAIL: cprince99@g.ucla.edu

all: lab1b-server lab1b-client


lab1b-server: lab1b-server.c
	gcc -o lab1b-server lab1b-server.c -Wall -Wextra -g -lz

lab1b-client: lab1b-client.c
	gcc -o lab1b-client lab1b-client.c -Wall -Wextra -g -lz
dist: lab1b-client.c lab1b-server.c README Makefile
	tar -czvf lab1b-505091865.tar.gz lab1b-client.c lab1b-server.c README Makefile
clean:
	rm lab1b-505091865.tar.gz lab1b-server lab1b-client
