# NAME: Collin Prince
# UID: 505091865
# EMAIL: cprince99@g.ucla.edu

FLAGS = -Wall -Wextra -lm -lmraa
SRCS = lab4c_tcp.c lab4c_tls.c Makefile README


build: lab4c_tcp lab4c_tls

lab4c_tcp: lab4c_tcp.c
	gcc -o lab4c_tcp lab4c_tcp.c $(FLAGS)

lab4c_tls: lab4c_tls.c
	gcc -o lab4c_tls lab4c_tls.c $(FLAGS) -lssl -lcrypto

dist:
	tar -czvf lab4c-505091865.tar.gz $(SRCS)

clean:
	rm -f lab4c_tls lab4c_tcp
	rm -f lab4c-505091865.tar.gz
