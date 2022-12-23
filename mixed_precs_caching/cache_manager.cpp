#include "cache_manager.hpp"
#include "evlfu_32.hpp"
#include "evlfu_16.hpp"
#include "evlfu_8.hpp"
#include "evlfu_4.hpp"
#include "aprx_embedding.hpp"
#include <chrono>

// g++ -O3 evlfu_4.cpp evlfu_8.cpp evlfu_16.cpp evlfu_32.cpp cache_manager.cpp -pthread; ./a.out
// g++ -shared -o libcachemanager.so -fPIC -O3 evlfu_4.cpp evlfu_8.cpp evlfu_16.cpp evlfu_32.cpp cache_manager.cpp -pthread
// mv *.so lib/

#define N_CACHING_LAYER       3           // 1 , 2 layers, or 3 layers (C1, C2, APRX_EV)
#define MAIN_PRECISION        8           // 32, 16, 8, or 4
#define SECONDARY_PRECISION   4           // must be lower than the MAIN_PRECISION
#define TOTAL_SIZE            75425       // 50000 30000 20000 8000 is 21%, 13%, and 3% in memory
#define SIZE_PROPORTION       "48-48-4"  // 30-50-20 | 35 55 10 |The size of C1 : C2 : C3. The total must be 100
                                          // The size of C1 : C2 : C3. The total must be 100. 
                                          // If empty, the size will be divided evenly
#define N_WARMUP_REQUEST      137521      // Must be the same as the warm-up workload at dlrm_C1_C2 

/*
    Calculating C3 Cache size:
        1 ev size:
            32bit  -> 32bit * 36 = 1152bit / 8 = 144 Byte
            8bit   -> 8bit * 36 =              =  36 Byte
            aprxEV -> Only store key as uInt4  =   4 Byte  (9x relative to 8bit)
*/

// DAN: Caching layers MUST be instantiated here!
#if N_CACHING_LAYER == 3
    bool init_second_layer = true;
    bool init_third_layer = true;
    // int cacheSize = TOTAL_SIZE / 3; // will give 33% of the space to the second layer and third layer
#elif N_CACHING_LAYER == 2
    bool init_second_layer = true;
    bool init_third_layer = false;
    int cacheSize = TOTAL_SIZE / 2; // will give 50% of the space to the second layer
#else
    bool init_second_layer = false;
    bool init_third_layer = false;
    int cacheSize = TOTAL_SIZE; 
#endif

#if MAIN_PRECISION == 32
    EVLFU_32BIT *evlfu_32bit = new EVLFU_32BIT(cacheSize, init_second_layer, SECONDARY_PRECISION);
#elif MAIN_PRECISION == 16
    EVLFU_16BIT *evlfu_16bit = new EVLFU_16BIT(cacheSize * 2, init_second_layer, SECONDARY_PRECISION);
#elif MAIN_PRECISION == 8
    EVLFU_8BIT *evlfu_8bit = new EVLFU_8BIT(TOTAL_SIZE, init_second_layer, SECONDARY_PRECISION, init_third_layer, SIZE_PROPORTION);
#elif MAIN_PRECISION == 4
    EVLFU_4BIT *evlfu_4bit = new EVLFU_4BIT(cacheSize * 8, init_second_layer, SECONDARY_PRECISION);
#else 
    #define INVALID_DIRECTIVE_ARGS
#endif

// int n_overhead_bytes = 16 * (N_EV_TABLE);
int total_bytes_to_send = (EV_DIMENSION * 4 * N_EV_TABLE); // + n_overhead_bytes;
int perfectHit = 0;

void *open_server_conn(void *vargp) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char read_buffer[READ_HANDSHAKE_SIZE + 1];

    int running = 1, event_count, i;

    printf("INFO: Opening socket connection to clients\n");
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        printf("ERROR: socket failed\n");
        return NULL;
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt))) {
        printf("ERROR: setsockopt\n");
        return NULL;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("ERROR: bind failed\n");
        return NULL;
    }
    if (listen(server_fd, 3) < 0) {
        printf("ERROR: listen\n");
        return NULL;
    }

    // create the epoll
    int epoll_fd = epoll_create(2);

    if (epoll_fd == -1) {
        fprintf(stderr, "Failed to create epoll file descriptor\n");
        return NULL;
    }
    printf("open_server_conn:: epoll_fd is = %d\n", epoll_fd);
    printf("    server_fd is = %d\n", server_fd);

    // init epoll events
    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = server_fd;  // fd to return the data

    // add sockfd to epoll with corresponding event data
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event)) {
        fprintf(stderr, "Failed to add file descriptor to epoll\n");
        close(epoll_fd);
        return NULL;
    }

    printf("\nopen_server_conn:: Waiting for client connection...\n");
    while (running) {
        event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, 30000);
        // printf("%d ready events\n", event_count);

        for (i = 0; i < event_count; i++) {
            printf("   \nopen_server_conn:: Server is establishing client connection \n");
            // socket that is connected to client
            if ((new_client_conn =
                     accept(server_fd, (struct sockaddr *)&address,
                            (socklen_t *)&addrlen)) < 0) {
                printf("ERROR: accept\n");
                return NULL;
            }
            // read 3 byte (or READ_HANDSHAKE_SIZE) from incoming client to get the length of its buffer
            int bytes_read = read(new_client_conn, read_buffer, READ_HANDSHAKE_SIZE);
            printf("   open_server_conn:: This is the length of the buffer that the client will be sending\n");
            printf("   open_server_conn:: message from client: %dB: \"%s\" \n", bytes_read, read_buffer);
            incoming_buffer_length = std::stoi(read_buffer);
            // assert(incoming_buffer_length > 0);

            // Send confirmation
            char message[HANDSHAKE_MSG_LEN];
            sprintf(message, "Hello from server, ready to read input buffer[%s]", read_buffer);
            send(new_client_conn, message, HANDSHAKE_MSG_LEN, 0);
            printf("      open_server_conn:: New client connection at fd = %d\n", new_client_conn);
            sem_post(&LOCK_WAITING_CLIENT_CONN);  // will start the worker
            // and let the worker to listen to this client
        }
    }
    close(server_fd);
    close(epoll_fd);
    return NULL;
}

extern "C" void test_arr (int *arr) {
    for (int i=0; i < 5; i++) {
        printf("key %d, ", arr[i]);
    }
    printf("\n");

    vector<int> v;
    v.assign(arr, arr+5); // 5 is size of array.

    for (int i=0; i < 5; i++) {
        printf("vec %d, ", arr[i]);
    }

    printf("\n");
}

void ev_lookup_based_on_vector_keys(vector<int> &group_row_ids) {
    std::vector<bool> aggHitMissRecord = std::vector<bool>(26);
    // Single layer caching
    #if N_CACHING_LAYER == 1
    {
        #if MAIN_PRECISION == 32
            perfectHit += evlfu_32bit->request_to_ev_lfu(group_row_ids, aggHitMissRecord, emb_weights_in_1d_floats);
        #elif MAIN_PRECISION == 16
            perfectHit += evlfu_16bit->request_to_ev_lfu(group_row_ids, aggHitMissRecord, emb_weights_in_chars);
        #elif MAIN_PRECISION == 8
            perfectHit += evlfu_8bit->request_to_ev_lfu(group_row_ids, aggHitMissRecord, emb_weights_in_chars);
        #elif MAIN_PRECISION == 4
            perfectHit += evlfu_4bit->request_to_ev_lfu(group_row_ids, aggHitMissRecord, emb_weights_in_chars);
        #endif
        
        // Manual Convert to floats
        for (int x = 0; x < N_EV_TABLE; x++) {
            #if MAIN_PRECISION == 16
                evlfu_16bit->chars_buffer_to_floats(emb_weights_in_chars[x], &(emb_weights_in_1d_floats[x * EV_DIMENSION]));
            #elif MAIN_PRECISION == 8
                evlfu_8bit->chars_buffer_to_floats(emb_weights_in_chars[x], &(emb_weights_in_1d_floats[x * EV_DIMENSION]));
            #elif MAIN_PRECISION == 4
                evlfu_4bit->chars_buffer_to_floats(emb_weights_in_chars[x], &(emb_weights_in_1d_floats[x * EV_DIMENSION]));
            #endif
        }
    }

    // Double layer caching
    #elif N_CACHING_LAYER == 2
    {
        #if MAIN_PRECISION == 32
            perfectHit += evlfu_32bit->request_to_c1_c2(group_row_ids, aggHitMissRecord, emb_weights_in_1d_floats, 0);
        #elif MAIN_PRECISION == 16
            perfectHit += evlfu_16bit->request_to_c1_c2(group_row_ids, aggHitMissRecord, emb_weights_in_1d_floats, 0);
        #elif MAIN_PRECISION == 8
            perfectHit += evlfu_8bit->request_to_c1_c2(group_row_ids, aggHitMissRecord, emb_weights_in_1d_floats, 0);
        #endif
    }

    // Triple layer caching
    #elif N_CACHING_LAYER == 3
    {
        #if MAIN_PRECISION == 32
            printf("ERROR: request_to_c1_c2_c3 for 32bit is NOT yet ready!\n");
            exit(-1);
        #elif MAIN_PRECISION == 16
            printf("ERROR: request_to_c1_c2_c3 for 16bit is NOT yet ready!\n");
            exit(-1);
        #elif MAIN_PRECISION == 8
            perfectHit += evlfu_8bit->request_to_c1_c2_c3(group_row_ids, aggHitMissRecord, emb_weights_in_1d_floats, 0);
        #endif
    }

    #else 
        printf("ERROR: cache_manager.cpp N_CACHING_LAYER is NOT recognized!!\n");
        exit(-1);
    #endif

    // return 0;
}

extern "C" float* ev_lookup(int *arr) {
    vector<int> group_row_ids;
    group_row_ids.assign(arr, arr + N_EV_TABLE); // 5 is size of array.
    
    ev_lookup_based_on_vector_keys(group_row_ids);
    return emb_weights_in_1d_floats;
}

extern "C" int ev_lookup_based_on_list_keys(int *arr) {
    // UNUSED!!
    printf("ERROR: This ev_lookup_based_on_list_keys() is outdated, better to use ev_lookup_based_on_vector_keys() instead!");
    // evlfu_32bit->check_size();
    exit(-1);
    // // Only accept arr that has 26 items
    // vector<int> group_row_ids;
    // group_row_ids.assign(arr, arr + N_EV_TABLE); // 5 is size of array.
    
    // // for (int i=0; i < N_EV_TABLE; i++) {
    // //     printf("vec %d, ", group_row_ids[i]);
    // // }

    // return ev_lookup_based_on_vector_keys(group_row_ids);
}

// get the result 

extern "C" float* get_ev_values(int *arr) {
    return emb_weights_in_1d_floats;
}


extern "C" void print_perfect_hit() {
    // Print the stats after completing the warm-up period
    printf("\n[epoll worker] C1_PRECISION    = %d\n", MAIN_PRECISION);
#if N_CACHING_LAYER == 2
    printf("[epoll worker] C2_PRECISION    = %d\n", SECONDARY_PRECISION);
#elif N_CACHING_LAYER == 3
    printf("[epoll worker] C2_PRECISION    = %d\n", SECONDARY_PRECISION);
    printf("[epoll worker] C3 APRX_EV      = ACTIVE\n");
    printf("[epoll worker] SIZE_PROPORTION = %s\n", SIZE_PROPORTION);

    #if MAIN_PRECISION == 32
        printf("ERROR: this C3 precision is not yet supported!!\n");
        exit(-1);
    #elif MAIN_PRECISION == 16
        printf("ERROR: this C3 precision is not yet supported!!\n");
        exit(-1);
    #elif MAIN_PRECISION == 8
        printf("[epoll worker] C3 Indiv-Hit    = %d\n", evlfu_8bit->aprx_ev_hit);
    #else 
        printf("ERROR: this C3 precision is not yet supported!!\n");
        exit(-1);
    #endif
    
    
#endif
    printf("[epoll worker] TOTAL_SIZE      = %d\n", TOTAL_SIZE);
    printf("[epoll worker] Perfect hit     = %d\n", perfectHit);
    perfectHit = 0; 
}

void *listen_and_process_client_request(void *vargp) {
    int *worker_tid = (int *)vargp;
    int event_count, i, byte_ctr = 0;
    int bytes_read = 0, bytes_sent = 0;
    // To handle incoming socket with different buffer length
    int buffer_size = worker_statuses[*worker_tid];
    int n_keys = buffer_size / 4; // 4 == 4 Byte == 32 bit == 1 int
    std::vector<int> group_row_ids = std::vector<int>(n_keys);

    byte_ctr++;  // dummy usage
    bytes_read++;  // dummy usage
    printf("[epoll worker] Worker thread (%d) is created!\n", *worker_tid);
    int counter = 0;
    int req_counter = 0;

    printf("  [epoll worker] Ready to read incoming buffer len = %d\n", buffer_size);

    char read_buffer[READ_SIZE + 1];

    // create the epoll
    int epoll_fd = epoll_create(1);

    if (epoll_fd == -1) {
        fprintf(stderr, "Failed to create epoll file descriptor\n");
        pthread_exit(NULL);
        return NULL;
    }
    printf("   [epoll worker] listen_and_process_client_request epoll_fd is = %d\n", epoll_fd);

    // init epoll events
    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = new_client_conn;  // fd to return the data

    // add sockfd to epoll with corresponding event data
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client_conn, &event)) {
        fprintf(stderr, "Failed to add file descriptor to epoll\n");
        close(epoll_fd);
        pthread_exit(NULL);
        return NULL;
    }

    printf("   [epoll worker] Worker %d is processing clients requests...\n", *worker_tid);
    int running = 1;
    while (running) {
        event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, 30000);

        for (i = 0; i < event_count; i++) {
            counter = counter + 1;
            // Getting grouped keys from the client (DLRM inference)
            bytes_read = read(events[i].data.fd, read_buffer, buffer_size);
            // printf("      %d bytes read.\n", bytes_read);
            // printf("[epoll worker] Perfect hit (check)   = %d\n", perfectHit);
            if (bytes_read <= 0) {
                // client closed the connection
                running = 0;
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, new_client_conn, &event);
            } else {
                req_counter += 1;
                // Convert the grouped keys to array of integer
                // printf("Req %d Got keys \n", req_counter);
                for( byte_ctr = 0; byte_ctr < n_keys; byte_ctr++ ) {
                    memcpy(&group_row_ids[byte_ctr], read_buffer + (byte_ctr * 4), 4); // 4 Byte == size of an int
                    // printf(" %d", group_row_ids[byte_ctr]);
                }
                // printf("\n");

                // Do EV-Lookup here
                ev_lookup_based_on_vector_keys(group_row_ids);
                // This overhead is the bytes used by vector to separate the next item; it is 16 bytes at the end
                // DAN: DO NOT Send vector over socket, the item is not in the right order as in the bytes!!!
                bytes_sent = send(events[i].data.fd, emb_weights_in_1d_floats, total_bytes_to_send, 0);
                if (bytes_sent < 0) {
                    printf("ERROR: Failed to sent out the data to client (socket fd: %d)!\n", events[i].data.fd);
                    // evlfu_32bit->check_size();
                    exit(-1);
                } else {
                    // printf("Successfully sent out %d bytes to fd %d from %d tables\n", bytes_sent, events[i].data.fd, N_EV_TABLE);
                }
                if (req_counter == N_WARMUP_REQUEST) { // the end of the warm-up request
                    // Print the stats after completing the warm-up period
                    print_perfect_hit();
                }
            }
        }
    }
    print_perfect_hit();
    worker_statuses[*worker_tid] = 0;  // put a NON busy flag
    printf("   [epoll worker] worker thread (%d) is killed\n", *worker_tid);

    close(epoll_fd);
    pthread_exit(NULL);
    return NULL;
}

int find_available_worker(int start_id) {
    int id = start_id, counter = 0;
    while (counter <= MAX_WORKER_THREADS) {
        // printf("MAX_WORKER_THREADS %d, id %d, worker_statuses[id] %d\n", MAX_WORKER_THREADS, id, worker_statuses[id]);
        if (id == MAX_WORKER_THREADS) {
            // reset the counter to worker-0
            id = 0;
        }
        if (worker_statuses[id] == 0) {
            // this worker is available
            printf("find_available_worker:: Worker #%d is available\n", id);
            return id;
        }
        id++;
        counter++;
    }
    // ALL the workers are busy
    // TODO: Handle this condition nicely
    printf("find_available_worker:: ERROR: Not enough worker!! Increase the max value!\n");
    return -1;
}


extern "C" void init_global_vars() {
    std::fill_n(worker_statuses, MAX_WORKER_THREADS, 0); 
    sem_init(&LOCK_WAITING_CLIENT_CONN, 0, 0);
    
    printf("INFO: MAX_WORKER_THREADS = %d  (capable of accepting those number of DLRM client)\n", MAX_WORKER_THREADS);
    pthread_create(&thd_server_socket_conn, NULL, open_server_conn, NULL);
}

// Currently, the server will always running; kill manually "ctrl + c"
extern "C" void start_server_threads() {
    // Worker threads will be spawned as long as we have client
    int worker_ids[MAX_WORKER_THREADS];
    int running = 1, idx = 0;

    while (running) {
        // Start the worker thread after we have at least 1 client
        sem_wait(&LOCK_WAITING_CLIENT_CONN);
        // find available worker
        idx = find_available_worker(idx);
        // put a busy flag ; which is the incoming_buffer_length
        // assert(idx >= 0);
        worker_statuses[idx] = incoming_buffer_length;  
        if (idx < 0) {
            printf("ERROR: index is invalid\n");
            exit(-1);
        }
        worker_ids[idx] = idx;
        pthread_create(&thd_req_processor[idx], NULL,
                       listen_and_process_client_request, &worker_ids[idx]);
        // Don't call join here, the main will be suspended
        idx++;
    }

    pthread_join(thd_server_socket_conn, NULL);
    sem_destroy(&LOCK_WAITING_CLIENT_CONN);
}

int main () {
    clock_t startTime, endTime;

    init_global_vars();
    start_server_threads();

    return 0;
}

// g++ -O3 evlfu_4.cpp cache_manager.cpp -pthread; ./a.out