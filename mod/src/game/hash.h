#pragma once
#include <cstddef>
#include <cstdint>

namespace bm::hash
{
    // Jenkins lookup3 hashlittle as this game compiles it, a = b = c =
    // length + 0xDEBA1DCD. A stringinfo row's u32 key is this over the row's
    // text, case and all: "CD_M0004_Dragon" gives 0x5FA926E8, which is the
    // key the file and the running def both hold. Taken from Master Looter's
    // NameId, which checked it against ids captured live.
    uint32_t Little(const char* text, size_t len);
    uint32_t Little(const char* text);
}
