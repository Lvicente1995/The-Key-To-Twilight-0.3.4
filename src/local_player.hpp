#pragma once
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"

// Multiplayer safety: only the game's authoritative local Link actor may be
// replaced by The Key to Twilight. Crests of Courage can create/draw remote
// player puppets through Link-adjacent render paths; those must remain fully
// owned by Crests.
namespace kingdom::local_player {

inline daAlink_c* get() noexcept {
    return static_cast<daAlink_c*>(dComIfGp_getLinkPlayer());
}

inline bool is(const daAlink_c* link) noexcept {
    return link && link == get();
}

} // namespace kingdom::local_player
