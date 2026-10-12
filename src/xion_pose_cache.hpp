#pragma once
#include "xion_model.hpp"
#include <cstring>

namespace kingdom::xion {
// Only geometry is cached. Camera/light/material state is still set per pass.
// capture() must invalidate this before changing the immutable cloth snapshot,
// actor, preview mode or model. Interpolated joints are compared on every draw.
class PoseCache {
    std::array<Matrix,kBoneCount> world_{};
    V3 origin_{};
    bool valid_=false;
public:
    void invalidate() {valid_=false;}
    bool matches(const std::array<Matrix,kBoneCount>& world,V3 origin) const {
        return valid_&&std::memcmp(world_.data(),world.data(),sizeof(world_))==0&&
            std::memcmp(&origin_,&origin,sizeof(origin))==0;
    }
    void commit(const std::array<Matrix,kBoneCount>& world,V3 origin) {
        world_=world;origin_=origin;valid_=true;
    }
};

inline bool invisibleBlendedMaterial(const Material& material,const RgbaImage& image) {
    // An opaque material can still write depth with zero alpha. Only this
    // renderer's blended, depth-write-disabled path can be skipped safely.
    if(!(material.flags&AlphaBlend)||image.pixels.empty())return false;
    for(std::size_t i=3;i<image.pixels.size();i+=4)if(image.pixels[i])return false;
    return true;
}
} // namespace kingdom::xion
