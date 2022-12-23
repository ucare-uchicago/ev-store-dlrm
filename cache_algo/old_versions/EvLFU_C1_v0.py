import pandas as pd
import torch
import sys
sys.path.append('emb_storage')
import storage_manager

###########################################EvLFU##########################################
cap_C1 = 500
min_C1 = 0
vals_C1 = dict()
counts_C1 = dict()
lists_C1 = dict()
lists_C1[0] = []
# flushing part:
nPerfectItem_C1 = 0
flushRate_C1 = 0.4
perfectItemCapacity_C1 = 1.0

def init():
    pass

def set(key, value, aggHit):
    global cap_C1, min_C1, vals_C1, counts_C1, lists_C1, nPerfectItem_C1, flushRate_C1, perfectItemCapacity_C1
    if cap_C1 <= 0:
        return
    if vals_C1.get(key) is not None:
        vals_C1[key] = value
        get_val_from_mem(key, aggHit)
        return

    # Flushing:
    if nPerfectItem_C1 >= int(cap_C1 * perfectItemCapacity_C1):
        # print("flushing!")
        for i in range(0, int(flushRate_C1 * cap_C1) + 1):
            evictKey = lists_C1.get(26)[0]
            lists_C1.get(26).remove(evictKey)
            vals_C1.pop(evictKey)
            counts_C1.pop(evictKey)

        nPerfectItem_C1 = len(lists_C1.get(26))
        if len(vals_C1) < cap_C1:
            min_C1 = aggHit

    # key allows to insert in the cache:
    if len(vals_C1) >= cap_C1:
        evictKey = lists_C1.get(min_C1)[0] # TODO: Use pop!!
        # print("lists_C1.get(min_C1 = " + str(min_C1) + ") = " + str(lists_C1.get(min_C1)))
        lists_C1.get(min_C1).remove(evictKey)
        try:
            vals_C1.pop(evictKey)
        except:
            print("KeyError when vals_C1.pop key =" + str(evictKey))
            print(vals_C1.keys())
            print("cap_C1 " + str(cap_C1))
            print(lists_C1.get(min_C1))
            exit(-1)
        try:
            counts_C1.pop(evictKey)
        except:
            print("KeyError when counts_C1.pop key =" + str(evictKey))
            exit(-1)

    # If the key is new, insert the value:
    vals_C1[key] = value
    counts_C1[key] = aggHit

    if lists_C1.get(aggHit) is None:
        lists_C1[aggHit] = []
        lists_C1 = dict(sorted(lists_C1.items()))
    # if (key in lists_C1[aggHit]):
    #     print("aggHit = " + str(aggHit))
    #     print(lists_C1[aggHit])
    #     print("ERROR 1: key already in lists, no need to append " + key)
    #     exit(-1)
    lists_C1.get(aggHit).append(key) # ========

    # Update minimum agghit
    if aggHit < min_C1:
        min_C1 = aggHit
    while (lists_C1.get(min_C1) is None) or len(lists_C1.get(min_C1)) == 0:
        min_C1 += 1

def get_val_from_mem(key, aggHit):  # Get From Mem
    global cap_C1, min_C1, vals_C1, counts_C1, lists_C1, nPerfectItem_C1, flushRate_C1, perfectItemCapacity_C1
    if vals_C1.get(key) is None:
        return None
    count = counts_C1.get(key)
    newCount = count
    if count < aggHit:
        newCount = aggHit
    counts_C1[key] = newCount
    lists_C1.get(count).remove(key)

    if count == min_C1:
        while (lists_C1.get(min_C1) is None) or len(lists_C1.get(min_C1)) == 0:
            min_C1 += 1
    if lists_C1.get(newCount) is None:
        lists_C1[newCount] = []
        lists_C1 = dict(sorted(lists_C1.items()))
    # if (key in lists_C1[newCount]):
    #     print("newCount = " + str(newCount))
    #     print(lists_C1[newCount])
    #     print("ERROR 3: key already in lists, no need to append " + key)
    #     exit(-1)
    lists_C1.get(newCount).append(key) # ========
    return vals_C1[key]

def update(key, tableId, rowId, aggHit, nGroup):
    # Get value from EV-LFU cache
    val = get_val_from_mem(key, aggHit)
    if val is None:
        # On MISS
        # Get value from secondary storage
        # ADDING IF CONDITION HERE IS SLOW!
        # if (storage_manager.storage_type == storage_manager.EmbStorage.DUMMY):
            # Dummy storage will always use tableid + rowId because the data are stored in 26 tables
        val = storage_manager.get_val_from_storage(tableId, rowId)
        # else :
            # faster for rocksdb
            # val = storage_manager.get_val_from_storage_by_key(key) #only on rocksdb 
        set(key, val, aggHit)
    return val

def request_to_ev_lfu( group_rowIds, use_gpu = False):
    recordHitOrMiss = []
    group_keys = []
    missing_keys = []
    emb_weights = []
    aggHit = 0
    global cap_C1, min_C1, vals_C1, counts_C1, lists_C1, nPerfectItem_C1, flushRate_C1, perfectItemCapacity_C1
    for i, rowId in enumerate(group_rowIds):
        # TableId is started at 1
        # Key for row3 of table1 is 1-3
        key = str(i+1) + "-" + str(rowId)
        group_keys.append(key)
        if vals_C1.get(key) is not None:
            recordHitOrMiss.append(True)
            aggHit += 1
        else:
            missing_keys.append(key)
            recordHitOrMiss.append(False)
    
    # TODO: Get the missing keys from storage
    # missing_keys

    if (use_gpu):
        # This code assume that we only run this on a single GPU node
        device = torch.device("cuda:0")

    for i, rowId in enumerate(group_rowIds):
        # The tableId is started at 1 instead of 0
        val = update(group_keys[i], i + 1, rowId, aggHit, len(recordHitOrMiss)) # the data could either come from EV-LFU or MemStor or PyrocksDB
        # convert list of embedding values to tensor 
        ev_tensor = torch.FloatTensor([val]) # val is a python list
        ev_tensor.requires_grad = True
        if (use_gpu):
            ev_tensor = ev_tensor.to(device)
        emb_weights.append(ev_tensor)

    if lists_C1.get(26) and not len(lists_C1.get(26)) == 0:
        nPerfectItem_C1 = len(lists_C1.get(26))

    return recordHitOrMiss, emb_weights
