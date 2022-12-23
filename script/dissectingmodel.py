import torch
import csv
import pandas as pd
import os
import numpy as np
import argparse

parser = argparse.ArgumentParser(
        description="Truncating weights of embedding layers"
    )

parser.add_argument("--num-of-dp", type=int, default=0)

global args
args = parser.parse_args()

num_of_dp = num_of_dp

for ev_idx in range(0, 26):
    # Read new EV Table from file
    new_ev_path = os.path.join("weights_and_biases/epoch-1/ev-table", "ev-table-" + str(ev_idx + 1) + ".csv")
    new_ev_df = pd.read_csv(new_ev_path, dtype=float, delimiter=',')
    
    # print(new_ev_df.head(3))
    
    # convert all values to specific number of decimals 
    new_ev_df = new_ev_df.round(num_of_dp)
    
    # create new path based on version
    new_ev_path_mod = os.path.join("weights_and_biases/epoch-1/ev-table", "ev-table-ver-" + str(num_of_dp) + "d.p.-" + str(ev_idx + 1) + ".csv")
    
    # save to new csv file
    new_ev_df.to_csv(path_or_buf=new_ev_path_mod, index=False)
    
#     new_ev_df = pd.read_csv(new_ev_path_mod, dtype=float, delimiter=',')
    
#     print(new_ev_df.head(3))
            
#     print(new_ev_arr)

    

# with open('weights_and_biases/test.csv', 'r') as f:    
#     reader_csv = csv.reader(f)
#     for row in reader_csv:
#         arr = list(row)
#         print(row)
#     print("done")