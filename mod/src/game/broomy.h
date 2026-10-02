#pragma once
#include <cstdint>
#include <string>

// Broomy's files, handed to the game as it reads them (gamefile), built from
// the game's own bytes (broomrows). A table's header is read and parsed
// before its body, and the header's offsets depend on the body, so the first
// half the game reads has the plugin read the other through the game's own
// loader and build both.
namespace bm::broomy
{
    // Which charts Broomy runs. The plugin runs 3, GoldStar's files holding
    // the Wyvern's charts with the broom's own paths written over the
    // Wyvern's, the rest of each path's bytes zero. The others were research:
    // 0 the Wyvern's own, 1 GoldStar's files holding the Wyvern's charts
    // unchanged, 2 every animation and blend path an alias the plugin loads
    // as the broom's.
    enum ChartMode { kChartsWyvern = 0, kChartsCopied = 1, kChartsAliased = 2, kChartsPadded = 3 };

    // From DllMain, before the game reads its first table.
    bool Install(int chartMode);

    // The shipped file an alias path in Broomy's charts loads, if it is one.
    bool AliasTarget(const char* path, std::string& to);

    // Broomy's characterinfo row once the game has read the table, else -1.
    int Row();
    // A characterinfo row's _mercenaryInfo as the file has it (a
    // mercenaryinfo key, 78 for the horses), or -1.
    int MercenaryKey(uint32_t row);
}
