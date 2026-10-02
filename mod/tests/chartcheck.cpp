// Offline check of Broomy's chart builder (src/game/broomchart.cpp).
//
//   chartcheck IN OUT
//
// IN holds the shipped m0004_dragon_upper.paac, m0004_ride_dragon_lower.paac,
// m0004_ride_dragon_upper.paac and characteractionpackagedescription.paacdesc.
// The check builds Broomy's charts and package list from them, writes them
// to OUT under the names the game reads them by, with aliases.txt listing
// every alias and where it loads from, and checks that every path the new
// charts name either is the Wyvern's own camera shake or redirects.
// `build.bat chartcheck IN OUT` builds and runs it.
#include <cstdio>
#include <cstring>
#include <string>

#include "game/broomchart.h"

namespace
{
    bool Load(const std::string& path, std::string& out)
    {
        out.clear();
        FILE* f = nullptr;
        if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return false;
        char chunk[65536];
        size_t n;
        while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) out.append(chunk, n);
        fclose(f);
        return true;
    }

    bool Save(const std::string& path, const std::string& data)
    {
        FILE* f = nullptr;
        if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return false;
        const bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
        fclose(f);
        return ok;
    }

    const char* Leaf(const char* path)
    {
        const char* s = strrchr(path, '/');
        return s ? s + 1 : path;
    }
}

int main(int argc, char** argv)
{
    using namespace bm::broomchart;
    if (argc != 3)
    {
        fprintf(stderr, "usage: chartcheck IN OUT\n");
        return 2;
    }
    const std::string in = std::string(argv[1]) + "/", out = std::string(argv[2]) + "/";
    int failed = 0;

    std::string wyvern[kChartCount], made[kChartCount], report, why;
    for (int c = 0; c < kChartCount; ++c)
        if (!Load(in + Leaf(kCharts[c].source), wyvern[c]))
        {
            printf("%s is missing from IN\n", Leaf(kCharts[c].source));
            return 1;
        }
    Aliases aliases;
    if (!BuildCharts(wyvern, made, aliases, report, why))
    {
        printf("charts FAILED: %s\n", why.c_str());
        return 1;
    }
    printf("charts: %s\n", report.c_str());
    for (int c = 0; c < kChartCount; ++c)
    {
        size_t changed = 0;
        for (size_t i = 0; i < made[c].size(); ++i) changed += made[c][i] != wyvern[c][i];
        printf("  %-28s %zu bytes, %zu changed\n", Leaf(kCharts[c].path), made[c].size(), changed);
        if (!Save(out + Leaf(kCharts[c].path), made[c])) ++failed;
    }

    // The padded style: the broom's paths written over the Wyvern's.
    {
        std::string padded[kChartCount];
        Aliases none;
        if (!BuildCharts(wyvern, padded, none, report, why, true))
        {
            printf("padded charts FAILED: %s\n", why.c_str());
            ++failed;
        }
        else
        {
            printf("padded: %s\n", report.c_str());
            for (int c = 0; c < kChartCount; ++c)
                if (!Save(out + "padded_" + Leaf(kCharts[c].path), padded[c])) ++failed;
        }
    }

    // Every alias, each of the three files the game loads for it.
    std::string list;
    for (const Alias& a : aliases.anims)
    {
        const std::string folder = a.wyvern.substr(0, a.wyvern.find_last_of('/') + 1);
        const std::string asked[3] = { "character/motion/" + folder + a.leaf + ".paa",
                                       "character/motion/motion_lod__/" + folder + a.leaf + "_lod.paa",
                                       "actionchart/bin__/animmeta/" + folder + a.leaf + ".paa_metabin" };
        list += a.wyvern + "\n";
        for (const std::string& p : asked)
        {
            std::string to, fallback;
            if (!Redirect(aliases, p.c_str(), to, &fallback))
            {
                printf("  NOT REDIRECTED: %s\n", p.c_str());
                ++failed;
                continue;
            }
            list += "    " + p + "\n      -> " + to + (fallback.empty() ? "" : "\n      or " + fallback) + "\n";
        }
    }
    for (const std::string& b : aliases.blends)
    {
        const std::string p = "character/binary/motionblending/monster/" + b + ".motionblending";
        std::string to;
        if (!Redirect(aliases, p.c_str(), to))
        {
            printf("  NOT REDIRECTED: %s\n", p.c_str());
            ++failed;
        }
        list += p + "\n      -> " + to + "\n";
    }
    // Paths that must pass untouched.
    const char* const untouched[] = { "character/motion/2_mon/cd_m0004_00_dragon/cd_m0004_00_wyvern/00_mon/"
                                      "cd_rd_wyvern_basic_00_00_nor_std_idle_00.paa",
                                      "character/motion/1_pc/1_phm/bm000_xxxxxxxxxxx.paa",
                                      "actionchart/bin__/animmeta/bm000_x.paa_metabin" };
    for (const char* p : untouched)
    {
        std::string to;
        if (Redirect(aliases, p, to))
        {
            printf("  REDIRECTED BY MISTAKE: %s -> %s\n", p, to.c_str());
            ++failed;
        }
    }
    if (!Save(out + "aliases.txt", list)) ++failed;

    std::string desc, newDesc;
    if (!Load(in + "characteractionpackagedescription.paacdesc", desc))
    {
        printf("characteractionpackagedescription.paacdesc is missing from IN\n");
        ++failed;
    }
    else if (!BuildPackageDesc(desc, newDesc, report, why))
    {
        printf("package list FAILED: %s\n", why.c_str());
        ++failed;
    }
    else
    {
        printf("package list: %s (%zu bytes from %zu)\n", report.c_str(), newDesc.size(), desc.size());
        if (!Save(out + "characteractionpackagedescription.paacdesc", newDesc)) ++failed;
    }
    printf(failed ? "%d FAILED\n" : "all good\n", failed);
    return failed ? 1 : 0;
}
