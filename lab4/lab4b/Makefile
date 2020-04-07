# NAME: Collin Prince
# UID: 505091865
# EMAIL: cprince99@g.ucla.edu

FLAGS = -Wall -Wextra -lm -lmraa 
SRCS = lab4b.c test_script Makefile README


build: lab4b

lab4b: lab4b.c
	gcc -o lab4b lab4b.c $(FLAGS)

check: lab4b
	./test_script

dist:
	tar -czvf lab4b-505091865.tar.gz $(SRCS)

clean:
	rm -f lab4b
	rm -f lab4b-505091865.tar.gz
