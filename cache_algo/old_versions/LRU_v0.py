import pandas as pd
import torch
import sys
sys.path.append('emb_storage')
import storage_manager
from functools import lru_cache

cap = -1
vals = dict() # each item is a dictionary embedding value
lru_list = [] # store the order of the keys in LRU manner

def init(capacity):
    global cap 
    cap = capacity

# Inserting the NEW key
def set(key, value):
    global vals, lru_list, cap

    if (len(lru_list) >= cap):
        # evicting the first key in LRU list
        vals.pop(lru_list.pop(0))
    
    # inserting new key
    vals[key] = value
    lru_list.append(key)

# single key request
def request(key, table_id, row_id):
    global vals, lru_list
    is_hit = False
    value = vals.get(key)
    if (value == None):
        # MISS: get value from secondary storage
        value = storage_manager.get_val_from_storage(table_id, row_id)
        set(key, value)
    else:
        # update the position; put it in the back of the Q
        lru_list.remove(key)
        lru_list.append(key)
        is_hit = True
    return value, is_hit

@lru_cache(maxsize=5000)
def request_memoization(key, table_id, row_id):
    # MISS: get value from secondary storage
    value = storage_manager.get_val_from_storage(table_id, row_id)
    set(key, value)
    return value, False

# Multi keys request
def request_to_lru( group_row_ids, use_gpu = False):
    arr_record_hit = []
    arr_emb_weights = []
    agg_hit = 0
    if (use_gpu):
        # This code assume that we only run this on a single GPU node
        device = torch.device("cuda:0")

    for i, row_id in enumerate(group_row_ids):
        # Table_id is started at 1
        # Key for row3 of table1 is 1-3
        key = str(i+1) + "-" + str(row_id)
        # val, is_hit = request_memoization(key, i+1, row_id)
        val, is_hit = request(key, i+1, row_id)
        # convert list of embedding values to tensor 
        ev_tensor = torch.FloatTensor([val]) # val is a python list
        ev_tensor.requires_grad = True
        if (use_gpu):
            # This code assume that we only run this on a single GPU node
            ev_tensor = ev_tensor.to(device)
        arr_emb_weights.append(ev_tensor)
        if is_hit:
            arr_record_hit.append(True)
            agg_hit += 1
        else:
            arr_record_hit.append(False)
    
    return arr_record_hit, arr_emb_weights

