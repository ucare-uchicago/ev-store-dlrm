#!/bin/gnuplot --persist

# base_input_dir="/mnt/extra/ev-store-dlrm/logs/inference=0.0022/use-evstore=True/extra-mem-load=8500/use-emb-cache=True/ev-lookup-only=False/cache-size=100000/emb-stor=sqlite"
base_input_dir="/mnt/extra/ev-store-dlrm/logs-old/inference=0.003/use-evstore=True/use-emb-cache=True/ev-lookup-only=False/cache-size=60340/emb-stor=cpp_caching_layer/cpp_algo"

# C1-C2-C3 eval!! Sell C3...
  # cache-size=60340
    # 49-49-2/1000n-euclid003-newrank-227/cpp_algo-cdf.csv
    # 8bit-4bit/cpp_algo-cdf.csv
    # 48-48-4/1000n-euclid003-newrank/cpp_algo-cdf.csv
# Edit the content of this file 
# Sample run: 
    # cd /mnt/extra/ev-store-dlrm
    # gnuplot script/gnuplot_graph/cdf_2_line.plt
# Make sure in that base_input_dir, you have evlfu-cdf.csv and lru-cdf.csv
file1="/8bit-4bit/cpp_algo-cdf.csv"
file2="/49-49-2/1000n-euclid003-newrank-227/cpp_algo-cdf.csv"
file3="/48-48-4/1000n-euclid003-newrank/cpp_algo-cdf.csv"
file4="/approx-emb-threshold=22/evlfu-cdf.csv"
file5="/approx-emb-threshold=23/evlfu-cdf.csv"

# set terminal qt 1
# set terminal postscript eps enhanced color 22 font ",19"
# outputFile="cdf_evlfu_vs_lru.eps"

set terminal pdf
outputFile=base_input_dir."/cdf_multi_line-1.pdf"

print "base_input_dir  : ", base_input_dir

# set term png
set size 0.6,0.7

set style line 1 linecolor rgb "red" lt 1 lw 3
set style line 2 linecolor rgb "green" lt 1 lw 3
set style line 3 linecolor rgb "cyan" lt 1 lw 3
set style line 4 linecolor rgb "orange" lt 1 lw 3
set style line 5 linecolor rgb "pink" lt 1 lw 3

# set title "CDF Latency "
set title "CDF Latency Comparison [L1 = 8bit; L2 = 4bit]\n [L3 is using 1000N-euclid method] [Cache size is 20%]"
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
     base_input_dir.file1 using 2:1 with lines ls 1 title "L1:L2 (size = 50:50)", \
     base_input_dir.file2 using 2:1 with lines ls 2 title "L1:L2:L3 (size = 49:49:2)", \
     base_input_dir.file3 using 2:1 with lines ls 3 title "L1:L2:L3 (size = 48:48:4)", \
     #base_input_dir.file4 using 2:1 with lines ls 4 title file4, \
     #base_input_dir.file5 using 2:1 with lines ls 5 title file5, \

print "output   : ", outputFile
#     write_io_data using 1:2 with lines ls 3 title "Write (avg: ".write_io_avg." us)", \
#     read_io_data using 1:2 with lines ls 2 title "Read (avg: ".read_io_avg." us)", \






# ========================== UNUSED ==========================
# ========================== UNUSED ==========================
# ========================== UNUSED ==========================
# plot \
#      base_input_dir.file1 using 2:1 with lines ls 1 title "vanilla EV-LFU [AUC = 0.80]", \
#      base_input_dir.file2 using 2:1 with lines ls 2 title "EV-LFU + Approx-Emb=15 [AUC = 0.55]", \
#      base_input_dir.file3 using 2:1 with lines ls 3 title "EV-LFU + Approx-Emb=21 [AUC = 0.64]", \
#      base_input_dir.file4 using 2:1 with lines ls 4 title "EV-LFU + Approx-Emb=22 [AUC = 0.69]", \
#      base_input_dir.file5 using 2:1 with lines ls 5 title "EV-LFU + Approx-Emb=23 [AUC = 0.74]", \