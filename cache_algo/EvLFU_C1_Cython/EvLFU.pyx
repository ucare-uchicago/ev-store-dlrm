from libcpp.vector cimport vector
from libcpp cimport bool
from libcpp.string cimport string

cdef extern from "evlfu.hpp":
    void init(int capacity)
    void request_to_ev_lfu(vector[int] &group_keys, vector[bool] &arr_record_hit, vector[vector[float]] &arr_emb_weights, bool use_gpu)
    void load_ev_tables()
    void close_ev_tables()

def cinit(int capacity):
    init(capacity)

def crequest(vector[int] group_keys, use_gpu):
    cdef vector[bool] arr_record_hit = [True] * 26
    cdef vector[vector[float]] arr_emb_weights = [[0.0]*36]*26
    request_to_ev_lfu(group_keys, arr_record_hit, arr_emb_weights, use_gpu)
    return arr_record_hit, arr_emb_weights


def cload_ev_tables():
    load_ev_tables()

def cclose_ev_tables():
    close_ev_tables()