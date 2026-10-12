#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <vector>

namespace kingdom::keyblade_arrays {
using Vector=std::array<float,3>;
using Color=std::array<std::uint8_t,4>;
static_assert(sizeof(Vector)==12&&sizeof(Color)==4);
constexpr bool littleEndian=std::endian::native==std::endian::little;
inline std::uint8_t colorByte(float value) {
    return static_cast<std::uint8_t>(std::clamp(value,0.f,255.f));
}

// One immutable, tightly packed allocation per GX attribute. Pointing all
// attributes at an interleaved Vertex allocation would upload it three times.
// Preserve the source F32 positions/normals without packing or quantization.
struct PartData {
    std::vector<Vector> positions,normals;
    std::vector<Color> colors;
    template<class Vertex> void initialize(std::span<const Vertex> source) {
        positions.resize(source.size());normals.resize(source.size());colors.resize(source.size());
        for(std::size_t i=0;i<source.size();++i) {
            const auto& v=source[i];
            positions[i]={v.position.x,v.position.y,v.position.z};
            normals[i]={v.normal.x,v.normal.y,v.normal.z};
            colors[i]={colorByte(v.color[0]*255.f),colorByte(v.color[1]*255.f),colorByte(v.color[2]*255.f),255};
        }
    }
    std::size_t mainStorageBytes() const {return positions.size()*sizeof(Vector)+normals.size()*sizeof(Vector)+colors.size()*sizeof(Color);}
    std::size_t shadowStorageBytes() const {return positions.size()*sizeof(Vector);}
    static constexpr std::size_t mainVertexStride=10; // POS/NRM/CLR0 IDX16 + CLR1 direct RGBA8.
    static constexpr std::size_t shadowVertexStride=2;
};
} // namespace kingdom::keyblade_arrays
