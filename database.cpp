#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include "database.h"

using namespace std;

ofstream fio("abc.bin", ios::binary|ios::app);
ifstream file("abc.bin", ios::binary);
map<u_int32_t, int> index;

void read_func();
void put();
void get();
void index_file();

int main() {
    index_file();
    cout << "Choose action: 1. Put, 2. Get" << endl;
    string action;
    cin >> action;
    if (action == "Put") {
        put();
    } else if (action == "Get") {
        get();
    }
    // read_func();
    return 0;
}

void index_file() {
    if (file.is_open()) {
        RecordHeader rh;
        file.read(reinterpret_cast<char*>(&rh), sizeof(rh));
        streampos offset = file.tellg();
        index.insert(make_pair(rh.key, offset));
    }
}

void read_func() {
    if (file.is_open()) {
        RecordHeader rh;
        file.read(reinterpret_cast<char*>(&rh), sizeof(rh));
        cout << rh.key << endl;
    }
}

void put() {
    uint32_t key;
    uint32_t value;
    cout << "Type key: ";
    cin >> key;
    cout << "Type value: ";
    cin >> value;
    RecordHeader rh = {key, value};
    fio.write(reinterpret_cast<char*>(&rh), sizeof(rh));
    fio.close();
}

void get() {
    cout << "Type key: ";
    int key;
    cin >> key;
    RecordHeader rh;
    int val;
    bool found = false;
    while (file.read(reinterpret_cast<char*>(&rh), sizeof(rh))) {
        if (rh.key == key) {
            val = rh.value;
            found = true;
        }
    }
    if (found) {
        cout << "Val is: " << val << endl;
    } else {
        cout << "Key not found" << endl;
    }
   
}