
#ifndef EVLFU_CACHE_MANAGER_INCLUDED
#define EVLFU_CACHE_MANAGER_INCLUDED

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
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/epoll.h>  // for epoll_create1(), epoll_ctl(), struct epoll_event
#include <sys/socket.h>
#include <cassert>

using namespace std;

#define EV_DIMENSION 36
#define N_EV_TABLE 26

#define PORT 8080
#define MAX_EVENTS 5
#define READ_SIZE 250
#define MAX_WORKER_THREADS 10
#define READ_HANDSHAKE_SIZE 3
#define HANDSHAKE_MSG_LEN 51

int worker_statuses[MAX_WORKER_THREADS];  // if the worker is available, the value will be 0
int add_new_client = 0;
int new_client_conn;
sem_t LOCK_WAITING_CLIENT_CONN;
int incoming_buffer_length = 0; // will hold the incoming buff len of the new client

pthread_t thd_server_socket_conn;
pthread_t thd_req_processor[MAX_WORKER_THREADS];

vector<bool> aggHitMissRecord = vector<bool>(N_EV_TABLE);
vector<char * > emb_weights_in_chars = vector<char *>(N_EV_TABLE);
float emb_weights_in_1d_floats[N_EV_TABLE * EV_DIMENSION];

// void init(int capacity);
// void request_to_ev_lfu(vector<int>& group_keys, vector<bool>& arr_record_hit, 
//         vector<char*>& arr_emb_weights, bool use_gpu);
// void load_ev_tables();
// void close_ev_tables();
vector<string> split(const string& s, const string& delim);

#endif