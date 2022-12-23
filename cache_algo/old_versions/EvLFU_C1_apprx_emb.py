import torch
import sys
sys.path.append('emb_storage')
import storage_manager
import random

###########################################EvLFU##########################################
cap_C1 = -1
min_C1 = 0
vals_C1 = dict() # each value is [embedding value, agg_hit]
lists_C1 = dict() # will group the keys based on the agg_hit (count)
# flushing part:
n_perfect_item_C1 = 0
flush_rate_C1 = 0.4
perfect_item_cap_C1 = 1.0
max_perfect_item_C1 = 0

def init(capacity):
    global lists_C1, max_perfect_item_C1, perfect_item_cap_C1, cap_C1
    cap_C1 = capacity
    # initializing the dict
    i = 0
    while (i <= 26):
        lists_C1[i] = []
        i += 1
    max_perfect_item_C1 = int(cap_C1 * perfect_item_cap_C1)

# Inserting the NEW key
def set(key, value, agg_hit):
    global cap_C1, min_C1, vals_C1, lists_C1, n_perfect_item_C1, max_perfect_item_C1, flush_rate_C1

    # Flushing:
    if n_perfect_item_C1 >= max_perfect_item_C1:
        print("flushing!")
        print("n_perfect_item_C1 = " + str(n_perfect_item_C1))
        print("max_perfect_item_C1 = " + str(max_perfect_item_C1))
        for i in range(0, int(flush_rate_C1 * cap_C1) + 1):
            key_to_evict = lists_C1[26].pop(0)
            vals_C1.pop(key_to_evict)
        # adjust the n_perfect_item counter
        n_perfect_item_C1 = len(lists_C1[26])
    else:
        # cache is full
        if len(vals_C1) >= cap_C1:
            # make a space for the new key
            while(lists_C1[min_C1] == []):
                # find the right key to pop
                # Update minimum agg_hit
                min_C1 += 1
                if (min_C1 > 26):
                    min_C1 = 1
            key_to_evict = lists_C1[min_C1].pop(0)
            vals_C1.pop(key_to_evict)

    # insert the new value:
    vals_C1[key] = [value, agg_hit]
    lists_C1[agg_hit].append(key) # ========

    if agg_hit < min_C1:
        min_C1 = agg_hit
    
def update_agg_hit(key, agg_hit):  # Get From Mem
    global vals_C1, lists_C1
    ev_vals = vals_C1.get(key)
    if ev_vals is None:
        return None
    # old_agg_hit = ev_vals[1]
    if ev_vals[1] < agg_hit:
        # update the old agg_hit
        lists_C1[ev_vals[1]].remove(key)
        lists_C1[agg_hit].append(key) # ========
        vals_C1[key][1] = agg_hit
        # Increase the min_freq if the current lists freq is []
            # Nope, the new aggHit can jump, No need to do anything
    return ev_vals[0]

# Updating the existing keys and inserting the missing keys
def update(key, table_id, row_id, agg_hit, missing_value = None):
    # TODO: This can be done in multi threaded way (on Java and C++)
    # Get value from EV-LFU cache
    val = update_agg_hit(key, agg_hit)
    if val:
        return val
    else:
        # On MISS: Get value from secondary storage
        # DON't put "IF CONDITION" HERE! IT IS SLOW!
        if missing_value is None:
            # this key might be kicked out while inserting previous key
            missing_value = storage_manager.get_val_from_storage(table_id, row_id)
            # missing_value = storage_manager.get_val_from_storage_by_key(key) #only on rocksdb 
        set(key, missing_value, agg_hit)
        return missing_value

def request_to_ev_lfu( group_row_ids, use_gpu = False, approx_emb_thres = -1, ev_dim = 36):
    arr_record_hit = []
    arr_group_keys = []
    arr_missing_keys = []
    arr_missing_values = []
    arr_emb_weights = []
    pick_random_ev = False
    random_ev_value = []
    # Generate random emb value for approximate embedding logic
    for idx in range(0, ev_dim):
        random_ev_value.append(random.uniform(-0.09, 0.09))
    agg_hit = 0
    global vals_C1, lists_C1, n_perfect_item_C1
    for i, row_id in enumerate(group_row_ids):
        # Table_id is started at 1
        # Key for row3 of table1 is 1-3
        key = str(i+1) + "-" + str(row_id)
        arr_group_keys.append(key)
        if key in vals_C1.keys():
            arr_record_hit.append(True)
            agg_hit += 1
        else:
            arr_missing_keys.append([i+1, row_id])
            arr_record_hit.append(False)
    
    if (approx_emb_thres > 0 and agg_hit >= approx_emb_thres):
        # Approximate embedding logic phase 1
        # The missing emb value will be randomly picked from memory
        pick_random_ev = True
    else: 
        # Get all missing keys from storage at once
        arr_missing_values = storage_manager.get_arr_val_from_storage(arr_missing_keys)

    if (use_gpu):
        # This code assume that we only run this on a single GPU node
        device = torch.device("cuda:0")

    # Update
    for i, row_id in enumerate(group_row_ids):
        # TODO: C++ and java code should do this in multithreaded way
        # The table_id is started at 1 instead of 0
        if (arr_record_hit[i]):
            val = update(arr_group_keys[i], i + 1, row_id, agg_hit) 
            random_ev_value = val # will be used by approximate emb logic
        else:
            if (pick_random_ev):
                # Approximate embedding logic phase 2
                # Pick random ev value from the memory, yes the accuracy might drop
                # There are many ways to get this random values; This alone can be deeply researched
                # 1. random.choice(list(vals_C1.values())) # Potentially SLOW
                # 2. Just randomize it 
                # random.uniform(-0.9, 0.9)
                # 3. Generate list of random ev at the init(), then pick one
                # 4. Just use the previous value
                val = tuple(random_ev_value)
                arr_record_hit[i] = True # Turn the MISS into a hit
            else:
                # plug the missing values that we get from secondary storage
                val = update(arr_group_keys[i], i + 1, row_id, agg_hit, arr_missing_values.pop(0)) 
        # convert list of embedding values to tensor 
        ev_tensor = torch.FloatTensor([val]) # val is a python list
        ev_tensor.requires_grad = True
        if (use_gpu):
            ev_tensor = ev_tensor.to(device)
        arr_emb_weights.append(ev_tensor)

    if agg_hit == 26:
        # update the number of perfect item
        n_perfect_item_C1 = len(lists_C1[26])
    return arr_record_hit, arr_emb_weights
