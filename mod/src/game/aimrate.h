#pragma once

namespace bm::aimrate
{
    // The aim IK smooths toward its target at the rate its frame event gives,
    // but while the target is far it pulls that rate toward 4 per second. On
    // the broom that made the nose and heading swing to the camera in about a
    // quarter second as flight began, whatever rate ikpatches.h sets. This
    // skips the pull for the aim whose reference bone is B_Body_00, the
    // broom's; every other aim IK in the game keeps it.
    bool Install();
}
