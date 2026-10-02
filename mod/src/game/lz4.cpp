#include "game/lz4.h"

namespace bm::lz4
{
    bool Decompress(const uint8_t* src, size_t srcLen, std::string& out, size_t size)
    {
        out.clear();
        out.reserve(size);
        size_t at = 0;
        while (at < srcLen)
        {
            const uint8_t token = src[at++];
            size_t literals = token >> 4;
            if (literals == 15)
            {
                uint8_t more;
                do
                {
                    if (at >= srcLen) return false;
                    more = src[at++];
                    literals += more;
                } while (more == 255);
            }
            if (literals > srcLen - at || out.size() + literals > size) return false;
            out.append(reinterpret_cast<const char*>(src + at), literals);
            at += literals;
            if (at == srcLen) break;   // the last sequence has no match
            if (srcLen - at < 2) return false;
            const size_t offset = src[at] | (static_cast<size_t>(src[at + 1]) << 8);
            at += 2;
            size_t match = (token & 15) + 4;
            if ((token & 15) == 15)
            {
                uint8_t more;
                do
                {
                    if (at >= srcLen) return false;
                    more = src[at++];
                    match += more;
                } while (more == 255);
            }
            if (offset == 0 || offset > out.size() || out.size() + match > size) return false;
            // Byte by byte: a match may overlap the bytes it is copying.
            size_t from = out.size() - offset;
            for (size_t i = 0; i < match; ++i) out.push_back(out[from + i]);
        }
        return out.size() == size;
    }

    std::string StoreLiterals(const std::string& plain)
    {
        std::string out;
        size_t n = plain.size();
        out.reserve(n + n / 255 + 16);
        if (n < 15)
            out.push_back(static_cast<char>(n << 4));
        else
        {
            out.push_back(static_cast<char>(0xF0));
            size_t rest = n - 15;
            while (rest >= 255)
            {
                out.push_back(static_cast<char>(255));
                rest -= 255;
            }
            out.push_back(static_cast<char>(rest));
        }
        out.append(plain);
        return out;
    }
}
