// Compile with ../src and a directory containing the v0.3.2 cloth header,
// renamed xion_cloth_baseline.hpp and its namespace changed to cloth_baseline.
// Arguments: xion.mesh xion.cloth captured-poses.bin (1944-byte pose records).
#include "../src/xion_cloth.hpp"
#include "xion_cloth_baseline.hpp"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
using namespace kingdom;using namespace kingdom::xion;
using Output=std::array<std::vector<SkinnedVertex>,kMaxMaterials>;
std::vector<std::uint8_t> file(const char* p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
Output skinAll(const Model& m,const std::array<Matrix,kBoneCount>& world){
    Output out;std::array<Matrix,kBoneCount> transforms,normals;
    for(std::size_t b=0;b<kBoneCount;++b){transforms[b]=multiply(world[b],m.inverseBind[b]);assert(normalTransform(transforms[b],normals[b]));}
    for(std::size_t mat=0;mat<m.materials.size();++mat){out[mat].resize(m.materials[mat].vertices.size());for(std::size_t v=0;v<out[mat].size();++v)out[mat][v]=skin(m.materials[mat].vertices[v],transforms,normals);}
    return out;
}
int main(int argc,char** argv){
    assert(argc==4);Model model;std::string error;assert(decodeModel(file(argv[1]),model,error));auto mask=file(argv[2]);auto poses=file(argv[3]);assert(!poses.empty()&&poses.size()%1944==0);
    cloth::System current;cloth_baseline::System baseline;assert(current.initialize(model,mask,error)&&baseline.initialize(model,mask,error));
    std::mt19937 rng(33);std::uniform_real_distribution<float> random(-100,100);
    for(unsigned i=0;i<100000;++i){
        V3 a{random(rng),random(rng),random(rng)},b{random(rng),random(rng),random(rng)},p{random(rng),random(rng),random(rng)},fallback{random(rng),random(rng),random(rng)};
        const float radius=1+std::fabs(random(rng))*.25f;
        if(i%4==0)b=a;
        if(i%7==0)p=a;
        if(i%11==0)p=a+V3{radius*(1.f+float(int(i%5)-2)*.0000001f),0,0};
        const auto x=cloth::project(p,{a,b,radius},fallback),y=cloth_baseline::project(p,{a,b,radius},fallback);
        assert(!std::memcmp(&x,&y,sizeof(x)));
    }
    const std::array<unsigned,7> chosen{2303,3080,3131,4874,4975,6291,8789};
    std::size_t samples=0,applied=0;double oldSeconds=0,newSeconds=0;volatile float consume=0;
    for(std::size_t record=0;record<poses.size()/1944;++record){
        std::array<Matrix,kBoneCount> world;std::memcpy(world.data(),poses.data()+record*1944+24,sizeof(world));
        const auto a=current.advance(model,world);const auto b=baseline.advance(model,world);
        assert(a.valid==b.valid&&a.count==b.count&&!std::memcmp(a.offsets.data(),b.offsets.data(),a.count*sizeof(V3)));
        const bool selected=std::find(chosen.begin(),chosen.end(),record)!=chosen.end();
        if(record%100&&!selected)continue;
        auto base=skinAll(model,world),first=base,second=base;
        current.apply(model,world,a,first);baseline.apply(model,world,b,second);
        for(std::size_t m=0;m<model.materials.size();++m){assert(!std::memcmp(first[m].data(),second[m].data(),first[m].size()*sizeof(SkinnedVertex)));samples+=first[m].size();}
        ++applied;
        if(!selected)continue;
        for(unsigned repeat=0;repeat<64;++repeat){
            first=base;second=base;
            auto oldRun=[&](){auto t=std::chrono::steady_clock::now();baseline.apply(model,world,b,second);oldSeconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count();consume=second[10][0].position.x;};
            auto newRun=[&](){auto t=std::chrono::steady_clock::now();current.apply(model,world,a,first);newSeconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count();consume=first[10][0].position.x;};
            if(repeat%2){newRun();oldRun();}else{oldRun();newRun();}
        }
    }
    std::cout<<"PASS 100000 capsule projections; "<<poses.size()/1944<<" snapshots; "<<applied<<" poses / "<<samples<<" skinned positions+normals byte-identical to v0.3.2\n";
    std::cout<<"448 alternating apply samples each: baseline "<<oldSeconds/448*1000<<" ms, optimized "<<newSeconds/448*1000<<" ms, speedup "<<oldSeconds/newSeconds<<"x (standalone CPU, not game FPS)\n";
}
