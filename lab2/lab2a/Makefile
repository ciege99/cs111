# NAME: Collin Prince
# ID: 505091865
# EMAIL: cprince99@g.ucla.edu

SRCS_ADD = lab2_add.c
SRCS_LIST = lab2_list.c SortedList.c
SRCS_H = SortedList.h
SRCS_C = $(SRCS_ADD) $(SRCS_LIST) $(SRCS_H)
SRCS_CSV = lab2_add.csv lab2_list.csv
SRCS_PNG = lab2_add-1.png lab2_add-2.png lab2_add-3.png lab2_add-4.png lab2_add-5.png \
	lab2_list-1.png lab2_list-2.png lab2_list-3.png lab2_list-4.png
SRCS_OTH = README Makefile
SRCS = $(SRCS_C) $(SRCS_CSV) $(SRCS_PNG) $(SRCS_OTH) test_script lab2_add.gp lab2_list.gp
FLAGS = -Wall -Wextra -lpthread


build: lab2_add lab2_list

lab2_add: lab2_add.c 
	gcc -o lab2_add $(SRCS_ADD) $(FLAGS)

lab2_list: lab2_list.c SortedList.c
	gcc -o lab2_list $(SRCS_LIST) $(FLAGS)

dist: tests graphs
	tar -czvf lab2a-505091865.tar.gz $(SRCS)

tests: lab2_add lab2_list
	-./test_script

graphs: tests
	./lab2_add.gp
	./lab2_list.gp


clean:
	rm -f lab2a-505091865.tar.gz
	rm -f lab2_add
	rm -f lab2_list

