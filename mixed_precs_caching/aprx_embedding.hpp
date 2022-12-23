
#ifndef APRX_EV_INCLUDED
#define APRX_EV_INCLUDED

#include <iostream>
#include <string>
#include <list>
#include <queue>          // std::queue
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
#include <cassert>

using namespace std;

class APRX_EV{
    #define N_EV_TABLE          26
    #define IO_JOB_Q_SIZE       50
    #define N_BATCH             10 // the batch idx vector is to handle parallel data insert/delete by different threads
    #define C3_EVICTION_METHOD   2 // 1 for basic_eviction(); 2 for recency-aware eviction
    
    // string APRX_EV_FILE_PATH = "/mnt/extra/ev-store-dlrm/stored_model/criteo_kaggle_all/alternative-keys/10n-euclid/binary/";
    // string APRX_EV_FILE_PATH = "/mnt/extra/ev-store-dlrm/stored_model/criteo_kaggle_all/alternative-keys/10n-cosine/binary/";
    // string APRX_EV_FILE_PATH = "/mnt/extra/ev-store-dlrm/stored_model/criteo_kaggle_all/alternative-keys/100n-euclid/binary/";
    // string APRX_EV_FILE_PATH = "/mnt/extra/ev-store-dlrm/stored_model/criteo_kaggle_all/alternative-keys/100n-euclid003/binary/";
    // string APRX_EV_FILE_PATH = "/mnt/extra/ev-store-dlrm/stored_model/criteo_kaggle_all/alternative-keys/1000n-euclid003/binary/";
    string APRX_EV_FILE_PATH = "/mnt/extra/ev-store-dlrm/stored_model/criteo_kaggle_all/alternative-keys/1000n-euclid003-newrank/binary/";
    // string APRX_EV_FILE_PATH = "/mnt/extra/ev-store-dlrm/stored_model/criteo_kaggle_all/alternative-keys/2000n-euclid003/binary/";

    struct C3_Value {
        uint32_t alt_key;
        bool recency_flag; // will be true if the value was recently used and got hit
    };

    vector<FILE*> files = vector<FILE*>(N_EV_TABLE);
    int TOTAL_BYTE_PER_ROW = 4; // in Bytes to read the unsigned int

    int cap_C3 = -1;
    unordered_map<string, C3_Value> vals_C3; // the alternative key is stored as unsigned integer to save space
    queue<string> lists_C3; // to store the LRU order of the keys; can be popped easily (help the eviction)

    // Multithreading
    #define N_THD__READ_ALTKEY_FILE 5 // thread::hardware_concurrency(); // 48 threads
    struct Job_Reading_Altkey {
        string key; // so that C1 and C2 can insert a string 
        int table_id;
        int row_id;
    };

    struct KV_Altkey {
        string key;
        uint32_t value;
    };

    vector<thread> thds_pool__read_altkey_file;
    sem_t LOCK_WAITING_OTHER_THDS;
    sem_t LOCK_THD_STATUSES[N_THD__READ_ALTKEY_FILE];  // if the worker is available, the value will be 0
    int curr_batch = -1; 
    int idx_of_ready_batch = -1;
    queue<int> arr_idx_of_ready_batch;
    vector< KV_Altkey > global__kv_altkey_to_insert =  vector<KV_Altkey>(IO_JOB_Q_SIZE);; // TODO: Don't forget the clear this once it is being inserted
    vector < vector<Job_Reading_Altkey>> queue_job__read_altkey = vector< vector< Job_Reading_Altkey >> (N_BATCH);;
    int STOP_ALL_THREADS = 0;

public:

    sem_t LOCK_WAITING_FOR_NEXT_BATCH; // make this public so that the main() can test it
    void thd_loop_read_altkey_file(int thd_id);

    APRX_EV(int capacity);
    void init_thread_file_reader();
    void open_aprx_ev_files();
    unsigned char* get_from_file_as_bytes(int table_id, int row_id);
    uint32_t get_from_file_as_uint(int table_id, int row_id);
    uint32_t convert_bytes_to_altkey(unsigned char* bytes_buff );
    void convert_bytes_to_altkey(unsigned char* bytes_buff, int *alt_table_id, int *alt_row_id );
    string convert_altkey_to_str(int alt_table_id, int alt_row_id);
    string convert_bytes_to_altkey_str(unsigned char* bytes_buff );
    void insert_altkey(string key, uint32_t alt_key);
    void insert_altkey_batched(vector<string> arr_keys, vector<uint32_t> arr_altkeys);
    void insert_altkey_batched_obj();
    void get_altkey(string key, int *alt_table_id, int *alt_row_id);
    string get_altkey_str(string key);
    vector<string> request_c3(vector<string>& arr_keys);
    void check_curr_batch_size();
    void add_key_to_batched_io(int table_id, int row_id);
    void add_arr_keys_to_batched_io(vector<string> arr_keys);
    void print_all_keys_in_c3();
    void printQueue(queue<string> q);
    void wake_all_threads_up();
    void set_recency_flag_c3(string key);
    void evict_one_key();
    void basic_eviction();
    void recency_aware_eviction();
};

#endif