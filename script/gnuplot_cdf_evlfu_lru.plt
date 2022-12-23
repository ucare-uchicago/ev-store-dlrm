#!/bin/gnuplot --persist

# Sample run: gnuplot -e "input_dir='/mnt/extra/ev-store-dlrm/logs/inference=0.0022/use-evstore=True/extra-mem-load=8500/use-emb-cache=True/ev-lookup-only=True/cache-size=230000/emb-stor=sqlite/'" /mnt/extra/ev-store-dlrm/script/gnuplot_cdf_evlfu_lru.plt
# Make sure in that input_dir, you have evlfu-cdf.csv and lru-cdf.csv

print "input_dir  : ", input_dir

file1=input_dir."/evlfu-cdf.csv"
file2=input_dir."/lru-cdf.csv"

# set terminal qt 1
# set terminal postscript eps enhanced color 22 font ",19"
# outputFile="cdf_evlfu_vs_lru.eps"

set terminal pdf font ",19"
# set terminal pdf font ",21"
outputFile=input_dir."/cdf_evlfu_vs_lru.pdf"


# set term png
set size 0.8,1

set style line 1 linecolor rgb "green" lt 2 lw 4
set style line 2 linecolor rgb "red" lt 1 lw 4
set style line 3 linecolor rgb "blue" lt 1 lw 4

set title "CDF Latency of EV-LFU vs LRU"
set xlabel "Latency (ms)"
set ylabel "CDF"

set yrange [0:1]
#set xrange [0:10000] # microsecond. Set this according to intended dimensions
set xrange [0:4]

set key right bottom
set key autotitle columnhead
set datafile separator "," # CSV file is seperated with ,
set output outputFile # set output file name to preference 

# adjust file names and legend labels for each line you plot
plot \
     file1 using 2:1 with lines ls 1 title "EV-LFU", \
     file2 using 2:1 with lines ls 2 title "LRU", \

print "output   : ", outputFile
#     write_io_data using 1:2 with lines ls 3 title "Write (avg: ".write_io_avg." us)", \
#     read_io_data using 1:2 with lines ls 2 title "Read (avg: ".read_io_avg." us)", \
