#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// One static table as the game ships it, two files in
// gamedata/binarystaticinfo__/bin:
//
//   NAME.staticinfoheader  a row count, then one (key, u32 offset) per row
//   NAME.staticinfobody    the records back to back
//
// The count is a u16 and the key 1, 2, 4 or 8 bytes wide, except
// characterappearanceindexinfo, whose count is a u32 and whose 12-byte
// entries are (u32 key, i32 tag, u32 offset), keyed by (key, tag). Nothing in
// the body says where a record ends, so a record runs to the next offset and
// a changed table is written whole: records in their body order, offsets
// worked out again. Parse refuses a table that does not write back byte for
// byte, so an edit never starts from a misread.
namespace bm::tablefile
{
    struct Key
    {
        uint64_t key = 0;
        int32_t  tag = 0;
        bool operator==(const Key& o) const { return key == o.key && tag == o.tag; }
    };

    class Table
    {
    public:
        bool Parse(const std::string& header, const std::string& body, std::string& why);
        bool Build(std::string& header, std::string& body, std::string& why) const;

        bool Wide() const { return wide_; }
        size_t Rows() const { return keys_.size(); }
        const Key& KeyAt(size_t row) const { return keys_[row]; }
        const std::string& RecordAt(size_t row) const { return records_[row]; }
        // Header row of a key, or -1.
        int RowOf(uint64_t key, int32_t tag = 0) const;
        const std::string* Record(uint64_t key, int32_t tag = 0) const;

        bool Set(uint64_t key, const std::string& record, int32_t tag = 0);
        // Adds a row at the end of the header and of the body. Its row is the
        // old row count. False when the key is taken.
        bool Append(uint64_t key, const std::string& record, int32_t tag = 0);

    private:
        static uint64_t Slot(uint64_t key, int32_t tag) { return key * 0x9E3779B97F4A7C15ull ^ static_cast<uint32_t>(tag); }

        bool wide_ = false;
        unsigned keySize_ = 0;
        std::vector<Key> keys_;             // header order
        std::vector<std::string> records_;  // by header row
        std::vector<size_t> bodyOrder_;     // header rows, in the order their records sit in the body
        std::unordered_multimap<uint64_t, size_t> index_;
    };

    uint32_t Crc32(const void* data, size_t n);
}
