#pragma once
#include "aurora/gfx.h"
#include <cstdint>
#include <string>

namespace kingdom::customization {
// Completed-frame statistics only: diagnostics, never a reservation or a
// claim that the next frame fits. Log new 128-KiB high-water bands, not frames.
inline void logRenderBudget() {
    static std::uint32_t vertexBand=0,storageBand=0,indexBand=0;
    if(applied.character!=CharacterSelection::Xion)return;
    const auto* stats=aurora_get_stats();if(!stats)return;
    constexpr std::uint32_t band=128u*1024u;
    const auto v=stats->lastVertSize/band,s=stats->lastStorageSize/band,i=stats->lastIndexSize/band;
    if(v<=vertexBand&&s<=storageBand&&i<=indexBand)return;
    vertexBand=std::max(vertexBand,v);storageBand=std::max(storageBand,s);indexBand=std::max(indexBand,i);
    const std::string message="Xion render budget (last completed frame): vertices="+
        std::to_string(stats->lastVertSize)+"/5242880, storage="+
        std::to_string(stats->lastStorageSize)+"/8388608, indices="+
        std::to_string(stats->lastIndexSize)+"/2097152 bytes.";
    svc_log->info(mod_ctx,message.c_str());
}
} // namespace kingdom::customization
