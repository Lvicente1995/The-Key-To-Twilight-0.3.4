#include "../src/xion_cloth.hpp"
#include "../src/xion_compact.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <cstring>
#include <chrono>
using namespace kingdom;using namespace kingdom::xion;
std::vector<std::uint8_t> file(const char* p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void skinAll(const Model& m,const std::array<Matrix,kBoneCount>& world,std::array<std::vector<SkinnedVertex>,kMaxMaterials>& out){
    std::array<Matrix,kBoneCount> transforms,normals;
    for(std::size_t b=0;b<kBoneCount;++b){transforms[b]=multiply(world[b],m.inverseBind[b]);assert(normalTransform(transforms[b],normals[b]));}
    for(std::size_t mat=0;mat<m.materials.size();++mat){out[mat].resize(m.materials[mat].vertices.size());for(std::size_t v=0;v<out[mat].size();++v)out[mat][v]=skin(m.materials[mat].vertices[v],transforms,normals);}
}
int main(int argc,char** argv){
    assert(argc==5);Model model;std::string error;assert(decodeModel(file(argv[1]),model,error));auto mask=file(argv[2]);auto poses=file(argv[3]);assert(poses.size()%1944==0);
    cloth::System cloth;assert(cloth.initialize(model,mask,error));assert(cloth.nodeCount()==96&&cloth.vertexCount()==6174);
    std::array<std::vector<bool>,kMaxMaterials> allowed;
    for(std::size_t m=0;m<model.materials.size();++m)allowed[m].resize(model.materials[m].vertices.size());
    {Reader r{mask.data(),mask.size(),8};r.u32();auto mc=r.u32(),nc=r.u32(),ec=r.u32(),rc=r.u32();r.u32();r.offset+=mc*4+nc*40+ec*4;for(unsigned i=0;i<rc;++i){auto m=r.u16(),v=r.u16();allowed[m][v]=true;r.offset+=28;}}
    {auto p=cloth::project({0,2,0},{{0,0,0},{0,4,0},3},{0,1,0});assert(std::fabs(std::sqrt(p.x*p.x+p.z*p.z)-3)<.0001f);}
    {auto bad=mask;bad.pop_back();cloth::System other;assert(!other.initialize(model,bad,error));bad=mask;bad[8]^=1;assert(!other.initialize(model,bad,error));}
    {auto changed=model;std::swap(changed.materials[10].vertices[0],changed.materials[10].vertices[1]);cloth::System other;assert(!other.initialize(changed,mask,error));}
    std::ofstream result(argv[4],std::ios::binary);result.write("XICHECK1",8);std::uint32_t count=7;result.write(reinterpret_cast<char*>(&count),4);
    const std::array<unsigned,7> chosen{2303,3080,3131,4874,4975,6291,8789};
    std::size_t changed=0,samples=0;double applySeconds=0;float maximumDisplacement=0;
    auto start=std::chrono::steady_clock::now();
    for(std::size_t record=0;record<poses.size()/1944;++record){
        std::array<Matrix,kBoneCount> world;std::memcpy(world.data(),poses.data()+record*1944+24,sizeof(world));
        const auto snapshot=cloth.advance(model,world);assert(snapshot.valid);
        if(std::find(chosen.begin(),chosen.end(),record)==chosen.end())continue;
        std::array<std::vector<SkinnedVertex>,kMaxMaterials> first;skinAll(model,world,first);const auto base=first;auto second=base;
        auto t=std::chrono::steady_clock::now();cloth.apply(model,world,snapshot,first);applySeconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count();
        cloth.apply(model,world,snapshot,second);
        V3 origin;std::memcpy(&origin,poses.data()+record*1944+12,sizeof(origin));
        for(std::size_t m=0;m<model.materials.size();++m)assert(compact::choose(model.materials[m].vertices,first[m],origin).position16);
        auto untouched=base;cloth.apply(model,world,{},untouched);
        for(std::size_t m=0;m<model.materials.size();++m)for(std::size_t v=0;v<first[m].size();++v){
            assert(!std::memcmp(&first[m][v],&second[m][v],sizeof(SkinnedVertex)));assert(!std::memcmp(&base[m][v],&untouched[m][v],sizeof(SkinnedVertex)));
            assert(finite(first[m][v].position)&&finite(first[m][v].normal));float d=length(first[m][v].position-base[m][v].position);maximumDisplacement=std::max(maximumDisplacement,d);
            if(!allowed[m][v])assert(!std::memcmp(&base[m][v],&first[m][v],sizeof(SkinnedVertex)));
            if(m==11||m<8)assert(d==0);if(d>.001f)++changed;++samples;
        }
        auto index=static_cast<std::uint32_t>(record);result.write(reinterpret_cast<const char*>(&index),4);
        for(std::size_t m=0;m<model.materials.size();++m)for(const auto& v:first[m])result.write(reinterpret_cast<const char*>(&v.position),12);
    }
    std::array<Matrix,kBoneCount> rest=model.bind;cloth.reset();auto a=cloth.advance(model,rest);for(auto& m:rest)m.m[0][3]+=1000;auto b=cloth.advance(model,rest);assert(a.valid&&b.valid);for(unsigned n=0;n<a.count;++n)assert(length(a.offsets[n]-b.offsets[n])<.01f);
    rest[0].m[0][0]=std::numeric_limits<float>::quiet_NaN();assert(!cloth.advance(model,rest).valid);
    assert(changed>0&&maximumDisplacement<=60.01f);
    std::cout<<"PASS: "<<poses.size()/1944<<" recorded ticks; "<<samples<<" vertices checked; "<<changed<<" displaced; maximum displacement "<<maximumDisplacement<<" cm; apply mean "<<applySeconds/7*1000<<" ms; total "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<" s\n";
}
