
#ifndef EVLFU_4BIT_INCLUDED
#define EVLFU_4BIT_INCLUDED

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
#include <bitset>
#include <cmath>
#include <pthread.h>
#include <thread>
#include <semaphore.h>

using namespace std;
class EVLFU_4BIT {
  // TODO: Should use template to handle different precision?
    struct Cache_data {
        // Store 32bit float; TODO: Must change this if the precision is not 32bit!
        char *embedding_value;
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
    #define BIT_PRECISION_4BIT 4 // this could be 32bit, 16bit, 8bit, or 4bit
    // BIT_Precision_4BIT impact the size of stored data

    int cap_C1 = -1, min_C1 = 0;
    unordered_map<string, Cache_data> vals_C1;
    vector<unordered_set<string>> lists_C1;

    int n_perfect_item_C1 = 0,
    max_perfect_item_C1 = 0;
    double flush_rate_C1 = 0.3,
    perfect_item_cap_C1 = 0.95,
    max_perfect_item_cap_C1 = 0;
    int TOTAL_BYTE_PER_ROW = EV_DIMENSION * BIT_PRECISION_4BIT / 8; // in Bytes
    int TOTAL_BYTE_PER_ITEM = -1; // in Bytes
    int TOTAL_CHUNK_PER_ROW = -1; // will be used to read the values

    vector<FILE*> files = vector<FILE*>(N_EV_TABLE);
    string EV_TABLE_PATH = "/mnt/extra/ev-store-dlrm/stored_model/criteo_kaggle_all_mmap/epoch-00/ev-table-4/binary/";
    // string EV_TABLE_PATH = "/mnt/extra/ev-store-dlrm/stored_model/criteo_kaggle_all_mmap/epoch-00/ev-table/binary/";
    string WORKLOAD_PATH =  "/mnt/extra/binding-cython-cpp/epoll/evlfu_cpp/workload/Archive-new-0.5M/";
    // string WORKLOAD_PATH =  "/mnt/extra/binding-cython-cpp/epoll/evlfu_cpp/workload/Archive-new-1.0M/";

    // to handle the secondary caching layer (smaller precision)
    vector<string> shared_arr_group_keys = vector<string>(N_EV_TABLE);

    // Multithreading
    #define N_THD__READ_EVTABLE_4BIT 3 // thread::hardware_concurrency(); // 48 threads
    struct Job_Reading_Evtable {
        int table_id;
        int row_id;
    };
    vector<thread> thds_pool__read_evtable;
    sem_t LOCK_WAITING_FOR_RESULT;
    sem_t LOCK_WAITING_OTHER_THDS;
    sem_t LOCK_THD_STATUSES[N_THD__READ_EVTABLE_4BIT];  // if the worker is available, the value will be 0
    vector<Job_Reading_Evtable> queue_job__read_evtable;
    vector<char*> global__arr_missing_values;
    int STOP_ALL_THREADS = 0;
    int secondary_precision = -1;

public:
    // multi layer caching 
    vector<Cache_data*> shared_arr_values_in_cache = vector<Cache_data*>(N_EV_TABLE);
    vector<char*> shared_arr_missing_values = vector<char*>(N_EV_TABLE);
  
    EVLFU_4BIT(int capacity, bool init_second_layer, int secondary_precision);
    ~EVLFU_4BIT();

    // multithreading ev-reader
    void init_thread_ev_reader();
    void thd_loop_read_evtable(int thd_id);
    void shutdown_evtable_reader_thds();
    
    void hello();
    void check_size();
    vector<string> split(const string& s, const string& delim);
    void load_ev_tables();
    void close_ev_tables();
    void setKey(string& key, char *value, int agg_hit);
    void update_agg_hit(Cache_data *value_in_cache, string& key, int agg_hit);
    uint as_uint(const float x);
    float as_float(const uint x);
    void print_bits_short(const ushort x);
    void print_bits(const float x);
    float half_to_float(const ushort x); // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    ushort float_to_half(const float x); // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    float int_to_float(int value);
    void print_ev_values(float *arr_floats);
    void chars_buffer_to_floats(char *buffer, float * floats);
    char* get_from_file(int table_id, int row_id);
    int request_to_ev_lfu(vector<int>& arr_row_ids, vector<bool>& arr_record_hit, vector<char *>& arr_emb_weights);
    int phase_1_find_keys_in_cache(vector<string>& c2_arr_group_keys, vector<bool>& c2_arr_record_hit);
    void phase_2_get_and_insert_missing_values(vector<string>& c2_arr_group_keys, vector<int>& arr_row_ids, 
        vector<bool>& c2_arr_idx_to_insert, vector<bool>& c2_arr_idx_to_update, int c1_c2_agg_hit, float * c1_c2_arr_emb_weights, int debug);
    vector<vector<int>> prepare_workload ();
};
#endif