import collections
import torch
import sys
sys.path.append('emb_storage')
import storage_manager
cap = -1
least_freq = 1
# to store the frequency of the keys 
node_for_freq = dict()
# to search the key within all the cached keys
node_for_key = dict()

def init(capacity):
    global cap 
    cap = capacity
    node_for_freq[1] = collections.OrderedDict() # For the frequency == 1

def _update( key, value, freq):
    # increment the frequency
    global node_for_key, node_for_freq, least_freq
    # remove the key from the old frequency
    node_for_freq[freq].pop(key)

    if len(node_for_freq[least_freq]) == 0:
        # update the least_freq if there is no more item in this frequency list
        least_freq += 1
        node_for_freq.pop(least_freq)
    
    # update frequency
    node_for_key[key][1] = freq + 1
    if ((freq + 1) not in node_for_freq.keys()):
        node_for_freq[freq + 1] = collections.OrderedDict()
    node_for_freq[freq + 1][key] = ""

def set( key, value):
    global node_for_key, node_for_freq, cap, least_freq

    # check if full
    if (len(node_for_key) >= cap):
        # evict 1 item
        key_to_remove = node_for_freq[least_freq].popitem(last=False)
        node_for_key.pop(key_to_remove)

    # Insert the new item
    node_for_key[key] = [value, 1]
    node_for_freq[1][key] = ""

    # update least freq
    least_freq = 1

def request(key, table_id, row_id):
    global node_for_key, node_for_freq
    # check if the key is cached
    if key in node_for_key:
        # Yes, get the value
        value, freq = node_for_key[key]
        # Update item's frequency
        _update(key, value, freq)
        return value, True
    else:
        # MISS: get value from secondary storage
        value = storage_manager.get_val_from_storage(table_id, row_id)
        set(key, value)
        return value, False

# Multi keys request
def request_to_lfu( group_row_ids, use_gpu = False):
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

