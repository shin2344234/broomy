#include "core/mod.h"

#include <atomic>
#include <cstdio>
#include <cwchar>
#include <string>
#include <unordered_map>

#include "core/log.h"
#include "core/paths.h"
#include "game/broomblend.h"
#include "game/broomchart.h"
#include "game/analogspeed.h"
#include "game/broomy.h"
#include "game/callgate.h"
#include "game/equipfix.h"
#include "game/farhook.h"
#include "game/gamefile.h"
#include "game/grant.h"
#include "game/mem.h"
#include "ini_default.h"
#include "version.h"

namespace
{
    std::atomic<bool> g_stop{false};
    HANDLE g_thread = nullptr;

    // The game is not the only process that loads this plugin.
    // crashpad_handler.exe does too, with a 671,744-byte image.
    constexpr size_t kMinGameImage = 64ull * 1024 * 1024;

    bool IsGame() { return bm::mem::Game().base && bm::mem::Game().size >= kMinGameImage; }

    int Clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

    int ReadSetting(const wchar_t* key, int fallback)
    {
        return static_cast<int>(GetPrivateProfileIntW(L"settings", key, fallback, bm::Paths::File(BM_INI).c_str()));
    }

    // A DMM install is the plugin on its own, so there is no ini beside it.
    // Write the documented one out when there is none. An existing file is
    // never touched.
    void WriteDefaultIni()
    {
        const std::wstring path = bm::Paths::File(BM_INI);
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return;
        FILE* f = nullptr;
        if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f)
        {
            LOG("[ini] %ls could not be written; every default is compiled in, so the plugin still runs.", BM_INI);
            return;
        }
        const bool ok = fwrite(kDefaultIni, 1, kDefaultIniSize, f) == kDefaultIniSize;
        fclose(f);
        LOG(ok ? "[ini] no %ls beside the plugin, so one was written with every setting at its default."
               : "[ini] %ls was created but not written in full. Delete it and it will be written again.", BM_INI);
    }

    bool g_serving = false;

    // Broomy's riding blend (broomblend.h): the broom's own clips, laid out
    // for Broomy's speeds so its nose pitches with the angle it flies at.
    // Every blend path in Broomy's charts names this file.
    const std::string g_blend(reinterpret_cast<const char*>(bm::broomchart::kBroomBlendBytes),
                              sizeof bm::broomchart::kBroomBlendBytes);
    constexpr char kBlendLeaf[] = "/broom_riding_move.motionblending";

    // Test builds: files in BroomyAnims beside the plugin go to the game in
    // place of the shipped file with the same name, so a clip made in CD
    // Animator can be tried without a pack. The pack index names clips by
    // file name alone, so that is the key. Loaded once, before the game's
    // first read, and never changed after.
    std::unordered_map<std::string, std::string> g_overrides;
    volatile LONG g_overrideLogged = 0;

    std::string LowerLeaf(const char* path)
    {
        const char* slash = strrchr(path, '/');
        std::string leaf = slash ? slash + 1 : path;
        for (char& c : leaf) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        return leaf;
    }

    void LoadOverrides()
    {
        const std::wstring dir = bm::Paths::Dir() + L"BroomyAnims\\";
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((dir + L"*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return;
        do
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            FILE* f = nullptr;
            if (_wfopen_s(&f, (dir + fd.cFileName).c_str(), L"rb") != 0 || !f) continue;
            std::string bytes;
            char buf[65536];
            size_t got;
            while ((got = fread(buf, 1, sizeof buf, f)) > 0) bytes.append(buf, got);
            fclose(f);
            char name[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, name, sizeof name, nullptr, nullptr);
            LOG("[anims] %s (%zu bytes) will go in for the shipped file of that name.", name, bytes.size());
            g_overrides[LowerLeaf(name)] = std::move(bytes);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }

    const std::string* Override(const char* path)
    {
        if (g_overrides.empty()) return nullptr;
        std::string target;
        const std::string leaf = LowerLeaf(bm::broomy::AliasTarget(path, target) ? target.c_str() : path);
        const auto it = g_overrides.find(leaf);
        if (it == g_overrides.end()) return nullptr;
        if (InterlockedIncrement(&g_overrideLogged) <= 32) LOG("[anims] %s gets BroomyAnims/%s.", path, leaf.c_str());
        return &it->second;
    }

    // Called on every async load, Read, find and existence check.
    const std::string* Supply(const char* path, std::string*)
    {
        if (const std::string* mine = Override(path)) return mine;
        const size_t n = strlen(path);
        if (n >= sizeof kBlendLeaf - 1 && _stricmp(path + n - (sizeof kBlendLeaf - 1), kBlendLeaf) == 0)
        {
            static volatile LONG logged = 0;
            if (InterlockedIncrement(&logged) <= 4) LOG("[blend] Broomy's riding blend goes in for %s.", path);
            return &g_blend;
        }
        return nullptr;
    }

    DWORD WINAPI Worker(LPVOID)
    {
        WriteDefaultIni();
        const bool give = ReadSetting(L"GiveBroomy", 1) != 0;
        LOG("[mod] %s %s for Crimson Desert 2.03.02 (exe 1.0.0.2976). GiveBroomy=%d", BM_NAME, BM_VERSION,
            give ? 1 : 0);
        if (!g_serving)
            LOG_ERR("[mod] Broomy's files are not being served (see the [files] line above), so Broomy does not exist "
                    "this session.");
        const bool granting = g_serving && give && bm::grant::Install();
        if (g_serving) bm::callgate::Install();
        if (g_serving) bm::equipfix::Install();
        bm::analogspeed::SetSlowest(ReadSetting(L"SlowestPush", 15));
        if (g_serving) bm::analogspeed::Install();
        if (g_serving && !give) LOG("[grant] GiveBroomy=0, so no save is given Broomy.");
        while (!g_stop.load())
        {
            Sleep(500);
            if (granting) bm::grant::Tick();
        }
        return 0;
    }
}

namespace bm::Mod
{
    void EarlyInstall(HMODULE module)
    {
        // On the loading thread, before the game's own start-up code runs,
        // so the files it reads at boot are seen too.
        Paths::Init(module);
        if (!IsGame()) return;
        bm::broomchart::Speeds speeds;
        speeds.ground = Clamp(ReadSetting(L"GroundSpeed", speeds.ground), 10, 1000);
        speeds.flight = Clamp(ReadSetting(L"FlightSpeed", speeds.flight), 10, 1000);
        speeds.boost = Clamp(ReadSetting(L"BoostSpeed", speeds.boost), 100, 500);
        speeds.climb = Clamp(ReadSetting(L"ClimbSpeed", speeds.climb), 10, 1000);
        bm::broomchart::SetSpeeds(speeds);
        LoadOverrides();
        g_serving = bm::broomy::Install(bm::broomy::kChartsPadded);
        if (g_serving) bm::gamefile::SupplyLoads(&Supply);
    }

    void Initialize(HMODULE module)
    {
        Paths::Init(module);
        if (!IsGame())
        {
            wchar_t name[96];
            _snwprintf_s(name, _countof(name), _TRUNCATE, L"%s.other", BM_FILEBASE);
            Log::ClaimSingle(name);
            wchar_t exe[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, exe, MAX_PATH);
            const wchar_t* leaf = wcsrchr(exe, L'\\');
            LOG("[mod] %ls (pid %lu) is not the game, so nothing is done here. The game's own log is %ls.log.",
                leaf ? leaf + 1 : exe, GetCurrentProcessId(), BM_FILEBASE);
            Log::Shutdown();
            return;
        }
        Log::Claim(BM_FILEBASE);
        g_thread = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
    }

    void Shutdown(bool processExiting)
    {
        g_stop.store(true);
        if (processExiting)
        {
            Log::Shutdown();
            return;
        }
        bm::analogspeed::Stop();
        if (g_thread)
        {
            WaitForSingleObject(g_thread, 3000);
            CloseHandle(g_thread);
            g_thread = nullptr;
        }
        farhook::RemoveAll();
        Log::Shutdown();
    }
}
