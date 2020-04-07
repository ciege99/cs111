# NAME: Collin Prince
# ID: 505091865
# EMAIL: cprince99@g.ucla.edu

SRCS_LIST = lab2_list.c SortedList.c
SRCS_H = SortedList.h
SRCS_C = $(SRCS_LIST) $(SRCS_H)
SRCS_CSV = lab2b_list.csv
SRCS_PNG = lab2b_1.png lab2b_2.png lab2b_3.png lab2b_4.png lab2b_5.png
SRCS_OTH = README Makefile profile.out
SRCS_SCR = test_script graph.gp
SRCS = $(SRCS_C) $(SRCS_CSV) $(SRCS_OTH) $(SRCS_SCR) $(SRCS_PNG)
FLAGS = -Wall -Wextra -lpthread -lprofiler -g 


build: lab2_list

lab2_list: lab2_list.c SortedList.c
	gcc -o lab2_list $(SRCS_LIST) $(FLAGS)

dist: tests graphs profile
	tar -czvf lab2b-505091865.tar.gz $(SRCS)

tests: lab2_list
	-./test_script

graphs: tests
	-./graph.gp

profile: lab2_list
	env LD_PRELOAD="/usr/lib64/libprofiler.so" \
	env CPUPROFILE=./prof.gperf ./lab2_list --iterations=1000 --threads=12 --sync=s
	/bin/pprof --text ./lab2_list ./prof.gperf > profile.out
	/bin/pprof --list=spin_thread ./lab2_list ./prof.gperf >> profile.out
	rm -f prof.gperf

clean:
	rm -f lab2b-505091865.tar.gz
	rm -f lab2_list

