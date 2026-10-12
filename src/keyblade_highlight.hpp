#pragma once
// Immutable material/light preparation; all seven native lights remain in use.
#include "chain_physics.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace kingdom::highlight {
constexpr std::size_t kIntervals=4096;
struct Material {
    float exponent{},f0[3]{},oneMinusF0[3]{};
    std::array<std::uint8_t,4> baseColor{};
    std::vector<float> specular;
};
inline std::uint8_t colorByte(float x){return static_cast<std::uint8_t>(std::clamp(x,0.f,255.f));}
inline Material prepareMaterial(const float rgba[4],float metallic,float roughness) {
    Material result;const float rough=std::clamp(roughness,.12f,1.f);
    result.exponent=std::clamp(2.f/(rough*rough)-2.f,2.f,128.f);
    for(unsigned c=0;c<3;++c){result.f0[c]=.04f*(1-metallic)+rgba[c]*metallic;result.oneMinusF0[c]=1-result.f0[c];result.baseColor[c]=colorByte(rgba[c]*255);}
    result.baseColor[3]=255;
    result.specular.resize(kIntervals+1);
    for(std::size_t i=0;i<=kIntervals;++i)result.specular[i]=std::pow(float(i)/float(kIntervals),result.exponent);
    return result;
}
class MaterialCache {
    struct Entry {std::array<float,6> key;Material material;};
    std::vector<Entry> entries_;
public:
    std::size_t intern(const float rgba[4],float metallic,float roughness) {
        const std::array<float,6> key{rgba[0],rgba[1],rgba[2],rgba[3],metallic,roughness};
        for(std::size_t i=0;i<entries_.size();++i)if(!std::memcmp(key.data(),entries_[i].key.data(),sizeof(key)))return i;
        entries_.push_back({key,prepareMaterial(rgba,metallic,roughness)});return entries_.size()-1;
    }
    const Material& operator[](std::size_t index)const{return entries_[index].material;}
    std::size_t size()const{return entries_.size();}
    void clear(){entries_.clear();}
};
struct Light {V3 position,direction,cosAtten,distAtten;float color[3];};
struct Lights {
    std::array<Light,7> values{};std::size_t count{};
    void add(V3 position,V3 direction,V3 cosAtten,V3 distAtten,std::array<std::uint8_t,3> color) {
        if(count==values.size()||!(color[0]||color[1]||color[2]))return;
        values[count++]={position,direction,cosAtten,distAtten,{float(color[0]),float(color[1]),float(color[2])}};
    }
};
inline float specular(float cosine,const Material& material) {
    if(!(cosine>=0.f&&cosine<=1.f)||material.specular.size()!=kIntervals+1)return std::pow(cosine,material.exponent);
    if(cosine==1.f)return material.specular.back();
    const float scaled=cosine*float(kIntervals);const auto index=static_cast<std::size_t>(scaled);
    const float a=material.specular[index],b=material.specular[index+1];
    return a+(b-a)*(scaled-float(index));
}
template<bool Lookup=true,bool FastFresnel=true>
inline V3 evaluate(V3 normal,V3 position,const Material& material,const Lights& lights) {
    const V3 eye=unit(position*-1.f);float rgb[3]={};
    for(std::size_t i=0;i<lights.count;++i) {
        const auto& light=lights.values[i];const V3 delta=light.position-position;
        const float distance=length(delta);const V3 l=unit(delta);
        const float diffuse=std::max(0.f,dot(normal,l));if(diffuse==0)continue;
        const V3 half=unit(l+eye);const float cosine=std::max(0.f,dot(l,light.direction));
        const float cosAttenuation=light.cosAtten.x+light.cosAtten.y*cosine+light.cosAtten.z*cosine*cosine;
        const float distAttenuation=light.distAtten.x+light.distAtten.y*distance+light.distAtten.z*distance*distance;
        const float attenuation=std::max(0.f,cosAttenuation/std::max(.000001f,distAttenuation));
        const float nh=std::max(0.f,dot(normal,half));
        const float power=Lookup?specular(nh,material):std::pow(nh,material.exponent);
        const float shine=power*diffuse*attenuation;
        const float f=1.f-std::max(0.f,dot(eye,half));float fresnel;
        if constexpr(FastFresnel){const float square=f*f;fresnel=(square*square)*f;}else fresnel=std::pow(f,5.f);
        for(unsigned c=0;c<3;++c)rgb[c]+=shine*(material.f0[c]+material.oneMinusF0[c]*fresnel)*light.color[c]*.8f;
    }
    return {rgb[0],rgb[1],rgb[2]};
}
} // namespace kingdom::highlight
