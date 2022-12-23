#!/usr/bin/env python
import socket
import datetime
import torch
import struct
import time
import numpy.ctypeslib as ctl
import ctypes

READ_HANDSHAKE_SIZE = 3 # 3 char to embed the buffer's length
HOST = "127.0.0.1"  # Standard loopback interface address (localhost)
PORT = 8080  # Port to listen on (non-privileged ports are > 1023)
N_EVTable = 26
EV_DIMENSION = 36

socket_conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
cache_manager_cpp = None
emb_weights_in_tensor = [None] * N_EVTable

# Calculate the overhead based on the number of ev tables (16 Bytes per item)
# n_overhead_bytes = 16 * (N_EVTable)
total_bytes_to_read = (EV_DIMENSION * 4 * N_EVTable); # + n_overhead_bytes
total_bytes_to_read_div_4 = int(total_bytes_to_read / 4)

def convert_dirty_floats_to_tensor(dirty_arr_floats, use_gpu = False):
    print("ERROR: Don't use this, because the dirty array coming from vector is NOT in the correct order");
    exit(-1)
    emb_weights_in_tensor = []
    start_idx = 0
    for table_idx in range(26):
        # The table_idx is started at 1 instead of 0
        end_idx = start_idx + 36
        # print( " start_idx " + str(start_idx) + " : " + str(end_idx))
        # ev_value = dirty_arr_floats[start_idx: end_idx]
        # print( " ev_value " + str(table_idx) + " :: " + str(ev_value))
        # convert list of embedding values to tensor 
        ev_tensor = torch.FloatTensor([dirty_arr_floats[start_idx: end_idx]]) # val is a python list
        ev_tensor.requires_grad = True
        if (use_gpu):
            # This code assume that we only run this on a single GPU node
            ev_tensor = ev_tensor.to(torch.device("cuda:0"))
        emb_weights_in_tensor.append(ev_tensor)
        
        # Include the 16 bytes overhead (dirt); or 4 dirty-floats
        start_idx = end_idx + 4
    return emb_weights_in_tensor

def convert_clean_floats_to_tensor(clean_arr_floats, use_gpu = False):
    emb_weights_in_tensor = []
    start_idx = 0
    for table_idx in range(26):
        # The table_idx is started at 1 instead of 0
        end_idx = start_idx + 36
        # convert list of embedding values to tensor 
        ev_tensor = torch.FloatTensor([clean_arr_floats[start_idx: end_idx]]) # val is a python list
        if (use_gpu):
            # This code assume that we only run this on a single GPU node
            ev_tensor = ev_tensor.to(torch.device("cuda:0"))
        emb_weights_in_tensor.append(ev_tensor)
        start_idx = end_idx
    return emb_weights_in_tensor

def init_ctypes_lib():
    global cache_manager_cpp
    print("Initiating ctypes cache_manager_cpp library (libcachemanager.so) ...")
    libdir = '/mnt/extra/ev-store-dlrm/mixed_precs_caching/lib/'
    cache_manager_cpp = ctl.load_library('libcachemanager-dlrm-2.so', libdir)

    # 1. To do EV-Lookup
    cache_manager_cpp.ev_lookup_based_on_list_keys.argtypes = [ctypes.POINTER(ctypes.c_int)]
    cache_manager_cpp.ev_lookup_based_on_list_keys.restype = ctypes.c_int

    # 2. To get the EV-values as a long bytes of floats
    cache_manager_cpp.get_ev_values.argtypes = None
    cache_manager_cpp.get_ev_values.restype = ctypes.POINTER(ctypes.c_float)

    # 3. To print and reset the perfect hit counter
    cache_manager_cpp.print_perfect_hit.argtypes = None
    cache_manager_cpp.print_perfect_hit.restype = None

    # 4. New compact method to do ev-lookup
    cache_manager_cpp.ev_lookup.argtypes = [ctypes.POINTER(ctypes.c_int)]
    cache_manager_cpp.ev_lookup.restype = ctypes.POINTER(ctypes.c_float)

def print_n_reset_perfect_hit():
    if (cache_manager_cpp != None):
        cache_manager_cpp.print_perfect_hit()

def establish_socket_conn():
    global socket_conn
    socket_conn.connect((HOST, PORT))

    # tell the server how long our buffer gonna be
    buffer_length = b"104"
    assert(len(buffer_length) == READ_HANDSHAKE_SIZE)
    socket_conn.sendall(buffer_length)

    data = socket_conn.recv(51)
    print("Client received handshake message: " + str(data))
    print("   len = " + str(len(data)))

def cache_lookup_via_socket(group_rowIds):
    # Convert to bytes (104B)
    # print(str(group_rowIds))
    ev_keys_in_blob = struct.pack('%si' % N_EVTable, *group_rowIds)
    # print("Sending group_rowIds [" + str(len(group_rowIds)) + "] = " + str(group_rowIds))
    socket_conn.sendall(ev_keys_in_blob)
    # print("len(blob) sent to server = " + str(len(ev_keys_in_blob)))

    # Read as much data from the socket
    received_buffer_bytes = socket_conn.recv(total_bytes_to_read)
    # print("total_bytes_to_read:  " + str(total_bytes_to_read))
    # print("Client received emb_weights:  " + str(len(received_buffer_bytes)))
    clean_arr_floats = struct.unpack('f' * total_bytes_to_read_div_4, received_buffer_bytes) 
    # for idx in range (0, 26):
    #     print(str(clean_arr_floats[idx*36 + 0]))
    return clean_arr_floats

def cache_lookup_via_ctypes(group_rowIds):
    return cache_manager_cpp.ev_lookup((ctypes.c_int * N_EVTable)(* group_rowIds))

    # group_rowIds_c = (ctypes.c_int * N_EVTable)(* group_rowIds)
    # # 1. Do EV-Lookup
    # cache_manager_cpp.ev_lookup_based_on_list_keys(group_rowIds_c)

    # # 2. Get the EV-values as a long bytes of floats
    # return cache_manager_cpp.get_ev_values()

def request_to_cpp_cache(group_rowIds, use_gpu = False, use_socket = False, evstore_gpu_id = 0):
    # print("--- at request_to_cpp_cache " + str(group_rowIds))
    if (use_socket): 
        # socket is SLOW (made up 50% of the total latency)
        clean_arr_floats = cache_lookup_via_socket(group_rowIds)
    else:
        # will use the ctypes library
        clean_arr_floats = cache_lookup_via_ctypes(group_rowIds)
    start_idx = 0
    for table_idx in range(N_EVTable):
        # The table_idx is started at 1 instead of 0
        end_idx = start_idx + EV_DIMENSION
        if (use_gpu):
            # This code assume that we only run this on a single GPU node
            
            # A. Create tensor directly in GPU
            # ev_tensor = torch.tensor([clean_arr_floats[start_idx: end_idx]], device=torch.device('cuda:0'))
            
            # B. Create tensor in CPU then move it to GPU [2s Faster]
            ev_tensor = torch.FloatTensor([clean_arr_floats[start_idx: end_idx]]) # val is a python list
            # ev_tensor = ev_tensor.to(torch.device("cuda:0"))
            ev_tensor = ev_tensor.to(torch.device("cuda:" + str(evstore_gpu_id)))
        else:
            # convert list of embedding values to tensor 
            ev_tensor = torch.FloatTensor([clean_arr_floats[start_idx: end_idx]]) # val is a python list
        emb_weights_in_tensor[table_idx] = ev_tensor
        start_idx = end_idx

    return emb_weights_in_tensor
    # return convert_clean_floats_to_tensor(clean_arr_floats, use_gpu)    

def request_to_cpp_cache_old_buggy(group_rowIds, use_gpu = False):
    # Convert to bytes (104B)
    # print(str(group_rowIds))
    ev_keys_in_blob = struct.pack('%si' % N_EVTable, *group_rowIds)
    # print("Sending group_rowIds [" + str(len(group_rowIds)) + "] = " + str(group_rowIds))
    socket_conn.sendall(ev_keys_in_blob)
    # print("len(blob) sent to server = " + str(len(ev_keys_in_blob)))

    # Read as much data from the socket
    received_buffer_bytes = socket_conn.recv(total_bytes_to_read)
    if (len(received_buffer_bytes) != total_bytes_to_read):
        # print("Client received per-table emb_weights: 26 * " + str(len(received_buffer_bytes)))
        total_bytes_per_item = len(received_buffer_bytes)
        clean_arr_floats = []
        # read per 144 bytes 
        clean_arr_floats += struct.unpack('f' * int(total_bytes_per_item/4) , received_buffer_bytes) 
        for table_idx in range (1, 26):
            received_buffer_bytes = socket_conn.recv(total_bytes_per_item)
            clean_arr_floats += struct.unpack('f' * int(total_bytes_per_item/4) , received_buffer_bytes) 
        return convert_clean_floats_to_tensor(clean_arr_floats, use_gpu)    
    else :  
        # print("Client received aggregated emb_weights: " + str(len(received_buffer_bytes)))
        # Deserialize the array of floats; must consider the 4bytes vector overhead!!
        dirty_arr_floats = struct.unpack('f' * int(total_bytes_to_read/4) , received_buffer_bytes) 
        return convert_dirty_floats_to_tensor(dirty_arr_floats, use_gpu)    

def run_test():
    global socket_conn
    group_rowIds = [3, 37, 113824, 54827, 6, 0, 112, 1, 0, 1188, 107, 102200, 105, 0, 73, 82752, 0, 49, 58, 3, 93967, 0, 9, 1886, 3, 1436]
    request_to_cpp_cache(group_rowIds)

def run_test_xx():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((HOST, PORT))
        buffer_length = b"104"
        assert(len(buffer_length) == READ_HANDSHAKE_SIZE)
        s.sendall(buffer_length)

        data = s.recv(51)
        print("Client received handshake message: " + str(data))
        print("len = " + str(len(data)))

        # Send dummy grouped keys
        # group_rowIds = []
        group_rowIds = [3, 37, 113824, 54827, 6, 0, 112, 1, 0, 1188, 107, 102200, 105, 0, 73, 82752, 0, 49, 58, 3, 93967, 0, 9, 1886, 3, 1436]
        N_EVTable = 26
        # for x in range(N_EVTable):
        #     key = 0  # Will always get row 0
        #     group_rowIds.append(key) 
        print("Sending group_rowIds [" + str(len(group_rowIds)) + "] = " + str(group_rowIds))
        
        # Convert to bytes (104B)
        ev_keys_in_blob = struct.pack('%si' % N_EVTable, *group_rowIds)
        s.sendall(ev_keys_in_blob)
        print("len(blob) sent to server = " + str(len(ev_keys_in_blob)))

        # Read as much data from the socket
        received_buffer_bytes = s.recv(total_bytes_to_read)
        print("Client received emb_weights: " + str(len(received_buffer_bytes)))

        # Deserialize the array of floats; must consider the 4bytes vector overhead!!
        dirty_arr_floats = struct.unpack('f' * int(total_bytes_to_read/4) , received_buffer_bytes) 

        emb_weights_in_tensor = convert_dirty_floats_to_tensor(dirty_arr_floats)    

        # print("Exit client")
        # exit()

        n_request = 100000
        t1 = datetime.datetime.now()
        for x in range(n_request):
            s.sendall(ev_keys_in_blob)
            data = s.recv(total_bytes_to_read)
            emb_weights_in_tensor = convert_dirty_floats_to_tensor(dirty_arr_floats)    
            # print("Received " + str(len(data)))
        t2 = datetime.datetime.now()
        elapsed_time = t2 - t1
        print("n_request = " + str(n_request) + "")
        print("elapsed time = " + str(elapsed_time.total_seconds()) + " s")

    # this version is sending real format (grouped-keys) data to server_v4


if __name__ == "__main__":
    establish_socket_conn()
    run_test()
