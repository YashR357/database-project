#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <filesystem>
#include "database.h"

using namespace std;

ofstream fio;
ifstream file("abc.bin", ios::binary);
map<string, streampos> index_map;

void put(string key, string value);
void get(string key);
void deleteKey(string key);
void write(RecordHeader rh, string key, string value);
void index_file();

int main() {
    index_file();
    
    while (true) {
        cout << "Choose action: 1. Put, 2. Get" << endl;
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
    }
    return 0;
}

void index_file() {
    if (file.is_open() && std::filesystem::file_size("abc.bin") > 0) {
        streampos offset = file.tellg();
        RecordHeader rh;
        while (file.read(reinterpret_cast<char*>(&rh), sizeof(rh))) {
            string key(rh.key_size, '\0');
            string value(rh.value_size, '\0');
            file.read(&key[0], rh.key_size);
            file.read(&value[0], rh.value_size);
            index_map[key] = offset;
            offset = file.tellg();
        }
    }
}

void put(string key, string value) {
    RecordHeader rh = {(u_int32_t) key.size(),(u_int32_t) value.size(), 0};
    if (!fio.is_open()) {
        fio.open("abc.bin", ios::binary|ios::app);
        streampos offset = fio.tellp();
        write(rh, key, value);
        fio.seekp(ios::end);
        index_map[key] = offset;
        fio.close();
    }
}

void get(string key) {
    RecordHeader rh;
    if (index_map.count(key) > 0) {
        streampos offset = index_map.at(key);
        file.clear();
        file.seekg(offset, ios::beg);
        file.read(reinterpret_cast<char*>(&rh), sizeof(rh));
        file.seekg(rh.key_size, ios::cur);
        string value(rh.value_size, '\0');
        file.read(&value[0], rh.value_size);
        cout << "Value is: " << value << endl;
    } else {
        cout << "Value not found." << endl;
    }
    
}

void deleteKey(string key) {
    RecordHeader rh;
    streampos off = index_map[key];
    file.seekg(off, ios::beg);
    file.read(reinterpret_cast<char*>(&rh), sizeof(rh));
    string new_key(rh.key_size, '\0');
    string value(rh.value_size, '\0');
    file.read(&new_key[0], rh.key_size);
    file.read(&value[0], rh.value_size);
    file.seekg(ios::app);
    rh.deleted = 1;
    write(rh, new_key, value);
    index_map.erase(key);
}

void write(RecordHeader rh, string key, string value) {
    fio.write(reinterpret_cast<char*>(&rh), sizeof(rh));
    fio.write(reinterpret_cast<char*>(&key[0]), rh.key_size);
    fio.write(reinterpret_cast<char*>(&value[0]), rh.value_size);
}