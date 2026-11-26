#include "game/tablefile.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>

namespace bm::tablefile
{
    namespace
    {
        uint64_t ReadKey(const uint8_t* p, unsigned size)
        {
            uint64_t v = 0;
            memcpy(&v, p, size);
            return v;
        }

        template <typename T> void Put(std::string& s, T v) { s.append(reinterpret_cast<const char*>(&v), sizeof v); }
    }

    uint32_t Crc32(const void* data, size_t n)
    {
        // Built once under the compiler's own guard: the first calls come from
        // two loader threads at once when they read two tables together.
        struct Lookup
        {
            uint32_t v[256];
            Lookup()
            {
                for (uint32_t i = 0; i < 256; ++i)
                {
                    uint32_t c = i;
                    for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                    v[i] = c;
                }
            }
        };
        static const Lookup lookup;
        const uint32_t* table = lookup.v;
        const uint8_t* p = static_cast<const uint8_t*>(data);
        uint32_t c = 0xFFFFFFFFu;
        for (size_t i = 0; i < n; ++i) c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
        return c ^ 0xFFFFFFFFu;
    }

    namespace
    {
        // interactioninfo, 3840: _dialogSetInfo moved from after _onPreemptionSuccessAiEventKey to the u32 before
        // the record's last 8 bytes. In the 3732 form `rec` is the name block, the key and the fields; the field
        // sits 50 bytes after the name block.
        constexpr size_t kDialogAfterName = 50;
        bool DialogToOld(std::string& rec)
        {
            uint32_t nameLen = 0;
            if (rec.size() < 4) return false;
            memcpy(&nameLen, rec.data(), 4);
            const size_t at = 4 + static_cast<size_t>(nameLen) + kDialogAfterName, n = rec.size();
            if (n < at + 4 + 12) return false;
            rec = rec.substr(0, at) + rec.substr(n - 12, 4) + rec.substr(at, n - 12 - at) + rec.substr(n - 8);
            return true;
        }
        bool DialogToNew(std::string& rec)
        {
            uint32_t nameLen = 0;
            if (rec.size() < 4) return false;
            memcpy(&nameLen, rec.data(), 4);
            const size_t at = 4 + static_cast<size_t>(nameLen) + kDialogAfterName, n = rec.size();
            if (n < at + 4 + 8) return false;
            rec = rec.substr(0, at) + rec.substr(at + 4, n - 8 - (at + 4)) + rec.substr(at, 4) + rec.substr(n - 8);
            return true;
        }
    }

    // The 3840 container. `matched` is false when the header is not that layout at all, so the caller goes on to
    // the older one; true with a false return means it is that layout and something in it is wrong.
    bool Table::ParseNew(const std::string& header, const std::string& body, const char* name, bool& matched,
                         std::string& why)
    {
        matched = false;
        const uint8_t* h = reinterpret_cast<const uint8_t*>(header.data());
        const size_t hn = header.size();
        for (int countWide = 0; countWide < 2; ++countWide)
        {
            const size_t head = countWide ? 4 : 2;
            if (hn < head) return true;
            uint32_t count = 0;
            if (countWide) memcpy(&count, h, 4);
            else count = static_cast<uint32_t>(h[0] | h[1] << 8);
            for (unsigned width : { 1u, 2u, 4u, 8u })
            {
                // The wide count goes with the 8-byte (key, tag) key, and only with it.
                if ((countWide != 0) != (width == 8)) continue;
                struct Row { uint64_t key; std::string name; uint32_t off, len; };
                std::vector<Row> rows;
                size_t p = head;
                bool fits = true;
                uint32_t expect = 0;
                for (uint32_t i = 0; i < count && fits; ++i)
                {
                    if (hn - p < width + 4) { fits = false; break; }
                    Row r{};
                    memcpy(&r.key, h + p, width);
                    p += width;
                    uint32_t nl;
                    memcpy(&nl, h + p, 4);
                    p += 4;
                    if (nl > 400 || hn - p < static_cast<size_t>(nl) + 8) { fits = false; break; }
                    r.name.assign(reinterpret_cast<const char*>(h + p), nl);
                    p += nl;
                    memcpy(&r.off, h + p, 4);
                    memcpy(&r.len, h + p + 4, 4);
                    p += 8;
                    if (r.off != expect || r.len > body.size() || r.off > body.size() - r.len) { fits = false; break; }
                    expect = r.off + r.len;
                    rows.push_back(std::move(r));
                }
                if (!fits || p != hn || expect != body.size() || rows.empty()) continue;
                // A record opens with its key, then its position.
                const unsigned kw = width;
                bool ok = true;
                for (size_t i = 0; i < rows.size() && ok; ++i)
                {
                    uint32_t index;
                    if (rows[i].len < kw + 4) { ok = false; break; }
                    memcpy(&index, body.data() + rows[i].off + kw, 4);
                    ok = index == i;
                }
                if (!ok) continue;

                matched = true;
                newLayout_ = true;
                wide_ = width == 8;
                keySize_ = wide_ ? 4 : width;
                moveDialog_ = name && !strcmp(name, "interactioninfo");
                for (size_t i = 0; i < rows.size(); ++i)
                {
                    const Row& r = rows[i];
                    std::string rec;
                    uint32_t nl = static_cast<uint32_t>(r.name.size());
                    rec.append(reinterpret_cast<const char*>(&nl), 4);
                    rec += r.name;
                    rec.append(body, r.off, kw);
                    rec.append(body, r.off + kw + 4, r.len - kw - 4);
                    if (moveDialog_ && !DialogToOld(rec))
                    {
                        why = "an interactioninfo record is too short to hold its fields";
                        return false;
                    }
                    Key k;
                    if (wide_)
                    {
                        const uint32_t tag32 = static_cast<uint32_t>(r.key >> 32);
                        memcpy(&k.tag, &tag32, 4);
                        k.key = static_cast<uint32_t>(r.key);
                    }
                    else
                        k.key = r.key;
                    keys_.push_back(k);
                    records_.push_back(std::move(rec));
                    bodyOrder_.push_back(i);
                    index_.emplace(Slot(k.key, k.tag), i);
                }
                std::string h2, b2;
                if (!BuildNew(h2, b2, why)) return false;
                if (h2 != header || b2 != body)
                {
                    why = "it does not write back byte for byte (the 3840 layout, with a duplicate key or a field this reader does not know)";
                    return false;
                }
                return true;
            }
        }
        return true;
    }

    bool Table::BuildNew(std::string& header, std::string& body, std::string& why) const
    {
        header.clear();
        body.clear();
        const size_t rows = keys_.size();
        const unsigned kw = wide_ ? 8 : keySize_;
        if (wide_) Put<uint32_t>(header, static_cast<uint32_t>(rows));
        else
        {
            if (rows > 0xFFFF)
            {
                why = "more rows than a u16 count holds";
                return false;
            }
            Put<uint16_t>(header, static_cast<uint16_t>(rows));
        }
        for (size_t row = 0; row < rows; ++row)
        {
            std::string rec = records_[row];
            uint32_t nl = 0;
            if (rec.size() < 4)
            {
                why = "a record has no name block";
                return false;
            }
            memcpy(&nl, rec.data(), 4);
            if (rec.size() < 4 + static_cast<size_t>(nl) + kw)
            {
                why = "a record is shorter than its name and key";
                return false;
            }
            if (moveDialog_ && !DialogToNew(rec))
            {
                why = "an interactioninfo record is too short to hold its fields";
                return false;
            }
            const std::string name = rec.substr(4, nl);
            std::string out = rec.substr(4 + nl, kw);
            Put<uint32_t>(out, static_cast<uint32_t>(row));
            out.append(rec, 4 + nl + kw, std::string::npos);
            if (wide_)
            {
                Put<uint32_t>(header, static_cast<uint32_t>(keys_[row].key));
                Put<int32_t>(header, keys_[row].tag);
            }
            else
                header.append(reinterpret_cast<const char*>(&keys_[row].key), keySize_);
            Put<uint32_t>(header, nl);
            header += name;
            Put<uint32_t>(header, static_cast<uint32_t>(body.size()));
            Put<uint32_t>(header, static_cast<uint32_t>(out.size()));
            body += out;
            if (body.size() > 0xFFFFFFFFull)
            {
                why = "the body outgrew a u32 offset";
                return false;
            }
        }
        return true;
    }

    bool Table::Parse(const std::string& header, const std::string& body, std::string& why, const char* name)
    {
        *this = Table();
        const uint8_t* h = reinterpret_cast<const uint8_t*>(header.data());
        const size_t hn = header.size();
        if (hn < 2)
        {
            why = "the header is under two bytes";
            return false;
        }
        {
            bool matched = false;
            if (!ParseNew(header, body, name, matched, why)) return false;
            if (matched) return true;
            *this = Table();
        }
        std::vector<std::pair<Key, uint32_t>> entries;
        const uint16_t count16 = static_cast<uint16_t>(h[0] | h[1] << 8);
        unsigned width = 0;
        if (count16 && (hn - 2) % count16 == 0) width = static_cast<unsigned>((hn - 2) / count16) - 4;
        if (width == 1 || width == 2 || width == 4 || width == 8)
        {
            keySize_ = width;
            for (size_t i = 0; i < count16; ++i)
            {
                const uint8_t* e = h + 2 + i * (width + 4);
                uint32_t off;
                memcpy(&off, e + width, 4);
                entries.push_back({ Key{ ReadKey(e, width), 0 }, off });
            }
        }
        else
        {
            uint32_t count32 = 0;
            if (hn >= 4) memcpy(&count32, h, 4);
            if (!count32 || hn - 4 != static_cast<size_t>(count32) * 12)
            {
                char text[120];
                snprintf(text, sizeof text, "a header of %zu bytes fits neither a u16 count of %u nor 12-byte entries",
                         hn, count16);
                why = text;
                return false;
            }
            wide_ = true;
            keySize_ = 4;
            for (size_t i = 0; i < count32; ++i)
            {
                const uint8_t* e = h + 4 + i * 12;
                uint32_t k, off;
                int32_t t;
                memcpy(&k, e, 4);
                memcpy(&t, e + 4, 4);
                memcpy(&off, e + 8, 4);
                entries.push_back({ Key{ k, t }, off });
            }
        }

        // A record runs from its offset to the next distinct one.
        std::vector<uint32_t> offs;
        offs.reserve(entries.size() + 1);
        for (const auto& e : entries) offs.push_back(e.second);
        std::sort(offs.begin(), offs.end());
        offs.erase(std::unique(offs.begin(), offs.end()), offs.end());
        if (!offs.empty() && offs.back() > body.size())
        {
            why = "a header offset runs past the end of the body";
            return false;
        }
        offs.push_back(static_cast<uint32_t>(body.size()));

        keys_.reserve(entries.size());
        records_.reserve(entries.size());
        std::vector<std::pair<uint32_t, size_t>> byOffset;
        byOffset.reserve(entries.size());
        for (size_t row = 0; row < entries.size(); ++row)
        {
            const uint32_t off = entries[row].second;
            // The last record's offset may equal body.size(), an empty record
            // at the very end. Nothing in `offs` is larger than it then, so
            // upper_bound returns end() and the sentinel is the answer.
            const auto next = std::upper_bound(offs.begin(), offs.end(), off);
            const uint32_t end = next == offs.end() ? static_cast<uint32_t>(body.size()) : *next;
            if (end < off)
            {
                why = "a header offset runs past the end of the body";
                return false;
            }
            keys_.push_back(entries[row].first);
            records_.emplace_back(body.data() + off, end - off);
            byOffset.push_back({ off, row });
            index_.emplace(Slot(entries[row].first.key, entries[row].first.tag), row);
        }
        std::stable_sort(byOffset.begin(), byOffset.end(),
                         [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& b : byOffset) bodyOrder_.push_back(b.second);

        std::string h2, b2;
        if (!Build(h2, b2, why)) return false;
        if (h2 != header || b2 != body)
        {
            why = "it does not write back byte for byte (a duplicate key or offset, or a layout this reader does not know)";
            return false;
        }
        return true;
    }

    bool Table::Build(std::string& header, std::string& body, std::string& why) const
    {
        if (newLayout_) return BuildNew(header, body, why);
        header.clear();
        body.clear();
        std::vector<uint32_t> offs(keys_.size());
        size_t total = 0;
        for (const std::string& r : records_) total += r.size();
        body.reserve(total);
        for (size_t row : bodyOrder_)
        {
            if (body.size() > 0xFFFFFFFFull)
            {
                why = "the body outgrew a u32 offset";
                return false;
            }
            offs[row] = static_cast<uint32_t>(body.size());
            body += records_[row];
        }
        if (wide_)
        {
            header.reserve(4 + keys_.size() * 12);
            Put<uint32_t>(header, static_cast<uint32_t>(keys_.size()));
            for (size_t row = 0; row < keys_.size(); ++row)
            {
                Put<uint32_t>(header, static_cast<uint32_t>(keys_[row].key));
                Put<int32_t>(header, keys_[row].tag);
                Put<uint32_t>(header, offs[row]);
            }
            return true;
        }
        if (keys_.size() > 0xFFFF)
        {
            why = "more rows than a u16 count holds";
            return false;
        }
        header.reserve(2 + keys_.size() * (keySize_ + 4));
        Put<uint16_t>(header, static_cast<uint16_t>(keys_.size()));
        for (size_t row = 0; row < keys_.size(); ++row)
        {
            header.append(reinterpret_cast<const char*>(&keys_[row].key), keySize_);
            Put<uint32_t>(header, offs[row]);
        }
        return true;
    }

    int Table::RowOf(uint64_t key, int32_t tag) const
    {
        auto range = index_.equal_range(Slot(key, tag));
        for (auto it = range.first; it != range.second; ++it)
            if (keys_[it->second] == Key{ key, tag }) return static_cast<int>(it->second);
        return -1;
    }

    const std::string* Table::Record(uint64_t key, int32_t tag) const
    {
        const int row = RowOf(key, tag);
        return row < 0 ? nullptr : &records_[row];
    }

    bool Table::Set(uint64_t key, const std::string& record, int32_t tag)
    {
        const int row = RowOf(key, tag);
        if (row < 0) return false;
        records_[row] = record;
        return true;
    }

    bool Table::Append(uint64_t key, const std::string& record, int32_t tag)
    {
        if (RowOf(key, tag) >= 0) return false;
        if (!wide_ && keySize_ < 8 && key >> (keySize_ * 8)) return false;
        const size_t row = keys_.size();
        keys_.push_back(Key{ key, tag });
        records_.push_back(record);
        bodyOrder_.push_back(row);
        index_.emplace(Slot(key, tag), row);
        return true;
    }
}
