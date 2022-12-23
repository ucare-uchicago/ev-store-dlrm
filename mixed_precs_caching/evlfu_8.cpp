#include "evlfu_8.hpp"


void EVLFU_8BIT::hello(){
    printf("This is EV-LFU cache for %dbit\n", BIT_PRECISION_8BIT);
}

void EVLFU_8BIT::check_size(){
    printf("Size of tbe %dbit EV-LFU is %d\n", BIT_PRECISION_8BIT, cap_C1);
    int total = 0;
    for (int i = 0; i <= N_EV_TABLE; i++) {
        printf("   lists_C1 %d : size = %ld\n", i, lists_C1[i].size());
        total += lists_C1[i].size();
    }
    printf("  Total = %d\n", total);
    printf("Size of vals_C1 %ld\n", vals_C1.size());
}

// temporal util function for the setup of the testing worklod, no need attention
vector<string> EVLFU_8BIT::split(const string& s, const string& delim) {
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

void EVLFU_8BIT::load_ev_tables() {
    for (int i = 0; i < N_EV_TABLE; i++) {
        string ev_table_path = EV_TABLE_PATH + "ev-table-" + to_string(i + 1) + ".bin";
        FILE* fp = fopen(ev_table_path.c_str(), "rb");
        if (fp != NULL) {
            files[i] = fp;
        }
    }
}

void EVLFU_8BIT::close_ev_tables() {
    for (int i = 0; i < N_EV_TABLE; i++) {
        FILE* fp = files[i];
        fclose(fp);
        fp = NULL;
    }
}

EVLFU_8BIT::EVLFU_8BIT(int total_capacity, bool init_second_layer, int secondary_precision, int init_third_layer, string size_proportion) {
    int capacity_c1 = -1;
    int capacity_c2 = -1;
    int capacity_c3 = -1;
    // IMPORTANT: UNCOMMENT THIS PART IF YOU WANT TO ENABLE 2 LAYERS CUSTOM proportion
    // if (! size_proportion.empty()) { // Enable proportional allocation anytime
    if (! size_proportion.empty() && init_third_layer) { // Only if C3 is enabled!
        // Must distribute the cache size allocation proportionally!
        printf("[%dbit] Size_proportion = %s\n", BIT_PRECISION_8BIT, size_proportion.c_str());
        int index = size_proportion.find('-');
        int prop_c1 = stoi(size_proportion.substr(0,index));
        size_proportion = size_proportion.substr(index + 1, size_proportion.size());
        index = size_proportion.find('-');
        int prop_c2 = stoi(size_proportion.substr(0,index));
        int prop_c3 = stoi(size_proportion.substr(index + 1, size_proportion.size()));

        assert (prop_c1 + prop_c2 + prop_c3 == 100);

        capacity_c1 = (prop_c1 * total_capacity/100) * 4;
        capacity_c2 = (prop_c2 * total_capacity/100) * 8;
        capacity_c3 = (prop_c3 * total_capacity/100) * 36;

        if (capacity_c3 <= 0) {
            // this allow us to test C1 and C2 using any size proportion
            init_third_layer = false;
        }
    } else { // other config lets just use evenly distributed allocation!!
        if (init_third_layer) {
            // if C1:C2:C3 = 33:33:33
            capacity_c1 = total_capacity/3 * 4;
            capacity_c2 = total_capacity/3 * 8;
            capacity_c3 = total_capacity/3 * 36;
        } else if (init_second_layer) {
            capacity_c1 = total_capacity/2 * 4;
            capacity_c2 = total_capacity/2 * 8;
        } else {
            capacity_c1 = total_capacity * 4 ;
        }
    }

    cap_C1 = capacity_c1;
    printf("[SIZE] capacity_c1 = %d\n", capacity_c1);
    printf("[SIZE] capacity_c2 = %d\n", capacity_c2);
    printf("[SIZE] capacity_c3 = %d\n", capacity_c3);
    
    printf("[%dbit] Cache size = %d\n", BIT_PRECISION_8BIT, cap_C1);
    int i = 0;
    unordered_set<string> emptySet = {};
    load_ev_tables();
    while (i <= N_EV_TABLE) {
        // printf("Init unordered_set!\n");
        unordered_set<string> newSet;
        lists_C1.push_back(newSet);
        i++;
    }
    max_perfect_item_C1 = int(cap_C1 * perfect_item_cap_C1);

    // Update TOTAL_CHUNK_PER_ROW and TOTAL_BYTE_PER_ITEM
    if (BIT_PRECISION_8BIT == 32) {
        TOTAL_BYTE_PER_ITEM = BIT_PRECISION_8BIT / 8;
        TOTAL_CHUNK_PER_ROW = EV_DIMENSION;
    } else if (BIT_PRECISION_8BIT == 16) {
        TOTAL_BYTE_PER_ITEM = BIT_PRECISION_8BIT / 8;
        TOTAL_CHUNK_PER_ROW = EV_DIMENSION;
    } else if (BIT_PRECISION_8BIT == 8) {
        // stored as char
        TOTAL_BYTE_PER_ITEM = BIT_PRECISION_8BIT / 8;
        TOTAL_CHUNK_PER_ROW = EV_DIMENSION;
    } else if (BIT_PRECISION_8BIT == 4) {
        // stored as half char
        // because the smallest item that we can read is a char (1Byte)
        TOTAL_BYTE_PER_ITEM = BIT_PRECISION_8BIT / 4;
        TOTAL_CHUNK_PER_ROW = EV_DIMENSION/2;
    } else {
        printf("ERROR: BIT_PRECISION_8BIT is unrecognized! Can't calculate TOTAL_CHUNK_PER_ROW\n");
        exit(-1);
    }

    // Initiate the second layer (C2)
    if (init_second_layer) {
        this->secondary_precision = secondary_precision;
        if (secondary_precision == 4) {
            evlfu_4bit = new EVLFU_4BIT(capacity_c2, false, -1);
            if (init_third_layer) {
                evlfu_4bit->is_c3_active = true;
            }
        } else {
            printf("ERROR: Secondary precision (%d) is NOT recognized! \n", secondary_precision);
            exit(-1);
        }
    }

    // initiate the multi thread EV table reader
    init_thread_ev_reader();

    // Initiate the third layer (C3)
    if (init_third_layer) {
        // Need to calculate aprx_ev capacity RELATIVE to 8bit precision capacity
        aprx_ev = new APRX_EV(capacity_c3);
        // aprx_ev = new APRX_EV(200); // small size for debugging
        is_c3_active = true;
    }
}

EVLFU_8BIT::~EVLFU_8BIT() {
    close_ev_tables();
}

void EVLFU_8BIT::shutdown_evtable_reader_thds() {
    printf("Starting the shutdown phase!\n");
    // Update global variable 
    STOP_ALL_THREADS = 1;

    // Increment Semaphore so that all thread will check the global status
    for (int thd_id = 0; thd_id < N_THD__READ_EVTABLE_8BIT; thd_id++) {
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
void EVLFU_8BIT::thd_loop_read_evtable(int thd_id) {
    // int thd_id = 0;
    printf("  EVLFU_8BIT THD %d: is ready to read EV-Table from file\n", thd_id);
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
                    // printf("Job read table %d  thd_id %d\n", curr_job.table_id, thd_id);
                    global__arr_missing_values[curr_job.table_id - 1] = get_from_file(curr_job.table_id, curr_job.row_id);
                    // printf("  Finish Job %d\n", job_id);
                    if (job_id == queue_job__read_evtable.size() - 1 ) { 
                        // printf("    %d The last job is DONE\n", thd_id);
                        // signal that all results are ready to consume 
                        for (int it = 0; it < N_THD__READ_EVTABLE_8BIT - 1; it++) {
                            // -1 no need to wait for itself
                            sem_wait(&LOCK_WAITING_OTHER_THDS);
                        }
                        // queue_job__read_evtable.clear();
                        // printf("Confirmed that ALL THDS are DONE == queue == %ld\n", queue_job__read_evtable.size());
                        sem_post(&LOCK_WAITING_FOR_RESULT);
                        keep_working = 0;
                    }
                    job_id += N_THD__READ_EVTABLE_8BIT;
                }
            }
        }
    }
};

void EVLFU_8BIT::init_thread_ev_reader() {
    // No thread will run without a ready-job 
    sem_init(&LOCK_WAITING_FOR_RESULT, 0, 0); // When all values are ready, the iter will be 1
    sem_init(&LOCK_WAITING_OTHER_THDS, 0, 0); // Wait all worker thds

    // Initialize thread pool
    for (int i = 0; i < N_THD__READ_EVTABLE_8BIT; i++) {
        sem_init(&LOCK_THD_STATUSES[i], 0, 0);
        thds_pool__read_evtable.push_back(thread(&EVLFU_8BIT::thd_loop_read_evtable, this, i));
    } 

    // Initialize results' holder 
    global__arr_missing_values.resize(N_EV_TABLE);
}

void EVLFU_8BIT::setKey(string& key, char *value, int agg_hit) {
    // printf("inside setKey\n");
    // Flushing:
    if (n_perfect_item_C1 >= max_perfect_item_C1) {
        int it = 0; 
        int nEvict = int(flush_rate_C1 * cap_C1);
        // printf("Start flushing!\n");
        // printf("n_perfect_item_C1 = %d\n", n_perfect_item_C1);
        // printf("max_perfect_item_C1 = %d\n", max_perfect_item_C1);
        // printf("lists_C1[N_EV_TABLE] size = %ld\n", lists_C1[N_EV_TABLE].size());
        // printf("nEvict = %d\n", nEvict);
        for (auto key_to_evict = lists_C1[N_EV_TABLE].begin(); it < nEvict && key_to_evict != lists_C1[N_EV_TABLE].end(); ) {
            // printf("key_to_evict = %s\n", (*key_to_evict).c_str());
            vals_C1.erase(*key_to_evict);
            // the key evicted by flushing mechanism is NOT added to C3 because it will overwhelmed the queue
            lists_C1[N_EV_TABLE].erase(*key_to_evict++);
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
            vals_C1.erase(*key_to_evict);
            if (is_c3_active) {
                // Will store this evicted key to C3!!
                shared__arr_evicted_keys.push_back(*key_to_evict);
            }
            lists_C1[min_C1].erase(*key_to_evict);
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

// Get from
void EVLFU_8BIT::update_agg_hit(Cache_data *value_in_cache, string& key, int agg_hit) {
    if (value_in_cache->agg_hit < agg_hit) {
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

typedef unsigned short ushort;
typedef unsigned int uint;

uint EVLFU_8BIT::as_uint(const float x) {
    return *(uint*)&x;
}
float EVLFU_8BIT::as_float(const uint x) {
    return *(float*)&x;
}

void EVLFU_8BIT::print_bits_short(const ushort x) {
    for(int i=15; i>=0; i--) {
        cout << ((x>>i)&1);
        if(i==15||i==10) cout << " ";
        if(i==10) cout << "      ";
    }
    cout << endl;
}
void EVLFU_8BIT::print_bits(const float x) {
    uint b = *(uint*)&x;
    for(int i=31; i>=0; i--) {
        cout << ((b>>i)&1);
        if(i==31||i==23) cout << " ";
        if(i==23) cout << "   ";
    }
    cout << endl;
}
float EVLFU_8BIT::half_to_float(const ushort x) { // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    const uint e = (x&0x7C00)>>10; // exponent
    const uint m = (x&0x03FF)<<13; // mantissa
    const uint v = as_uint((float)m)>>23; // evil log2 bit hack to count leading zeros in denormalized format
    return as_float((x&0x8000)<<16 | (e!=0)*((e+112)<<23|m) | ((e==0)&(m!=0))*((v-37)<<23|((m<<(150-v))&0x007FE000))); // sign : normalized : denormalized
}
ushort EVLFU_8BIT::float_to_half(const float x) { // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    const uint b = as_uint(x)+0x00001000; // round-to-nearest-even: add last bit after truncated mantissa
    const uint e = (b&0x7F800000)>>23; // exponent
    const uint m = b&0x007FFFFF; // mantissa; in line below: 0x007FF000 = 0x00800000-0x00001000 = decimal indicator flag - initial rounding
    return (b&0x80000000)>>16 | (e>112)*((((e-112)<<10)&0x7C00)|m>>13) | ((e<113)&(e>101))*((((0x007FF000+m)>>(125-e))+1)>>1) | (e>143)*0x7FFF; // sign : normalized : denormalized : saturate
}

void EVLFU_8BIT::print_ev_values(float *arr_floats) {
    for( int idx = 0; idx < EV_DIMENSION; idx++ ) {
        printf("Value = %f, ", arr_floats[idx]);
    }
    printf("\n");
}

void EVLFU_8BIT::chars_buffer_to_floats(char *buffer, float * floats) {
    unsigned char value;
    for( int chunk_idx = 0; chunk_idx < TOTAL_CHUNK_PER_ROW; chunk_idx++ ) {
        memcpy(&value, buffer + (chunk_idx * TOTAL_BYTE_PER_ITEM), TOTAL_BYTE_PER_ITEM); // 4 Byte == size of an int or float
        // Based on /mnt/extra/ev-store-dlrm/script/reduce_precision.py
        floats[chunk_idx] = (((float)value / 254) * 2 ) - 1 ;
        // printf("Value = %f, ", arr_floats[chunk_idx]);
    }
}

char* EVLFU_8BIT::get_from_file(int table_id, int row_id) {
    // cout << "getting from: " << to_string(table_id) << "-" <<row_id << endl;
    FILE* fp = files[table_id - 1];
    // printf("TOTAL_BYTE_PER_ROW %d \n", TOTAL_BYTE_PER_ROW);
    // printf("TOTAL_BYTE_PER_ITEM %d \n", TOTAL_BYTE_PER_ITEM);
    // char buffer[TOTAL_BYTE_PER_ROW]; // 144B == 36dims * 32bit / 8
    char *buffer = (char*) malloc(TOTAL_BYTE_PER_ROW ); // FASTEST
    // char *buffer = new char[TOTAL_BYTE_PER_ROW](); // 144B == 36dims * 32bit / 8
    // Dan: buffer must be char, don't use ushort! 
    // char *buffer malloc and new won't work unless you specify the exact size on fread;
    // This is where the floats are allocated; the other should just point to it
    // ushort* missing_value = (ushort*) malloc(sizeof(ushort) * EV_DIMENSION);
    // float* xx;
    if (fp != NULL) {
        fseek(fp, (int)(TOTAL_BYTE_PER_ROW * row_id), SEEK_SET);  //定位到第二行，每个英文字符大小为1
        // read to our buffer
        if (fread(buffer, TOTAL_BYTE_PER_ROW, 1, fp)){ 
            return buffer;            
            // for( int chunk_idx = 0; chunk_idx < TOTAL_CHUNK_PER_ROW; chunk_idx++ ) {
            //     // float res = half_to_float(missing_value[chunk_idx]);
            //     printf("Key = %d, ", value);
            //     // exit(-1);
            // }
            // printf("\n");
            // exit(-1);
        } else {
            printf("ERROR: Failed to read the chunks from ev table file!\n");
            return NULL;
        }; 
    }
    else {
        cout << "Get EV ERROR!!!" << endl;
    }
    return NULL;
}

void EVLFU_8BIT::phase_2_get_and_insert_missing_values(vector<string>& c2_arr_group_keys, vector<int>& arr_row_ids, 
        vector<bool>& c2_arr_idx_to_insert, vector<bool>& c2_arr_idx_to_update, int c1_c2_agg_hit, 
        float * c1_c2_arr_emb_weights, int debug = 0) {
    // C2: Update aggHit before any item got trashed
    for (int i = 0; i < N_EV_TABLE; i++) {
        if (c2_arr_idx_to_update[i]) { // C2: Hit values
            //  Update the aggHit and get the value
            update_agg_hit(shared_arr_values_in_cache[i], c2_arr_group_keys[i], c1_c2_agg_hit);
            chars_buffer_to_floats(shared_arr_values_in_cache[i]->embedding_value, &(c1_c2_arr_emb_weights[i * EV_DIMENSION]));
        } 
    }

    for (int i = 0; i < N_EV_TABLE; i++) {
        if (c2_arr_idx_to_insert[i]){ // C2: Missing values
            // Get missing key from storage
            shared_arr_missing_values[i] = get_from_file(i + 1, arr_row_ids[i]);
            // Insert key to the cache
            setKey(c2_arr_group_keys[i], shared_arr_missing_values[i], c1_c2_agg_hit);
            // convert the value
            chars_buffer_to_floats(shared_arr_missing_values[i], &(c1_c2_arr_emb_weights[i * EV_DIMENSION]));
        }
    }

    if (c1_c2_agg_hit == N_EV_TABLE)
        // update the number of perfect
        n_perfect_item_C1 = lists_C1[N_EV_TABLE].size();
}

int EVLFU_8BIT::phase_1_find_keys_in_cache(vector<string>& c2_arr_group_keys, vector<bool>& c2_arr_record_hit) {
    int c2_agg_hit = 0;
    for (int i = 0; i < N_EV_TABLE; i++) {
        auto search_result = vals_C1.find(c2_arr_group_keys[i]);
        if (search_result != vals_C1.end()) {
            c2_arr_record_hit[i] = true;
            shared_arr_values_in_cache[i] = &(search_result->second);
            c2_agg_hit++;
        } else {
            c2_arr_record_hit[i] = false;
        }
    }
    return c2_agg_hit;
}

char * EVLFU_8BIT::request_by_key(string key) {
    auto search_result = vals_C1.find(key);
    if (search_result != vals_C1.end()) { // C1 Hit
        // printf("EVLFU_8BIT Hit alternative key!\n");
        // printf("      aggHit = %d\n", (search_result->second).agg_hit);
        // char * embedding_value = (search_result->second).embedding_value;
        // float * emb_weights_in_floats = (float*) malloc(sizeof(float) * EV_DIMENSION);
        // chars_buffer_to_floats_debug(embedding_value, emb_weights_in_floats);
        // print_ev_values(emb_weights_in_floats);
        return ((search_result->second).embedding_value);
    } else { // C1 Miss
        return NULL;
    }
}

char * EVLFU_8BIT::find_approximate_ev(string key) {
    string alt_key = aprx_ev->get_altkey_str(key);
    if (alt_key.empty()) {
        // can't find the alternative key, do nothing
        return NULL;
    } else {
        // printf("found the alternative key %s  (ori key = %s)\n", alt_key.c_str(), key.c_str());
        // check C1 
        char *cached_data = request_by_key(alt_key);
        if (cached_data != NULL) { // C1 Hit
            return cached_data;
        } else { // C1 Miss
            // check C2
            return evlfu_4bit->request_by_key(alt_key);
        }
    }
}

int EVLFU_8BIT::request_to_c1_c2_c3(vector<int>& arr_row_ids, vector<bool>& c1_arr_record_hit, 
        float * c1_c2_arr_emb_weights, int debug = 0) {
    // printf("Inside request_to_c1_c2_c3\n");
    // exit(-1);
    vector<char*> c1_arr_missing_values = vector<char*>(N_EV_TABLE);
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
    int c2_agg_hit = evlfu_4bit->phase_1_find_keys_in_cache(shared_arr_group_keys, c2_arr_record_hit);
    
    // calculate the combined aggHit
    c1_c2_agg_hit = c2_agg_hit;
    // C1: probe the cache
    for (int i = 0; i < N_EV_TABLE; i++) {
        // printf("    %d   %d\n", i, (c2_arr_record_hit[i] == true));
        auto search_result = vals_C1.find(shared_arr_group_keys[i]);
        if (search_result != vals_C1.end()) { // C1 Hit
            c1_arr_record_hit[i] = true;
            // This is buggy, the search_result might be trashed soon; Already HANDLED!!
            c1_arr_values_in_cache[i] = &(search_result->second);
            c1_agg_hit++;
            c2_arr_idx_to_update[i] = false; // C2 hands off on this item

            if (!c2_arr_record_hit[i]) { // Hit at C1 only
                c1_c2_agg_hit ++;
            }
        } else { // C1 Miss
            c1_arr_record_hit[i] = false;
            if (!c2_arr_record_hit[i]) { // Cond 259: Miss at both C1 and C2
                // Try to find the alternative key inside C3, then get the approximate embedding!!
                char *embedding_value = find_approximate_ev(shared_arr_group_keys[i]);
                if (embedding_value != NULL) { // C3 HIT ; C3 LOGIC!!
                    // Update the location of this alt_key in C3, make it more secure
                    aprx_ev->set_recency_flag_c3(shared_arr_group_keys[i]); // set the flag = true

                    // printf(", C3 probe hit i = %d\n", i);
                    aprx_ev_hit += 1;
                    // Utilize this approximate ev values to improve the hit rate!
                    c1_c2_agg_hit ++; // Hit at C3!!
                    
                    // Piggyback the hit status in the c1_arr_record_hit, the c1_arr_values_in_cache[i].agg_hit will be -1
                    c1_arr_record_hit[i] = true;
                    shared_arr_c3_aprx_ev[i] = {embedding_value, -1};
                    c1_arr_values_in_cache[i] = &(shared_arr_c3_aprx_ev[i]);
                    
                    // float * emb_weights_in_floats = (float*) malloc(sizeof(float) * EV_DIMENSION);
                    // chars_buffer_to_floats(c1_arr_values_in_cache[i]->embedding_value, emb_weights_in_floats);
                    // print_ev_values(emb_weights_in_floats);

                    // C2 will NOT insert nor update this item
                    c2_arr_idx_to_insert[i] = false;
                    c2_arr_idx_to_update[i] = false; // C2 hands off on this item
                } else {
                    c2_arr_idx_to_insert[i] = true;
                    c2_arr_idx_to_update[i] = false; // C2 hands off on this item
                }
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
        for (int thd_id = 0; thd_id < N_THD__READ_EVTABLE_8BIT; thd_id++) {
            // update worker status
            sem_post(&LOCK_THD_STATUSES[thd_id]); // tell them to be ready to work
        }
    }

    if (should_update_c2) {
        // C2: Update aggHit or insert the new value
        evlfu_4bit->phase_2_get_and_insert_missing_values(shared_arr_group_keys, arr_row_ids, c2_arr_idx_to_insert, 
            c2_arr_idx_to_update, c1_c2_agg_hit, c1_c2_arr_emb_weights, debug);

        // C2 insert the evicted values to C3!!
        if (evlfu_4bit->shared__arr_evicted_keys.size() > 0) {
            aprx_ev->add_arr_keys_to_batched_io(evlfu_4bit->shared__arr_evicted_keys);
            evlfu_4bit->shared__arr_evicted_keys.clear();
        }
    }

    if (queue_job__read_evtable.size() > 0) {
        // C1: Wait for the results
        sem_wait(&LOCK_WAITING_FOR_RESULT);
    }

    // C1: Update aggHit or insert the new value
    for (int i = 0; i < N_EV_TABLE; i++) {
        // Merge the floats 
        if (c1_arr_record_hit[i]) {
            // C3 LOGIC; checking whether this is C3 hit or not
            if (c1_arr_values_in_cache[i]->agg_hit == -1) {
                // A C3 hit
                // No need to update anything
                // printf("C3 HIT!!!! i = %d\n", i);
            } else {
                // A true C1 hit
                update_agg_hit(c1_arr_values_in_cache[i], shared_arr_group_keys[i], c1_c2_agg_hit);
            }
            // Convert char buffer to float
            chars_buffer_to_floats(c1_arr_values_in_cache[i]->embedding_value, &(c1_c2_arr_emb_weights[i * EV_DIMENSION]));
        } else {
            // C1: Plug the missing values: already in floats*
            if (global__arr_missing_values[i]) {
                setKey(shared_arr_group_keys[i], global__arr_missing_values[i], c1_c2_agg_hit);
                chars_buffer_to_floats(global__arr_missing_values[i], &(c1_c2_arr_emb_weights[i * EV_DIMENSION]));
            } else {
                // This missing value is handled/inserted by C2 
            }
        }
    }

    if (shared__arr_evicted_keys.size() > 0) {
        // C1 insert the evicted values to C3
        aprx_ev->add_arr_keys_to_batched_io(shared__arr_evicted_keys);
        shared__arr_evicted_keys.clear();
    }

    if (c1_c2_agg_hit == N_EV_TABLE) {
        // update the number of perfect
        n_perfect_item_C1 = lists_C1[N_EV_TABLE].size();
        return 1;
    }

    return 0;
}

int EVLFU_8BIT::request_to_c1_c2(vector<int>& arr_row_ids, vector<bool>& c1_arr_record_hit, 
        float * c1_c2_arr_emb_weights, int debug = 0) {
    vector<char*> c1_arr_missing_values = vector<char*>(N_EV_TABLE);
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
    int c2_agg_hit = evlfu_4bit->phase_1_find_keys_in_cache(shared_arr_group_keys, c2_arr_record_hit);
    
    // calculate the combined aggHit
    c1_c2_agg_hit = c2_agg_hit;
    // C1: probe the cache
    for (int i = 0; i < N_EV_TABLE; i++) {
        // printf("    %d   %d\n", i, (c2_arr_record_hit[i] == true));
        auto search_result = vals_C1.find(shared_arr_group_keys[i]);
        if (search_result != vals_C1.end()) { // C1 Hit
            c1_arr_record_hit[i] = true;
            // This is buggy, the search_result might be trashed soon; Already HANDLED!!
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
        for (int thd_id = 0; thd_id < N_THD__READ_EVTABLE_8BIT; thd_id++) {
            // update worker status
            sem_post(&LOCK_THD_STATUSES[thd_id]); // tell them to be ready to work
        }
    }

    if (should_update_c2) {
        // C2: Update aggHit or insert the new value
        evlfu_4bit->phase_2_get_and_insert_missing_values(shared_arr_group_keys, arr_row_ids, c2_arr_idx_to_insert, 
            c2_arr_idx_to_update, c1_c2_agg_hit, c1_c2_arr_emb_weights, debug);
    }

    if (queue_job__read_evtable.size() > 0) {
        // C1: Wait for the results
        sem_wait(&LOCK_WAITING_FOR_RESULT);
    }

    // C1: Update aggHit or insert the new value
    for (int i = 0; i < N_EV_TABLE; i++) {
        // Merge the floats 
        if (c1_arr_record_hit[i]) {
            update_agg_hit(c1_arr_values_in_cache[i], shared_arr_group_keys[i], c1_c2_agg_hit);
            chars_buffer_to_floats(c1_arr_values_in_cache[i]->embedding_value, &(c1_c2_arr_emb_weights[i * EV_DIMENSION]));
        } else {
            // C1: Plug the missing values: already in floats*
            if (global__arr_missing_values[i]) {
                setKey(shared_arr_group_keys[i], global__arr_missing_values[i], c1_c2_agg_hit);
                chars_buffer_to_floats(global__arr_missing_values[i], &(c1_c2_arr_emb_weights[i * EV_DIMENSION]));
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

int EVLFU_8BIT::request_to_ev_lfu(vector<int>& arr_row_ids, vector<bool>& arr_record_hit, 
        vector<char *>& arr_emb_weights) {
    vector<Cache_data*> arr_values_in_cache = vector<Cache_data*>(N_EV_TABLE);
    int agg_hit = 0;
    // Construct the keys
    for (int i = 0; i < N_EV_TABLE; i++) {
        shared_arr_group_keys[i] = to_string(i + 1) + "-" + to_string(arr_row_ids[i]);
    }
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

    // printf("Done submitting jobs total = %d\n", n_miss);
    if (queue_job__read_evtable.size() > 0) {
        // Wake up the worker; the jobs are ready
        for (int thd_id = 0; thd_id < N_THD__READ_EVTABLE_8BIT; thd_id++) {
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
        // shutdown_evtable_reader_thds();
        // exit(-1);
    }

    // Update aggHit or insert the new value
    for (int i = 0; i < N_EV_TABLE; i++) {
        if (arr_record_hit[i]) {
            update_agg_hit(arr_values_in_cache[i], shared_arr_group_keys[i], agg_hit);
            arr_emb_weights[i] = arr_values_in_cache[i]->embedding_value;
        } else {
            // Plug the missing values:
            setKey(shared_arr_group_keys[i], global__arr_missing_values[i], agg_hit);
            arr_emb_weights[i] = global__arr_missing_values[i];
        }
    }

    if (agg_hit == N_EV_TABLE) {
        // update the number of perfect
        n_perfect_item_C1 = lists_C1[N_EV_TABLE].size();
        return 1;
    }
    return 0;
}

vector<vector<int>> EVLFU_8BIT::prepare_workload () {
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

int main8ss() {
    EVLFU_8BIT *evlfu_8bit = new EVLFU_8BIT(7680*2, false, -4, false, "");
    // Prepare workload
    vector<vector<int>> groupedWorkloadKeys = evlfu_8bit->prepare_workload();
    // Done merging ALL workloads!
    int perfectHit = 0;
    cout << "Start caching!!" << endl;
    auto start = std::chrono::high_resolution_clock::now();

    vector<bool> aggHitMissRecord = vector<bool>(N_EV_TABLE);
    vector<char * > emb_weights_in_chars = vector<char *>(N_EV_TABLE);
    vector<float * > emb_weights_in_floats = vector<float *>(N_EV_TABLE);

    // allocate memory for vector of emb_weights
    for (int idx = 0; idx < N_EV_TABLE; idx++) {
       emb_weights_in_floats[idx] = (float*) malloc(sizeof(float) * EV_DIMENSION);
    }

    for (int i = 0; i < groupedWorkloadKeys.size(); i++) {
        evlfu_8bit->request_to_ev_lfu(groupedWorkloadKeys[i], aggHitMissRecord, emb_weights_in_chars);

        for (int x = 0; x < N_EV_TABLE; x++) {
            evlfu_8bit->chars_buffer_to_floats(emb_weights_in_chars[x], emb_weights_in_floats[x]);
        }
        // send emb_weights_in_floats to the cache_manager

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

    // free memory for vector of emb_weights
    for (int idx = 0; idx < N_EV_TABLE; idx++) {
        if (emb_weights_in_floats[idx]) {
            free(emb_weights_in_floats[idx]);
        }
    }
    return 0;
}
// g++ -O3 evlfu_8.cpp -pthread; ./a.out
