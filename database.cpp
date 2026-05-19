#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <filesystem>
#include <regex>
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
void read_in_file(const std::filesystem::path& filename, map<string, Value>& currentMap);
void write_map(const std::filesystem::path& filename, const map<string, Value>& currentMap);
vector<std::filesystem::path> readsstables_in_id_order();
int get_sstable_id(const std::filesystem::path& file_path);
void compact();



// void index_file();

int main() {
    // index_file();
    recover();
    
    while (true) {
        cout << "Choose action: 1. Put, 2. Get, 3. Delete, 4. Compact" << endl;
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
        } else if (action == "Compact" || action == "c" || action == "C" || action == "4") {
            compact();
            cout << "Compaction complete." << endl;
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
    namespace fs = std::filesystem;
    vector<fs::path> s = readsstables_in_id_order();
    if (s.empty()) {
        return;
    }

    map<string, Value> currentMap = {};
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        read_in_file(*it, currentMap);
    }

    const fs::path dir{"./sstables"};
    int max_id = get_sstable_id(s.back());
    const int next_id = max_id + 1;
    const fs::path target_file = dir / ("sstable_" + to_string(next_id) + ".bin");
    const fs::path temp_file = dir / ("sstable_" + to_string(next_id) + ".tmp");
    write_map(temp_file, currentMap);

    for (const auto& old_file : s) {
        if (old_file != target_file && old_file != temp_file) {
            fs::remove(old_file);
        }
    }
    fs::rename(temp_file, target_file);
}

int get_sstable_id(const std::filesystem::path& file_path) {
    static const regex sstable_name_pattern(R"(sstable_(\d+)\.bin)");
    smatch m;
    const string filename = file_path.filename().string();
    if (regex_match(filename, m, sstable_name_pattern)) {
        return stoi(m[1].str());
    }
    return -1;
}

vector<std::filesystem::path> readsstables_in_id_order() {
    namespace fs = std::filesystem;
    fs::path p{"./sstables"};
    vector<fs::path> files;
    if (!fs::exists(p)) {
        return files;
    }

    for (const auto& path : fs::directory_iterator(p)) {
        if (get_sstable_id(path.path()) >= 0) {
            files.push_back(path.path());
        }
    }

    sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
        return get_sstable_id(a) < get_sstable_id(b);
    });
    return files;
}

void read_in_file(const std::filesystem::path& filename, map<string, Value>& currentMap) {
    if (std::filesystem::exists(filename) && std::filesystem::file_size(filename) > 0) {
        ifstream input(filename, ios::binary);
        if (!input.is_open()) {
            cerr << "Failed to open sstable: " << filename << endl;
            return;
        }
        RecordHeader rh;
        while (input.read(reinterpret_cast<char*>(&rh), sizeof(rh))) {
            string key(rh.key_size, '\0');
            string value(rh.value_size, '\0');
            input.read(&key[0], rh.key_size);
            input.read(&value[0], rh.value_size);
            if (rh.deleted == 1) {
                currentMap.erase(key);
            } else {
                currentMap[key] = {value, false};
            }
        }
    }
}

void write_map(const std::filesystem::path& filename, const map<string, Value>& currentMap) {
    ofstream out_file(filename, ios::binary | ios::trunc);
    if (!out_file.is_open()) {
        cerr << "Failed to open compacted file: " << filename << endl;
        return;
    }

    for (const auto& [key, entry] : currentMap) {
        if (entry.deleted) {
            continue;
        }
        RecordHeader rh{
            static_cast<uint32_t>(key.size()),
            static_cast<uint32_t>(entry.value.size()),
            0
        };
        write(out_file, rh, key, entry.value);
    }
}

void write(ofstream& outputfile, RecordHeader rh, string key, string value) {
    outputfile.write(reinterpret_cast<char*>(&rh), sizeof(rh));
    outputfile.write(reinterpret_cast<char*>(&key[0]), rh.key_size);
    outputfile.write(reinterpret_cast<char*>(&value[0]), rh.value_size);
}
