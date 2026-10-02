#pragma once
#include <cstdint>

// Byte patterns and layouts for Crimson Desert 2.03.02 (exe 1.0.0.2976).
// Each says what it finds and where it was on this exe; the plugin always
// searches and never trusts the offset. private/KNOWLEDGE.md has the story
// behind each one.

namespace bm::sig
{
    // ---- Files -------------------------------------------------------------

    // pa::ResourceLoader (vtable +0x571BE40). Slot 3 (+0x12D00B0) finds the
    // pack source for a path, slot 13 (+0x12D1560) checks that one exists.
    // gamefile hooks both.
    inline constexpr const char* kSig_LoaderFindSource =
        "48 89 5C 24 18 48 89 54 24 10 55 56 57 41 56 41 57 48 81 EC 90 00 00 00 41 8B E9 4D";
    inline constexpr const char* kSig_LoaderExists =
        "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 32 DB 48 8B F2 83 79 18 00";

    // Read, slot 8 (+0x12D0350): (loader, path, buffer, name, u32, u32, u32
    // flags). It finds the source and has it fill the buffer. The path is an
    // engine string: a pointer to a string object whose first field is the
    // char*. Static tables, string tables, UI xml and character
    // descriptions all come through here.
    inline constexpr const char* kSig_ResourceRead =
        "48 89 5C 24 08 57 48 83 EC 40 49 8B D9 49 8B F8 44 8B 8C 24 80 00 00 00 4C 8B C2 48 8D 54 24 30 "
        "E8 ?? ?? ?? ?? 90 48 8B 54 24 30 48 85 D2 74 ?? 48 8B 4A 20";

    // The engine path the table loader hands Read: +0x12C68B0 builds one
    // from a C string (path out, char*, u8 0), +0x394DB0 releases it. The
    // release pattern also matches +0x44FFA0, the release of another string
    // type, so the one taken is the one whose empty-string global (the lea
    // at +0x12) is the builder's (the lea at +0x27).
    inline constexpr const char* kSig_MakePath =
        "48 8B C4 48 89 58 18 48 89 48 08 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 40 01 00 00 45 0F B6 C8 4C 8B C2 "
        "4C 8B F1";
    inline constexpr const char* kSig_ReleasePath =
        "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B 19 48 8D 35 ?? ?? ?? ?? 48 8B F9 48 3B DE 74 ?? 8B 43 10 "
        "85 C0 78";
    inline constexpr unsigned kOff_MakePath_EmptyLea = 0x27;
    inline constexpr unsigned kOff_ReleasePath_EmptyLea = 0x12;

    // The game's allocation idiom, `mov edx,10h; mov ecx,18h; cmp byte
    // [r15],0; je; call A; jmp; call B`: A(size, align) when the thread
    // block's byte +0x1FD is set, B when it is clear. Every copy calls the
    // same two (+0x47EFA78, +0x47EF9B0), so the first will do.
    inline constexpr const char* kSig_AllocIdiom = "BA 10 00 00 00 B9 18 00 00 00 41 80 3F 00 74 07 E8 ?? ?? ?? ?? EB 05 E8";
    inline constexpr unsigned    kOff_AllocIdiom_Set = 16;
    inline constexpr unsigned    kOff_AllocIdiom_Clear = 23;
    // The read buffer's release (+0x13739B0): takes the size off the memory
    // tag's count at [rip] (+0x1E, a lea) and frees the data with the free
    // matching the same byte (+0x4B set, +0x64 clear). Unique on this exe.
    inline constexpr const char* kSig_BufferRelease =
        "48 89 5C 24 08 57 48 83 EC 20 0F BE 41 10 48 8B D9 3C FF 74 15 44 8B 41 08 8B D0 41 F7 D8 48 8D 05";
    inline constexpr unsigned    kOff_BufferRelease_TagBytes = 0x1E;
    inline constexpr unsigned    kOff_BufferRelease_FreeSet = 0x4B;
    inline constexpr unsigned    kOff_BufferRelease_FreeClear = 0x64;

    // The appearance XML loader (+0x2439D90), called from
    // AppearanceTableDataAsyncLoadingTask's execute with the task: path as
    // an engine string at +0x18, load result byte at +0x20 (2 loaded), and
    // at +0x28 the loaded buffer, whose first field is the decrypted text.
    // Appearance files stream through the loader's async slot and never
    // reach Read, so this is where the plugin hands over the broom's.
    inline constexpr const char* kSig_AppearanceLoad =
        "48 89 4C 24 08 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 08 E2 FF FF B8 F8 1E 00 00";
    inline constexpr unsigned kOff_AppTask_Path = 0x18;
    inline constexpr unsigned kOff_AppTask_Buffer = 0x28;

    // The loader's async load, vtable slot 9 (+0x12D03E0): (loader, handle
    // out, u8* status, path, holder, name, three u32). Animations, their LOD
    // copies, blends and skeletons all load through it, the path an engine
    // string of the same kind the path builder makes (both use the empty
    // string at +0x6A65940). On a job thread it queues an AsyncFileLoadTask
    // that takes its own reference to the path; otherwise it calls Read.
    inline constexpr const char* kRtti_ResourceLoader = ".?AVResourceLoader@pa@@";
    inline constexpr unsigned    kSlot_LoaderAsync = 9;

    // ---- The grant ---------------------------------------------------------

    // The client's request sender (+0x35C670): send(ctx, buffer, 4, message,
    // u16 length, u8 0, u16 0x10), the message a u16 id, a byte, then the
    // payload. The first twelve bytes are two stores and two pushes.
    inline constexpr const char* kSig_SendRequest =
        "48 89 5C 24 10 4C 89 4C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 20 45 8B E0 4C 8B F2 4C 8B E9 BD FD 01 00 00";
    inline constexpr uint16_t kReq_LoadingComplete = 0x098D;   // TrocTrLoadingCompleteReq
    // TrocTrHireMercenaryToTargetReq: u16 id, u8 0, u16 payload length 5,
    // u32 actor id, u8 0. The server hires that actor for the sender.
    inline constexpr uint16_t kReq_HireToTarget = 0x0B8F;

    // The server's hire (+0x2BADC00): (clan component, u32* error, u32*
    // actor id, u8, u8). It looks the actor up by id, reads its
    // characterinfo row from the status component (status +0x30), then that
    // row's _mercenaryInfo. The first twelve bytes are two stores and two
    // pushes.
    inline constexpr const char* kSig_ServerHire =
        "48 89 5C 24 10 4C 89 44 24 18 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 C0 48 81 EC 40 01 00 00 "
        "45 0F B6 E9 49 8B F8 4C 8B FA 48 8B F1";
    // The hire's actor lookup, lookup(manager, out, id), which leaves the
    // actor at out+8 and a found byte at out+0x10. The same body exists five
    // times in the exe, so the one the hire uses is taken from the hire's
    // own call (the E8 at hire+0xB0, after `mov rcx,[rcx+48h]`) and its
    // prologue checked.
    inline constexpr unsigned    kOff_Hire_LookupCallSig = 0xAC;
    inline constexpr unsigned    kOff_Hire_LookupCall = 0xB0;
    inline constexpr const char* kSig_HireLookupCall = "48 8B 49 48 E8";
    inline constexpr const char* kSig_ActorLookup =
        "44 89 44 24 18 48 89 54 24 10 53 55 56 57 41 56 48 83 EC 30 48 8B FA 48 8B F1";
    inline constexpr unsigned kOff_Lookup_Actor   = 0x08;
    inline constexpr unsigned kOff_Lookup_Found   = 0x10;
    inline constexpr unsigned kOff_Actor_Block    = 0x68;
    inline constexpr unsigned kOff_Actor_Kind     = 0x88;
    inline constexpr unsigned kOff_Kind_Byte      = 0x01;
    inline constexpr unsigned kOff_Block_Status   = 0x20;
    inline constexpr unsigned kOff_Block_Owner    = 0x118;
    inline constexpr unsigned kOff_Owner_Id       = 0x18;
    inline constexpr unsigned kOff_Status_CharRow = 0x30;
    // The hire takes only an actor whose kind byte is 4, 5 or 6 and whose
    // owner id is 0; anything else fails with the error at +0x6CF7AA4.
    inline constexpr uint8_t  kActorKindHireLo = 4;
    inline constexpr uint8_t  kActorKindHireHi = 6;
    inline constexpr uint8_t  kMercTypeHorse   = 1;   // _mercenaryInfo of the wild horses

    // pa::ServerMercenaryClanActorComponent, the server's roster. At +0x18 a
    // map of every owned mercenary, whose count is the component's +0x2C and
    // element pointers its +0x40; an element's entry at +0x10, and the
    // entry's characterinfo row as a u16 at +0x20, and the actor id of the
    // mercenary while it is out at +0x50 (0 while stored). A mount whose
    // entry names an actor is not spawned again when called.
    inline constexpr const char* kRtti_ServerClan = ".?AVServerMercenaryClanActorComponent@pa@@";
    inline constexpr unsigned kOff_Clan_Count   = 0x2C;
    inline constexpr unsigned kOff_Clan_Array   = 0x40;
    inline constexpr unsigned kOff_ClanEl_Entry = 0x10;
    inline constexpr unsigned kOff_ClanEnt_Row  = 0x20;
    inline constexpr unsigned kOff_ClanEnt_Actor = 0x50;

    // ---- The call --------------------------------------------------------

    // Kliff's client chart component. Its vtable slot 78 (+0x360BE0 on this
    // exe) checks a chart leaf that carries data, (component, pointer to the
    // token pointer, ...), returning 0, 1 or 2; slot 77 checks the built-in
    // leaves below 0x83. A token is a u32 leaf id and its arguments.
    inline constexpr const char* kRtti_ClientChartComponent = ".?AVClientCharacterControlActorComponent@pa@@";
    inline constexpr unsigned    kSlot_ChartDataLeaf = 78;
    // Leaf 0x1D1 (reserve slot key): 1 while the chosen slot is another one.
    // The call branches of common_upper_branchset ask it of VehicleSlot
    // (+0x140E5), VehicleSlot_Dragon (+0x14119) and VehicleSlot_Mechanic
    // (+0x1414D).
    inline constexpr uint32_t kLeaf_SlotChosen = 0x1D1;
    inline constexpr uint32_t kSlot_Vehicle = 1000006;   // reserveslot VehicleSlot
    inline constexpr uint32_t kSlot_Broomy = 1000032;    // reserveslot VehicleSlot_Broomy

    // The frame event lookup (+0x1F78A20): (frame event request, out, actor,
    // u32). It finds the frame event the request names in the action the
    // actor plays; out+8 then points at the event's data. For the call
    // event (ClientFrameEventCallMercenaryReservedSlot) the data's +0x0C is
    // the reserve slot key it calls, and the server's call reads it there
    // (+0x2BA9BEA, +0x2BAB659). Client and server both use it.
    inline constexpr const char* kSig_FrameEventLookup =
        "48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 41 56 41 57 48 8D 6C 24 B9 48 81 EC 90 00 00 00 0F B6 41 08 "
        "33 DB 48 89 1A";
    inline constexpr unsigned kOff_FrameEventOut_Data = 0x08;
    inline constexpr unsigned kOff_CallEvent_SlotKey  = 0x0C;
    // How long after Broomy's wedge request the chart's VehicleSlot branch is
    // opened for it. The chart takes the branch in the frame after the
    // request; left open, the branch fires again every two seconds.
    inline constexpr uint32_t kCallWindowMs = 500;

    // The server's handler of the client's movement request (0x0B0C, static
    // descriptor +0x6A6FC30, slot 2 of its live vtable): (this, u32* error,
    // message), the sender actor at message+0. Found at +0x29784C0.
    inline constexpr const char* kSig_ServerMoveHandler =
        "48 89 5C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 60 FF FF FF 48 81 EC B0 01 00 00 4D 8B F0 48 8B DA "
        "48 8B F9";
    // The sender actor as the hire request's handler (+0x2A2F160) reads it:
    // [+0x88]+1 the kind byte (1 the player), +0x96 a ready byte, and
    // [[+0x68]+0x110] its clan component.
    inline constexpr unsigned kOff_Sender_Kind  = 0x88;
    inline constexpr unsigned kOff_Sender_Ready = 0x96;
    inline constexpr unsigned kOff_Sender_Block = 0x68;
    inline constexpr unsigned kOff_Block_Clan   = 0x110;

    inline constexpr uint32_t kGrantDelayMs = 3000;    // after loading completes
    inline constexpr uint32_t kGrantRetryMs = 5000;    // after a hire that failed or never came
    inline constexpr int      kGrantTries   = 5;
}
