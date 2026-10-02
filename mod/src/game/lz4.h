#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

// LZ4 blocks, the one compression the plugin has to read and write: a
// string table (.paloc) wraps its entries in one block.
namespace bm::lz4
{
    // Decodes a raw block into exactly `size` bytes. False when the block is
    // malformed or does not come out at that size.
    bool Decompress(const uint8_t* src, size_t srcLen, std::string& out, size_t size);

    // A valid block that holds `plain` as one run of literals. No
    // compression, which the game's decoder does not need.
    std::string StoreLiterals(const std::string& plain);
}
