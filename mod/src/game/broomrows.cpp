#include "game/broomrows.h"

#include <cstdio>
#include <cstring>

#include "game/hash.h"
#include "game/lz4.h"
#include "game/tablefile.h"

// Each new row starts as a copy of a shipped one and changes only what makes
// it Broomy's. Field positions in a characterinfo record move with the
// name's length and the digits in its localized strings, so every change
// finds its field by the value it holds and checks what is around it, and
// refuses when the value is not there exactly once.

namespace bm::broomrows
{
    namespace
    {
        constexpr const char* kBroomyName = "Riding_Broomy_1";
        constexpr uint8_t  kMercKey = 87;
        constexpr const char* kMercName = "Vehicle_Broom";
        constexpr uint16_t kVehicleKey = 20000;
        constexpr const char* kVehicleName = "Broom";

        // Riding_AlpineIbex_1, a riding row the game no longer uses.
        constexpr uint32_t kIbexKey = 29448;
        constexpr uint16_t kIbexVehicle = 16993;       // AlpineIbex
        constexpr uint64_t kFaintSeconds = 1;          // the ibex's is 300
        constexpr uint8_t  kHorseMerc = 78;            // Vehicle_Horse
        constexpr uint8_t  kSpecialMerc = 81;          // Vehicle_Special, the ibex row's list
        constexpr uint8_t  kVehicleGroup = 65;         // mercenarygroupinfo Vehicle
        constexpr uint32_t kVehicleSlot = 1000006;     // reserveslot VehicleSlot, the saddle wedge
        // interactioninfo Broom_Ride, a cut row that seats the rider on the
        // broom's B_Rider_01 with pivots of its own, whose last field Kliff's
        // riding chart tests to pick the broom's mount; under Wyvern_Ride
        // (1000005) Kliff rode Broomy in the Wyvern's poses. There is no
        // Broom_RideAir, so a mount in the air is still the Wyvern's.
        constexpr uint32_t kWyvernRide[2] = { 1000068, 1000264 };   // Broom_Ride, Wyvern_RideAir
        constexpr uint32_t kIbexRide[2] = { 1000098, 1000085 };

        // The charts of GoldStar, a cut dragon no character uses, which the
        // plugin serves as the broom's own (broomchart.h); the Wyvern's when
        // it cannot.
        constexpr const char* kOwnUpper = "CD_M0004_Ride_GoldStar_Upper";
        constexpr const char* kOwnLower = "CD_M0004_Ride_GoldStar_Lower";
        constexpr const char* kWyvernUpper = "CD_M0004_Dragon";
        constexpr const char* kWyvernLower = "CD_M0004_Dragon_Lower";
        bool g_ownCharts = true;
        constexpr const char* kGameplay = "animal_seal";
        constexpr const char* kSkeleton = "character/model/4_riding/cd_r0032_00_broom/cd_r0032_00_broom.pab";
        constexpr const char* kPortrait = "cd_portraitimage_Broomy";
        constexpr const char* kPortraitFile = "UI/texture/icon/itemicon_prefab_cd_t0000_broom_0001.dds";
        // The map icon. vehicleinfo names a uimaptextureinfo row, the row
        // names a component of the map's markup (minimapicon.thtml and
        // worldmapicon.thtml), and the component's CSS class draws a white
        // silhouette that a hired mount's class tints #74d4e5. The ibex's
        // row is 5347; Broomy's is a copy under its own key naming its own
        // component, drawn with the broom item's map icon.
        constexpr uint32_t kIbexMapTexture = 5347;
        constexpr const char* kIbexMapName = "Actor_Vehicle_AlpineIbex_Hired";
        constexpr const char* kIbexMapComponent = "MapIcon_ActorVehicleAlpineIbex_Hired";
        constexpr const char* kMapName = "Actor_Vehicle_Broomy_Hired";
        constexpr const char* kMapComponent = "MapIcon_ActorVehicleBroomy_Hired";
        constexpr const char* kIbexMapImage = "textureid(cd_Icon_map_AlpineIbex)";
        constexpr const char* kMapImage = "textureid(cd_Icon_map_item_broom_00)";
        constexpr uint32_t kUnset = 0xEAC5E173;        // the tables' "no string" key

        // The ibex row's seven asset strings, _upperActionChartPackageGroupName
        // to _skeletonVariationName, as they stand in its record.
        constexpr const char* kIbexStrings[7] = {
            "CD_R0001_Horse", nullptr, "animal_horse",
            "character/appearance/4_riding/cd_r0002_00_alpineibex/cd_r0002_00_alpineibex_0001_00001.app_xml",
            nullptr, "character/model/4_riding/cd_r0002_00_fourfeet/cd_r0002_00_horse/cd_r0002_00_horse.pab",
            "character/binary/skeletonvariation/4_riding/cd_r0002_00_fourfeet/cd_r0002_00_alpineibex/"
            "cd_r0002_00_alpineibex_0001.pabc" };
        constexpr const char* kIbexPortrait = "cd_portraitimage_Riding_AlpineIbex_1";
        // Of the 40 one-byte flags _isCatchable to _isDisplayLevelForUI,
        // which end 4 bytes before _uiPortraitPath, _isHirable is the 15th.
        constexpr int kFlagsBeforePortrait = 44;
        constexpr int kHirableIndex = 14;
        constexpr uint32_t kNameSub = 0x30, kHireSub = 0x32;

        uint32_t Key(const char* text) { return bm::hash::Little(text); }
        const char* ChartUpper() { return g_ownCharts ? kOwnUpper : kWyvernUpper; }
        const char* ChartLower() { return g_ownCharts ? kOwnLower : kWyvernLower; }

        template <typename T> std::string Bytes(T v) { return std::string(reinterpret_cast<const char*>(&v), sizeof v); }
        template <typename T> void Poke(std::string& s, size_t at, T v) { memcpy(&s[at], &v, sizeof v); }
        template <typename T> T Peek(const std::string& s, size_t at)
        {
            T v{};
            if (at + sizeof v <= s.size()) memcpy(&v, s.data() + at, sizeof v);
            return v;
        }

        std::string NameBlock(const char* text) { return Bytes<uint32_t>(static_cast<uint32_t>(strlen(text))) + text; }

        // A localized string field: u8 3, u32 sub, u32 character key, u32
        // length, then the decimal digits of (key << 32) | sub.
        std::string Localized(uint32_t key, uint32_t sub)
        {
            char digits[32];
            snprintf(digits, sizeof digits, "%llu", (static_cast<unsigned long long>(key) << 32) | sub);
            return std::string(1, '\x03') + Bytes<uint32_t>(sub) + Bytes<uint32_t>(key) +
                   Bytes<uint32_t>(static_cast<uint32_t>(strlen(digits))) + digits;
        }

        size_t Count(const std::string& hay, const std::string& needle)
        {
            size_t n = 0;
            for (size_t at = hay.find(needle); at != std::string::npos; at = hay.find(needle, at + 1)) ++n;
            return n;
        }

        // Where `needle` sits, when it sits there exactly once.
        bool Once(const std::string& rec, const std::string& needle, const char* what, size_t& at, std::string& why)
        {
            const size_t n = Count(rec, needle);
            if (n != 1)
            {
                why = std::string(what) + (n ? " is in the record more than once" : " is not in the record");
                return false;
            }
            at = rec.find(needle);
            return true;
        }

        bool Splice(std::string& rec, const std::string& from, const std::string& to, const char* what, std::string& why)
        {
            size_t at;
            if (!Once(rec, from, what, at, why)) return false;
            rec.replace(at, from.size(), to);
            return true;
        }

        // ---- the tables ----------------------------------------------------

        bool Strings(tablefile::Table& t, std::string& report, std::string& why)
        {
            const char* texts[] = { ChartUpper(), ChartLower(), kGameplay, kAppearancePath, kSkeleton, kPortrait,
                                    kMapComponent };
            int added = 0;
            for (const char* text : texts)
            {
                const uint32_t key = Key(text);
                const size_t len = strlen(text);
                if (const std::string* rec = t.Record(key))
                {
                    // key u32, four zero bytes, a flag byte, u32 length, text
                    if (Peek<uint32_t>(*rec, 9) != len || rec->compare(13, len, text) != 0)
                    {
                        why = std::string("stringinfo's key for \"") + text + "\" holds another string";
                        return false;
                    }
                    continue;
                }
                const std::string rec = Bytes<uint32_t>(key) + Bytes<uint32_t>(0) + std::string(1, '\0') +
                                        Bytes<uint32_t>(static_cast<uint32_t>(len)) + text;
                if (!t.Append(key, rec))
                {
                    why = "a string row could not be added";
                    return false;
                }
                ++added;
            }
            char line[96];
            snprintf(line, sizeof line, "stringinfo: %d new strings", added);
            report = line;
            return true;
        }

        void MercenaryInfoByRow(const tablefile::Table& t, std::vector<int16_t>& out)
        {
            out.assign(t.Rows(), -1);
            for (size_t row = 0; row < t.Rows(); ++row)
            {
                const std::string& rec = t.RecordAt(row);
                const std::string hire = Localized(static_cast<uint32_t>(t.KeyAt(row).key), kHireSub);
                const size_t at = rec.find(hire);
                if (at != std::string::npos && at > 0 && rec.find(hire, at + 1) == std::string::npos)
                    out[row] = static_cast<uint8_t>(rec[at - 1]);
            }
        }

        bool Character(tablefile::Table& t, std::string& report, std::string& why, CharacterFacts* facts)
        {
            const std::string* src = t.Record(kIbexKey);
            if (!src)
            {
                why = "characterinfo has no Riding_AlpineIbex_1 row";
                return false;
            }
            std::string rec = *src;
            const std::string oldName = NameBlock("Riding_AlpineIbex_1");
            if (rec.compare(4, oldName.size(), oldName) != 0)
            {
                why = "characterinfo 29448 is not Riding_AlpineIbex_1";
                return false;
            }
            Poke<uint32_t>(rec, 0, kBroomyKey);
            rec.replace(4, oldName.size(), NameBlock(kBroomyName));

            // Its localized strings carry the character key twice, raw and
            // as the decimal digits of (key << 32) | sub.
            bool name = false;
            for (uint32_t sub = 0x30; sub < 0x40; ++sub)
            {
                const std::string old = Localized(kIbexKey, sub);
                const size_t n = Count(rec, old);
                if (n > 1)
                {
                    why = "a localized string is in the ibex row twice";
                    return false;
                }
                if (n == 1)
                {
                    rec.replace(rec.find(old), old.size(), Localized(kBroomyKey, sub));
                    name = name || sub == kNameSub;
                }
            }
            if (!name)
            {
                why = "the ibex row's name string was not found";
                return false;
            }
            if (rec.find(Bytes<uint32_t>(kIbexKey)) != std::string::npos)
            {
                why = "the ibex's key is still somewhere in the record";
                return false;
            }

            // The seven asset strings, in one run.
            std::string oldRun, newRun;
            for (const char* s : kIbexStrings) oldRun += Bytes<uint32_t>(s ? Key(s) : kUnset);
            const uint32_t ours[7] = { Key(ChartUpper()), Key(ChartLower()), Key(kGameplay), Key(kAppearancePath), kUnset,
                                       Key(kSkeleton), kUnset };
            for (uint32_t k : ours) newRun += Bytes<uint32_t>(k);
            if (!Splice(rec, oldRun, newRun, "the ibex's asset strings", why)) return false;

            // _vehicleInfo, a u16 key just before _callMercenaryCoolTime (300).
            size_t at;
            if (!Once(rec, Bytes<uint16_t>(kIbexVehicle) + Bytes<uint16_t>(300), "the ibex's vehicle key", at, why))
                return false;
            Poke<uint16_t>(rec, at, kVehicleKey);
            // _callMercenaryCoolTime, u64 seconds. Under _mercenaryCoolTimeType
            // 1 it is how long a fainted mount cannot be called ("Cannot summon
            // because it's fainted"). One second rather than 0 keeps the timer,
            // and the HP refill at its end, running as for the ibex.
            if (Peek<uint64_t>(rec, at + 2) != 300)
            {
                why = "the ibex's call cooldown is not 300";
                return false;
            }
            Poke<uint64_t>(rec, at + 2, kFaintSeconds);

            // _mercenaryInfo, the u8 just before the hire message's
            // localized string.
            if (!Once(rec, Localized(kBroomyKey, kHireSub), "the hire message", at, why)) return false;
            if (at == 0 || static_cast<uint8_t>(rec[at - 1]) != kSpecialMerc)
            {
                why = "the ibex row's mercenary list is not Vehicle_Special";
                return false;
            }
            rec[at - 1] = static_cast<char>(kMercKey);

            // _uiPortraitPath, and _isHirable counted back from it.
            if (!Once(rec, Bytes<uint32_t>(Key(kIbexPortrait)), "the ibex's portrait", at, why)) return false;
            Poke<uint32_t>(rec, at, Key(kPortrait));
            if (at < static_cast<size_t>(kFlagsBeforePortrait))
            {
                why = "the portrait sits too early in the record";
                return false;
            }
            const size_t flags = at - kFlagsBeforePortrait;
            for (int i = 0; i < 40; ++i)
                if (static_cast<uint8_t>(rec[flags + i]) > 2)
                {
                    why = "the flag run before the portrait is not what the Wyvern's row showed";
                    return false;
                }
            if (rec[flags + kHirableIndex] != 0)
            {
                why = "the ibex row is already hirable, which it was not";
                return false;
            }
            rec[flags + kHirableIndex] = 1;

            // _interactionInfoList: the ibex's two ride interactions become
            // Broom_Ride and the Wyvern's mount in the air.
            const std::string ibexList = Bytes<uint32_t>(2) + Bytes<uint32_t>(kIbexRide[0]) + Bytes<uint32_t>(kIbexRide[1]);
            const std::string wyvernList =
                Bytes<uint32_t>(2) + Bytes<uint32_t>(kWyvernRide[0]) + Bytes<uint32_t>(kWyvernRide[1]);
            if (!Splice(rec, ibexList, wyvernList, "the ibex's interaction list", why)) return false;

            const int row = static_cast<int>(t.Rows());
            if (!t.Append(kBroomyKey, rec))
            {
                why = "characterinfo already has key 1900001";
                return false;
            }
            if (facts)
            {
                facts->broomyRow = row;
                MercenaryInfoByRow(t, facts->mercInfo);
            }
            char line[128];
            snprintf(line, sizeof line, "characterinfo: %u %s at row %d, from Riding_AlpineIbex_1", kBroomyKey,
                     kBroomyName, row);
            report = line;
            return true;
        }

        bool Mercenary(tablefile::Table& t, std::string& report, std::string& why, CharacterFacts* facts)
        {
            const std::string* src = t.Record(kHorseMerc);
            const std::string old = NameBlock("Vehicle_Horse");
            if (!src || src->empty() || static_cast<uint8_t>((*src)[0]) != kHorseMerc || src->compare(1, old.size(), old) != 0)
            {
                why = "mercenaryinfo 78 is not Vehicle_Horse";
                return false;
            }
            std::string rec = *src;
            rec[0] = static_cast<char>(kMercKey);
            rec.replace(1, old.size(), NameBlock(kMercName));
            // _mainDischargeableType 2, _spawnPositionType 3, _summonOwnerOption,
            // _inventoryOwnerType 2, then _allowedSpawnActorTypeList [5]. With
            // the horses' option 3, Damiane's saddle wedge showed her own horse,
            // not Kliff's, and no Broomy (Seth, 4 October). Vehicle_Special's 2
            // shows her the Wyvern, so Broomy's list takes 2.
            if (!Splice(rec, std::string("\x02\x03\x03\x02\x01\x00\x00\x00\x05", 9),
                        std::string("\x02\x03\x02\x02\x01\x00\x00\x00\x05", 9), "Vehicle_Horse's summon owner option",
                        why))
                return false;
            const int row = static_cast<int>(t.Rows());
            if (!t.Append(kMercKey, rec))
            {
                why = "mercenaryinfo already has key 87";
                return false;
            }
            if (facts) facts->broomyList = row;
            char line[128];
            snprintf(line, sizeof line, "mercenaryinfo: %u %s at index %d, from Vehicle_Horse with summon owner option 2",
                     kMercKey, kMercName, row);
            report = line;
            return true;
        }

        bool Group(tablefile::Table& t, std::string& report, std::string& why)
        {
            const std::string* src = t.Record(kVehicleGroup);
            if (!src)
            {
                why = "mercenarygroupinfo has no Vehicle group";
                return false;
            }
            std::string rec = *src;
            const std::string oldList = Bytes<uint32_t>(6) + std::string("\x4E\x4F\x50\x52\x51\x41", 6);
            const std::string newList = Bytes<uint32_t>(7) + std::string("\x4E\x4F\x50\x52\x51\x41", 6) +
                                        std::string(1, static_cast<char>(kMercKey));
            if (!Splice(rec, oldList, newList, "the Vehicle group's list", why)) return false;
            t.Set(kVehicleGroup, rec);
            report = "mercenarygroupinfo: Vehicle lists Vehicle_Broom";
            return true;
        }

        // VehicleSlot, the saddle wedge, takes Broomy's list as its third,
        // after the horses' (78) and Vehicle_Special's (81). A list in two
        // slots crashed the game at boot (28 September), so Broomy has no
        // slot of its own.
        bool Slot(tablefile::Table& t, std::string& report, std::string& why)
        {
            const std::string* src = t.Record(kVehicleSlot);
            const std::string name = NameBlock("VehicleSlot");
            if (!src || Peek<uint32_t>(*src, 0) != kVehicleSlot || src->compare(4, name.size(), name) != 0)
            {
                why = "reserveslot 1000006 is not VehicleSlot";
                return false;
            }
            std::string rec = *src;
            const std::string lists = std::string(1, static_cast<char>(kHorseMerc)) + static_cast<char>(kSpecialMerc);
            if (!Splice(rec, Bytes<uint32_t>(2) + lists, Bytes<uint32_t>(3) + lists + static_cast<char>(kMercKey),
                        "VehicleSlot's mercenary lists", why))
                return false;
            t.Set(kVehicleSlot, rec);
            report = "reserveslot: VehicleSlot, the saddle wedge, takes Broomy's list after the horses' and "
                     "Vehicle_Special's";
            return true;
        }

        // Broom_Ride's two pivots are name blocks "TempPivotKey_Broom_Ride_1"
        // and "_2", each followed by the same length of data: the seat bone,
        // the position and turn, and last the key of the mount action. The
        // mount's riding chart (the Wyvern's) has one mount action; Broomy's
        // has it under both keys (broomchart.cpp and rightmountpatches.h),
        // and Broomy enters its riding chart only from a pivot whose key it
        // has. Until 0.12.24 the first pivot became a copy of the second and
        // Kliff got on from one side only. Both now stay as shipped; this
        // only checks they still end with the keys the chart is given.
        bool Interaction(tablefile::Table& t, std::string& report, std::string& why)
        {
            const std::string* src = t.Record(kWyvernRide[0]);
            const std::string name = NameBlock("Broom_Ride");
            const std::string first = NameBlock("TempPivotKey_Broom_Ride_1");
            const std::string second = NameBlock("TempPivotKey_Broom_Ride_2");
            const size_t a = src ? src->find(first) : std::string::npos;
            const size_t b = src ? src->find(second) : std::string::npos;
            if (!src || src->compare(4, name.size(), name) != 0 || a == std::string::npos || b == std::string::npos ||
                b < a + first.size())
            {
                why = "interactioninfo 1000068 is not Broom_Ride with its two pivots in order";
                return false;
            }
            const size_t from = a + first.size(), to = b + second.size(), n = b - from;
            constexpr char kKey1[4] = { '\xD3', '\x63', '\x2C', '\x26' }, kKey2[4] = { '\x4B', '\x68', '\x72', '\xA2' };
            if (to + n > src->size() || src->compare(from + n - 0x38, 4, kKey1, 4) != 0 ||
                src->compare(to + n - 0x38, 4, kKey2, 4) != 0)
            {
                why = "Broom_Ride's pivots do not end with their mount action keys where expected";
                return false;
            }
            report = "interactioninfo: Broom_Ride keeps both pivots, mount keys 262C63D3 and A272684B";
            return true;
        }

        bool Vehicle(tablefile::Table& t, std::string& report, std::string& why)
        {
            const std::string* src = t.Record(kIbexVehicle);
            const std::string old = NameBlock("AlpineIbex");
            if (!src || Peek<uint16_t>(*src, 0) != kIbexVehicle || src->compare(2, old.size(), old) != 0)
            {
                why = "vehicleinfo 16993 is not AlpineIbex";
                return false;
            }
            std::string rec = *src;
            Poke<uint16_t>(rec, 0, kVehicleKey);
            rec.replace(2, old.size(), NameBlock(kVehicleName));
            // _uiMapTextureInfo, a u32 key: Broomy's map row, not the ibex's.
            if (!Splice(rec, Bytes<uint32_t>(kIbexMapTexture), Bytes<uint32_t>(kBroomyKey), "the ibex's map icon", why))
                return false;
            if (!t.Append(kVehicleKey, rec))
            {
                why = "vehicleinfo already has key 20000";
                return false;
            }
            report = "vehicleinfo: 20000 Broom, from AlpineIbex";
            return true;
        }

        // key, tag, 0, u8 0, "0_Base", appearance string key at +0x11, f32
        // scale at +0x15, key at +0x19.
        bool AppearanceIndex(tablefile::Table& t, std::string& report, std::string& why)
        {
            const std::string* src = t.Record(kIbexKey, -2);
            if (!src || src->size() != 29 || Peek<uint32_t>(*src, 0) != kIbexKey || Peek<uint32_t>(*src, 25) != kIbexKey ||
                Peek<uint32_t>(*src, 0x11) != Key(kIbexStrings[3]))
            {
                why = "the ibex's appearance index row is not the 29-byte shape naming its appearance";
                return false;
            }
            std::string rec = *src;
            Poke<uint32_t>(rec, 0, kBroomyKey);
            Poke<uint32_t>(rec, 0x11, Key(kAppearancePath));
            Poke<float>(rec, 0x15, 1.0f);
            Poke<uint32_t>(rec, 25, kBroomyKey);
            if (!t.Append(kBroomyKey, rec, -2))
            {
                why = "the appearance index already has (1900001, -2)";
                return false;
            }
            report = "characterappearanceindexinfo: (1900001, -2), the broom's appearance at scale 1";
            return true;
        }

        // u32 key, the name, the component's stringinfo key, then two
        // localized strings: u8 0x19, u32 sub (0x190, 0x191), u32 key, u32
        // length, the digits of (key << 32) | sub.
        bool MapTexture(tablefile::Table& t, std::string& report, std::string& why)
        {
            const std::string* src = t.Record(kIbexMapTexture);
            const std::string old = NameBlock(kIbexMapName);
            if (!src || Peek<uint32_t>(*src, 0) != kIbexMapTexture || src->compare(4, old.size(), old) != 0)
            {
                why = "uimaptextureinfo 5347 is not Actor_Vehicle_AlpineIbex_Hired";
                return false;
            }
            std::string rec = *src;
            Poke<uint32_t>(rec, 0, kBroomyKey);
            rec.replace(4, old.size(), NameBlock(kMapName));
            if (!Splice(rec, Bytes<uint32_t>(Key(kIbexMapComponent)), Bytes<uint32_t>(Key(kMapComponent)),
                        "the ibex's map component", why))
                return false;
            int strings = 0;
            for (uint32_t sub = 0x190; sub < 0x1A0; ++sub)
            {
                // Localized() without its leading type byte, which is 0x19 here.
                const std::string from = Localized(kIbexMapTexture, sub).substr(1);
                const size_t n = Count(rec, from);
                if (n > 1)
                {
                    why = "a localized string is in the ibex's map row twice";
                    return false;
                }
                if (n == 1)
                {
                    rec.replace(rec.find(from), from.size(), Localized(kBroomyKey, sub).substr(1));
                    ++strings;
                }
            }
            if (!strings || rec.find(Bytes<uint32_t>(kIbexMapTexture)) != std::string::npos)
            {
                why = "the ibex's map row does not carry its key where it was";
                return false;
            }
            if (!t.Append(kBroomyKey, rec))
            {
                why = "uimaptextureinfo already has key 1900001";
                return false;
            }
            report = "uimaptextureinfo: 1900001 Actor_Vehicle_Broomy_Hired, from the ibex's";
            return true;
        }

        // The game checks at boot that every uimaptextureinfo row is in a
        // filter group, and stops loading when one is not (launcher log:
        // "checkValid ... UIFilterGroup", 4 October). The ibex's is in
        // Group_Invisible (1000015): u32 key, the name, u8 0, u32 1, u32
        // count, then entries of a u32 map key and five zero bytes. Broomy's
        // goes in after the ibex's.
        bool FilterGroup(tablefile::Table& t, std::string& report, std::string& why)
        {
            constexpr uint32_t kGroup = 1000015;
            const std::string* src = t.Record(kGroup);
            const std::string name = NameBlock("Group_Invisible");
            const size_t countAt = 4 + name.size() + 5, list = countAt + 4;
            if (!src || Peek<uint32_t>(*src, 0) != kGroup || src->compare(4, name.size(), name) != 0 ||
                src->size() < list)
            {
                why = "uifiltergroupinfo 1000015 is not Group_Invisible";
                return false;
            }
            std::string rec = *src;
            const uint32_t count = Peek<uint32_t>(rec, countAt);
            const std::string zeros(5, '\0');
            size_t ibex = std::string::npos;
            for (uint32_t i = 0; i < count; ++i)
            {
                const size_t at = list + i * 9;
                if (at + 9 > rec.size() || rec.compare(at + 4, 5, zeros) != 0)
                {
                    why = "Group_Invisible's list is not the shape it was";
                    return false;
                }
                const uint32_t key = Peek<uint32_t>(rec, at);
                if (key == kBroomyKey)
                {
                    why = "Group_Invisible already lists 1900001";
                    return false;
                }
                if (key == kIbexMapTexture) ibex = at;
            }
            if (ibex == std::string::npos)
            {
                why = "Group_Invisible does not list the ibex's map row";
                return false;
            }
            rec.insert(ibex + 9, Bytes<uint32_t>(kBroomyKey) + zeros);
            Poke<uint32_t>(rec, countAt, count + 1);
            t.Set(kGroup, rec);
            report = "uifiltergroupinfo: Group_Invisible lists Broomy's map row next to the ibex's";
            return true;
        }

        // Every occurrence of `from` in `s` replaced by `to`.
        std::string Replaced(std::string s, const std::string& from, const std::string& to)
        {
            for (size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size()))
                s.replace(at, from.size(), to);
            return s;
        }
    }

    void UseOwnCharts(bool own) { g_ownCharts = own; }

    bool BuildTable(const char* name, const std::string& header, const std::string& body, std::string& outHeader,
                    std::string& outBody, std::string& report, std::string& why, CharacterFacts* facts)
    {
        tablefile::Table t;
        std::string parse;
        if (!t.Parse(header, body, parse))
        {
            why = std::string("the shipped table does not parse: ") + parse;
            return false;
        }
        bool ok = false;
        if (!strcmp(name, "stringinfo")) ok = Strings(t, report, why);
        else if (!strcmp(name, "characterinfo")) ok = Character(t, report, why, facts);
        else if (!strcmp(name, "mercenaryinfo")) ok = Mercenary(t, report, why, facts);
        else if (!strcmp(name, "mercenarygroupinfo")) ok = Group(t, report, why);
        else if (!strcmp(name, "reserveslot")) ok = Slot(t, report, why);
        else if (!strcmp(name, "vehicleinfo")) ok = Vehicle(t, report, why);
        else if (!strcmp(name, "characterappearanceindexinfo")) ok = AppearanceIndex(t, report, why);
        else if (!strcmp(name, "interactioninfo")) ok = Interaction(t, report, why);
        else if (!strcmp(name, "uimaptextureinfo")) ok = MapTexture(t, report, why);
        else if (!strcmp(name, "uifiltergroupinfo")) ok = FilterGroup(t, report, why);
        else why = "not a table Broomy changes";
        return ok && t.Build(outHeader, outBody, why);
    }

    // "paloc", four zero bytes, u32 packed size at +9, u32 plain size at
    // +13, zeros to +0x200, then one LZ4 block. The plain list is entries of
    // u64 3, u32 length and the key's decimal digits, u32 length and the
    // text, then a u32 entry count. A character's name is keyed
    // (characterKey << 32) | 0x30.
    bool BuildPaloc(const std::string& game, std::string& out, std::string& why)
    {
        constexpr size_t kWrap = 0x200;
        if (game.size() < kWrap || game.compare(0, 5, "paloc") != 0)
        {
            why = "character.paloc is not wrapped the way this reader expects";
            return false;
        }
        const uint32_t packed = Peek<uint32_t>(game, 9), plainSize = Peek<uint32_t>(game, 13);
        if (game.size() != kWrap + packed)
        {
            why = "character.paloc's packed size does not match the file";
            return false;
        }
        std::string plain;
        if (!lz4::Decompress(reinterpret_cast<const uint8_t*>(game.data()) + kWrap, packed, plain, plainSize))
        {
            why = "character.paloc's block does not decompress";
            return false;
        }
        // Walk the entries: the count must come out right, and Broomy must
        // not be in it yet.
        char key[32];
        snprintf(key, sizeof key, "%llu", (static_cast<unsigned long long>(kBroomyKey) << 32) | kNameSub);
        size_t at = 0;
        uint32_t entries = 0;
        uint64_t lastKind = 3;
        while (plain.size() - at > 4)
        {
            if (plain.size() - at < 16)
            {
                why = "character.paloc's list ends inside an entry";
                return false;
            }
            lastKind = Peek<uint64_t>(plain, at);
            const uint32_t klen = Peek<uint32_t>(plain, at + 8);
            if (klen > plain.size() - at - 12)
            {
                why = "character.paloc's list ends inside a key";
                return false;
            }
            const bool ours = klen == strlen(key) && plain.compare(at + 12, klen, key) == 0;
            at += 12 + klen;
            if (plain.size() - at < 4)
            {
                why = "character.paloc's list ends inside an entry";
                return false;
            }
            const uint32_t vlen = Peek<uint32_t>(plain, at);
            if (vlen > plain.size() - at - 4)
            {
                why = "character.paloc's list ends inside a text";
                return false;
            }
            at += 4 + vlen;
            ++entries;
            if (ours)
            {
                why = "character.paloc already names Broomy";
                return false;
            }
        }
        if (plain.size() - at != 4 || Peek<uint32_t>(plain, at) != entries)
        {
            why = "character.paloc's entry count does not match its entries";
            return false;
        }
        std::string entry = Bytes<uint64_t>(lastKind) + Bytes<uint32_t>(static_cast<uint32_t>(strlen(key))) + key +
                            Bytes<uint32_t>(static_cast<uint32_t>(strlen(kDisplayName))) + kDisplayName;
        plain.replace(at, 4, entry + Bytes<uint32_t>(entries + 1));

        const std::string block = lz4::StoreLiterals(plain);
        out.assign(game, 0, kWrap);
        Poke<uint32_t>(out, 9, static_cast<uint32_t>(block.size()));
        Poke<uint32_t>(out, 13, static_cast<uint32_t>(plain.size()));
        out += block;
        return true;
    }

    bool BuildPortraits(const std::string& game, std::string& out, std::string& why)
    {
        if (game.find(std::string("Name=\"") + kPortrait + "\"") != std::string::npos)
        {
            why = "the portrait list already names Broomy's portrait";
            return false;
        }
        if (game.find("<Texture ") == std::string::npos)
        {
            why = "the portrait list has no Texture entries";
            return false;
        }
        size_t end = game.size();
        while (end && game[end - 1] == '\n') --end;
        out.assign(game, 0, end);
        out += std::string("\n<Texture Name=\"") + kPortrait + "\" Filename=\"" + kPortraitFile +
               "\" Type=\"Image\" GetRect=\"0,0,256,256\"/>\n";
        return true;
    }

    bool BuildMapIcons(bool markup, const std::string& game, std::string& out, std::string& why)
    {
        if (game.find("Broomy") != std::string::npos)
        {
            why = "it already names Broomy";
            return false;
        }
        if (markup)
        {
            // The ibex's component, copied after itself as Broomy's.
            const size_t start = game.find(std::string("<component name=\"") + kIbexMapComponent + "\"");
            const size_t close = start == std::string::npos ? start : game.find("</component>", start);
            if (close == std::string::npos)
            {
                why = "it has no component for the hired ibex";
                return false;
            }
            const size_t end = close + strlen("</component>");
            out.assign(game, 0, end);
            out += "\n\t" + Replaced(game.substr(start, end - start), "AlpineIbex", "Broomy");
            out.append(game, end, std::string::npos);
            return true;
        }
        // Each line of the ibex's classes, copied after itself as Broomy's,
        // drawn with the broom.
        size_t copied = 0, from = 0;
        out.clear();
        while (from < game.size())
        {
            size_t eol = game.find('\n', from);
            eol = eol == std::string::npos ? game.size() : eol + 1;
            const std::string line = game.substr(from, eol - from);
            out += line;
            if (line.find("-AlpineIbex") != std::string::npos)
            {
                std::string ours = Replaced(Replaced(line, kIbexMapImage, kMapImage), "AlpineIbex", "Broomy");
                if (ours.empty() || ours.back() != '\n') ours += '\n';
                out += ours;
                ++copied;
            }
            from = eol;
        }
        if (!copied || out.find(kMapImage) == std::string::npos)
        {
            why = "it has no class drawing the ibex";
            return false;
        }
        return true;
    }

    bool BuildPoseModifiers(const std::string& game, std::string& out, std::string& why)
    {
        constexpr const char* kList = "<PoseModifierDataList Type=\"AimIK\">";
        constexpr const char* kSkeleton = "cd_r0032_00_broom.pab";
        if (game.find(kSkeleton) != std::string::npos)
        {
            why = "the pose modifier list already names the broom's skeleton";
            return false;
        }
        const size_t at = game.find(kList);
        if (at == std::string::npos)
        {
            why = "the pose modifier list has no aim IK list";
            return false;
        }
        const size_t after = at + strlen(kList);
        out.assign(game, 0, after);
        out += std::string("\n    <PoseModifierData>\n\t\t<KeyList>\n\t\t\t<KeyName>") + kSkeleton +
               "</KeyName>\n\t\t</KeyList>\n\t\t<DefaultData>\n\t\t\t<AimIK MaxRotationAngle=\"90\">\n"
               "\t\t\t\t<AimBone AlignWeight=\"1.0\">B_Body_00</AimBone>\n\t\t\t</AimIK>\n\t\t</DefaultData>\n"
               "    </PoseModifierData>\n";
        out.append(game, after, std::string::npos);
        return true;
    }

    // The Wyvern's description with the parts that fit only the Wyvern cut:
    // under it, quadruped foot IK looked for feet the broom does not have and
    // pulled it half under the ground whenever it moved on the ground.
    bool BuildDescription(const std::string& wyvern, std::string& out, std::string& why)
    {
        struct Cut { const char* open; const char* close; };
        const Cut cuts[] = { { "<PoseModifierDesc>", "</PoseModifierDesc>" },
                             { "<FreeClimbPartInfo ", "/>" },
                             { "<ClimbSocketInfo>", "</ClimbSocketInfo>" },
                             { "<HitPartGroupInfo>", "</HitPartGroupInfo>" } };
        out = wyvern;
        for (const Cut& c : cuts)
        {
            const size_t a = out.find(c.open);
            if (a == std::string::npos || out.find(c.open, a + 1) != std::string::npos)
            {
                why = std::string("the Wyvern's description does not have exactly one ") + c.open;
                return false;
            }
            size_t b = out.find(c.close, a);
            if (b == std::string::npos)
            {
                why = std::string("the Wyvern's description does not close ") + c.open;
                return false;
            }
            b += strlen(c.close);
            // and the whitespace after it, as the old build's regex took it
            while (b < out.size() && (out[b] == ' ' || out[b] == '\t' || out[b] == '\r' || out[b] == '\n')) ++b;
            out.erase(a, b - a);
        }
        if (out.find("FootIK") != std::string::npos || out.find("<PhysicsData") == std::string::npos)
        {
            why = "the broom's description did not come out as intended";
            return false;
        }
        return true;
    }

    const std::string& AppearanceText()
    {
        static const std::string text =
            "\xEF\xBB\xBF\r\n<Appearance>\r\n\t<Customization CustomizationFile=\"\" DecorationParamFile=\"\"/>\r\n"
            "\t<Nude>\r\n\t\t<Prefab Name=\"cd_r0032_00_broom_0001\"/>\r\n\t</Nude>\r\n</Appearance>";
        return text;
    }
}
