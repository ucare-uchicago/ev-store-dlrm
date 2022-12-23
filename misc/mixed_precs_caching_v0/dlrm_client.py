#!/usr/bin/env python
import socket
import datetime
import torch
import struct

READ_HANDSHAKE_SIZE = 3 # 3 char to embed the buffer's length
HOST = "127.0.0.1"  # Standard loopback interface address (localhost)
PORT = 8080  # Port to listen on (non-privileged ports are > 1023)
N_EVTable = 26
EV_DIMENSION = 36

socket_conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Calculate the overhead based on the number of ev tables (16 Bytes per item)
n_overhead_bytes = 16 * (N_EVTable)
total_bytes_to_read = (EV_DIMENSION * 4 * N_EVTable) + n_overhead_bytes

def convert_dirty_floats_to_tensor(dirty_arr_floats, use_gpu = False):
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
        # print( " start_idx " + str(start_idx) + " : " + str(end_idx))
        # ev_value = clean_arr_floats[start_idx: end_idx]
        # print( " ev_value " + str(table_idx) + " :: " + str(ev_value))
        # convert list of embedding values to tensor 
        ev_tensor = torch.FloatTensor([clean_arr_floats[start_idx: end_idx]]) # val is a python list
        ev_tensor.requires_grad = True
        if (use_gpu):
            # This code assume that we only run this on a single GPU node
            ev_tensor = ev_tensor.to(torch.device("cuda:0"))
        emb_weights_in_tensor.append(ev_tensor)
    return emb_weights_in_tensor

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

def request_to_cpp_cache(group_rowIds, use_gpu = False):
    # Convert to bytes (104B)
    ev_keys_in_blob = struct.pack('%si' % N_EVTable, *group_rowIds)
    # print("Sending group_rowIds [" + str(len(group_rowIds)) + "] = " + str(group_rowIds))
    socket_conn.sendall(ev_keys_in_blob)
    # print("len(blob) sent to server = " + str(len(ev_keys_in_blob)))

    # Read as much data from the socket
    received_buffer_bytes = socket_conn.recv(total_bytes_to_read)
    if (len(received_buffer_bytes) != total_bytes_to_read):
        print("Client received per-table emb_weights: 26 * " + str(len(received_buffer_bytes)))
        total_bytes_per_item = len(received_buffer_bytes)
        clean_arr_floats = []
        # read per 144 bytes 
        clean_arr_floats += struct.unpack('f' * int(total_bytes_per_item/4) , received_buffer_bytes) 
        for table_idx in range (1, 26):
            received_buffer_bytes = socket_conn.recv(total_bytes_per_item)
            clean_arr_floats += struct.unpack('f' * int(total_bytes_per_item/4) , received_buffer_bytes) 
        return convert_clean_floats_to_tensor(clean_arr_floats, use_gpu)    
    else :  
        print("Client received aggregated emb_weights: " + str(len(received_buffer_bytes)))
        # Deserialize the array of floats; must consider the 4bytes vector overhead!!
        dirty_arr_floats = struct.unpack('f' * int(total_bytes_to_read/4) , received_buffer_bytes) 
        return convert_dirty_floats_to_tensor(dirty_arr_floats, use_gpu)    


def run_test():
    global socket_conn
    group_rowIds = [10, 255, 11, 10, 0, 0, 32, 5, 0, 1793, 31, 11, 30, 2, 849, 11, 0, 548, 1, 2, 11, 0, 0, 9, 1, 2]
    # group_rowIds = [3, 37, 113824, 54827, 6, 0, 112, 1, 0, 1188, 107, 102200, 105, 0, 73, 82752, 0, 49, 58, 3, 93967, 0, 9, 1886, 3, 1436]
    # for i in range(1500):
    request_to_cpp_cache([10,11,6421490,1504776,8,1,73,1,1,2,70,5353557,34,1,6264,3591169,8,2923,1,2,4578662,0,1,5751,1,2])
    request_to_cpp_cache([20,11,6421490,1504776,8,1,73,1,1,2,70,5353557,34,1,6264,3591169,8,2923,1,2,4578662,0,1,5751,1,2])
    request_to_cpp_cache([377,11,6421490,1504776,8,1,73,1,1,2,70,5353557,34,1,6264,3591169,8,2923,1,2,4578662,0,1,5751,1,2])
    request_to_cpp_cache([378,11,6421490,1504776,8,1,73,1,1,2,70,5353557,34,1,6264,3591169,8,2923,1,2,4578662,0,1,5751,1,2])
    request_to_cpp_cache([379,11,6421490,1504776,8,1,73,1,1,2,70,5353557,34,1,6264,3591169,8,2923,1,2,4578662,0,1,5751,1,2])
    request_to_cpp_cache([226,249,151,134,1,2,8844,5,0,2,1585,151,1339,8,1805,147,3,1044,1,2,150,0,0,104,1,2])
    request_to_cpp_cache([20,11,209,178,0,0,289,5,0,2,260,205,240,1,2417,199,3,156,1,2,202,0,0,143,1,2])
    request_to_cpp_cache([377,67,8832365,111,0,0,2047,6,0,5887,1474,123,1252,1,124,120,1,101,1,2,123,1,1,4,1,2])
    # request_to_cpp_cache(group_rowIds)


# [ev-id 0] = -0.0118286 -0.0257423 -0.0105527 -0.0165145 0.0259212 -0.000496262 0.00899556 -0.0258302 -7.32102e-05 -0.0255866 0.00748907 -0.00971568 -0.00204157 -0.0247979 0.0240845 0.00585811 -0.0259234 0.0114902 0.0159778 0.00974667 0.0137542 -0.00840011 0.0224833 0.0014523 -0.00417723 0.0197986 0.0207735 -0.00555079 0.0103659 -0.0182052 0.0104716 0.00257201 0.0170066 0.0110418 -0.00589427 0.0231391 

# Problem 1-10 1-20 1-377 1-378 1-379 1-1 1-8
# -0.012063718,-0.0035619228,-0.028493173,-0.02096814,-0.011454079,-0.024047744,-0.014990017,-0.020034054,0.014460706,-0.0116268,-0.011440023,-0.011354606,0.001819424,-0.018255766,0.01616487,0.015255951,0.004157157,-0.007361739,-0.0072275857,-0.024254704,0.014960542,0.016049543,0.0035394796,-0.0044761677,0.0048186546,0.009322171,0.0077805617,0.011221018,0.0076178666,-0.008534477,0.023551298,-0.004714916,0.012456482,0.0012225911,-0.010289915,0.006965042
# -0.0005619606,-0.025187105,-0.006937083,-0.010206628,-0.004518399,-0.023439443,0.0090357475,0.0089431275,-0.015652837,-0.007234479,0.03529073,0.025364285,0.0004733443,0.019256467,0.028824251,0.02453832,-0.008280633,0.0184457,0.004552156,0.025388695,0.0036203926,-0.021109695,-0.0074236086,-0.021850634,-0.00047273433,0.014826082,0.016198795,0.01604572,0.0008851446,0.00016810624,-0.0114049185,0.018675482,0.018332042,0.01771497,0.0040037436,0.0059220567
# -0.011828578,-0.025742263,-0.010552726,-0.016514547,0.025921218,-0.00049626245,0.00899556,-0.025830174,-7.3210205e-05,-0.025586586,0.0074890726,-0.009715679,-0.0020415743,-0.024797872,0.024084508,0.005858106,-0.025923358,0.011490162,0.015977819,0.009746674,0.013754238,-0.0084001105,0.022483278,0.0014522977,-0.0041772295,0.019798603,0.02077349,-0.005550787,0.010365909,-0.01820517,0.010471553,0.0025720114,0.017006556,0.011041806,-0.005894272,0.02313906
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

        # Calculate the overhead based on the number of ev tables
        n_overhead_bytes = 4 * 4 * (N_EVTable)
        total_bytes_to_read = (36 * 4 * N_EVTable) + n_overhead_bytes

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
