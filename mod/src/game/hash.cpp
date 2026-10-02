#include "game/hash.h"

#include <cstring>

namespace
{
    inline uint32_t rot(uint32_t x, int k) { return (x << k) | (x >> (32 - k)); }
}

namespace bm::hash
{
    uint32_t Little(const char* text, size_t len)
    {
        const unsigned char* k = reinterpret_cast<const unsigned char*>(text);
        uint32_t a, b, c;
        a = b = c = static_cast<uint32_t>(len) + 0xDEBA1DCDu;
        size_t i = 0;
        while (len - i > 12)
        {
            a += k[i] | (k[i+1] << 8) | (k[i+2] << 16) | (k[i+3] << 24);
            b += k[i+4] | (k[i+5] << 8) | (k[i+6] << 16) | (k[i+7] << 24);
            c += k[i+8] | (k[i+9] << 8) | (k[i+10] << 16) | (k[i+11] << 24);
            a -= c; a ^= rot(c, 4);  c += b;
            b -= a; b ^= rot(a, 6);  a += c;
            c -= b; c ^= rot(b, 8);  b += a;
            a -= c; a ^= rot(c, 16); c += b;
            b -= a; b ^= rot(a, 19); a += c;
            c -= b; c ^= rot(b, 4);  b += a;
            i += 12;
        }
        unsigned char t[12] = {};
        for (size_t j = i; j < len; ++j) t[j - i] = k[j];
        if (len - i)
        {
            a += t[0] | (t[1] << 8) | (t[2] << 16) | (t[3] << 24);
            b += t[4] | (t[5] << 8) | (t[6] << 16) | (t[7] << 24);
            c += t[8] | (t[9] << 8) | (t[10] << 16) | (t[11] << 24);
            c ^= b; c -= rot(b, 14);
            a ^= c; a -= rot(c, 11);
            b ^= a; b -= rot(a, 25);
            c ^= b; c -= rot(b, 16);
            a ^= c; a -= rot(c, 4);
            b ^= a; b -= rot(a, 14);
            c ^= b; c -= rot(b, 24);
        }
        return c;
    }

    uint32_t Little(const char* text) { return Little(text, strlen(text)); }
}
