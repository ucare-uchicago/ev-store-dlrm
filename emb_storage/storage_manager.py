import pandas as pd
import os
import torch
import storage_dummy
import storage_rocksdb
from storage_rocksdb import RocksDBClient
import storage_sqlite
from storage_sqlite import SQLiteClient
# import storage_sqlite_1_tab
# from storage_sqlite_1_tab import SQLiteClient
import file_read
import mmap_file_read
import socket
import struct
import sys

sys.path.append('../cache_algo/EvLFU_C1_Cython')
import EvLFU

class EmbStorage:
    DUMMY = 1
    ROCKSDB = 2
    FILEPY = 3
    MMAPFILEPY = 4
    SQLITE = 5
    FILEC = 6
    CPP_CACHING_LAYER = 7

HOST = '127.0.0.1'
PORTS = [65480, 65471, 65442, 65443, 65444]
NSOCKET = 1

# Holding all socket connection
arr_socket_conn = []

# Default embedding storage is a dummy in-memory storage
storage_type = EmbStorage.DUMMY 
# The bit-precision of embedding table
ev_precs = 32
# To get the number of unique value per table as recognized by the model
training_config_path = "/should/point/to/training_config_path" # get the value from dlrm_pytorch_c1

rocksdb_client = None
sqlite_client = None

def init_socket_client():
    global arr_socket_conn
    for i in range(NSOCKET):
        s_conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s_conn.connect(('127.0.0.1', PORTS[i]))    
        arr_socket_conn.append(s_conn)
    print("All sockets are connected! " + str(PORTS))

# DISCONTINUED for good :)
def get_val_from_storage_by_key(key):
    print("ERROR: Please use the get_value by tableId and rowId")
    exit()

    if (storage_type == EmbStorage.ROCKSDB):
        # 2. Using RocksDB TODO
        val = rocksdb_client.getByKey(key)
    elif (storage_type == EmbStorage.SQLITE):
        # 3. Using SQLite TODO
        print("ERROR: SQLite is not ready yet")
        exit (-1)
    else :
        print("ERROR: Type of Embedding Storage is not supporting reading value by key = " + key)
        exit (-1)
    return val

# The EV-LFU caching algo is calling this function on cache-MISS
# USE THIS: PREFERABLE, because the read from file will use the tableId to locate the file!
def get_val_from_storage(tableId, rowId): 
    # DAN: Pyrockdb client should be integrated here
    # key Format: #EVTABLE-#INDEX
    if (storage_type == EmbStorage.DUMMY) :
        # 1. Using dummyMemStor
        val = storage_dummy.get(tableId, rowId)
    elif (storage_type == EmbStorage.ROCKSDB):
        # 2. Using rocksdb 
        val = rocksdb_client.get(tableId, rowId)
    elif (storage_type == EmbStorage.FILEPY):
        # 3. Using plain file 
        val = file_read.get(tableId, rowId)
    elif (storage_type == EmbStorage.MMAPFILEPY):
        # 4. Using plain mmap file 
        val = mmap_file_read.get(tableId, rowId)
    elif (storage_type == EmbStorage.SQLITE):
        # 5. Using SQLite 
        val = sqlite_client.get(tableId, rowId)
    else :
        print("ERROR: Type of Embedding Storage is invalid!")
        exit (-1)
    return val

def get_arr_val_from_storage(keys):
    # This if-else condition is cheap! no need to remove (sqlite: from 115 to 114 s)
    arr_values = []
    # key Format: #EVTABLE-#INDEX
    if (storage_type == EmbStorage.DUMMY) :
        # 1. Using dummyMemStor
        for tableId, rowId in keys:
            arr_values.append(storage_dummy.get(tableId, rowId))
    elif (storage_type == EmbStorage.ROCKSDB):
        # 2. Using rocksdb 
        for tableId, rowId in keys:
            arr_values.append(rocksdb_client.get(tableId, rowId))
    elif (storage_type == EmbStorage.FILEPY):
        # 3. Using plain file 
        for tableId, rowId in keys:
            arr_values.append(file_read.get(tableId, rowId))
    elif (storage_type == EmbStorage.MMAPFILEPY):
        # 4. Using plain mmap file 
        for tableId, rowId in keys:
            arr_values.append(mmap_file_read.get(tableId, rowId))
    elif (storage_type == EmbStorage.SQLITE):
        # 5. Using SQLite 
        for tableId, rowId in keys:
            arr_values.append(sqlite_client.get(tableId, rowId))
    else :
        print("ERROR: Type of Embedding Storage is invalid!")
        exit (-1)
    return arr_values

def request_to_emb_storage(group_rowIds, use_gpu = False):
    # Send request straight to the embedding storage, by passing the caching
    emb_weights = []
    for i, rowId in enumerate(group_rowIds):
        # The tableId is started at 1 instead of 0
        # val = get_val_from_storage_by_key(str(i + 1) + "-" + str(rowId))
        val = get_val_from_storage(i + 1, rowId)
        # convert list of embedding values to tensor 
        ev_tensor = torch.FloatTensor([val]) # val is a python list
        ev_tensor.requires_grad = True
        if (use_gpu):
            # This code assume that we only run this on a single GPU node
            ev_tensor = ev_tensor.to(torch.device("cuda:0"))
        emb_weights.append(ev_tensor)
    return -1, emb_weights

def load_ev_table_into_emb_stor(ev_path_c1, overwrite_db = True):
    # we need to overwrite_db if the ev_path is different than the last one
    if (storage_type == EmbStorage.DUMMY) :
        storage_dummy.load(ev_path_c1)
    elif (storage_type == EmbStorage.ROCKSDB):
        global rocksdb_client
        rocksdb_client = RocksDBClient()
        if (overwrite_db):
            rocksdb_client.load(ev_path_c1, ev_precs)
        rocksdb_client.open_db_conn()
    elif (storage_type == EmbStorage.FILEPY):
        file_read.open_files_as_binary(ev_path_c1)
    elif (storage_type == EmbStorage.MMAPFILEPY):
        mmap_file_read.open_files_as_binary(ev_path_c1)
    elif (storage_type == EmbStorage.SQLITE):
        global sqlite_client
        sqlite_client = SQLiteClient()
        if (overwrite_db):
            sqlite_client.load(ev_path_c1, ev_precs)
        sqlite_client.open_db_conn()
    elif (storage_type == EmbStorage.FILEC):
        print("Using C open bin file!")
        EvLFU.cload_ev_tables()
        print("Loading compelete!!")
    else :
        print("ERROR: Type of Embedding Storage is invalid!")
        exit (-1)

def close_any_db_conn():
    if (storage_type == EmbStorage.ROCKSDB):
        # Using RocksDB TODO
        rocksdb_client.close_db_conn()
    elif (storage_type == EmbStorage.FILEPY):
        file_read.close()
    elif (storage_type == EmbStorage.MMAPFILEPY):
        mmap_file_read.close()
    elif (storage_type == EmbStorage.SQLITE):
        # Using SQLite TODO
        # print("ERROR: SQLite is not ready yet")
        # exit (-1)
        sqlite_client.close_db_conn()
    elif (storage_type == EmbStorage.FILEC):
        EvLFU.cclose_ev_tables()
    else :
        print("INFO: No connection to be closed! storage type = " + str(storage_type))
    print("All db connections are closed!")

# UNUSED
def request_to_multi_socket_storage(group_rowIds, use_gpu = False):
    # Send request straight to the embedding storage, by passing the caching
    emb_weights = []
    arr_vals = []
    if (use_gpu):
        # This code assume that we only run this on a single GPU node
        device = torch.device("cuda:0")
    group_keys = []
    for i, rowId in enumerate(group_rowIds):
        # The tableId is started at 1 instead of 0
        group_keys.append(str(i+1) + "-" + str(rowId))
    
    # Send to multiple sockets
    arr_socket_conn[0].sendall(str.encode("\n".join(group_keys)))
    # arr_socket_conn[1].sendall(str.encode("\n".join(group_keys[15:26])))
    # arr_socket_conn[2].sendall(str.encode("\n".join(group_keys[10:15])))
    # arr_socket_conn[3].sendall(str.encode("\n".join(group_keys[15:20])))
    # arr_socket_conn[4].sendall(str.encode("\n".join(group_keys[20:26])))

    # read emb values 
    for i in range(26):
        blob = arr_socket_conn[0].recv(144)
        val = struct.unpack('f'*36, blob[0:144])
        ev_tensor = torch.FloatTensor([val]) # val is a python list
        ev_tensor.requires_grad = True
        if (True):
            ev_tensor = ev_tensor.to(device)
        emb_weights.append(ev_tensor)
    # for i in range(11):
    #     blob = arr_socket_conn[1].recv(144)
    #     arr_vals.append(struct.unpack('f'*36, blob[0:144]))
    # for i in range(5):
    #     blob = arr_socket_conn[2].recv(144)
    #     arr_vals.append(struct.unpack('f'*36, blob[0:144]))
    # for i in range(5):
    #     blob = arr_socket_conn[3].recv(144)
    #     arr_vals.append(struct.unpack('f'*36, blob[0:144]))
    # for i in range(6):
    #     blob = arr_socket_conn[4].recv(144)
    #     arr_vals.append(struct.unpack('f'*36, blob[0:144]))

    # convert list of embedding values to tensor 
    # for val in arr_vals:
        # ev_tensor = torch.FloatTensor([val]) # val is a python list
        # ev_tensor.requires_grad = True
        # if (use_gpu):
        #     ev_tensor = ev_tensor.to(device)
        # emb_weights.append(ev_tensor)
    return -1, emb_weights
