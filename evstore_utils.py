import os
import pandas as pd
# import EvLFU
import struct
from pathlib import Path
import ast
import numpy as np
import torch

TRAINING_CONFIG_FILE = "training_config.txt"

# Replacing the current embedding layer at the current model
def load_new_ev_table (ld_model, ev_path):
    print("Load new set of EV Table from = " + ev_path)
    for ev_idx in range(0, 26):
        new_ev_path = os.path.join(ev_path, "ev-table-"+ str(ev_idx + 1) + ".csv")
        new_ev_df = pd.read_csv(new_ev_path, dtype=float, delimiter=',')
        # # Convert to numpy first before to tensor
        new_ev_arr = new_ev_df.to_numpy()
        # # Convert to tensor
        new_ev_tensor = torch.FloatTensor(new_ev_arr)

        print("Loading NEW EV per embedding layer = " + new_ev_path)

        # Create key since the entire model will be accessed
        key = str("emb_l."+str(ev_idx)+".weight")
        # Replace the current embedding tensor with the new one based on the key
        ld_model["state_dict"][key] = new_ev_tensor
    print("Done loading all EV-Table from " + ev_path)

def store_training_config(file_path, table_feature_map, nbatches, nbatches_test, ln_emb, m_den):
    # store the config to a file to avoid redoing the computation during inference-only
    with open(file_path, 'w') as f:
        f.write('The order of the arguments: table_feature_map, nbatches, nbatches_test, ln_emb, m_den\n')
        f.write(str(table_feature_map)+"\n")
        f.write(str(nbatches)+"\n")
        f.write(str(nbatches_test)+"\n")
        f.write(str(ln_emb.tolist())+"\n")
        f.write(str(m_den)+"\n")
    print("Done writing training config to : " + file_path + "\n")

def read_training_config(file_path):
    print("Read training config from : " + file_path)
    with open(file_path) as f:
        lines = [line.rstrip() for line in f]

    table_feature_map = ast.literal_eval(lines[1])
    nbatches = int(lines[2])
    nbatches_test = int(lines[3])
    ln_emb = np.array(ast.literal_eval(lines[4]))
    m_den = int(lines[5])
    return table_feature_map, nbatches, nbatches_test, ln_emb, m_den

def prepare_inference_trace_folder(input_data_name, percent_data_for_inference):
    print("Create folder to store the model and ev-tables")
    outdir = os.path.join("logs", "inf-workload-traces", input_data_name, "inference=" + str(percent_data_for_inference))
    Path(outdir).mkdir(parents=True, exist_ok=True)
    return outdir

def write_inf_workload_to_file(workload_traces_outdir, arr_inference_workload):
    # Create + open 26 different files
    print("Total inference = " + str(len(arr_inference_workload)))
    arrfile = []
    
    for idx in range(0,26):
        arrfile.append(open(workload_traces_outdir + "/workload-group-" + str(idx + 1) + ".csv",'w'))
        arrfile[idx].write("G" + str(idx + 1) + "_key\n")

    for grouped_keys in arr_inference_workload:
        id = 0
        for key in grouped_keys:
            arrfile[id].write(key + "\n")
            id += 1