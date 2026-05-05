#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <filesystem>
#include "database.h"

using namespace std;

ofstream fio;
ifstream file("abc.bin", ios::binary);
map<u_int32_t, streampos> index_map;

void put();
void get();
void index_file();

int main() {
    index_file();
    
    while (true) {
        cout << "Choose action: 1. Put, 2. Get" << endl;
        string action;
        cin >> action;
        if (action == "Put") {
            put();
        } else if (action == "Get") {
            get();
        } else {
            return 0;
        }
    }
    return 0;
}

void index_file() {
    if (file.is_open() && std::filesystem::file_size("abc.bin") > 0) {
        streampos offset = file.tellg();
        RecordHeader rh;
        while (file.read(reinterpret_cast<char*>(&rh), sizeof(rh))) {
            index_map[rh.key] = offset;
            cout << "offset at: " << offset << " Key is: " << rh.key << " Value is: " << rh.value << endl;
            offset = file.tellg();
        }
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
    if (!fio.is_open()) {
        fio.open("abc.bin", ios::binary|ios::app);
        streampos offset = fio.tellp();
        fio.write(reinterpret_cast<char*>(&rh), sizeof(rh));
        index_map[rh.key] = offset;
        fio.close();
    }
}

void get() {
    cout << "Type key: ";
    int key;
    cin >> key;
    RecordHeader rh;
    int val;
    if (index_map.count(key) > 0) {
        streampos offset = index_map.at(key);
        cout << "Offset for key is: " << offset << endl;
        file.clear();
        file.seekg(offset, ios::beg);
        file.read(reinterpret_cast<char*>(&rh), sizeof(rh));
        cout << "Key is: " << rh.key << " Value is: " << rh.value << endl;
    } else {
        cout << "Value not found." << endl;
    }
    
}