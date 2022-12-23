#include "evlfu_32.hpp"

void EVLFU_32BIT::hello(){
    printf("This is EV-LFU cache for %dbit\n", BIT_PRECISION_32BIT);
}

void EVLFU_32BIT::check_size(){
    printf("Size of tbe %dbit EV-LFU is %d\n", BIT_PRECISION_32BIT, cap_C1);
    int total = 0;
    for (int i = 0; i <= N_EV_TABLE; i++) {
        printf("   lists_C1 %d : size = %ld\n", i, lists_C1[i].size());
        total += lists_C1[i].size();
    }
    printf("  Total = %d\n", total);
    printf("Size of vals_C1 %ld\n", vals_C1.size());
}

// temporal util function for the setup of the testing worklod, no need attention
vector<string> EVLFU_32BIT::split(const string& s, const string& delim) {
    vector<string> elems;
    size_t pos = 0;
    size_t len = s.length();
    size_t delim_len = delim.length();
    if (delim_len == 0)
        return elems;
    while (pos < len) {
        int find_pos = s.find(delim, pos);
        if (find_pos < 0) {
            elems.push_back(s.substr(pos, len - pos));
            break;
        }
        elems.push_back(s.substr(pos, find_pos - pos));
        pos = find_pos + delim_len;
    }
    return elems;
}

void EVLFU_32BIT::load_ev_tables() {
    for (int i = 0; i < N_EV_TABLE; i++) {
        string ev_table_path = EV_TABLE_PATH + "ev-table-" + to_string(i + 1) + ".bin";
        FILE* fp = fopen(ev_table_path.c_str(), "rb");
        // printf("Opening file ev %s\n", ev_table_path.c_str());
        if (fp != NULL) {
            files[i] = fp;
        } else {
            printf("ERROR: Failed to load_ev_tables() when opening %s\n", ev_table_path.c_str());
            exit(-1);
        }
    }
}

void EVLFU_32BIT::close_ev_tables() {
    for (int i = 0; i < N_EV_TABLE; i++) {
        FILE* fp = files[i];
        fclose(fp);
        fp = NULL;
    }
}

// g++ -O3 evlfu_4.cpp evlfu_8.cpp evlfu_16.cpp evlfu_32.cpp -pthread; ./a.out
EVLFU_32BIT::EVLFU_32BIT(int capacity, bool init_second_layer, int secondary_precision) {
    cap_C1 = capacity;
    printf("[%dbit] Cache size = %d\n", BIT_PRECISION_32BIT, cap_C1);
    int i = 0;
    unordered_set<string> emptySet = {};
    load_ev_tables();

    while (i <= N_EV_TABLE) {
        unordered_set<string> newSet;
        lists_C1.push_back(newSet);
        i++;
    }
    max_perfect_item_C1 = int(cap_C1 * perfect_item_cap_C1);

    // Update TOTAL_CHUNK_PER_ROW and TOTAL_BYTE_PER_ITEM
    if (BIT_PRECISION_32BIT == 32) {
        TOTAL_BYTE_PER_ITEM = BIT_PRECISION_32BIT / 8;
        TOTAL_CHUNK_PER_ROW = EV_DIMENSION;
    } else if (BIT_PRECISION_32BIT == 16) {
        TOTAL_BYTE_PER_ITEM = BIT_PRECISION_32BIT / 8;
        TOTAL_CHUNK_PER_ROW = EV_DIMENSION;
    } else if (BIT_PRECISION_32BIT == 8) {
        // stored as char
        TOTAL_BYTE_PER_ITEM = BIT_PRECISION_32BIT / 8;
        TOTAL_CHUNK_PER_ROW = EV_DIMENSION;
    } else if (BIT_PRECISION_32BIT == 4) {
        // stored as half char
        // because the smallest item that we can read is a char (1Byte)
        TOTAL_BYTE_PER_ITEM = BIT_PRECISION_32BIT / 4;
        TOTAL_CHUNK_PER_ROW = EV_DIMENSION/2;
    } else {
        printf("ERROR: BIT_PRECISION_32BIT is unrecognized! Can't calculate TOTAL_CHUNK_PER_ROW\n");
        exit(-1);
    }

    // Initiate the second layer (C2) // Modify manually ()
    if (init_second_layer) {
        this->secondary_precision = secondary_precision;
        if (secondary_precision == 16) {
            evlfu_16bit = new EVLFU_16BIT(cap_C1 * 2, false, -1);
        } else if (secondary_precision == 8) {
            evlfu_8bit = new EVLFU_8BIT(cap_C1 * 4, false, -1, false, "");
        } else if (secondary_precision == 4) {
            evlfu_4bit = new EVLFU_4BIT(cap_C1 * 8, false, -1);
        } else {
            printf("ERROR: Secondary precision (%d) is NOT recognized! \n", secondary_precision);
            exit(-1);
        }
    }

    // initiate the multi thread EV table reader
    init_thread_ev_reader();
}

EVLFU_32BIT::~EVLFU_32BIT(){
    close_ev_tables();
}

void EVLFU_32BIT::shutdown_evtable_reader_thds() {
    printf("Starting the shutdown phase!\n");
    // Update global variable 
    STOP_ALL_THREADS = 1;

    // Increment Semaphore so that all thread will check the global status
    for (int thd_id = 0; thd_id < N_THD__READ_EVTABLE_32BIT; thd_id++) {
        sem_post(&LOCK_THD_STATUSES[thd_id]); // tell them to wake up
    }
    printf("Wake up workers!\n");

    // Join all threads.
    int thd_id = 0;
    for (thread &th : thds_pool__read_evtable) {
        sem_destroy(&LOCK_THD_STATUSES[thd_id]);
        th.join();
        thd_id++;
    }
    thds_pool__read_evtable.clear();  
    
    // Destroy the rest of semaphore
    sem_destroy(&LOCK_WAITING_FOR_RESULT);
    sem_destroy(&LOCK_WAITING_OTHER_THDS);
}

// SHOULD Each thread has its own data pool??
void EVLFU_32BIT::thd_loop_read_evtable(int thd_id) {
    // int thd_id = 0;
    printf("  EVLFU_32BIT THD %d: is ready to read EV-Table from file\n", thd_id);
    while(true) {
        sem_wait(&LOCK_THD_STATUSES[thd_id]); // To prevent redoing the task/jobs

        if (STOP_ALL_THREADS) {
            printf("Stopping thread %d .. \n", thd_id);
            pthread_exit(NULL);
        } else {
            // Let's work 
            // printf("I am here thd %d\n", thd_id);
            int job_id = thd_id; // correspond to the table_id at the job
            int keep_working = 1;
            while (keep_working) {
                // iterate to every each of the job and work on it
                if (job_id >= queue_job__read_evtable.size()) { 
                    // printf("   THD  %d Done doing the jobs\n", thd_id);
                    keep_working = 0;
                    sem_post(&LOCK_WAITING_OTHER_THDS);
                    // Done working: Do not try redo the jobs!
                    // DO NOT busy waiting; Wait until new jobs are posted
                } else {
                    auto curr_job = queue_job__read_evtable[job_id];
                    // printf("Job read table %d\n", curr_job.table_id);
                    global__arr_missing_values[curr_job.table_id - 1] = get_from_file(curr_job.table_id, curr_job.row_id);
                    // printf("  Finish Job %d\n", job_id);
                    if (job_id == queue_job__read_evtable.size() - 1 ) { 
                        // printf("    %d The last job is DONE\n", thd_id);
                        // signal that all results are ready to consume 
                        for (int it = 0; it < N_THD__READ_EVTABLE_32BIT - 1; it++) {
                            // -1 no need to wait for itself
                            sem_wait(&LOCK_WAITING_OTHER_THDS);
                        }
                        // queue_job__read_evtable.clear();
                        // printf("Confirmed that ALL THDS are DONE == queue == %ld\n", queue_job__read_evtable.size());
                        sem_post(&LOCK_WAITING_FOR_RESULT);
                        keep_working = 0;
                    }
                    job_id += N_THD__READ_EVTABLE_32BIT;
                }
            }
        }
    }
};

void EVLFU_32BIT::init_thread_ev_reader() {
    // No thread will run without a ready-job 
    sem_init(&LOCK_WAITING_FOR_RESULT, 0, 0); // When all values are ready, the iter will be 1
    sem_init(&LOCK_WAITING_OTHER_THDS, 0, 0); // Wait all worker thds

    // Initialize thread pool
    for (int i = 0; i < N_THD__READ_EVTABLE_32BIT; i++) {
        sem_init(&LOCK_THD_STATUSES[i], 0, 0);
        thds_pool__read_evtable.push_back(thread(&EVLFU_32BIT::thd_loop_read_evtable, this, i));
    } 

    // Initialize results' holder ; start at id 1
    global__arr_missing_values.resize(N_EV_TABLE);
}

void EVLFU_32BIT::setKey(string& key, float *value, int agg_hit) {
    // printf("inside setKey\n");
    // Flushing:
    if (n_perfect_item_C1 >= max_perfect_item_C1) {
        int it = 0; 
        int nEvict = int(flush_rate_C1 * cap_C1);
        for (auto key_to_evict = lists_C1[N_EV_TABLE].begin(); it < nEvict && key_to_evict != lists_C1[N_EV_TABLE].end(); ) {
            vals_C1.erase(*key_to_evict);
            lists_C1[N_EV_TABLE].erase(*key_to_evict++);
            // exit(-1);
            it++;
        }
        n_perfect_item_C1 -= int(flush_rate_C1 * cap_C1);
    } else {
        // cache is full
        if (int(vals_C1.size()) >= cap_C1) {
            // make a space for the new key:
            while (lists_C1[min_C1].empty()) {
                // find the right key to pop
                // Update minimum agg_hit
                min_C1++;
                if (min_C1 > N_EV_TABLE)
                    min_C1 = 1;
            }
            auto key_to_evict = lists_C1[min_C1].begin();
            vals_C1.erase(*key_to_evict); // This should happen before lists_C1.erase()
            if (is_c3_active) {
                // Will store this evicted key to C3!!
                shared__arr_evicted_keys.push_back(*key_to_evict);
                // TODO: Consume the shared__arr_evicted_keys later in *c1_c2_c3
            }
            lists_C1[min_C1].erase(key_to_evict);
            // checkSize();
            // exit(-1);
        }
    }
    // insert the new value:
    vals_C1[key] = {value, agg_hit};
    // for( int chunk_idx = 0; chunk_idx < 36; chunk_idx++ ) {
    //     printf("value = %f, ", vals_C1[key].embedding_value[chunk_idx]);
    // }

    lists_C1[agg_hit].insert(key);
    if (agg_hit < min_C1)
        min_C1 = agg_hit;
}

// Update the aggHit of the current value stored in the cache
void EVLFU_32BIT::update_agg_hit(Cache_data *value_in_cache, string& key, int agg_hit, int debug = 0) {
    if (value_in_cache->agg_hit >= 0 && value_in_cache->agg_hit < agg_hit) {
        // avoid modifying an orphaned pointer
        // this current key (value_in_cache) might be kicked out just recently; 
        // no need to reinsert it :)
        auto search_result = lists_C1[value_in_cache->agg_hit].find(key);
        if ( search_result != lists_C1[value_in_cache->agg_hit].end()) {
            // NEW-UPDATE: incrementally increase the aggHit [SLOWER]
            // agg_hit = value_in_cache->agg_hit + 1;

            lists_C1[value_in_cache->agg_hit].erase(search_result);
            lists_C1[agg_hit].insert(key);
            vals_C1[key].agg_hit = agg_hit;
        } else {
            // This condition seems to be never happenned
            // But, it save to have the if checker NULL condition :)
        }
    }
}


void EVLFU_32BIT::print_ev_values(float *arr_floats) {
    for( int idx = 0; idx < EV_DIMENSION; idx++ ) {
        printf("Value = %f, ", arr_floats[idx]);
    }
    printf("\n");
}

// Directly returns the floats!
float* EVLFU_32BIT::get_from_file(int table_id, int row_id) {
    // cout << "getting from: " << to_string(table_id) << "-" <<row_id << endl;
    FILE* fp = files[table_id - 1];
    // printf("TOTAL_BYTE_PER_ROW %d \n", TOTAL_BYTE_PER_ROW);
    // printf("TOTAL_BYTE_PER_ITEM %d \n", TOTAL_BYTE_PER_ITEM);
    char buffer[TOTAL_BYTE_PER_ROW]; // 144B == 36dims * 32bit / 8
    // char *buffer = malloc and new won't work!;
    // This is where the floats are allocated; the other should just point to it
    float* missing_value = (float*) malloc(sizeof(float) * EV_DIMENSION);
    if (fp != NULL) {
        fseek(fp, (int)(TOTAL_BYTE_PER_ROW * row_id), SEEK_SET);  //定位到第二行，每个英文字符大小为1
        // TODO: Store the buffer directly
        if (fread(buffer, sizeof(buffer), 1, fp)){
            // read to our buffer
            // TODO: Duplicate this file to handle each precision reading
            for( int chunk_idx = 0; chunk_idx < TOTAL_CHUNK_PER_ROW; chunk_idx++ ) {
                memcpy(&missing_value[chunk_idx], buffer + (chunk_idx * TOTAL_BYTE_PER_ITEM), TOTAL_BYTE_PER_ITEM); // 4 Byte == size of an int or float
                // printf(" %f, ", missing_value[chunk_idx]);
            }
            // printf("\n\n");
            // exit(-1);
        } else {
            cout << "getting from: " << to_string(table_id) << "-" <<row_id << endl;
            printf("  TOTAL_BYTE_PER_ROW %d \n", TOTAL_BYTE_PER_ROW);
            printf("  TOTAL_BYTE_PER_ITEM %d \n", TOTAL_BYTE_PER_ITEM);
            printf("  ERROR: Failed to read the chunks from ev table file!\n");
            return NULL;
        }; 
    }
    else {
        cout << "Get EV ERROR!!!" << endl;
    }
    return missing_value;
}

// Must enable double layer initialization!!
int EVLFU_32BIT::request_to_c1_c2(vector<int>& arr_row_ids, vector<bool>& c1_arr_record_hit, 
        float * c1_c2_arr_emb_weights, int debug = 0) {
    vector<float*> c1_arr_missing_values = vector<float*>(N_EV_TABLE);
    vector<Cache_data*> c1_arr_values_in_cache = vector<Cache_data*>(N_EV_TABLE);
    vector<bool> c2_arr_idx_to_update(N_EV_TABLE, true); 
    vector<bool> c2_arr_idx_to_insert(N_EV_TABLE, false); // by default will not insert any new keys

    int c1_agg_hit = 0;
    bool should_update_c2 = true;

    // C1: Construct the keys
    for (int i = 0; i < N_EV_TABLE; i++) {
        // share this to C2 thread
        shared_arr_group_keys[i] = to_string(i + 1) + "-" + to_string(arr_row_ids[i]);
    }
    // C2: probe the cache
    int c2_agg_hit = -1;
    if (secondary_precision == 16) {
        c2_agg_hit = evlfu_16bit->phase_1_find_keys_in_cache(shared_arr_group_keys, c2_arr_record_hit);
    } else if (secondary_precision == 8) {
        c2_agg_hit = evlfu_8bit->phase_1_find_keys_in_cache(shared_arr_group_keys, c2_arr_record_hit);
    } else if (secondary_precision == 4) {
        c2_agg_hit = evlfu_4bit->phase_1_find_keys_in_cache(shared_arr_group_keys, c2_arr_record_hit);
    }
  
    // calculate the combined aggHit
    c1_c2_agg_hit = c2_agg_hit;
    // C1: probe the cache
    for (int i = 0; i < N_EV_TABLE; i++) {
        // printf("    %d   %d\n", i, (c2_arr_record_hit[i] == true));
        auto search_result = vals_C1.find(shared_arr_group_keys[i]);
        if (search_result != vals_C1.end()) { // C1 Hit
            c1_arr_record_hit[i] = true;
            // TODO: This is buggy, the search_result might be trashed soon
            c1_arr_values_in_cache[i] = &(search_result->second);
            c1_agg_hit++;
            c2_arr_idx_to_update[i] = false; // C2 hands off on this item

            if (!c2_arr_record_hit[i]) { // Hit at C1 only
                c1_c2_agg_hit ++;
            }
        } else { // C1 Miss
            c1_arr_record_hit[i] = false;
            if (!c2_arr_record_hit[i]) { // Cond 259: Miss at both C1 and C2
                c2_arr_idx_to_insert[i] = true;
                c2_arr_idx_to_update[i] = false; // C2 hands off on this item
            }
        }
    }
    // Initialize jobs queue
    queue_job__read_evtable.clear();
    for (int idx = 0; idx < N_EV_TABLE; idx++) {
        // nullify old values
        global__arr_missing_values[idx] = NULL;
    }

    if (int(vals_C1.size()) >= cap_C1) {
        // C1 cache is FULL
        if (c1_c2_agg_hit < high_agghit_threshold) {
            // C1 and C2 share the missing keys insertion
            for (int i = 0; i < N_EV_TABLE; i++) {
                if (!c2_arr_record_hit[i]){ // Miss at both C1 and C2
                    c2_arr_idx_to_update[i] = false; // C2 hands off on this item
                    if (i % 2 == 1) {
                        // C1: take 50% missing keys
                        queue_job__read_evtable.push_back(Job_Reading_Evtable{i + 1, arr_row_ids[i]});
                        // c1_arr_missing_values[i] = get_from_file(i + 1, arr_row_ids[i]);
                        // C2: take 50% missing keys
                        c2_arr_idx_to_insert[i] = false;
                    } 
                }
            }
        }
        // Intuition: When we have high aggHit >=23, usually the missing keys are the most useless keys 
        // C2: Insert all missing keys; supported by Cond 259
    } else {
        // C1 Cache is NOT full: Put everything to C1
        // Get all missing keys from storage at once:
        for (int i = 0; i < N_EV_TABLE; i++) {
            if (!c1_arr_record_hit[i]){ // C1 miss 
                queue_job__read_evtable.push_back(Job_Reading_Evtable{i + 1, arr_row_ids[i]});
                // c1_arr_missing_values[i] = get_from_file(i + 1, arr_row_ids[i]);
            } 
        }
        // C2: MUST be empty at this condition
        should_update_c2 = false;
        c1_c2_agg_hit = c1_agg_hit;
    }

    if (queue_job__read_evtable.size() > 0) {
        // C1: Wake up the worker; the jobs are ready
        for (int thd_id = 0; thd_id < N_THD__READ_EVTABLE_32BIT; thd_id++) {
            // update worker status
            sem_post(&LOCK_THD_STATUSES[thd_id]); // tell them to be ready to work
        }
    }
    
    if (should_update_c2) {
        // C2: Update aggHit or insert the new value
        if (secondary_precision == 16) {
            evlfu_16bit->phase_2_get_and_insert_missing_values(shared_arr_group_keys, arr_row_ids, c2_arr_idx_to_insert, 
                c2_arr_idx_to_update, c1_c2_agg_hit, c1_c2_arr_emb_weights, debug);
        } else if (secondary_precision == 8) {
            evlfu_8bit->phase_2_get_and_insert_missing_values(shared_arr_group_keys, arr_row_ids, c2_arr_idx_to_insert, 
                c2_arr_idx_to_update, c1_c2_agg_hit, c1_c2_arr_emb_weights, debug);
        } else if (secondary_precision == 4) {
            evlfu_4bit->phase_2_get_and_insert_missing_values(shared_arr_group_keys, arr_row_ids, c2_arr_idx_to_insert, 
                c2_arr_idx_to_update, c1_c2_agg_hit, c1_c2_arr_emb_weights, debug);
        }
    }

    if (queue_job__read_evtable.size() > 0) {
        // C1: Wait for the results
        sem_wait(&LOCK_WAITING_FOR_RESULT);
    }
    
    // C1: Update aggHit or insert the new value
    for (int i = 0; i < N_EV_TABLE; i++) {
        int offset_bytes = i * EV_DIMENSION;
        // Merge the floats 
        if (c1_arr_record_hit[i]) {
            update_agg_hit(c1_arr_values_in_cache[i], shared_arr_group_keys[i], c1_c2_agg_hit);
            memcpy( c1_c2_arr_emb_weights + offset_bytes, c1_arr_values_in_cache[i]->embedding_value, TOTAL_BYTE_PER_ROW);

        } else {
            // C1: Plug the missing values:
            if (global__arr_missing_values[i]) {
                setKey(shared_arr_group_keys[i], global__arr_missing_values[i], c1_c2_agg_hit);
                memcpy( c1_c2_arr_emb_weights + offset_bytes, global__arr_missing_values[i], TOTAL_BYTE_PER_ROW);
            } else {
                // This missing value is handled/inserted by C2 
            }
        }
    }

    if (c1_c2_agg_hit == N_EV_TABLE) {
        // update the number of perfect
        n_perfect_item_C1 = lists_C1[N_EV_TABLE].size();
        return 1;
    }

    return 0;
}

float * EVLFU_32BIT::request_by_key(string key) {
    auto search_result = vals_C1.find(key);
    if (search_result != vals_C1.end()) { // C1 Hit
        // printf("EVLFU_32BIT Hit alternative key!\n");
        return (search_result->second).embedding_value;
    } else { // C1 Miss
        return NULL;
    }
}

int EVLFU_32BIT::request_to_ev_lfu(vector<int>& arr_row_ids, vector<bool>& arr_record_hit, float *emb_weights_in_1d_floats) {
    vector<Cache_data*> arr_values_in_cache = vector<Cache_data*>(N_EV_TABLE);
    int agg_hit = 0;
    // Construct the keys
    for (int i = 0; i < N_EV_TABLE; i++) {
        shared_arr_group_keys[i] = to_string(i + 1) + "-" + to_string(arr_row_ids[i]);
        // printf("%s, ", shared_arr_group_keys[i].c_str());
    }
    // printf("\n");
    // probe the cache
    for (int i = 0; i < N_EV_TABLE; i++) {
        auto search_result = vals_C1.find(shared_arr_group_keys[i]);
        if (search_result != vals_C1.end()) {
            arr_record_hit[i] = true;
            arr_values_in_cache[i] = &(search_result->second);
            agg_hit++;
        } else {
            arr_record_hit[i] = false;
        }
    }

    // Initialize jobs queue
    queue_job__read_evtable.clear();

    // printf("Start submitting jobs \n");
    // Get all missing keys from storage at once:
    for (int i = 0; i < N_EV_TABLE; i++) {
        //  Multithreaded!!!
        if (!arr_record_hit[i]){
            // Push/Submit job
            queue_job__read_evtable.push_back(Job_Reading_Evtable{i + 1, arr_row_ids[i]});
        }
    }

    if (queue_job__read_evtable.size() > 0) {
        // Wake up the worker; the jobs are ready
        for (int thd_id = 0; thd_id < N_THD__READ_EVTABLE_32BIT; thd_id++) {
            // update worker status
            sem_post(&LOCK_THD_STATUSES[thd_id]); // tell them to be ready to work
        }
        // Wait for the results
        // printf("Wait for the results \n");
        sem_wait(&LOCK_WAITING_FOR_RESULT);
        // printf("I got all the results!!!!! ==== \n");
        // for( int byte_ctr = 0; byte_ctr < 36; byte_ctr++ ) {
        //     printf("value = %f, ", global__arr_missing_values[1][byte_ctr]);
        // }
        // printf("Check the global values\n");
        // shutdown();
        // exit(-1);
    } 

    // Update aggHit or insert the new value
    for (int i = 0; i < N_EV_TABLE; i++) {
        int offset_bytes = i * EV_DIMENSION;
        if (arr_record_hit[i]) {
            update_agg_hit(arr_values_in_cache[i], shared_arr_group_keys[i], agg_hit);
            memcpy( emb_weights_in_1d_floats + offset_bytes, arr_values_in_cache[i]->embedding_value, TOTAL_BYTE_PER_ROW);
        } else {
            // Plug the missing values:
            setKey(shared_arr_group_keys[i], global__arr_missing_values[i], agg_hit);
            // arr_emb_weights[i] = global__arr_missing_values[i];
            memcpy( emb_weights_in_1d_floats + offset_bytes, global__arr_missing_values[i], TOTAL_BYTE_PER_ROW);
        }
    }

    // printf("test emb_weights_in_1d_floats %f\n", emb_weights_in_1d_floats[0]);
    // exit(-1);

    if (agg_hit == N_EV_TABLE) {
        // update the number of perfect
        n_perfect_item_C1 = lists_C1[N_EV_TABLE].size();
        return 1;
    }

    return 0;
}

vector<vector<int>> EVLFU_32BIT::prepare_workload () {
    //[1] For real workload test.
    string workload_dir = WORKLOAD_PATH;
    string* workload_files = new string[N_EV_TABLE];
    vector<vector<int>> arrRawWorkload;
    for (int i = 1; i <= N_EV_TABLE; i++) {
        workload_files[i - 1] = workload_dir;
        workload_files[i - 1] += "workload-group-" + to_string(i);
        workload_files[i - 1] += ".csv";
    }

    for (int i = 0; i < N_EV_TABLE; i++) {
        ifstream in(workload_files[i]);
        string line;
        vector<int> workload;
        if (in.fail()) {
            cout << "ERROR: File not found" << endl;
            exit(-1);
        }
        while (getline(in, line) && in.good()) {
            workload.push_back(atoi(split(line, "-")[1].c_str()));
        }
        in.close();
        // cout << "end!" << endl;
        arrRawWorkload.push_back(workload);
    }
    vector<vector<int>> groupedWorkloadKeys = vector<vector<int>>(arrRawWorkload[0].size());
    for (int i = 0; i < arrRawWorkload[0].size(); i++) {
        vector<int> group_keys = vector<int>(N_EV_TABLE);
        for (int j = 0; j < arrRawWorkload.size(); j++) {
            group_keys[j] = arrRawWorkload[j][i];
        }
        groupedWorkloadKeys[i] = group_keys;
    }
    // Done merging ALL workloads!
    return groupedWorkloadKeys;
}

// g++ -O3 evlfu_4.cpp evlfu_8.cpp evlfu_16.cpp evlfu_32.cpp -pthread; ./a.out
int main32() {
    int capacity = 8000;
    EVLFU_32BIT *evlfu_32bit = new EVLFU_32BIT(capacity, false, 8);
    // Prepare workload
    vector<vector<int>> groupedWorkloadKeys = evlfu_32bit->prepare_workload();
    // Done merging ALL workloads!
    int perfectHit = 0;
    cout << "Start caching!!" << endl;

    vector<bool> aggHitMissRecord = vector<bool>(N_EV_TABLE);
    vector<float*> emb_weights = vector<float*>(N_EV_TABLE);
    float emb_weights_in_1d_floats[N_EV_TABLE * EV_DIMENSION];

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < groupedWorkloadKeys.size(); i++) {
        evlfu_32bit->request_to_ev_lfu(groupedWorkloadKeys[i], aggHitMissRecord, emb_weights_in_1d_floats);
        // perfectHit += evlfu_32bit->request_to_c1_c2(groupedWorkloadKeys[i], aggHitMissRecord, emb_weights, 0);

        // if (i % 100 == 0)
        //     printf("req %d    %d\n", i, perfectHit);
        for (int x = 0; x < N_EV_TABLE; x++) {
            for (int y = 0; y < 36; y++) {
                // To test the overhead of converting low BIT_precision_32BIT!
                // emb_weights[x][y] = emb_weights[x][y];
                // cout << emb_weights[x][y] << " ";
            }
            // cout << endl;
        }
        // exit(-1);
        bool flag = true;
        for (int j = 0; j < aggHitMissRecord.size(); j++) {
            if (!aggHitMissRecord[j]) {
                flag = false;
                break;
            }
        }
        if (flag) {
            perfectHit++;
        }
    }
    auto stop = std::chrono::high_resolution_clock::now();
    printf("perfect hit:%d\n", perfectHit);
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(stop - start);
    cout << "The run time is: " << duration.count() << "s" << endl;

    evlfu_32bit->shutdown_evtable_reader_thds();
    return 0;
}

// g++ -O3 evlfu_32.cpp -pthread; ./a.out