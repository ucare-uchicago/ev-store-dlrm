#!/usr/bin/env python
import numpy.ctypeslib as ctl
import ctypes
import time
N_EVTable = 26

libdir = './lib/'

cache_manager_cpp = ctl.load_library('libcachemanager.so', libdir)

arr =  [1,2,3,4,5]
arr_c = (ctypes.c_int * 5)(*arr)

cache_manager_cpp.test_arr.argtypes = [ctypes.POINTER(ctypes.c_int)]
cache_manager_cpp.test_arr.restype = None
cache_manager_cpp.test_arr(arr_c)

# arr_rows_id =  [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26]
# arr_rows_id =  [1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2]
arr_rows_id = [3, 127, 294, 254, 0, 4, 1519, 0, 0, 3, 473, 286, 280, 2, 1073, 275, 6, 203, 0, 1, 280, 0, 1, 199, 4, 43]
arr_rows_id_c = (ctypes.c_int * N_EVTable)(* arr_rows_id)


# 1. Do EV-Lookup, check the perfect-hit 
cache_manager_cpp.ev_lookup_based_on_list_keys.argtypes = [ctypes.POINTER(ctypes.c_int)]
cache_manager_cpp.ev_lookup_based_on_list_keys.restype = ctypes.c_int
is_perfect_hit = cache_manager_cpp.ev_lookup_based_on_list_keys(arr_rows_id_c)
print(is_perfect_hit)
is_perfect_hit = cache_manager_cpp.ev_lookup_based_on_list_keys(arr_rows_id_c)
print(is_perfect_hit)

# 2. Get the EV-values as a long bytes of floats
cache_manager_cpp.get_ev_values.argtypes = None
cache_manager_cpp.get_ev_values.restype = ctypes.POINTER(ctypes.c_float)
ev_values = cache_manager_cpp.get_ev_values()
print(ev_values[0:36])

# for idx in range (0, N_EVTable):
    # print(ev_values[idx])

# This is to start the socket server
# cache_manager_cpp.init_global_vars.argtypes = []
# cache_manager_cpp.init_global_vars()

# py_start_server_threads = lib.start_server_threads
# py_start_server_threads.argtypes = []
# py_start_server_threads()
# time.sleep(10)



# # calling libcmult.so::cmult(int int_param, float float_param)
# py_cmult = lib2.cmult
# py_cmult.argtypes = [ctypes.c_int, ctypes.c_float]
# results = py_cmult(2, 3.3)
# print("cmult = " + str(results))

# # calling liblibrary.so::print_value(int x)
# py_print_value = lib3.print_value
# py_print_value.argtypes = [ctypes.c_int]
# py_print_value(2)
# print("py_print_value done ")

