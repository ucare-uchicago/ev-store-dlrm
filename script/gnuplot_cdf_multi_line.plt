#!/bin/gnuplot --persist

input_dir="/mnt/extra/ev-store-dlrm/logs/inference=0.0022/use-evstore=True/extra-mem-load=8500/use-emb-cache=True/ev-lookup-only=False/cache-size=100000/emb-stor=sqlite"
# input_dir="/mnt/extra/ev-store-dlrm/logs/inference=0.0022/use-evstore=True/extra-mem-load=8500/use-emb-cache=True/ev-lookup-only=False/cache-size=100000/emb-stor=filepy"

# Edit the content of this file 
# Sample run: gnuplot script/gnuplot_cdf_multi_line.plt
# Make sure in that input_dir, you have evlfu-cdf.csv and lru-cdf.csv

file1="/evlfu-cdf.csv"
file2="/approx-emb-threshold=15/evlfu-cdf.csv"
file3="/approx-emb-threshold=21/evlfu-cdf.csv"
file4="/approx-emb-threshold=22/evlfu-cdf.csv"
file5="/approx-emb-threshold=23/evlfu-cdf.csv"

# set terminal qt 1
# set terminal postscript eps enhanced color 22 font ",19"
# outputFile="cdf_evlfu_vs_lru.eps"

set terminal pdf
outputFile=input_dir."/cdf_multi_line.pdf"

print "input_dir  : ", input_dir

# set term png
set size 0.8,1

set style line 1 linecolor rgb "red" lt 1 lw 3
set style line 2 linecolor rgb "green" lt 1 lw 3
set style line 3 linecolor rgb "cyan" lt 1 lw 3
set style line 4 linecolor rgb "orange" lt 1 lw 3
set style line 5 linecolor rgb "pink" lt 1 lw 3

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
     input_dir.file1 using 2:1 with lines ls 1 title file1, \
     input_dir.file2 using 2:1 with lines ls 2 title file2, \
     input_dir.file3 using 2:1 with lines ls 3 title file3, \
     input_dir.file4 using 2:1 with lines ls 4 title file4, \
     input_dir.file5 using 2:1 with lines ls 5 title file5, \

print "output   : ", outputFile
#     write_io_data using 1:2 with lines ls 3 title "Write (avg: ".write_io_avg." us)", \
#     read_io_data using 1:2 with lines ls 2 title "Read (avg: ".read_io_avg." us)", \






# ========================== UNUSED ==========================
# ========================== UNUSED ==========================
# ========================== UNUSED ==========================
# plot \
#      input_dir.file1 using 2:1 with lines ls 1 title "vanilla EV-LFU [AUC = 0.80]", \
#      input_dir.file2 using 2:1 with lines ls 2 title "EV-LFU + Approx-Emb=15 [AUC = 0.55]", \
#      input_dir.file3 using 2:1 with lines ls 3 title "EV-LFU + Approx-Emb=21 [AUC = 0.64]", \
#      input_dir.file4 using 2:1 with lines ls 4 title "EV-LFU + Approx-Emb=22 [AUC = 0.69]", \
#      input_dir.file5 using 2:1 with lines ls 5 title "EV-LFU + Approx-Emb=23 [AUC = 0.74]", \