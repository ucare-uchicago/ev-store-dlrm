#include "aprx_embedding.hpp"

APRX_EV::APRX_EV(int capacity) {
    cap_C3 = capacity;
    printf("[AprxEV] APRX_EV_FILE_PATH = %s\n", APRX_EV_FILE_PATH.c_str());
    printf("[AprxEV] Cache size = %d\n", cap_C3);
    open_aprx_ev_files();
    // initiate the multi thread EV table reader
    init_thread_file_reader();
}

void APRX_EV::init_thread_file_reader() {
    // No thread will run without a ready-job 
    sem_init(&LOCK_WAITING_FOR_NEXT_BATCH, 0, 0); // When all values are ready, the iter will be 1
    sem_init(&LOCK_WAITING_OTHER_THDS, 0, 0); // Wait all worker thds

    // Initialize thread pool
    for (int i = 0; i < N_THD__READ_ALTKEY_FILE; i++) {
        sem_init(&LOCK_THD_STATUSES[i], 0, 0);
        thds_pool__read_altkey_file.push_back(thread(&APRX_EV::thd_loop_read_altkey_file, this, i));
    } 

    // initialize batched io holder
    for (int batch_id = 0; batch_id < N_BATCH; batch_id++) {
        queue_job__read_altkey.push_back({});
    }

    // init the current batch that is available to submit job
    curr_batch = 0;

    // sanity check 
    assert(cap_C3 >= IO_JOB_Q_SIZE); // otherwise, the batch insertion will be more complex 
}

// SHOULD Each thread has its own data pool??
void APRX_EV::thd_loop_read_altkey_file(int thd_id) {
    // int thd_id = 0;
    printf("  AprxEV THD %d: is ready to read alternative-key from file\n", thd_id);
    while(true) {
        sem_wait(&LOCK_THD_STATUSES[thd_id]); // To prevent redoing the task/jobs
        assert(idx_of_ready_batch >= 0); // make sure that the idx of ready batch is valid
        if (STOP_ALL_THREADS) {
            printf("Stopping thread %d .. \n", thd_id);
            pthread_exit(NULL);
        } else {
            // Let's work 
            // printf("thd %d just woken up! ready to work\n", thd_id);
            int job_id = thd_id; // correspond to the table_id at the job
            int keep_working = 1;
            while (keep_working) {
                // iterate to every each of the job and work on it
                if (job_id >= queue_job__read_altkey[idx_of_ready_batch].size()) { 
                    // printf("   THD  %d Done doing the jobs\n", thd_id);
                    keep_working = 0;
                    sem_post(&LOCK_WAITING_OTHER_THDS);
                    // Done working: Do not try redo the jobs!
                    // DO NOT busy waiting; Wait until new jobs are posted
                } else {
                    auto curr_job = queue_job__read_altkey[idx_of_ready_batch][job_id];
                    
                    // printf("Thd %d Before parsing \n", thd_id);
                    // printf("Thd %d  key %s \n", thd_id, curr_job.key.c_str());
                    // parse the key string to int 
                    if (! curr_job.key.empty()) {
                        int index = curr_job.key.find('-');
                        curr_job.table_id = stoi(curr_job.key.substr(0,index));
                        curr_job.row_id = stoi(curr_job.key.substr(index + 1, curr_job.key.size()));
                    }

                    uint32_t value = get_from_file_as_uint(curr_job.table_id, curr_job.row_id);
                    string key = to_string(curr_job.table_id) + "-" + to_string(curr_job.row_id);
                    // printf("Thd %d do Job %d read altkey file %d   Got key = %s ; altkey = %u\n", thd_id, job_id, curr_job.table_id, key.c_str(), value);
                    struct KV_Altkey kv_obj = { key, value };

                    global__kv_altkey_to_insert[job_id] = kv_obj;
                    // printf("  AltKey to insert key = %s\n", global__kv_altkey_to_insert[job_id].key.c_str());
                    // printf("  Finish Job %d\n", job_id);
                    if (job_id == queue_job__read_altkey[idx_of_ready_batch].size() - 1 ) { 
                        // printf("  thd %d: The last job is DONE\n", thd_id); // UNCOMMENT-THIS-FOR-DEBUGGING
                        // signal that all results are ready to consume 
                        for (int it = 0; it < N_THD__READ_ALTKEY_FILE - 1; it++) {
                            // -1 no need to wait for itself
                            sem_wait(&LOCK_WAITING_OTHER_THDS);
                        }
                        // printf("  thd %d: All workers are stopped working on the jobs\n", thd_id); // UNCOMMENT-THIS-FOR-DEBUGGING
                        queue_job__read_altkey[idx_of_ready_batch].clear();
                        // printf("  thd %d: The current queue job have been emptied\n", thd_id); // UNCOMMENT-THIS-FOR-DEBUGGING
                        keep_working = 0;
                        // this last thread trigger inserting the value to L3
                        insert_altkey_batched_obj(); 
                        // printf("  thd %d: All new values have been inserted to L3 or C3\n\n", thd_id); // UNCOMMENT-THIS-FOR-DEBUGGING

                        // try checking the next available batch and will wake the workers up
                        sem_wait(&LOCK_WAITING_FOR_NEXT_BATCH);
                        wake_all_threads_up();
                    }
                    job_id += N_THD__READ_ALTKEY_FILE;
                }
            }
        }
    }
};

void APRX_EV::wake_all_threads_up() {
    // This will be called whenever new batch is ready to process
    // Or, when there are multiple batches ready to process back to back
    if (arr_idx_of_ready_batch.size() > 0) {
        // set the id idx_of_ready_batch
        idx_of_ready_batch = arr_idx_of_ready_batch.front();
        arr_idx_of_ready_batch.pop();

        // Wake up the worker; the jobs are ready
        // printf("Waking up ALL workers!\n"); // UNCOMMENT-THIS-FOR-DEBUGGING
        for (int thd_id = 0; thd_id < N_THD__READ_ALTKEY_FILE; thd_id++) {
            // update worker status
            sem_post(&LOCK_THD_STATUSES[thd_id]); // tell them to be ready to work
        }
        // Wait for the results [No-NEED] 
    } else {
        // No need to wake up any workers because there are no more available batch to work on
        idx_of_ready_batch = -1; // this will let the add_key_to_batched_io() to start the workers
    }
}

void APRX_EV::check_curr_batch_size(){
    if (queue_job__read_altkey[curr_batch].size() == IO_JOB_Q_SIZE) {
        
        // printf("The jobs in this curr_batch (%d) is ready to be processed\n", curr_batch); // UNCOMMENT-THIS-FOR-DEBUGGING
        if (queue_job__read_altkey.size() > 0) {
            // add the curr_batch id to the arr of ready batches
            arr_idx_of_ready_batch.push(curr_batch);
            // Wake the workers up
            if (idx_of_ready_batch == -1) {
                wake_all_threads_up();
            } else {
                // the workers are still working, so just signal that the new batch is ready
                sem_post(&LOCK_WAITING_FOR_NEXT_BATCH);
            }
        }

        // increment the current batch size
        curr_batch += 1;
        if (curr_batch == N_BATCH) {
            curr_batch = 0;
        }
        if (arr_idx_of_ready_batch.size() >= N_BATCH - 1) {
            printf("ERROR: Too many items in the queue. Not enough space to store the new items. ALl the batches are ready to process!\n");
            exit(-1);
        }
    }
}

// will be called by L1 and L2
void APRX_EV::add_key_to_batched_io(int table_id, int row_id) {
    // row id is started at 0 ; while the table_id is started at 1
    // ONLY the main thread insert the job, so  no need to put semaphore lock in this queue
    queue_job__read_altkey[curr_batch].push_back({"", table_id, row_id});

    // if the current io batch is full (IO_JOB_Q_SIZE), tell the worker to start working
    check_curr_batch_size();
}

void APRX_EV::add_arr_keys_to_batched_io(vector<string> arr_keys) {
    for (string key_str: arr_keys) {
        queue_job__read_altkey[curr_batch].push_back({key_str, 0, 0});
        // if the current io batch is full (IO_JOB_Q_SIZE), tell the worker to start working
        check_curr_batch_size();
    }
}

void APRX_EV::open_aprx_ev_files() {
    // printf("Opening ev-tables \n");
    for (int i = 0; i < N_EV_TABLE; i++) {
        string aprx_ev_file = APRX_EV_FILE_PATH + "ev-table-" + to_string(i + 1) + ".bin";
        FILE* fp = fopen(aprx_ev_file.c_str(), "rb");
        if (fp != NULL) {
            files[i] = fp;
        }
    }
}

unsigned char* APRX_EV::get_from_file_as_bytes(int table_id, int row_id) {
    // row id is started at 0 ; while the table_id is started at 1
    // cout << "getting from: " << to_string(table_id) << "-" <<row_id << endl;
    FILE* fp = files[table_id - 1];
    // printf("file fp %d \n", fileno(fp));
    // printf("TOTAL_BYTE_PER_ROW %d \n", TOTAL_BYTE_PER_ROW);
    unsigned char *buffer = ( unsigned char*) malloc(TOTAL_BYTE_PER_ROW ); // FASTEST
    // Dan: buffer must be char, don't use ushort! 
    // char *buffer malloc and new won't work unless you specify the exact size on fread;
    // Dan: We must use unsigned char, otherwise the data will be wrong when the value is BIG
    // This is where the floats are allocated; the other should just point to it
    if (fp != NULL) {
        fseek(fp, (int)(TOTAL_BYTE_PER_ROW * row_id), SEEK_SET);  //定位到第二行，每个英文字符大小为1
        // read to our buffer
        if (fread(buffer, TOTAL_BYTE_PER_ROW, 1, fp)){ 
            return buffer;
        } else {
            printf("ERROR: Failed to get altkey from file! %d-%d\n", table_id, row_id);
            return NULL;
        }; 
    }
    else {
        cout << "Get EV ERROR!!!" << endl;
    }
    printf("ERROR: Failed to get altkey from file! %d-%d\n", table_id, row_id);
    exit(-1);
    return NULL;
}


uint32_t APRX_EV::get_from_file_as_uint(int table_id, int row_id) {
    // row id is started at 0 ; while the table_id is started at 1
    // cout << "getting from: " << to_string(table_id) << "-" <<row_id << endl;
    FILE* fp = files[table_id - 1];
    // printf("file fp %d \n", fileno(fp));
    // printf("TOTAL_BYTE_PER_ROW %d \n", TOTAL_BYTE_PER_ROW);
    unsigned char *buffer = ( unsigned char*) malloc(TOTAL_BYTE_PER_ROW ); // FASTEST
    // Dan: buffer must be char, don't use ushort! 
    // char *buffer malloc and new won't work unless you specify the exact size on fread;
    // Dan: We must use unsigned char, otherwise the data will be wrong when the value is BIG
    // This is where the floats are allocated; the other should just point to it
    if (fp != NULL) {
        fseek(fp, (int)(TOTAL_BYTE_PER_ROW * row_id), SEEK_SET);  //定位到第二行，每个英文字符大小为1
        // read to our buffer
        if (fread(buffer, TOTAL_BYTE_PER_ROW, 1, fp)){ 
            uint32_t alt_key = convert_bytes_to_altkey(buffer);
            free(buffer);
            return alt_key;
        } else {
            printf("ERROR: Failed to get altkey as_uint from file! %d-%d\n", table_id, row_id);
            return -1;
        }; 
    }
    else {
        cout << "Get EV ERROR!!!" << endl;
    }
    printf("ERROR: Failed to get altkey as_uint from file! %d-%d\n", table_id, row_id);
    exit(-1);
    return -1;
}

uint32_t APRX_EV::convert_bytes_to_altkey(unsigned char* bytes_buff ) {
    // printf("--%d--", *bytes_buff);
    // cout << *bytes_buff << endl;
    // for( int byte_ctr = 0; byte_ctr < 4; byte_ctr++ ) {
    //     printf("value = %d, ", bytes_buff[byte_ctr]);
    // }
    // printf("\n");
    return (bytes_buff[0] << 24) + (bytes_buff[1] << 16) + (bytes_buff[2] << 8) + bytes_buff[3]; // big endian
    // uint32_t alt_key = bytes_buff[0] + (bytes_buff[1] << 8) + (bytes_buff[2] << 16) + (bytes_buff[3] << 24); // little endian
}

void APRX_EV::convert_bytes_to_altkey(unsigned char* bytes_buff, int *alt_table_id, int *alt_row_id ) {
    // row id is started at 0 ; while the table_id is started at 1
    uint32_t alt_key = (bytes_buff[0] << 24) + (bytes_buff[1] << 16) + (bytes_buff[2] << 8) + bytes_buff[3]; // big endian
    *alt_row_id = alt_key / 100;
    *alt_table_id = alt_key % 100;
    // printf("Value alt_key: %d  (Table id = %d; row id = %d)\n", alt_key, alt_table_id, alt_row_id);
}

string APRX_EV::convert_bytes_to_altkey_str(unsigned char* bytes_buff ) {
    uint32_t alt_key = (bytes_buff[0] << 24) + (bytes_buff[1] << 16) + (bytes_buff[2] << 8) + bytes_buff[3]; // big endian
    int alt_row_id = alt_key / 100;
    int alt_table_id = alt_key % 100;
    return to_string(alt_table_id) + "-" + to_string(alt_row_id);
    // printf("Value alt_key: %d  (Table id = %d; row id = %d)\n", alt_key, alt_table_id, alt_row_id);
}

string APRX_EV::convert_altkey_to_str(int alt_table_id, int alt_row_id){
    // row id is started at 0 ; while the table_id is started at 1
    // convert to string key representation YY-XXXXXX => tableId-rowId
    return to_string(alt_table_id) + "-" + to_string(alt_row_id);
}

void APRX_EV::insert_altkey(string key, uint32_t alt_key){
    // is the L3 full?
    if (int(vals_C3.size()) >= cap_C3) {
        // C3 cache is FULL
        evict_one_key();
    }

    // insert the new value
    lists_C3.push(key);
    vals_C3[key] = {alt_key, false};
}

void APRX_EV::insert_altkey_batched(vector<string> arr_keys, vector<uint32_t> arr_altkeys){
    // perhaps, the batched size is 30 = IO_JOB_Q_SIZE
    // is the L3 full?
    if (int(vals_C3.size()) >= cap_C3) {
        // C3 cache is FULL
        for (int it; it < IO_JOB_Q_SIZE; it++) {
            evict_one_key();
        }
    }
    // insert the new value
    for (int idx; idx < IO_JOB_Q_SIZE; idx++) {
        lists_C3.push(arr_keys[idx]);
        vals_C3[arr_keys[idx]] = {arr_altkeys[idx], false};
    }
}

void APRX_EV::insert_altkey_batched_obj(){
    // done by the last worker thread
    // printf("START Inserting the alkey, IO_JOB_Q_SIZE = %d \n", IO_JOB_Q_SIZE);
    // sanity check 
    assert(global__kv_altkey_to_insert.size() == IO_JOB_Q_SIZE);
    // calculate how many item to delete
    int n_erase = (vals_C3.size() + IO_JOB_Q_SIZE) - cap_C3;

    if (n_erase > 0) {
        // C3 cache is FULL
        for (int it; it < n_erase; it++) {
            evict_one_key();
        }
    }
    // insert the new value
    for (int idx; idx < IO_JOB_Q_SIZE; idx++) {
        lists_C3.push(global__kv_altkey_to_insert[idx].key);
        vals_C3[global__kv_altkey_to_insert[idx].key] = {global__kv_altkey_to_insert[idx].value, false};
        // We might push a duplicate key inside the lists_C3!! It's okay
        // try resetting the value for debugging
        // global__kv_altkey_to_insert[idx].key = "0-0";
    }
    // printf("DONE  Inserting the altkey, done by the last worker thread\n"); // UNCOMMENT-THIS-FOR-DEBUGGING
    // Do NOT clear up global__kv_altkey_to_insert because we will overwrite the value on the next batch
    // print_all_keys_in_c3(); // UNCOMMENT-THIS-FOR-DEBUGGING
}

void APRX_EV::get_altkey(string key, int *alt_table_id, int *alt_row_id) {
    // row id is started at 0 ; while the table_id is started at 1
    auto search_result = vals_C3.find(key);
    if (search_result != vals_C3.end()) {
        // it's a HIT
        uint32_t alt_key = search_result->second.alt_key;
        *alt_row_id = alt_key / 100;
        *alt_table_id = alt_key % 100;
    } else {
        *alt_table_id = -1; // it's a miss!!
    }
}

string APRX_EV::get_altkey_str(string key) {
    auto search_result = vals_C3.find(key);
    if (search_result != vals_C3.end()) {
        // it's a HIT
        uint32_t alt_key = search_result->second.alt_key;
        return to_string(alt_key % 100) + "-" + to_string(alt_key / 100);
    } else {
        return string();
    }
}

void APRX_EV::basic_eviction() {
    vals_C3.erase(lists_C3.front());
    lists_C3.pop(); // do this after the lists_C3.front()!!
}

void APRX_EV::recency_aware_eviction() {
    // Eviction Logic #2
    bool evicted = false;
    while(! evicted) {
        // find the next candidate
        // printf("find the lists_C3.front size = %ld  %ld \n", lists_C3.size(), vals_C3.size());
        string candidate_key_to_evict = lists_C3.front();
        auto search_result = vals_C3.find(candidate_key_to_evict);
        // printf("Find the candidate to evict search_result->second = flag = %d\n", search_result->second.recency_flag);
        if (search_result != vals_C3.end()) {
            bool flag = search_result->second.recency_flag;
            if (flag == true) {
                // printf("change flag to false\n");
                search_result->second.recency_flag = false;
                // re-push this key to the end of the queue list
                lists_C3.push(candidate_key_to_evict);
                lists_C3.pop();
            } else {
                // Evict the key that has recency_flag = false
                // printf("will do basic eviction\n");
                basic_eviction();
                evicted = true;
            }
        } else {
            // it's okay, there are some redundancy in the lists_C3; the vals_C3 can't hold redundant insert!
            lists_C3.pop(); // just pop this old redundant key!
        }
    }
}

void APRX_EV::evict_one_key() {
#if C3_EVICTION_METHOD == 1
    // Eviction Logic #1
    basic_eviction();
#elif C3_EVICTION_METHOD == 2
    recency_aware_eviction();
#else
    printf("ERROR: C3_EVICTION_METHOD is unrecognized!\n");
    exit(-1);
#endif
}

void APRX_EV::set_recency_flag_c3(string key) {
    // put in the head of the list to avoid early eviction
    auto search_result = vals_C3.find(key);
    if (search_result != vals_C3.end()) {
        // set the recency_flag to true
        search_result->second.recency_flag = true;
    } else {
        // do Nothing
    }
}

vector<string> APRX_EV::request_c3(vector<string>& arr_keys) {
    // Do L3 lookup
    vector<string> alt_keys;
    for(string key : arr_keys) {
        alt_keys.push_back(get_altkey_str(key));
    }
    return alt_keys;
}

void APRX_EV::printQueue(queue<string> q) {
    //printing content of queue 
    while (!q.empty()){
        cout<<" "<<q.front();
        q.pop();
    }
    cout<<endl;
}

void APRX_EV::print_all_keys_in_c3() {
    // C3 is the same as L3 it's just referring to our third level cache
    printf("====================================================\n");
    printQueue(lists_C3);
}

int main8() {
    int capacity = 52;
    APRX_EV *aprx_ev = new APRX_EV(capacity);
    unsigned char * bytes_buff = aprx_ev->get_from_file_as_bytes(3, 1483535);
    string cache_key = aprx_ev->convert_bytes_to_altkey_str(bytes_buff);

    // value = 0, value = 0, value = 0, value = 4, value = 0, value = 0, value = 0, value = 5, Hello world!
    printf("Hello world! %s\n", cache_key.c_str());

    // test queue/dequeue function

    // 1. Insert
    aprx_ev->insert_altkey("1-123", aprx_ev->get_from_file_as_uint(1, 123));
    aprx_ev->insert_altkey("2-245", aprx_ev->get_from_file_as_uint(2, 245));
    aprx_ev->insert_altkey("3-560", aprx_ev->get_from_file_as_uint(3, 560));
    aprx_ev->insert_altkey("4-800", aprx_ev->get_from_file_as_uint(4, 800));

    // 2. Get
    string result_altkey = aprx_ev->get_altkey_str("1-123");
    // if ( ! result_altkey.empty())
    printf("result_altkey %s \n", result_altkey.c_str());
    result_altkey = aprx_ev->get_altkey_str("2-245");
    printf("result_altkey %s \n", result_altkey.c_str());
    result_altkey = aprx_ev->get_altkey_str("3-560");
    printf("result_altkey %s \n", result_altkey.c_str());
    result_altkey = aprx_ev->get_altkey_str("4-800");
    printf("result_altkey %s \n", result_altkey.c_str());

    // Look for this words to debug: UNCOMMENT-THIS-FOR-DEBUGGING
    // printf("Look for this words to debug: UNCOMMENT-THIS-FOR-DEBUGGING\n");

    int n_keys = 100;
    // insert to batched_io 
    for(int idx = 1; idx <= n_keys; idx++) {
        aprx_ev->add_key_to_batched_io(1, idx); // always use table 1
    }
    
    // sleep(5);
    // for(int idx = 1; idx <= n_keys; idx++) {
    //     aprx_ev->add_key_to_batched_io(2, idx); // always use table 2
    // }

    // for(int idx = 1; idx <= n_keys; idx++) {
    //     aprx_ev->add_key_to_batched_io(3, idx); // always use table 2
    // }

    sleep(5);
    printf("Done sleeping\n");
    
    // Test whether the worker correctly insert the L3 values
    // while (true) {
        // sem_wait(&(aprx_ev->LOCK_WAITING_FOR_NEXT_BATCH));
        // aprx_ev->print_all_keys_in_c3();
    // }
    // single thread to do L3 lookup at any moment
    free(bytes_buff);
    return 0;
}

// cd /mnt/extra/ev-store-dlrm/mixed_precs_caching/ 
// g++ -O3 evlfu_4.cpp evlfu_8.cpp evlfu_16.cpp evlfu_32.cpp aprx_embedding.cpp -o aprx_embedding.out -pthread ; ./aprx_embedding.out
