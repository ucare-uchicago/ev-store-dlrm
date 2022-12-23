import pandas as pd
import os
import torch
import struct
import sys
sys.path.append('../../')

import evstore_utils
import storage_manager

EvTable_C1 = []
MAX_BUFFER = 256
BINARY_DIR_NAME = "binary/"
TOTAL_EV_TABLE = 26
EV_DIMENSION = 36


def load(ev_path_c1):
    # return load_as_binary(ev_path_c1)
    return load_as_list(ev_path_c1)

def get(tableId, rowId):
    # tableId started at id = 1
    # return get_as_binary(tableId, rowId)
    return get_as_list(tableId, rowId)

def get_nrows_pertable(file_path):
    _, _, _, ln_emb, _ = evstore_utils.read_training_config(file_path)
    return ln_emb

# Load value as bytes!!
def load_as_binary(ev_path_c1, bit_precision = 32):
    print("**************** Loading EV Table to DummyMemoryStorage")
    print("**************** Load new set of EV Table from = " + ev_path_c1)
    global EvTable_C1
    EvTable_C1.append("Buffer: table0 is not used")
    BYTE_PRECISION = int(bit_precision/8)
    TOTAL_BYTE_PER_ROW = EV_DIMENSION * BYTE_PRECISION
    ln_emb = get_nrows_pertable(storage_manager.training_config_path)

    for ev_idx in range(0, 26):
        binFilename = "ev-table-" + str(ev_idx + 1) + ".bin"
        bin_ev_path = os.path.join(ev_path_c1, BINARY_DIR_NAME, binFilename)
        print("************* Loading Binnary EV = " + bin_ev_path)

        curr_table = []
        # put
        with open(bin_ev_path, 'rb') as f:
            data = f.read()
            num_of_indexes = len(data) // TOTAL_BYTE_PER_ROW
            assert(ln_emb[ev_idx] == num_of_indexes)
            for i in range(0, num_of_indexes):
                # put
                byte_offset = BYTE_PRECISION * i * EV_DIMENSION # 36 -> dimension
                blob = data[ byte_offset : byte_offset + TOTAL_BYTE_PER_ROW]
                curr_table.append(blob)

                # Try reading the blob 
                # print(struct.unpack('f'*36, blob[0:144]))
        f.close()
        EvTable_C1.append(curr_table)
    print("**************** All EvTable loaded in the Memory!")

def get_as_binary(tableId, rowId):
    # tableId started at id = 1
    global EvTable_C1
    blob = EvTable_C1[tableId][rowId]
    return struct.unpack('f'*36, blob)

def load_as_list(ev_path_c1):
    print("**************** Loading EV Table to DummyMemoryStorage")
    print("**************** Load new set of EV Table from = " + ev_path_c1)
    global EvTable_C1
    EvTable_C1.append("Buffer: table0 is not used")
    for ev_idx in range(0, 26):
        # Read new EV Table from file
        ev_path = os.path.join(ev_path_c1,
                                   "ev-table-" + str(ev_idx + 1) + ".csv")
        print("********************* Loading EV = " + ev_path)
        new_ev_df = pd.read_csv(ev_path, dtype=float, delimiter=',')
        # Convert to numpy first before to tensor
        new_ev_arr = new_ev_df.to_numpy()
        # Convert to tensor
        # Option 1: Store it as numpy array (Slower for reading)
        # EvTable_C1[ev_idx + 1] = new_ev_arr
        # Option 2: Store it as pure python list
        EvTable_C1.append(new_ev_arr.tolist())
    print("**************** All EvTable loaded in the Memory!")

def get_as_list(tableId, rowId):
    # tableId started at id = 1
    global EvTable_C1
    # print(EvTable_C1[tableId][rowId])
    # exit()
    return EvTable_C1[tableId][rowId]
