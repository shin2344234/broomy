#pragma once
#include <cstdint>
#include <string>

// Files the plugin hands the game in place of its own, which is how a
// plugin-only install gives the game Broomy's rows, name, portrait,
// appearance and gameplay data without a pack of its own.
//
// Two ways in, because the game reads files two ways. Read (the resource
// loader's slot 8) fills a buffer for tables, string tables, UI xml,
// character descriptions, charts and animation metadata: the hook lets the
// game read its own file, then swaps in the plugin's bytes. Appearance
// files, animations and blends stream through the async slot instead and
// never reach Read. The appearance loader is hooked and given the plugin's
// text for the length of its parse; the async slot itself is hooked so an
// animation or blend path can load another shipped file.
namespace bm::gamefile
{
    // A file's path as the game names it, lower case, forward slashes.
    // Returns true to look at the file at all; called on every read, so it
    // must be quick.
    typedef bool (*Wants)(const char* path);
    // The game's own bytes of `path` (found says whether a pack had it).
    // Returns true with `out` to hand the game `out` instead.
    typedef bool (*Serve)(const char* path, bool found, const std::string& game, std::string& out);
    // The appearance text to parse for `path`, or null for the game's own.
    typedef const std::string* (*Appearance)(const char* path);
    // For a path the async slot is asked to load: true with `to` to load
    // that file in its place. Called on every async load, so it must be
    // quick for a path it does not want.
    typedef bool (*Redirect)(const char* path, std::string& to);
    // For a path Read is asked for that Wants: true with `instead` (and a
    // `fallback` for when that one does not read, or empty) to have Read
    // fill the buffer from that file. The buffer is then the game's own
    // allocation, which matters for files whose loader frees the buffer
    // itself, as the chart loader does. Read's hook may call ReadFile here.
    typedef bool (*ReadInstead)(const char* path, std::string& instead, std::string& fallback);
    // After a ReadInstead read: may change the bytes in place, never the size.
    typedef void (*Patch)(const char* path, uint8_t* data, uint32_t size);

    struct Hooks
    {
        Wants wants;
        Serve serve;
        Appearance appearance;
        Redirect redirect;
        ReadInstead readInstead;
        Patch patch;
    };

    // Research: every file Read fills from the game's own packs, after the
    // read. Called on every read, so it must be quick.
    typedef void (*Seen)(const char* path, const uint8_t* data, uint32_t size);

    // Research: the bytes the plugin has for a file the async slot loads, or
    // null. The async slot queues a load on a loader thread and never reaches
    // Read; for a path this answers, the loader thread is made to look like
    // any other for the one call, so the slot reads through Read, which then
    // fills the buffer with these bytes. Empty bytes keep what the game read.
    // A file no pack holds also needs `standIn`, a shipped file the find and
    // existence checks see in its place. Called on every async load, Read,
    // find and existence check; `standIn` is null except from the last two.
    typedef const std::string* (*Supply)(const char* path, std::string* standIn);

    // From DllMain, before the game reads its first file.
    bool Install(const Hooks& hooks);
    void Observe(Seen seen);
    void SupplyLoads(Supply supply);
    // Whether the async slot is hooked, so Redirect is heard.
    bool Redirecting();

    // Reads another file through the game's own loader. Only inside Serve,
    // on the thread of the read the game is making.
    bool ReadFile(const char* path, std::string& out);
}
