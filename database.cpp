#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <filesystem>
#include "database.h"

using namespace std;

struct Value {
    string value;
    bool deleted;
};

ofstream fio("abc.bin", ios::binary|ios::app);
ifstream file("abc.bin", ios::binary);
map<string, Value> memtable;
int sstableid;


void put(string key, string value);
void get(string key);
void deleteKey(string key);
void write(ofstream& outputfile, RecordHeader rh, string key, string value);
bool checksize();
void flush_memtable();
void recover();
set<std::filesystem::path> readsstables();
void read_in_file(std::filesystem::path filename, map<string, Value> currentMap);



// void index_file();

int main() {
    // index_file();
    recover();
    
    while (true) {
        cout << "Choose action: 1. Put, 2. Get, 3. Delete" << endl;
        string action;
        cin >> action;
        if (action == "Put" || action == "p" || action == "P" || action == "1") {
            string key;
            string value;
            cout << "Type key: ";
            cin >> key;
            cout << "Type value: ";
            cin >> value;
            put(key, value);
        } else if (action == "Get" || action == "g" || action == "G" || action == "2") {
            cout << "Type key: ";
            string key;
            cin >> key;
            get(key);
        } else if (action == "Delete" || action == "d" || action == "D" || action == "3") {
            cout << "Type key: ";
            string key;
            cin >> key;
            deleteKey(key);
        } else {
            return 0;
        }
        if (checksize()) {
            flush_memtable();
        }
        set<std::filesystem::path> set = readsstables();
        for (auto& iter : set) {
            cout << iter << endl;
        }
    }
    
    return 0;
}

// void index_file() {
//     if (file.is_open() && std::filesystem::exists("abc.bin") && std::filesystem::file_size("abc.bin") > 0) {
//         file.clear();
//         file.seekg(0, ios::beg);
//         streampos offset = file.tellg();
//         RecordHeader rh;
//         while (file.read(reinterpret_cast<char*>(&rh), sizeof(rh))) {
//             string key(rh.key_size, '\0');
//             string value(rh.value_size, '\0');
//             file.read(&key[0], rh.key_size);
//             file.read(&value[0], rh.value_size);
//             if (rh.deleted == 1) {
//                 index_map.erase(key);
//             } else {
//                 index_map[key] = offset;
//             }
//             offset = file.tellg();
//         }
//     }
// }

void recover() {
    if (file.is_open() && std::filesystem::exists("abc.bin") && std::filesystem::file_size("abc.bin") > 0) {
        file.clear();
        file.seekg(0, ios::beg);
        RecordHeader rh;
        while (file.read(reinterpret_cast<char*>(&rh), sizeof(rh))) {
            string key(rh.key_size, '\0');
            string value(rh.value_size, '\0');
            file.read(&key[0], rh.key_size);
            file.read(&value[0], rh.value_size);
            if (rh.deleted == 1) {
                memtable[key] = {"", true};
            } else {
                memtable[key] = {value, false};
            }
        }
    }
}

void put(string key, string value) {
    RecordHeader rh = {(u_int32_t) key.size(),(u_int32_t) value.size(), 0};
    write(fio, rh, key, value);
    memtable[key] = {value, false};
}

bool checksize() {
    return memtable.size() > 1;
}

//TODO: Have immutable memtable and mutable memtable so you can swap them and then flush the memtable and the active memtable can continue to serve writes and reads.
void flush_memtable() {
    namespace fs = std::filesystem;
    const fs::path dir{"./sstables"};
    fs::create_directories(dir);
    fs::path filename = dir / ("sstable_" + to_string(sstableid++) + ".bin");
    ofstream ssio(filename, ios::binary);
    for (auto &pair : memtable) {
        RecordHeader rh;
        rh.key_size = pair.first.size();
        rh.value_size = pair.second.value.size();
        rh.deleted = pair.second.deleted;
        write(ssio, rh, pair.first, pair.second.value);
    }
    memtable = {};
}

set<std::filesystem::path> readsstables() {
    namespace fs = std::filesystem;
    fs::path p{"./sstables"};
    set<fs::path> set;
    if (!fs::exists(p)) {
        return set;
    }
    for (auto& path : fs::directory_iterator(p)) {
        set.insert(path);
    }
    return set;
}



void get(string key) {
    if (memtable.count(key) > 0 && !memtable[key].deleted) {
        cout << "Value is: " << memtable[key].value << endl;
    } else {
        cout << "Value not found." << endl;
    }
    
}

void deleteKey(string key) {
    auto it = memtable.find(key);
    if (it == memtable.end()) {
        cout << "Value not found." << endl;
        return;
    }
    RecordHeader rh;
    string new_key(key.size(), '\0');
    string value(0, '\0');
    rh.deleted = 1;
    write(fio, rh, new_key, value);
    memtable[key] = {"", true};
}

// void appendLog(string key, string value) {

// }

void compact() {
    set<std::filesystem::path> s = readsstables();
    map<string, Value> currentMap = {};
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        read_in_file(it, &currentMap);
        write_map(it, &currentMap);
    }

}

void read_in_file(std::filesystem::path filename, map<string, Value> currentMap) {
    if (file.is_open() && std::filesystem::exists(filename) && std::filesystem::file_size(filename) > 0) {
        file.clear();
        file.seekg(0, ios::beg);
        streampos offset = file.tellg();
        RecordHeader rh;
        while (file.read(reinterpret_cast<char*>(&rh), sizeof(rh))) {
            string key(rh.key_size, '\0');
            string value(rh.value_size, '\0');
            file.read(&key[0], rh.key_size);
            file.read(&value[0], rh.value_size);
            if (rh.deleted == 1) {
                currentMap.erase(key);
            } else {
                currentMap[key] = offset;
            }
            offset = file.tellg();
        }
    }
}

void write_map(std::filesystem::path filename, map<string, Value> currentMap) {
    
}

void write(ofstream& outputfile, RecordHeader rh, string key, string value) {
    outputfile.write(reinterpret_cast<char*>(&rh), sizeof(rh));
    outputfile.write(reinterpret_cast<char*>(&key[0]), rh.key_size);
    outputfile.write(reinterpret_cast<char*>(&value[0]), rh.value_size);
}
