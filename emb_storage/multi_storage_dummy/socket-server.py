#!/usr/bin/env python3
import argparse
import socket
import pandas as pd
import os
import torch
import struct

parser = argparse.ArgumentParser(description="EvLFU server")
parser.add_argument("--port", type=int, default=8000)
parser.add_argument("--ev-path", type=str, default="")
args = parser.parse_args()

# Call EvLFU service through socket
HOST = '127.0.0.1'  # Standard loopback interface address (localhost)
PORT = args.port  # 65432        # Port to listen on (non-privileged ports are > 1023)
MAX_BUFFER = 1024
BINARY_DIR_NAME = "binary/"
TOTAL_EV_TABLE = 26
EV_DIMENSION = 36

# This is ROCKSDB client or dummyMemStor client

EvTable_C1 = []

# Load value as bytes!!
def load(ev_path_c1, bit_precision = 32):
    # We are still storing it as array of floats. TODO: Store it as binary!
    print("**************** Loading EV Table to DummyMemoryStorage")
    print("**************** Load new set of EV Table from = " + ev_path_c1)
    global EvTable_C1
    EvTable_C1.append("Buffer: table0 is not used")
    BYTE_PRECISION = int(bit_precision/8)
    TOTAL_BYTE_PER_ROW = EV_DIMENSION * BYTE_PRECISION

    for ev_idx in range(0, 26):
        binFilename = "ev-table-" + str(ev_idx + 1) + ".bin"
        bin_ev_path = os.path.join(ev_path_c1, BINARY_DIR_NAME, binFilename)
        print("************* Loading Binnary EV = " + bin_ev_path)

        curr_table = []
        # put
        with open(bin_ev_path, 'rb') as f:
            data = f.read()
            num_of_indexes = len(data) // TOTAL_BYTE_PER_ROW
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

def load_as_list(ev_path_c1):
    # We are still storing it as array of floats. TODO: Store it as binary!
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
        break
    print("**************** All EvTable loaded in the Memory!")

def get(tableId, rowId):
    # tableId started at id = 1
    global EvTable_C1
    return EvTable_C1[tableId][rowId]

def get_many(arrTableId, arrRowId):
    # tableId started at id = 1
    global EvTable_C1
    arrVal = []
    for i in range(len(arrTableId)):
        arrVal.append(EvTable_C1[arrTableId[i]][arrRowId[i]])
    # return array of values
    return arrVal

def listen():
    print("This server is ready to look up the ev-value based on the key!")
    print("Start listening at port: " + str(args.port))
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen()
        conn, addr = s.accept()
        with conn:
            print('Connected to client at: ', addr)
            while True:
                buf = conn.recv(MAX_BUFFER)
                if buf:

                    keys = str(buf, 'utf8').split('\n')
                    # print("keys: " + str(keys))
                    for key in keys:
                        tableId, rowId = key.split('-', 2)
                        val = get(int(tableId), int(rowId))
                        conn.sendall(val)
                    # print("Done sending the values of " + str(keys))

                    # tableId, rowId = str(buf, 'utf8').split('-', 2)
                    # val = get(int(tableId), int(rowId))
                    # print(val)
                    # print(struct.unpack('f'*36, val[0:144]))
                    # conn.sendall(val)

if __name__=="__main__":
    load(args.ev_path)
    listen()
