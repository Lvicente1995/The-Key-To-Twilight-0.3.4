// Standalone C++20 tests; keep assertions enabled (no NDEBUG).
// cl /std:c++20 /EHsc /O2 /I../src xion_arrays_test.cpp
#include "xion_arrays.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
using namespace kingdom;using namespace kingdom::xion;
template<class T>T fetched(const void* bytes,std::size_t stride,std::uint16_t index) {
    T value;std::memcpy(&value,static_cast<const std::uint8_t*>(bytes)+index*stride,sizeof(value));return value;
}
int main(int argc,char** argv) {
    Material material;material.vertices.resize(3);material.factor={.5f,2.f,.25f,.5f};
    for(auto& vertex:material.vertices){vertex.color={100,200,80,128};vertex.u=.25f;vertex.v=.75f;}
    arrays::MaterialData data;data.initialize(material);
    assert(data.constantColor&&data.uvPacked&&data.colors[0]==(arrays::Color{50,255,20,64}));
    assert(data.mainVertexStride()==6&&data.shadowVertexStride(false)==2&&data.shadowVertexStride(true)==4);
    material.vertices[1].color[3]=12;material.vertices[2].color[0]=40;material.vertices[2].u=3;
    data.initialize(material);
    assert(!data.constantColor&&!data.uvPacked&&data.uvStride()==8);
    assert(data.mainVertexStride()==8&&data.shadowVertexStride(true)==6);
    for(std::uint16_t i=0;i<3;++i) {
        const auto c=fetched<arrays::Color>(data.colors.data(),sizeof(arrays::Color),i);
        for(unsigned k=0;k<4;++k)assert(c[k]==arrays::colorByte(material.vertices[i].color[k]*material.factor[k]));
        const auto uv=fetched<arrays::Uv32>(data.uvData(),data.uvStride(),i);
        assert(uv[0]==material.vertices[i].u&&uv[1]==material.vertices[i].v);
    }
    std::vector<SkinnedVertex> vertices(3);const V3 origin{10000,-3000,20000};
    for(unsigned i=0;i<3;++i)vertices[i]={origin+V3{float(i),float(i*2),float(i*3)},{0,0,1}};
    arrays::PoseData pose;pose.initialize(3);assert(pose.update(vertices,origin)&&pose.positionPacked&&pose.positionStride()==6);
    auto* position16=pose.position16.data();auto* position32=pose.position32.data();auto* normals=pose.normals.data();
    for(std::uint16_t i=0;i<3;++i) {
        const auto p=fetched<arrays::Position16>(pose.positionData(),pose.positionStride(),i);
        assert(p==(arrays::Position16{std::int16_t(i*64),std::int16_t(i*128),std::int16_t(i*192)}));
        assert(fetched<arrays::Normal>(pose.normals.data(),sizeof(arrays::Normal),i)==(arrays::Normal{0,0,64}));
    }
    vertices[1].position.x+=1000;assert(pose.update(vertices,origin)&&!pose.positionPacked&&pose.positionStride()==12);
    for(std::uint16_t i=0;i<3;++i) {
        const auto p=fetched<arrays::Position32>(pose.positionData(),pose.positionStride(),i);
        assert(p==(arrays::Position32{vertices[i].position.x,vertices[i].position.y,vertices[i].position.z}));
    }
    assert(position16==pose.position16.data()&&position32==pose.position32.data()&&normals==pose.normals.data());
    vertices[0].normal.x=std::numeric_limits<float>::quiet_NaN();assert(!pose.update(vertices,origin));
    vertices.pop_back();assert(!pose.update(vertices,origin));
    std::cout<<"PASS indexed attribute strides/fetches, uniform color+alpha registers, varying color+alpha fallback, F32 UV/position fallback, no pose-array reallocations, invalid values\n";
    if(argc<2)return 0;
    std::ifstream stream(argv[1],std::ios::binary);assert(stream);
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream),{}};Model model;std::string error;assert(decodeModel(bytes,model,error));
    std::size_t mainVertex=0,shadowVertex=0,mainStorage=0,shadowStorage=0,mainIndices=0,shadowIndices=0;
    std::size_t checked=0;
    for(const auto& m:model.materials) {
        arrays::MaterialData d;d.initialize(m);assert(d.constantColor&&d.uvPacked);
        arrays::PoseData p;p.initialize(m.vertices.size());std::vector<SkinnedVertex> v;
        for(const auto& s:m.vertices)v.push_back({s.position,s.normal});
        assert(p.update(v,{0,0,0})&&p.positionPacked);
        // Resolve every referenced triangle corner through the exact stored
        // index/stride used by GX, comparing against the prior direct payload.
        for(const auto index:m.indices) {
            assert(index<m.vertices.size());const auto& s=m.vertices[index];
            arrays::Position16 expectedPosition{};arrays::Uv16 expectedUv{};
            assert(compact::position(s.position,{0,0,0},expectedPosition)&&compact::uv(s.u,s.v,expectedUv));
            assert(fetched<arrays::Position16>(p.positionData(),p.positionStride(),index)==expectedPosition);
            assert(fetched<arrays::Uv16>(d.uvData(),d.uvStride(),index)==expectedUv);
            assert(fetched<arrays::Normal>(p.normals.data(),sizeof(arrays::Normal),index)==
                (arrays::Normal{arrays::normalByte(s.normal.x),arrays::normalByte(s.normal.y),arrays::normalByte(s.normal.z)}));
            for(unsigned k=0;k<4;++k)assert(d.colors[0][k]==arrays::colorByte(s.color[k]*m.factor[k]));
            ++checked;
        }
        mainVertex+=m.vertices.size()*d.mainVertexStride();mainStorage+=p.mainStorageBytes(d);mainIndices+=m.indices.size()*2;
        if(!(m.flags&AlphaBlend)) {
            const bool cutout=m.flags&AlphaTest;
            shadowVertex+=m.vertices.size()*d.shadowVertexStride(cutout);shadowStorage+=p.shadowStorageBytes(d,cutout);shadowIndices+=m.indices.size()*2;
        }
    }
    assert(mainVertex==138660&&shadowVertex==46108&&mainStorage==300430&&shadowStorage==137740);
    std::cout<<"PASS actual mesh: "<<checked<<" triangle corners identical to compact direct attributes\n"
             <<"Vertex arena main="<<mainVertex<<", shadow="<<shadowVertex<<", total="<<mainVertex+shadowVertex<<" bytes\n"
             <<"Storage arena main="<<mainStorage<<", shadow="<<shadowStorage<<", total="<<mainStorage+shadowStorage<<" bytes\n"
             <<"Index arena main="<<mainIndices<<", shadow="<<shadowIndices<<", total="<<mainIndices+shadowIndices<<" bytes (unchanged)\n"
             <<"Budgets exclude alignment; storage estimates count one upload per bound attribute per material/pass.\n";
}
