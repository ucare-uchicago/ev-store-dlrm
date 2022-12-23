// CPP program to illustrate the
// unordered_set::erase() function
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
using namespace std;

int * test1(int *d){
    int *i = d;
    return i;
    // return NULL;
} 

// g++ -O3 test.cpp -pthread; ./a.out
float cpy_float[30];

int main()
{
    float real_float = 0.002345f;
    float arr_floats[] = {0.002345f, 0.00023f};
    float *fl;
    fl = arr_floats;
    
    memcpy(cpy_float + 1, fl, 8);
    printf("float real_float %f\n", real_float);
    printf("float *fl %f\n", *fl);
    printf("float cpy_float %f\n", cpy_float[0]);
    printf("float cpy_float %f\n", cpy_float[1]);

    exit(-1);
    int f = 4;  
    int *x = test1(&f);
    if (x != NULL)
        printf("x %d\n", *x);
    unordered_set<string> sampleSet = { "geeks1", "for", "geeks2", "ggeee" };
    // unordered_map<string> sampleMap = { "geeks1":2, "geeks1":3, "geeks2":4, "ggeee":2 };
 
    // erases a particular element
    sampleSet.erase("geeks1");
 
    // displaying the set after removal
    cout << "Elements: ";
    for (auto it = sampleSet.begin(); it != sampleSet.end(); it++) {
        cout << *it << " ";
    }
 
    sampleSet.insert("geeks1");
    // erases from where for is
    sampleSet.erase(sampleSet.find("for"), sampleSet.end());
    string str = "sss";
    // displaying the set after removal
    cout << "\nAfter second removal set : \n";
    for (auto it = sampleSet.begin(); it != sampleSet.end(); it++) {
        printf("key_to_evict = %s\n", (*it).c_str());
        str = *it;
        // cout << *it << " ";
    }
    sampleSet.erase(str);
 
    cout << "\nAfter second removal set :\n";
    for (auto it = sampleSet.begin(); it != sampleSet.end(); it++) {
        printf("key_to_evict = %s\n", (*it).c_str());
    }

    float* missing_value = (float*) malloc(sizeof(float) * 36);
    missing_value[0] = 1.23;
    printf("test %f\n", missing_value[0]);
    return 0;
}