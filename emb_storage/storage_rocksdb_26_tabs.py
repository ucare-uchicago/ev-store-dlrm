import pyrocksdb
import time
import os
import pandas as pd
import argparse
import struct
import numpy as np
from array import *
from tqdm import tqdm
import struct
import torch
import shutil
from pathlib import Path
import sys
sys.path.append('../../')

import evstore_utils
import storage_manager

ROCKSDB_DB_PATH = "/mnt/extra/db-ev-storage/rocksdb/"
BINARY_DIR_NAME = "binary/"
TOTAL_EV_TABLE = 26
EV_DIMENSION = 36

class RocksDBClient:

    # will read the BINARY values from the rocksdb
    def get(self, tableId, rowId):
        opts = pyrocksdb.ReadOptions()
        # assert(tableId >= 1)
        # assert(tableId <= TOTAL_EV_TABLE)
        # tableId started at 1, but the db connection started at id 0
        blob = self.arr_db_conn[tableId - 1].get(self.read_opts, str(rowId))
        # convert to float list
        return struct.unpack('f'*36, blob.data[0:144])

    def open_db_conn(self):
        print("Will prepare db connection")
        opts = pyrocksdb.Options()
        # for multi-thread
        opts.IncreaseParallelism()
        opts.OptimizeLevelStyleCompaction()
        for i in range(TOTAL_EV_TABLE):
            db_conn = pyrocksdb.DB()
            status = db_conn.open(opts, os.path.join(ROCKSDB_DB_PATH, "ev-table-" + str(i+1) + ".db"))
            assert(status.ok())
            self.arr_db_conn.append(db_conn)
        print("All db connections are ready!")

    def close_db_conn(self):
        print("Closing rocksdb connections")
        for db_conn in self.arr_db_conn:
            db_conn.close()

    def get_nrows_pertable(self, file_path):
        _, _, _, ln_emb, _ = evstore_utils.read_training_config(file_path)
        return ln_emb

    def load(self, ev_dir, bit_precision = 32):
        # delete the db dir if exists
        if os.path.exists(ROCKSDB_DB_PATH) and os.path.isdir(ROCKSDB_DB_PATH):
            shutil.rmtree(ROCKSDB_DB_PATH)
        # recreate the dir to hold new rocksdb data
        Path(os.path.join(ROCKSDB_DB_PATH)).mkdir(parents=True, exist_ok=True)

        print("**************** Loading EV Table to ROCKSDB")
        print("**************** Load new set of EV Table from = " + ev_dir)

        assert(bit_precision%4 == 0)
        ln_emb = self.get_nrows_pertable(storage_manager.training_config_path)

        BYTE_PRECISION = int(bit_precision/8)
        TOTAL_BYTE_PER_ROW = EV_DIMENSION * BYTE_PRECISION
        # Storing binary ev-tables to rocksDB
        for ev_idx in range(0, TOTAL_EV_TABLE):
            binFilename = "ev-table-" + str(ev_idx + 1) + ".bin"

            # RocksDB loads the BINARY EV-Tables!
            bin_ev_path = os.path.join(ev_dir, BINARY_DIR_NAME, binFilename)
            print("************* Loading EV = " + bin_ev_path)

            db = pyrocksdb.DB()
            opts = pyrocksdb.Options()
            # for multi-thread
            opts.IncreaseParallelism()
            opts.OptimizeLevelStyleCompaction()
            opts.create_if_missing = True
            dbFilename = "ev-table-" + str(ev_idx + 1) + ".db" 
            dbFilename = os.path.join(ROCKSDB_DB_PATH, dbFilename)
            #print(dbFilename)
            s = db.open(opts, dbFilename)
            assert(s.ok())
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
                    k = str(i)
                    db.put(opts, k, v)
                db.close()
                print("              === db-path: " + dbFilename)
            f.close()
        print("**************** All EvTable loaded in the RocksDB!")
    
    def __init__(self):
        self.arr_db_conn = []
        self.read_opts = pyrocksdb.ReadOptions()

