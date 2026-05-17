struct RecordHeader {
    uint32_t key_size;
    uint32_t value_size;
    uint8_t deleted;
};