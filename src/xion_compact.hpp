#pragma once
// GX direct-vertex compression only; the source mesh and skinning stay F32.
#include "xion_model.hpp"
#include <span>

namespace kingdom::xion::compact {
constexpr unsigned kPositionFraction=6,kUvFraction=14;
constexpr float kPositionStep=1.0f/(1u<<kPositionFraction);
constexpr float kUvStep=1.0f/(1u<<kUvFraction);

inline bool fixed16(float value,unsigned fraction,std::int16_t& out) {
    if(!std::isfinite(value)||fraction>15)return false;
    const double rounded=std::round(double(value)*double(1u<<fraction));
    if(rounded<-32768.0||rounded>32767.0)return false;
    out=static_cast<std::int16_t>(rounded);return true;
}
inline bool position(V3 world,V3 origin,std::array<std::int16_t,3>& out) {
    const V3 local=world-origin;
    return fixed16(local.x,kPositionFraction,out[0])&&
           fixed16(local.y,kPositionFraction,out[1])&&
           fixed16(local.z,kPositionFraction,out[2]);
}
inline bool uv(float u,float v,std::array<std::int16_t,2>& out) {
    return fixed16(u,kUvFraction,out[0])&&fixed16(v,kUvFraction,out[1]);
}
struct Format {
    bool position16=true,uv16=true;
    std::size_t mainStride() const {return (position16?6:12)+3+4+(uv16?4:8);}
    std::size_t shadowStride(bool cutout) const {return (position16?6:12)+(cutout?4+(uv16?4:8):0);}
};
inline Format choose(std::span<const Vertex> source,std::span<const SkinnedVertex> skinned,V3 origin) {
    Format format;
    if(source.size()!=skinned.size())return {false,false};
    std::array<std::int16_t,3> p{};std::array<std::int16_t,2> t{};
    for(std::size_t i=0;i<source.size();++i) {
        if(format.position16&&!position(skinned[i].position,origin,p))format.position16=false;
        if(format.uv16&&!uv(source[i].u,source[i].v,t))format.uv16=false;
        if(!format.position16&&!format.uv16)break;
    }
    return format;
}
inline Matrix translatedView(const Matrix& view,V3 origin) {
    Matrix out=view;const auto translated=point(view,origin);
    out.m[0][3]=translated.x;out.m[1][3]=translated.y;out.m[2][3]=translated.z;
    return out;
}
} // namespace kingdom::xion::compact
