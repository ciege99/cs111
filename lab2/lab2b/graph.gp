#! /usr/bin/gnuplot
#
# purpose:
#	 generate data reduction graphs for the multi-threaded list project
#
# input: lab2b_list.csv
#	1. test name
#	2. # threads
#	3. # iterations per thread
#	4. # lists
#	5. # operations performed (threads x iterations x (ins + lookup + delete))
#	6. run time (ns)
#	7. run time per operation (ns)
#
# output:

# general plot parameters
set terminal png
set datafile separator ","

# Runtime cost for mutex vs spinlock with multiple threads
set title "List-1: Throughput vs Threads"
set xlabel "Threads"
set logscale x 2
set ylabel "Throughput"
set logscale y 10
set output 'lab2b_1.png'

# grep multiple threads with --sync=m or s options and 1000 iterations
plot \
    "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
    title 'mutex' with linespoints lc rgb 'red', \
    "< grep -e 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
    title 'spinlock' with linespoints lc rgb 'green'


# compare thread count (x) to avg lock time & avg time per op (y) -- lab2b_2.png
set title "List-2: Average time  vs Threads"
set xlabel "Threads"
set logscale x 2
set ylabel "Average time (ns)"
set logscale y 10
set output 'lab2b_2.png'

plot \
    "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($7) \
    title 'average time per operation' with linespoints lc rgb 'red', \
    "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($8) \
    title 'average wait-for-lock time' with linespoints lc rgb 'green'

# compare new multi list performance with no sync vs sync (and yield=id) -- lab2b_3.png
set title "List-3: Unprotected and Protected Multi-lists Comparison"
set xlabel "Threads"
set logscale x 2
set ylabel "Iterations"
set logscale y 2
set output 'lab2b_3.png'

plot \
    "< grep -e 'list-id-none,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3)\
    title 'unprotected' with points lc rgb 'green', \
    "< grep -e 'list-id-m,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
    title 'mutex' with points lc rgb 'blue', \
    "< grep -e 'list-id-s,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
    title 'spin' with points lc rgb 'orange'

unset xtics
set xtics

    
# mutex lock performance
set title "List-4: Throughput vs Threads (Mutex lock)"
set xlabel "Threads"
set logscale x 2
set ylabel "Throughput"
set logscale y 10
set output 'lab2b_4.png'
set key left top
plot \
    "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
    title 'mutex - 1 list' with linespoints lc rgb 'blue', \
    "< grep -e 'list-none-m,[0-9]*,1000,4,' lab2b_list.csv" using ($2):(1000000000/($7)) \
    title 'mutex -  4 lists' with linespoints lc rgb 'red', \
    "< grep -e 'list-none-m,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1000000000/($7)) \
    title 'mutex - 8 lists' with linespoints lc rgb 'green', \
    "< grep -e 'list-none-m,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1000000000/($7)) \
    title 'mutex - 16 lists' with linespoints lc rgb 'orange'

# mutex lock performance
set title "List-5: Throughput vs Threads (Spin lock)"
set xlabel "Threads"
set logscale x 2
set ylabel "Throughput"
set logscale y 10
set output 'lab2b_5.png'

plot \
    "< grep -e 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
    title 'spin - 1 list' with linespoints lc rgb 'blue', \
    "< grep -e 'list-none-s,[0-9]*,1000,4,' lab2b_list.csv" using ($2):(1000000000/($7)) \
    title 'spin - 4 lists' with linespoints lc rgb 'red', \
    "< grep -e 'list-none-s,[0-9]*,1000,8,' lab2b_list.csv" using ($2):(1000000000/($7)) \
    title 'spin - 8 lists' with linespoints lc rgb 'green', \
    "< grep -e 'list-none-s,[0-9]*,1000,16,' lab2b_list.csv" using ($2):(1000000000/($7)) \
    title 'spin - 16 lists' with linespoints lc rgb 'orange'

