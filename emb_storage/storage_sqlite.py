import sqlite3
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

SQLITE_DB_DIR = "/mnt/extra/db-ev-storage/sqlite/"
BINARY_DIR_NAME = "binary/"
TOTAL_EV_TABLE = 26
EV_DIMENSION = 36
DB_NAME = "ev-table-all.db"

class SQLiteClient:

    # will read the BINARY values from the SQLiteDB
    def get(self, tableId, rowId):
        # TableId start from index 1
        # assert(tableId >= 1)
        # assert(tableId <= TOTAL_EV_TABLE)
        # The row at SQLite is started from 1 instead of 0
        realRowId = rowId + 1 + self.db_add_up_tables[tableId-1]
        blob = self.db_cursor.execute("SELECT * FROM tab1 where rowid={};".format(realRowId)).fetchone()
        # print(tableId)
        # print(rowId)
        # print(blob)
        # assert(blob != None)
        return struct.unpack('f'*EV_DIMENSION, blob[0])

    def get_nrows_pertable(self, file_path):
        _, _, _, ln_emb, _ = evstore_utils.read_training_config(file_path)
        return ln_emb

    def load(self, ev_dir, bit_precision = 32):
        # delete the db dir if exists
        if os.path.exists(SQLITE_DB_DIR) and os.path.isdir(SQLITE_DB_DIR):
            shutil.rmtree(SQLITE_DB_DIR)
        # recreate the dir to hold new sqlite data
        Path(os.path.join(SQLITE_DB_DIR)).mkdir(parents=True, exist_ok=True)
        db = sqlite3.connect(self.db_file_path)
        db_cursor = db.cursor()

        print("**************** Loading EV Table to SQLite")
        print("**************** Load new set of EV Table from = " + ev_dir)

        assert(bit_precision%4 == 0)
        ln_emb = self.get_nrows_pertable(storage_manager.training_config_path)

        BYTE_PRECISION = int(bit_precision/8)
        TOTAL_BYTE_PER_ROW = EV_DIMENSION * BYTE_PRECISION
        table_name = "tab1"
        # Storing binary ev-tables to SQLite
        for ev_idx in range(0, TOTAL_EV_TABLE):
            bin_filename = "ev-table-" + str(ev_idx + 1) + ".bin"
            # table_name = "ev_table_" + str(ev_idx + 1)

            db_cursor.execute("CREATE TABLE if not exists " + table_name + " (b BLOB);")

            # SQLite loads the BINARY EV-Tables!
            bin_ev_path = os.path.join(ev_dir, BINARY_DIR_NAME, bin_filename)
            print("************* Loading EV = " + bin_ev_path)
            # put
            with open(bin_ev_path, 'rb') as f:
                data = f.read()
                num_of_indexes = len(data) // TOTAL_BYTE_PER_ROW

                # Verify that the number of unique values per table is the same as what the DLRM model expect
                assert(ln_emb[ev_idx] == num_of_indexes)
                
                bin_ev_path = "/home/cc/ev-tables-sqlite/bin_workload"

                #for nrow in tqdm(range(0, num_of_indexes)):
                for i in range(0, num_of_indexes):
                    # put
                    byte_offset = BYTE_PRECISION * i * EV_DIMENSION # 36 -> dimension
                    v = data[ byte_offset : byte_offset + TOTAL_BYTE_PER_ROW]
                    k = str(ev_idx+1) + "-" + str(i)
                    db_cursor.execute("insert into " + table_name + " values(?)", (v, ))
                print("              === db-path: " + table_name)
            f.close()
        print("**************** All EvTable loaded in the SQLite!")
        db.commit()
        db.close()

    def open_db_conn(self):
        print("Will prepare db connection")
        self.db_conn = sqlite3.connect(self.db_file_path)
        self.db_cursor = self.db_conn.cursor()
        print("All db connections are ready!")

    def close_db_conn(self):
        print("Closing sqlite connections")
        self.db_conn.close()

    def __init__(self):
        self.db_conn = None
        self.db_cursor = None
        self.db_file_path = os.path.join(SQLITE_DB_DIR, DB_NAME)
        self.db_ln_tables = self.get_nrows_pertable(storage_manager.training_config_path)
        self.db_add_up_tables = [0 for _ in range(len(self.db_ln_tables))]
        for i in range(len(self.db_ln_tables)-1):
            self.db_add_up_tables[i+1] = self.db_add_up_tables[i] + self.db_ln_tables[i]
