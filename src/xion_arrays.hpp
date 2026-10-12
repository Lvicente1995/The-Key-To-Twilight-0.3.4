#pragma once
// Attribute storage for public GXSetArray + GX_INDEX16. Separate arrays avoid
// uploading an entire interleaved buffer once for every attribute.
#include "xion_compact.hpp"

namespace kingdom::xion::arrays {
using Position16=std::array<std::int16_t,3>;
using Position32=std::array<float,3>;
using Normal=std::array<std::int8_t,3>;
using Uv16=std::array<std::int16_t,2>;
using Uv32=std::array<float,2>;
using Color=std::array<std::uint8_t,4>;
static_assert(sizeof(Position16)==6&&sizeof(Position32)==12&&sizeof(Normal)==3);
static_assert(sizeof(Uv16)==4&&sizeof(Uv32)==8&&sizeof(Color)==4);
constexpr bool kLittleEndian=std::endian::native==std::endian::little;
inline std::uint8_t colorByte(float x) {return static_cast<std::uint8_t>(std::clamp(x,0.f,255.f));}
inline std::int8_t normalByte(float x) {return static_cast<std::int8_t>(std::clamp(std::lround(x*64.f),-64l,64l));}

struct MaterialData {
    std::vector<Uv16> uv16;
    std::vector<Uv32> uv32;
    std::vector<Color> colors;
    bool uvPacked=true,constantColor=true;
    void initialize(const Material& material) {
        const auto count=material.vertices.size();uv16.resize(count);uv32.resize(count);colors.resize(count);
        uvPacked=true;constantColor=true;
        for(std::size_t v=0;v<count;++v) {
            const auto& source=material.vertices[v];
            uvPacked=compact::uv(source.u,source.v,uv16[v])&&uvPacked;
            uv32[v]={source.u,source.v};
            for(unsigned k=0;k<4;++k)colors[v][k]=colorByte(source.color[k]*material.factor[k]);
            if(v&&colors[v]!=colors[0])constantColor=false;
        }
    }
    const void* uvData() const {return uvPacked?static_cast<const void*>(uv16.data()):uv32.data();}
    std::uint8_t uvStride() const {return uvPacked?sizeof(Uv16):sizeof(Uv32);}
    std::size_t uvBytes() const {return colors.size()*uvStride();}
    std::size_t mainVertexStride() const {return constantColor?6:8;}
    std::size_t shadowVertexStride(bool cutout) const {return 2+(cutout?(constantColor?2:4):0);}
};
struct PoseData {
    std::vector<Position16> position16;
    std::vector<Position32> position32;
    std::vector<Normal> normals;
    bool positionPacked=true;
    void initialize(std::size_t count) {position16.resize(count);position32.resize(count);normals.resize(count);}
    bool update(std::span<const SkinnedVertex> vertices,V3 origin) {
        if(vertices.size()!=position16.size()||vertices.size()!=position32.size()||vertices.size()!=normals.size())return false;
        positionPacked=true;
        for(std::size_t v=0;v<vertices.size();++v) {
            const auto& source=vertices[v];
            if(!finite(source.position)||!finite(source.normal))return false;
            positionPacked=compact::position(source.position,origin,position16[v])&&positionPacked;
            position32[v]={source.position.x,source.position.y,source.position.z};
            normals[v]={normalByte(source.normal.x),normalByte(source.normal.y),normalByte(source.normal.z)};
        }
        return true;
    }
    const void* positionData() const {return positionPacked?static_cast<const void*>(position16.data()):position32.data();}
    std::uint8_t positionStride() const {return positionPacked?sizeof(Position16):sizeof(Position32);}
    std::size_t positionBytes() const {return normals.size()*positionStride();}
    std::size_t mainStorageBytes(const MaterialData& material) const {
        return positionBytes()+normals.size()*sizeof(Normal)+material.uvBytes()+
            (material.constantColor?0:material.colors.size()*sizeof(Color));
    }
    std::size_t shadowStorageBytes(const MaterialData& material,bool cutout) const {
        return positionBytes()+(cutout?material.uvBytes()+(material.constantColor?0:material.colors.size()*sizeof(Color)):0);
    }
};
} // namespace kingdom::xion::arrays
