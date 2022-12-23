#include "evlfu.hpp"

using namespace std;

// global variables:
int cap_C1 = -1, min_C1 = 0;
unordered_map<string, Cache_data> vals_C1;
unordered_map<int, list<string>> lists_C1;

int n_perfect_item_C1 = 0,
    max_perfect_item_C1 = 0;
double flush_rate_C1 = 0.4,
       perfect_item_cap_C1 = 1.0,
       max_perfect_item_cap_C1 = 0;

vector<FILE *> files = vector<FILE *>(26);
string EV_TABLE_PATH = "/mnt/extra/ev-store-dlrm/stored_model/criteo_kaggle_all_mmap/epoch-00/ev-table/binary/";
string WORKLOAD_PATH = "/home/cc/workload/Archive-new-0.5M/";

// temporal util function for the setup of the testing worklod, no need attention
vector<string> split(const string &s, const string &delim)
{
    vector<string> elems;
    size_t pos = 0;
    size_t len = s.length();
    size_t delim_len = delim.length();
    if (delim_len == 0)
        return elems;
    while (pos < len)
    {
        int find_pos = s.find(delim, pos);
        if (find_pos < 0)
        {
            elems.push_back(s.substr(pos, len - pos));
            break;
        }
        elems.push_back(s.substr(pos, find_pos - pos));
        pos = find_pos + delim_len;
    }
    return elems;
}

void load_ev_tables(){
    for(int i=0;i<26;i++){
        string ev_table_path = EV_TABLE_PATH + "ev-table-" + to_string(i+1) + ".bin";
        FILE *fp = fopen(ev_table_path.c_str(), "rb");
        if(fp!=NULL){
            files[i] = fp;
        }
    }
}

void close_ev_tables(){
    for(int i=0;i<26;i++){
        FILE * fp = files[i];
        fclose(fp);
        fp = NULL;
    }
}

void init(int capacity)
{
    cap_C1 = capacity;
    int i = 0;
    while (i <= 26)
        lists_C1[i++] = list<string>();
    max_perfect_item_C1 = int(cap_C1 * perfect_item_cap_C1);
}

void set(string &key, vector<float> &value, int agg_hit)
{
    // Flushing:
    if (n_perfect_item_C1 >= max_perfect_item_C1)
    {
        printf("flushing!\n");
        printf("n_perfect_item_C1 = %d\n", n_perfect_item_C1);
        printf("max_perfect_item_C1 = %d\n", max_perfect_item_C1);
        for (int i = 0; i <= int(flush_rate_C1 * cap_C1); i++)
        {
            string key_to_evict = lists_C1[26].front();
            lists_C1[26].pop_front();
            vals_C1.erase(key_to_evict);
        }
        n_perfect_item_C1 -= int(flush_rate_C1 * cap_C1);
    }
    else
    {
        // cache is full
        if (int(vals_C1.size()) >= cap_C1)
        {
            // make a space for the new key:
            while (lists_C1[min_C1].empty())
            {
                // find the right key to pop
                // Update minimum agg_hit
                min_C1++;
                if (min_C1 > 26)
                    min_C1 = 1;
            }
            string key_to_evict = lists_C1[min_C1].front();
            lists_C1[min_C1].pop_front();
            vals_C1.erase(key_to_evict);
        }
    }
    // insert the new value:
    vals_C1[key] = Cache_data(value, agg_hit);
    lists_C1[agg_hit].push_back(key);
    if (agg_hit < min_C1)
        min_C1 = agg_hit;
}

// Get from
vector<float> update_agg_hit(string &key, int agg_hit)
{
    if (vals_C1.count(key) == 0)
        return vector<float>(0);
    Cache_data ev_vals = vals_C1[key];
    if (ev_vals.agg_hit < agg_hit)
    {
        lists_C1[ev_vals.agg_hit].remove(key);
        lists_C1[agg_hit].push_back(key);
        vals_C1[key].agg_hit = agg_hit;
    }
    return ev_vals.embedding_value;
}

vector<float> get_from_file(int table_id, int row_id){
    // cout << "getting from: " << to_string(table_id) << "-" <<row_id << endl;
    FILE *fp = files[table_id-1];
    // cout << files[0] << endl;
    // cout << cap_C1;
    // exit(0);
    char buffer[144];
    vector<float> missing_value = vector<float>(36);
    if(fp != NULL){
        fseek(fp, (int)(144*row_id), SEEK_SET);  //定位到第二行，每个英文字符大小为1
        fread(buffer, sizeof(buffer), 1, fp); // read to our buffer
        for (int i = 0; i < 36; i++) {
            missing_value[i] = (*(float*)(buffer + 4 * i));
        }
        // for (int i = 0; i < 36; i++) {
        //     cout << missing_value[i] << endl;
        // }
        // exit(0);
        // fclose(fp);
        // fp = NULL;
    }else{
        cout << "Get EV ERROR!!!" << endl;
    }
    return missing_value;
}

vector<float> update(string &key, int table_id, int row_id, int agg_hit, vector<float> missing_value = vector<float>())
{
    vector<float> val = update_agg_hit(key, agg_hit);
    if (!val.empty())
        return val;
    if (missing_value.empty())
    {
        // Get value from secondary storage!!
        //cout << "Key: " << key << endl;
        missing_value = get_from_file(table_id, row_id);
    }
    set(key, missing_value, agg_hit);
    return missing_value;
}

void request_to_ev_lfu(vector<int> &group_keys, vector<bool> &arr_record_hit, vector<vector<float>> &arr_emb_weights, bool use_gpu = false)
{
    vector<string> arr_group_keys = vector<string>(26);
    vector<int *> arr_missing_keys;
    vector<vector<float>> arr_missing_values = vector<vector<float>>(26);
    int agg_hit = 0;
    for (int i = 0; i < 26; i++)
    {
        int row_id = group_keys[i];
        // cout << row_id;
        string key = to_string(i + 1) + "-";
        key += to_string(row_id);
        // cout << key << endl;
        arr_group_keys[i] = key;

        if (vals_C1.count(key) != 0)
        {
            arr_record_hit[i] = true;
            agg_hit++;
        }
        else
        {
            int tmp[2] = {i + 1, row_id};
            arr_missing_keys.push_back(tmp);
            arr_record_hit[i] = false;
        }
    }
    // Get all missing keys from storage at once:
    for (int i = 0; i < 26; i++)
        arr_missing_values[i] = get_from_file(i+1, group_keys[i]);
    /////////////////////////////////////////////

    // if (use_gpu)
    // {
    //
    // }
    /////////////////////////////////////////////
    for (int i = 0; i < 26; i++)
    {
        // Multithreading!
        ////////////////////////////////////////////////
        int row_id = group_keys[i];
        vector<float> val, ev_tensor;

        if (arr_record_hit[i])
            val = update(arr_group_keys[i], i + 1, row_id, agg_hit);

        else
        // Plug the missing values:
        {
            val = update(arr_group_keys[i], i + 1, row_id, agg_hit, arr_missing_values[i]);
        }

        // To torch.tensor
        ev_tensor = val;
        // use GPU
        // if (use_gpu)
        //     ev_tensor = ev_tensor;
        /////////////////////////////
        arr_emb_weights[i] = ev_tensor;
    }
    if (agg_hit == 26)
        // update the number of perfect
        n_perfect_item_C1 = lists_C1[26].size();
}

int main()
{
    // The following [1] code is for algo consistency testing.
    // The uncommented code [2] generate the workload randomly (for quick test).

    //[1] For real workload test.
    string workload_dir = WORKLOAD_PATH;
    string *workload_files = new string[26];
    vector<vector<int>> arrRawWorkload;
    for (int i = 1; i <= 26; i++)
    {
        workload_files[i - 1] = workload_dir;
        workload_files[i - 1] += "workload-group-" + to_string(i);
        workload_files[i - 1] += ".csv";
    }

    for (int i = 0; i < 26; i++)
    {
        ifstream in(workload_files[i]);
        string line;
        vector<int> workload;
        if (in.fail())
        {
            cout << "File not found" << endl;
            return 0;
        }
        while (getline(in, line) && in.good())
        {
            workload.push_back(atoi(split(line, "-")[1].c_str()));
        }
        in.close();
        // cout << "end!" << endl;
        arrRawWorkload.push_back(workload);
    }
    vector<vector<int>> groupedWorkloadKeys = vector<vector<int>>(arrRawWorkload[0].size());
    for (int i = 0; i < arrRawWorkload[0].size(); i++)
    {
        vector<int> group_keys = vector<int>(26);
        for (int j = 0; j < arrRawWorkload.size(); j++)
        {
            group_keys[j] = arrRawWorkload[j][i];
        }
        groupedWorkloadKeys[i] = group_keys;
    }
    //// [2] for random quick test:
    // int totalWorkload = 1000000; // 1 million
    // vector<vector<int>> groupedWorkloadKeys(totalWorkload, vector<int>(26, 0));
    // for (int i = 0; i < totalWorkload; i++)
    //{
    //     for (int j = 0; j < 26; j++)
    //     {
    //         groupedWorkloadKeys[i][j] = rand();
    //     }
    // }

    // Done merging ALL workloads!
    // Run the alg:
    load_ev_tables();
    init(768);
    int perfectHit = 0;
    cout << "Start caching!!" << endl;
    clock_t startTime, endTime;
    startTime = clock();
    for (int i = 0; i < groupedWorkloadKeys.size(); i++)
    {
        vector<bool> aggHitMissRecord = vector<bool>(26);
        vector<vector<float>> emb_weights = vector<vector<float>>(26);
        request_to_ev_lfu(groupedWorkloadKeys[i], aggHitMissRecord, emb_weights);
        // for (int x = 0; x < 26; x++) {
        //     for (int y = 0; y < 36; y++) {
        //         cout << emb_weights[x][y] << " ";
        //     }
        //     cout << endl;
        // }
        bool flag = true;
        for (int j = 0; j < aggHitMissRecord.size(); j++)
        {
            if (!aggHitMissRecord[j])
            {
                flag = false;
                break;
            }
        }
        if (flag)
        {
            perfectHit++;
        }
    }
    printf("perfect hit:%d\n", perfectHit);
    endTime = clock();
    cout << "The run time is: " << (double)(endTime - startTime) / CLOCKS_PER_SEC << "s" << endl;
    close_ev_tables();
    return 0;
}