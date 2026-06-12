#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <filesystem>
#include <regex>
#include <mutex>
#include <thread>
#include <atomic>
#include "database.h"

using namespace std;

struct Value {
    string value;
    bool deleted;
};


map<string, Value> memtable;
map<string, Value> secondary_memtable;
int sstableid;
int walid;
std::filesystem::path active_wal_path;
void put(string key, string value);
void get(string key);
void deleteKey(string key);
void write(ofstream& outputfile, RecordHeader rh, string key, string value);
bool checksize();
void flush_memtable(int frozen_walid, const std::filesystem::path frozen_wal_path, map<string, Value> frozen_memtable);
void recover();
void initialize_sstableid();
void initialize_walid();
std::filesystem::path get_wal_directory();
std::filesystem::path get_wal_path(int id);
set<std::filesystem::path> readsstables();
void read_in_file(const std::filesystem::path& filename, map<string, Value>& currentMap);
void write_map(const std::filesystem::path& filename, const map<string, Value>& currentMap);
vector<std::filesystem::path> readsstables_in_id_order();
vector<std::filesystem::path> readwals_in_id_order();
int get_sstable_id(const std::filesystem::path& file_path);
int get_walid(const std::filesystem::path& file_path);
void compact();
void apply_record_to_memtable(const RecordHeader& rh, const string& key, const string& value);
std::mutex compact_lock;
std::mutex io_lock;
std::atomic_bool flush_in_progress{false};
ofstream fio;


// void index_file();

int main() {
    // index_file();
    namespace fs = std::filesystem;
    fs::create_directories("./wal");
    fs::create_directories("./sstables");
    initialize_walid();
    recover();
    initialize_sstableid();
    active_wal_path = get_wal_path(walid);
    fio.open(active_wal_path, ios::binary | ios::app);
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
            std::thread t(compact);
            t.detach();
            
        } else {
            return 0;
        }
        if (checksize()) {
            if (flush_in_progress.load()) {
                continue;
            }
            if (!io_lock.try_lock()) {
                continue;
            }
            secondary_memtable = memtable;
            memtable = {};
            const int frozen_walid = walid;
            const std::filesystem::path frozen_wal_path = active_wal_path;
            walid += 1;
            active_wal_path = get_wal_path(walid);
            fio.close();
            fio.open(active_wal_path, ios::binary | ios::app);
            const map<string, Value> frozen_memtable = secondary_memtable;
            flush_in_progress.store(true);
            thread t([frozen_walid, frozen_wal_path, frozen_memtable]() {
                flush_memtable(frozen_walid, frozen_wal_path, frozen_memtable);
                flush_in_progress.store(false);
            });
            t.detach();
            io_lock.unlock();
        }
        set<std::filesystem::path> set = readsstables();
        for (auto& iter : set) {
            cout << iter << endl;
        }
    }
    
    return 0;
}


void recover() {
    for (const auto& wal_path : readwals_in_id_order()) {
        ifstream wal_stream(wal_path, ios::binary);
        if (!wal_stream.is_open() || std::filesystem::file_size(wal_path) == 0) {
            continue;
        }

        RecordHeader rh;
        while (wal_stream.read(reinterpret_cast<char*>(&rh), sizeof(rh))) {
            string key(rh.key_size, '\0');
            string value(rh.value_size, '\0');
            if (rh.key_size > 0) {
                wal_stream.read(key.data(), rh.key_size);
            }
            if (rh.value_size > 0) {
                wal_stream.read(value.data(), rh.value_size);
            }
            apply_record_to_memtable(rh, key, value);
        }
    }
}

void initialize_sstableid() {
    const vector<std::filesystem::path> files = readsstables_in_id_order();
    if (files.empty()) {
        sstableid = 0;
        return;
    }

    sstableid = get_sstable_id(files.back()) + 1;
}

void initialize_walid() {
    const vector<std::filesystem::path> files = readwals_in_id_order();
    if (files.empty()) {
        walid = 0;
        return;
    }
    walid = get_walid(files.back());
}

std::filesystem::path get_wal_directory() {
    return std::filesystem::path{"./wal"};
}

std::filesystem::path get_wal_path(int id) {
    return get_wal_directory() / std::filesystem::path("wal_" + to_string(id) + ".bin");
}

void put(string key, string value) {
    RecordHeader rh = {(u_int32_t) key.size(),(u_int32_t) value.size(), 0};
    write(fio, rh, key, value);
    memtable[key] = {value, false};
}

bool checksize() {
    return memtable.size() > 1;
}

void flush_memtable(int frozen_walid, const std::filesystem::path frozen_wal_path, map<string, Value> frozen_memtable) {
    namespace fs = std::filesystem;
    const fs::path dir{"./sstables"};
    fs::create_directories(dir);
    fs::path filename = dir / ("sstable_" + to_string(sstableid++) + ".bin");
    ofstream ssio(filename, ios::binary);
    if (!ssio.is_open()) {
        cerr << "Failed to open sstable for write: " << filename << endl;
        return;
    }
    for (auto &pair : frozen_memtable) {
        RecordHeader rh;
        rh.key_size = pair.first.size();
        rh.value_size = pair.second.value.size();
        rh.deleted = pair.second.deleted;
        write(ssio, rh, pair.first, pair.second.value);
    }
    if (fs::exists(frozen_wal_path)) {
        fs::remove(frozen_wal_path);
    }
    (void)frozen_walid;
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
    auto mem_it = memtable.find(key);
    if (mem_it != memtable.end()) {
        if (!mem_it->second.deleted) {
            cout << "Value is: " << mem_it->second.value << endl;
        } else {
            cout << "Value not found." << endl;
        }
        return;
    }

    auto frozen_it = secondary_memtable.find(key);
    if (frozen_it != secondary_memtable.end()) {
        if (!frozen_it->second.deleted) {
            cout << "Value is: " << frozen_it->second.value << endl;
        } else {
            cout << "Value not found." << endl;
        }
        return;
    }

    vector<std::filesystem::path> files = readsstables_in_id_order();

    for (auto it = files.rbegin(); it != files.rend(); ++it) {
        ifstream input(*it, ios::binary);
        if (!input.is_open()) {
            continue;
        }

        RecordHeader rh;
        while (input.read(reinterpret_cast<char*>(&rh), sizeof(rh))) {
            string file_key(rh.key_size, '\0');
            string file_value(rh.value_size, '\0');
            if (rh.key_size > 0) {
                input.read(file_key.data(), rh.key_size);
            }
            if (rh.value_size > 0) {
                input.read(file_value.data(), rh.value_size);
            }

            if (file_key == key) {
                if (rh.deleted == 1) {
                    cout << "Value not found." << endl;
                } else {
                    cout << "Value is: " << file_value << endl;
                }
                return;
            }
        }
    }

    cout << "Value not found." << endl;
}

void deleteKey(string key) {
    RecordHeader rh{
        static_cast<uint32_t>(key.size()),
        0,
        1
    };
    write(fio, rh, key, "");
    memtable[key] = {"", true};
}

// void appendLog(string key, string value) {

// }

void compact() {
    if (!compact_lock.try_lock()) {
        return;
    }
    namespace fs = std::filesystem;
    vector<fs::path> s = readsstables_in_id_order();
    if (s.empty()) {
        compact_lock.unlock();
        return;
    }

    map<string, Value> currentMap = {};
    for (const auto& file : s) {
        read_in_file(file, currentMap);
    }

    const fs::path dir{"./sstables"};
    const int next_id = get_sstable_id(s.back()) + 1;
    const fs::path target_file = dir / ("sstable_" + to_string(next_id) + ".bin");
    const fs::path temp_file = dir / ("sstable_" + to_string(next_id) + ".tmp");
    write_map(temp_file, currentMap);

    if (!fs::exists(temp_file)) {
        compact_lock.unlock();
        return;
    }

    std::error_code rename_error;
    fs::rename(temp_file, target_file, rename_error);
    if (rename_error) {
        cerr << "Failed to publish compacted file: " << rename_error.message() << endl;
        compact_lock.unlock();
        return;
    }
    for (const auto& old_file : s) {
        if (old_file != target_file) {
            std::error_code remove_error;
            fs::remove(old_file, remove_error);
        }
    }
    cout << "Compaction complete." << endl;
    compact_lock.unlock();
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

int get_walid(const std::filesystem::path& file_path) {
    static const regex wal_name_pattern(R"(wal_(\d+)\.bin)");
    smatch m;
     const string filename = file_path.filename().string();
    if (regex_match(filename, m, wal_name_pattern)) {
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

vector<std::filesystem::path> readwals_in_id_order() {
    namespace fs = std::filesystem;
    fs::path p{"./wal"};
    vector<fs::path> files;
    if (!fs::exists(p)) {
        return files;
    }

    for (const auto& path : fs::directory_iterator(p)) {
        if (get_walid(path.path()) >= 0) {
            files.push_back(path.path());
        }
    }

    sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
        return get_walid(a) < get_walid(b);
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
            if (rh.key_size > 0) {
                input.read(key.data(), rh.key_size);
            }
            if (rh.value_size > 0) {
                input.read(value.data(), rh.value_size);
            }
            if (rh.deleted == 1) {
                currentMap[key] = {"", true};
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

void apply_record_to_memtable(const RecordHeader& rh, const string& key, const string& value) {
    if (rh.deleted == 1) {
        memtable[key] = {"", true};
    } else {
        memtable[key] = {value, false};
    }
}

void write(ofstream& outputfile, RecordHeader rh, string key, string value) {
    outputfile.write(reinterpret_cast<char*>(&rh), sizeof(rh));
    if (rh.key_size > 0) {
        outputfile.write(key.data(), rh.key_size);
    }
    if (rh.value_size > 0) {
        outputfile.write(value.data(), rh.value_size);
    }
}
