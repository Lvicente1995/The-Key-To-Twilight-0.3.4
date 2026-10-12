// C++20, no game/graphics dependencies. Keep assertions enabled.
// cl /std:c++20 /EHsc /O2 /I../src xion_compact_test.cpp
// xion_compact_test.exe ../res/characters/xion/xion.mesh [private-pose-capture.bin]
#include "xion_compact.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
using namespace kingdom;
using namespace kingdom::xion;

std::vector<std::uint8_t> load(const char* path) {
    std::ifstream stream(path,std::ios::binary);assert(stream);
    return {std::istreambuf_iterator<char>(stream),{}};
}
float coordError(V3 a,V3 b) {return std::max({std::fabs(a.x-b.x),std::fabs(a.y-b.y),std::fabs(a.z-b.z)});}
V3 unpack(const std::array<std::int16_t,3>& p) {return {p[0]*compact::kPositionStep,p[1]*compact::kPositionStep,p[2]*compact::kPositionStep};}
int main(int argc,char** argv) {
    std::int16_t q=123;
    assert(compact::fixed16(-512,6,q)&&q==-32768);
    assert(compact::fixed16(511.984375f,6,q)&&q==32767);
    assert(!compact::fixed16(512,6,q));assert(!compact::fixed16(-512.02f,6,q));
    assert(!compact::fixed16(std::numeric_limits<float>::infinity(),6,q));
    assert(!compact::fixed16(std::numeric_limits<float>::quiet_NaN(),6,q));
    assert(!compact::fixed16(1,32,q));
    std::array<std::int16_t,2> uv{};
    assert(compact::uv(0,1,uv)&&uv[0]==0&&uv[1]==16384);
    assert(!compact::uv(2,0,uv));assert(compact::uv(-2,0,uv));
    std::vector<Vertex> source(1);std::vector<SkinnedVertex> output(1);
    source[0].u=.25f;source[0].v=.75f;
    output[0].position={1000001,2000002,-999997};const V3 farOrigin{1000000,2000000,-1000000};
    auto format=compact::choose(source,output,farOrigin);
    assert(format.position16&&format.uv16&&format.mainStride()==17&&format.shadowStride(false)==6&&format.shadowStride(true)==14);
    output[0].position.x+=600;format=compact::choose(source,output,farOrigin);
    assert(!format.position16&&format.uv16&&format.mainStride()==23);
    source[0].u=3;format=compact::choose(source,output,farOrigin);
    assert(!format.position16&&!format.uv16&&format.mainStride()==27);
    output[0].position=farOrigin;format=compact::choose(source,output,farOrigin);
    assert(format.position16&&!format.uv16&&format.mainStride()==21);
    output[0].position.z=std::numeric_limits<float>::quiet_NaN();
    assert(!compact::choose(source,output,farOrigin).position16);
    source.clear();assert(!compact::choose(source,output,farOrigin).position16);

    // Camera, shadow and reflected views preserve relative translation;
    // translating the position matrix never changes its normal rotation.
    std::mt19937 random(913);std::uniform_real_distribution<float> local(-200,200),world(-100000,100000),angle(-3.14f,3.14f);
    float worstPosition=0,worstUv=0,worstView=0;
    for(unsigned i=0;i<10000;++i) {
        const V3 origin{world(random),world(random),world(random)};
        const V3 p=origin+V3{local(random),local(random),local(random)};
        std::array<std::int16_t,3> packed{};assert(compact::position(p,origin,packed));
        const V3 decoded=unpack(packed),relative=p-origin;
        worstPosition=std::max(worstPosition,coordError(decoded,relative));
        assert(coordError(decoded,relative)<=compact::kPositionStep*.50001f);
        const float a=angle(random),c=std::cos(a),s=std::sin(a);
        Matrix view=identity();view.m[0][0]=c;view.m[0][2]=s;view.m[2][0]=-s;view.m[2][2]=c;
        if(i%2)view.m[1][1]=-1; // Mirror transform.
        const V3 moved=direction(view,origin);view.m[0][3]=-moved.x;view.m[1][3]=-moved.y;view.m[2][3]=-moved.z;
        const Matrix shifted=compact::translatedView(view,origin);
        const float error=coordError(point(view,p),point(shifted,decoded));worstView=std::max(worstView,error);
        assert(error<.04f); // Includes cancellation already present in F32 world/view.
        for(int r=0;r<3;++r)for(int k=0;k<3;++k)assert(view.m[r][k]==shifted.m[r][k]);
        const float u=local(random)/200,v=local(random)/200;assert(compact::uv(u,v,uv));
        const float ue=std::max(std::fabs(u-uv[0]*compact::kUvStep),std::fabs(v-uv[1]*compact::kUvStep));
        worstUv=std::max(worstUv,ue);assert(ue<=compact::kUvStep*.50001f);
    }
    std::cout<<"PASS fixed-point bounds, NaN/infinity rejection, independent F32 fallbacks, 10000 translated/reflected views\n"
             <<"max local position error cm="<<worstPosition<<", UV="<<worstUv<<", F32 view-space error cm="<<worstView<<'\n';
    if(argc<2)return 0;
    Model model;std::string error;assert(decodeModel(load(argv[1]),model,error));
    std::size_t mainBefore=0,mainAfter=0,shadowBefore=0,shadowAfter=0;
    for(const auto& material:model.materials) {
        std::vector<SkinnedVertex> vertices;vertices.reserve(material.vertices.size());
        for(const auto& vertex:material.vertices)vertices.push_back({vertex.position,vertex.normal});
        const auto f=compact::choose(material.vertices,vertices,{0,0,0});assert(f.position16&&f.uv16);
        mainBefore+=vertices.size()*27;mainAfter+=vertices.size()*f.mainStride();
        if(!(material.flags&AlphaBlend)) {
            const bool cutout=material.flags&AlphaTest;
            shadowBefore+=vertices.size()*(cutout?24:12);shadowAfter+=vertices.size()*f.shadowStride(cutout);
        }
    }
    std::cout<<"PASS actual mesh "<<model.vertexCount<<" vertices, "<<model.materials.size()<<" materials; all source UV/position values fit\n"
             <<"vertex bytes excluding at most3 alignment bytes per draw: main "<<mainBefore<<" -> "<<mainAfter
             <<", shadow "<<shadowBefore<<" -> "<<shadowAfter<<", combined "<<mainBefore+shadowBefore<<" -> "<<mainAfter+shadowAfter<<'\n';
    if(argc<3)return 0;
    const auto poses=load(argv[2]);constexpr std::size_t recordBytes=1944;assert(poses.size()%recordBytes==0);
    const std::array<unsigned,8> selected{0,2303,3080,3131,4874,4975,6291,8789};std::size_t checked=0;
    for(unsigned record:selected) {
        assert((record+1)*recordBytes<=poses.size());const auto* data=poses.data()+record*recordBytes;
        V3 origin;std::memcpy(&origin,data+12,12);
        std::array<Matrix,kBoneCount> joints,transforms,normals;
        std::memcpy(joints.data(),data+24,sizeof(joints));
        for(unsigned j=0;j<kBoneCount;++j) {transforms[j]=multiply(joints[j],model.inverseBind[j]);assert(normalTransform(transforms[j],normals[j]));}
        for(const auto& material:model.materials) {
            std::vector<SkinnedVertex> vertices;vertices.reserve(material.vertices.size());
            for(const auto& vertex:material.vertices)vertices.push_back(skin(vertex,transforms,normals));
            const auto f=compact::choose(material.vertices,vertices,origin);assert(f.position16&&f.uv16);checked+=vertices.size();
        }
    }
    std::cout<<"PASS "<<checked<<" skinned samples across8 actual captured poses, no F32 fallback required\n";
}
