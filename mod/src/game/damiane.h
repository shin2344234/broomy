#pragma once
#include <cstdint>
#include <string>

#include "game/damianepatches.h"
#include "game/tablefile.h"

// Damiane on Broomy. Kliff, Damiane and Oongka ride with Kliff's two riding
// charts, and each broom action in them has only his node, so Damiane played
// his clips with her hands open beside the handle (Oongka's fit). The game
// plays the node whose group hash is the rider's, so damianepatches.h gives
// every broom action a node of hers, naming her own clips and lean blend
// (damianeclips.h, mod.cpp). broomy.cpp serves the charts built here,
// riderfix finds its words in the lower one through Moved, and
// tests/chartcheck.cpp checks both against the shipped files.
namespace bm::damiane
{
    // `shipped` with her nodes spliced in. False when it is not the file they
    // were made for, or the result is not the chart they were made in.
    inline bool Build(const Chart& c, const std::string& shipped, std::string& out)
    {
        if (shipped.size() != c.shippedSize || bm::tablefile::Crc32(shipped.data(), shipped.size()) != c.shippedCrc)
            return false;
        out.clear();
        out.reserve(c.builtSize);
        size_t at = 0;
        for (size_t i = 0; i < c.count; ++i)
        {
            const Splice& s = c.splices[i];
            out.append(shipped, at, s.at - at);
            out.append(reinterpret_cast<const char*>(c.blob) + s.from, s.size);
            at = s.at + s.cut;
        }
        out.append(shipped, at, std::string::npos);
        return out.size() == c.builtSize && bm::tablefile::Crc32(out.data(), out.size()) == c.builtCrc;
    }

    // Where a byte of the shipped chart sits in the built one: moved on by
    // every splice before it, and by an insertion right at it. Meaningless for
    // a byte a splice replaces.
    inline uint32_t Moved(const Chart& c, uint32_t offset)
    {
        int64_t by = 0;
        for (size_t i = 0; i < c.count; ++i)
        {
            const Splice& s = c.splices[i];
            if (s.at > offset || (s.at == offset && s.cut)) break;
            by += static_cast<int64_t>(s.size) - s.cut;
        }
        return static_cast<uint32_t>(offset + by);
    }
}
