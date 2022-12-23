
#ifndef EVLFU_32_INCLUDED
#define EVLFU_32_INCLUDED

#include <iostream>
#include <string>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <fstream>
#include <stdio.h>
#include <ctime>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <pthread.h>
#include <thread>
#include <semaphore.h>
#include "evlfu_16.hpp"
#include "evlfu_8.hpp"
#include "evlfu_4.hpp"
#include <chrono>
#include "aprx_embedding.hpp"

using namespace std;
class EVLFU_32BIT {
    struct Cache_data {
        // Store 32bit float; TODO: Must change this if the precision is not 32bit!
        float *embedding_value;
        int agg_hit;
        ~Cache_data(){
            // Dan: This might not be needed
            if (!embedding_value) {
                free(embedding_value);
            }
        }
    };

    #define EV_DIMENSION 36
    #define N_EV_TABLE 26
    #define BIT_PRECISION_32BIT 32 // this could be 32bit, 16bit, 8bit, or 4bit
    // BIT_Precision_32BIT impact the size of stored data

    // global variables:
    int cap_C1 = -1, min_C1 = 0;
    unordered_map<string, Cache_data> vals_C1;
    vector<unordered_set<string>> lists_C1;

    int n_perfect_item_C1 = 0,
    max_perfect_item_C1 = 0;
    double flush_rate_C1 = 0.3,
    perfect_item_cap_C1 = 0.95,
    max_perfect_item_cap_C1 = 0;
    int TOTAL_BYTE_PER_ROW = EV_DIMENSION * BIT_PRECISION_32BIT / 8; // in Bytes
    int TOTAL_BYTE_PER_ITEM = -1; // in Bytes
    int TOTAL_CHUNK_PER_ROW = -1; // will be used to read the values

    vector<FILE*> files = vector<FILE*>(N_EV_TABLE);
    string EV_TABLE_PATH = "/mnt/extra/ev-store-dlrm/stored_model/criteo_kaggle_all_mmap/epoch-00/ev-table/binary/";
    // string EV_TABLE_PATH = "/mnt/extra/ev-store-dlrm/stored_model/criteo_kaggle_all_mmap/epoch-00/ev-table/binary/";
    string WORKLOAD_PATH =  "/mnt/extra/binding-cython-cpp/epoll/evlfu_cpp/workload/Archive-new-0.5M/";
    // string WORKLOAD_PATH =  "/mnt/extra/binding-cython-cpp/epoll/evlfu_cpp/workload/Archive-new-1.0M/";

    // to handle the secondary caching layer (smaller precision)
    vector<string> shared_arr_group_keys = vector<string>(N_EV_TABLE);
    vector<bool> c2_arr_record_hit = vector<bool>(N_EV_TABLE);
    int c2_agg_hit;
    int c1_c2_agg_hit;
    EVLFU_16BIT *evlfu_16bit;
    EVLFU_8BIT *evlfu_8bit;
    EVLFU_4BIT *evlfu_4bit;
    int high_agghit_threshold = 23;
    
    // Multithreading
    #define N_THD__READ_EVTABLE_32BIT 3 // thread::hardware_concurrency(); // 48 threads
    struct Job_Reading_Evtable {
        int table_id;
        int row_id; // to mark that this is the last job; to check whether all other jobs are done
    };
    vector<thread> thds_pool__read_evtable;
    sem_t LOCK_WAITING_FOR_RESULT;
    sem_t LOCK_WAITING_OTHER_THDS;
    sem_t LOCK_THD_STATUSES[N_THD__READ_EVTABLE_32BIT];  // if the worker is available, the value will be 0
    vector<Job_Reading_Evtable> queue_job__read_evtable;
    vector<float*> global__arr_missing_values;
    int STOP_ALL_THREADS = 0;
    int secondary_precision = -1;

  public:
    vector<string> shared__arr_evicted_keys;
    int aprx_ev_hit = 0;
    bool is_c3_active = false;

    EVLFU_32BIT(int capacity, bool init_second_layer, int secondary_precision);
    ~EVLFU_32BIT();

    // multithreading ev-reader
    void init_thread_ev_reader();
    void thd_loop_read_evtable(int thd_id);
    void shutdown_evtable_reader_thds();
    
    void hello();
    void check_size();
    vector<string> split(const string& s, const string& delim);
    void load_ev_tables();
    void close_ev_tables();
    void setKey(string& key, float *value, int agg_hit);
    void update_agg_hit(Cache_data *value_in_cache, string& key, int agg_hit, int debug);
    uint as_uint(const float x);
    float as_float(const uint x);
    void print_bits_short(const ushort x);
    void print_bits(const float x);
    float half_to_float(const ushort x); // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    ushort float_to_half(const float x); // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    float int_to_float(int value);
    void print_ev_values(float *arr_floats);
    float* get_from_file(int table_id, int row_id);
    int request_to_ev_lfu(vector<int>& arr_row_ids, vector<bool>& arr_record_hit, float *emb_weights_in_1d_floats);
    int request_to_c1_c2(vector<int>& arr_row_ids, vector<bool>& arr_record_hit, float * arr_emb_weights, int debug);
    vector<vector<int>> prepare_workload ();
    float * request_by_key(string key);
};

#endif