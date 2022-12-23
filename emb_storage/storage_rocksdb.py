import pyrocksdb
import time
import os
import pandas as pd
import argparse
import struct
import numpy as np
from array import *
from tqdm import tqdm
import torch
import shutil
from pathlib import Path
import sys
sys.path.append('../../')

import evstore_utils
import storage_manager

ROCKSDB_DB_DIR = "/mnt/extra/db-ev-storage/rocksdb/"
BINARY_DIR_NAME = "binary/"
TOTAL_EV_TABLE = 26
EV_DIMENSION = 36

class RocksDBClient:

    # will read the BINARY values from the rocksdb
    def get(self, tableId, rowId):
        # TableId start from index 1
        # assert(tableId >= 1)
        # assert(tableId <= TOTAL_EV_TABLE)
        # tableId started at 1, but the db connection started at id 0
        blob = self.db_conn.get(self.read_opts, str(tableId) + "-" + str(rowId))
        # convert to float list
        # return struct.unpack('f'*EV_DIMENSION, blob.data[0:144])
        return struct.unpack('f'*EV_DIMENSION, blob.data)
    
    # will read the BINARY values from the rocksdb
    def getByKey(self, key):
        # TableId start from index 1
        # tableId started at 1, but the db connection started at id 0
        blob = self.db_conn.get(self.read_opts, key)
        # convert to float list
        print(blob)
        val = struct.unpack('f'*EV_DIMENSION, blob.data)
        print(val)
        exit()
        return val

    def open_db_conn(self):
        print("Will prepare db connection")
        opts = pyrocksdb.Options()
        # for multi-thread
        opts.IncreaseParallelism()
        opts.OptimizeLevelStyleCompaction()
        self.db_conn = pyrocksdb.DB()
        status = self.db_conn.open(opts, os.path.join(ROCKSDB_DB_DIR, "ev-table-all.db"))
        assert(status.ok())
        print("All db connections are ready!")

    def close_db_conn(self):
        print("Closing rocksdb connections")
        self.db_conn.close()

    def get_nrows_pertable(self, file_path):
        _, _, _, ln_emb, _ = evstore_utils.read_training_config(file_path)
        return ln_emb

    def load(self, ev_dir, bit_precision = 32):
        # delete the db dir if exists
        if os.path.exists(ROCKSDB_DB_DIR) and os.path.isdir(ROCKSDB_DB_DIR):
            shutil.rmtree(ROCKSDB_DB_DIR)
        # recreate the dir to hold new rocksdb data
        Path(os.path.join(ROCKSDB_DB_DIR)).mkdir(parents=True, exist_ok=True)

        print("**************** Loading EV Table to ROCKSDB")
        print("**************** Load new set of EV Table from = " + ev_dir)

        assert(bit_precision%4 == 0)
        ln_emb = self.get_nrows_pertable(storage_manager.training_config_path)

        BYTE_PRECISION = int(bit_precision/8)
        TOTAL_BYTE_PER_ROW = EV_DIMENSION * BYTE_PRECISION

        db = pyrocksdb.DB()
        opts = pyrocksdb.Options()
        # for multi-thread
        opts.IncreaseParallelism()
        opts.OptimizeLevelStyleCompaction()
        opts.create_if_missing = True
        db_filename = "ev-table-all.db" 
        db_filename = os.path.join(ROCKSDB_DB_DIR, db_filename)
        #print(db_filename)
        s = db.open(opts, db_filename)
        assert(s.ok())
        
        # Storing binary ev-tables to rocksDB
        for ev_idx in range(0, TOTAL_EV_TABLE):
            bin_filename = "ev-table-" + str(ev_idx + 1) + ".bin"

            # RocksDB loads the BINARY EV-Tables!
            bin_ev_path = os.path.join(ev_dir, BINARY_DIR_NAME, bin_filename)
            print("************* Loading EV = " + bin_ev_path)

            # put
            with open(bin_ev_path, 'rb') as f:
                data = f.read()
                num_of_indexes = len(data) // TOTAL_BYTE_PER_ROW

                # Verify that the number of unique values per table is the same as what the DLRM model expect
                assert(ln_emb[ev_idx] == num_of_indexes)

                opts = pyrocksdb.WriteOptions()
                #for nrow in tqdm(range(0, num_of_indexes)):
                for i in range(0, num_of_indexes):
                    # put
                    byte_offset = BYTE_PRECISION * i * EV_DIMENSION # 36 -> dimension
                    v = data[ byte_offset : byte_offset + TOTAL_BYTE_PER_ROW]
                    k = str(ev_idx+1) + "-" + str(i)
                    db.put(opts, k, v)
                print("              === db-path: " + db_filename)
            f.close()
        print("**************** All EvTable loaded in the RocksDB!")
        db.close()
    
    def __init__(self):
        self.db_conn = None
        self.read_opts = pyrocksdb.ReadOptions()

