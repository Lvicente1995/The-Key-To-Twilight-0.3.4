#include "../src/keyblade_arrays.hpp"
#include "../src/chain_physics.hpp"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <string>

using namespace kingdom;
struct Vertex {V3 position,normal;float color[4],metallic,roughness;};
static_assert(sizeof(Vertex)==48);
struct Reader {
    std::vector<std::uint8_t> bytes;std::size_t offset=0;
    template<class T>T read(){assert(offset+sizeof(T)<=bytes.size());T v;std::memcpy(&v,bytes.data()+offset,sizeof(v));offset+=sizeof(v);return v;}
    std::string text(std::size_t size){assert(offset+size<=bytes.size());std::string v(reinterpret_cast<const char*>(bytes.data()+offset),size);offset+=size;return v;}
};
std::uint8_t oldColor(float x){return static_cast<std::uint8_t>(std::clamp(x,0.f,255.f));}
void check(const Vertex& v,const keyblade_arrays::PartData& a,std::size_t i) {
    assert(!std::memcmp(&v.position,a.positions[i].data(),12));
    assert(!std::memcmp(&v.normal,a.normals[i].data(),12));
    for(unsigned c=0;c<3;++c)assert(a.colors[i][c]==oldColor(v.color[c]*255.f));
    assert(a.colors[i][3]==255); // Matches the existing solid weapon shader.
}
void mixedPayload() {
    // Native VCD order: three big-endian IDX16 attributes, then direct RGBA8.
    // This checks layout/indices; it does not execute the host's FIFO worker.
    std::array<std::uint8_t,10> payload{};
    constexpr std::uint16_t vertex=15432;
    for(unsigned attr=0;attr<3;++attr){payload[attr*2]=vertex>>8;payload[attr*2+1]=vertex&255;}
    payload[6]=21;payload[7]=83;payload[8]=249;payload[9]=255;
    for(unsigned attr=0;attr<3;++attr)assert((payload[attr*2]*256u+payload[attr*2+1])==vertex);
    assert(payload[6]==21&&payload[7]==83&&payload[8]==249&&payload[9]==255);
    std::vector<Vertex> source(2);
    source[0]={{-0.f,32768.25f,-100000.f},{.12345679f,-.76543212f,1.f},{-1.f,.12345679f,2.f,.1f},.5f,.6f};
    source[1]=source[0];source[1].position.x=0.f;
    keyblade_arrays::PartData data;data.initialize(std::span<const Vertex>(source));
    check(source[0],data,0);check(source[1],data,1);
    assert(data.positions[0][0]==data.positions[1][0]);
    assert(std::signbit(data.positions[0][0])&&!std::signbit(data.positions[1][0]));
    auto moved=std::move(data);check(source[0],moved,0);
    std::cout<<"PASS: mixed VCD payload; exact F32 attributes including signed zero/large coordinates; old color clamp/truncation and solid alpha.\n";
}
int main(int argc,char** argv) {
    mixedPayload();if(argc==1)return 0;
    assert(argc==2);Reader r;std::ifstream input(argv[1],std::ios::binary);assert(input);
    r.bytes={std::istreambuf_iterator<char>(input),{}};
    assert(r.text(8)=="KKMESH02");const auto count=r.read<std::uint32_t>();assert(count==16);r.read<V3>();
    std::size_t totalVertices=0,totalIndices=0,mainStorage=0,shadowStorage=0;
    for(unsigned p=0;p<count;++p) {
        auto name=r.text(r.read<std::uint32_t>());r.read<V3>();r.read<V3>();auto n=r.read<std::uint32_t>();
        std::vector<Vertex> original,unique;std::vector<std::uint16_t> indices;
        std::unordered_map<std::string,std::uint16_t> byVertex;
        for(unsigned i=0;i<n;++i) {
            const auto v=r.read<Vertex>();original.push_back(v);
            std::string key(reinterpret_cast<const char*>(&v),sizeof(v));auto at=byVertex.find(key);
            if(at==byVertex.end()) {
                assert(unique.size()<65535);const auto index=static_cast<std::uint16_t>(unique.size());
                unique.push_back(v);byVertex.emplace(std::move(key),index);indices.push_back(index);
            } else indices.push_back(at->second);
        }
        keyblade_arrays::PartData data;data.initialize(std::span<const Vertex>(unique));
        const auto* positionAddress=data.positions.data();const auto* normalAddress=data.normals.data();const auto* colorAddress=data.colors.data();
        for(std::size_t i=0;i<indices.size();++i)check(original[i],data,indices[i]);
        for(unsigned repeat=0;repeat<3;++repeat)for(std::size_t i=0;i<unique.size();++i)check(unique[i],data,i);
        assert(data.positions.data()==positionAddress&&data.normals.data()==normalAddress&&data.colors.data()==colorAddress);
        assert(data.mainStorageBytes()==unique.size()*28&&data.shadowStorageBytes()==unique.size()*12);
        totalVertices+=unique.size();totalIndices+=indices.size();mainStorage+=data.mainStorageBytes();shadowStorage+=data.shadowStorageBytes();
    }
    assert(r.offset==r.bytes.size()&&totalVertices==26131&&totalIndices==156216);
    std::cout<<"PASS: "<<count<<" parts, "<<totalVertices<<" unique vertices, "<<totalIndices<<" original triangle corners: source positions/normals/base colors preserved exactly; immutable replay addresses stable.\n";
    std::cout<<"Vertex arena before alignment: old main "<<totalVertices*32<<", shadow "<<totalVertices*12
             <<"; new main "<<totalVertices*keyblade_arrays::PartData::mainVertexStride<<", shadow "<<totalVertices*keyblade_arrays::PartData::shadowVertexStride
             <<"; storage main "<<mainStorage<<", shadow "<<shadowStorage<<" bytes. Triangle index uploads unchanged.\n";
}
