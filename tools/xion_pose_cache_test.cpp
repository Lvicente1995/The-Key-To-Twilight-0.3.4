#include "../src/xion_pose_cache.hpp"
#include "../src/xion_arrays.hpp"
#include "../src/xion_cloth.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace kingdom;
using namespace kingdom::xion;
using Clock=std::chrono::steady_clock;

std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream f(path,std::ios::binary);assert(f);
    return {std::istreambuf_iterator<char>(f),{}};
}

// Mirrors prepareSkin's cache/write protocol, without pretending to execute
// native FIFO commands. Submission retains pending reads even on a cache hit.
struct MockPacket {
    PoseCache cache;
    bool pending=false;
    unsigned syncs=0,writes=0;
    bool prepare(const std::array<Matrix,kBoneCount>& world,V3 origin,bool fail=false) {
        if(cache.matches(world,origin))return true;
        cache.invalidate();
        if(pending){++syncs;pending=false;}
        ++writes; // Represents even a partially completed write.
        if(fail)return false;
        cache.commit(world,origin);return true;
    }
    void submit(){pending=true;}
    void capture(){cache.invalidate();}
};

void protocolTests() {
    std::array<Matrix,kBoneCount> world;world.fill(identity());V3 origin{};
    MockPacket p;assert(p.prepare(world,origin));p.submit();
    assert(p.prepare(world,origin));assert(p.prepare(world,origin));
    assert(p.writes==1&&p.syncs==0&&p.pending);
    // A presentation interpolation change can occur inside the same game tick.
    world[14].m[0][3]=.001f;
    assert(p.prepare(world,origin));assert(p.writes==2&&p.syncs==1&&!p.pending);
    p.submit();origin.x=100.f;assert(p.prepare(world,origin));
    assert(p.writes==3&&p.syncs==2);
    // Actor/model/cloth snapshot/preview are capture inputs, not matrix keys.
    for(unsigned changedCaptureInput=0;changedCaptureInput<4;++changedCaptureInput) {
        p.submit();p.capture();assert(!p.cache.matches(world,origin));
        assert(p.prepare(world,origin));
    }
    assert(p.writes==7&&p.syncs==6);
    p.submit();auto changed=world;changed[3].m[1][3]=3;
    assert(!p.prepare(changed,origin,true));
    assert(!p.cache.matches(changed,origin)&&!p.cache.matches(world,origin));
    assert(p.writes==8&&p.syncs==7&&!p.pending);
    assert(p.prepare(changed,origin));assert(p.writes==9&&p.syncs==7);
    std::cout<<"PASS: same-pose reuse; within-tick joint/origin changes; capture invalidation; partial failure; mock FIFO synchronization only before writes.\n";
}

void filterTests() {
    Material material;material.flags=AlphaBlend;
    RgbaImage image;image.width=2;image.height=1;image.pixels={255,20,80,0,255,255,255,0};
    assert(invisibleBlendedMaterial(material,image));
    image.pixels[7]=1;assert(!invisibleBlendedMaterial(material,image));
    image.pixels[7]=0;material.flags=0;assert(!invisibleBlendedMaterial(material,image));
    material.flags=AlphaTest;assert(!invisibleBlendedMaterial(material,image));
    material.flags=AlphaBlend;image.pixels.clear();assert(!invisibleBlendedMaterial(material,image));
    std::cout<<"PASS: only entirely zero-alpha blended textures skipped; opaque, cutout, alpha=1 and empty images retained.\n";
}

struct CpuPacket {
    const Model& model;
    cloth::System& clothSystem;
    cloth::Snapshot snapshot;
    std::array<std::vector<SkinnedVertex>,kMaxMaterials> skinned;
    std::array<arrays::PoseData,kMaxMaterials> packed;
    PoseCache cache;
    unsigned builds=0;
    CpuPacket(const Model& m,cloth::System& c,cloth::Snapshot s):model(m),clothSystem(c),snapshot(s) {
        for(std::size_t i=0;i<m.materials.size();++i) {
            skinned[i].resize(m.materials[i].vertices.size());packed[i].initialize(skinned[i].size());
        }
    }
    void prepare(const std::array<Matrix,kBoneCount>& world,V3 origin,bool useCache) {
        if(useCache&&cache.matches(world,origin))return;
        cache.invalidate();std::array<Matrix,kBoneCount> transforms,normals;
        for(std::size_t j=0;j<kBoneCount;++j) {
            transforms[j]=multiply(world[j],model.inverseBind[j]);assert(normalTransform(transforms[j],normals[j]));
        }
        for(std::size_t m=0;m<model.materials.size();++m)
            for(std::size_t v=0;v<skinned[m].size();++v)skinned[m][v]=skin(model.materials[m].vertices[v],transforms,normals);
        clothSystem.apply(model,world,snapshot,skinned);
        for(std::size_t m=0;m<model.materials.size();++m)assert(packed[m].update(skinned[m],origin));
        ++builds;cache.commit(world,origin);
    }
};

void actualAssetTests(const char* meshPath,const char* clothPath,const char* posePath) {
    Model model;std::string error;assert(decodeModel(readFile(meshPath),model,error));
    auto resourceRoot=std::filesystem::path(meshPath).parent_path().parent_path().parent_path();
    unsigned skipped=0;std::size_t skippedVertices=0;
    for(const auto& material:model.materials) {
        RgbaImage image;assert(decodeImage(readFile(resourceRoot/material.texturePath),image,error));
        if(invisibleBlendedMaterial(material,image)) {
            ++skipped;skippedVertices+=material.vertices.size();
            assert(material.name=="n_ex033_E_mat2_inst");
            std::cout<<"Skipped invisible asset material: "<<material.name<<", "<<material.vertices.size()<<" vertices.\n";
        } else if(!(material.flags&AlphaBlend))assert(!invisibleBlendedMaterial(material,image));
    }
    assert(skipped==1&&skippedVertices==348);
    cloth::System clothSystem;assert(clothSystem.initialize(model,readFile(clothPath),error));
    auto records=readFile(posePath);constexpr std::size_t record=4874,stride=1944;
    assert(records.size()%stride==0&&records.size()>=(record+1)*stride);
    std::array<Matrix,kBoneCount> world;V3 origin;cloth::Snapshot snapshot;
    // Short lead-in supplies a nonempty moving-cloth snapshot; this is a CPU
    // performance workload, not a replacement for the full cloth replay test.
    for(std::size_t i=record-30;i<=record;++i) {
        std::memcpy(world.data(),records.data()+i*stride+24,sizeof(world));
        snapshot=clothSystem.advance(model,world);assert(snapshot.valid);
    }
    std::memcpy(&origin,records.data()+record*stride+12,sizeof(origin));
    CpuPacket uncached(model,clothSystem,snapshot),cached(model,clothSystem,snapshot);
    uncached.prepare(world,origin,false);cached.prepare(world,origin,true);
    uncached.builds=cached.builds=0;
    constexpr unsigned rounds=40,passes=3;
    auto run=[&](CpuPacket& packet,bool reuse) {
        const auto start=Clock::now();
        for(unsigned r=0;r<rounds;++r) {
            packet.cache.invalidate(); // Every new captured snapshot invalidates.
            for(unsigned p=0;p<passes;++p)packet.prepare(world,origin,reuse);
        }
        return std::chrono::duration<double,std::milli>(Clock::now()-start).count();
    };
    const double uncachedMs=run(uncached,false),cachedMs=run(cached,true);
    assert(uncached.builds==rounds*passes&&cached.builds==rounds);
    for(std::size_t m=0;m<model.materials.size();++m) {
        assert(uncached.packed[m].positionPacked==cached.packed[m].positionPacked);
        assert(uncached.packed[m].position16==cached.packed[m].position16);
        assert(uncached.packed[m].position32==cached.packed[m].position32);
        assert(uncached.packed[m].normals==cached.packed[m].normals);
        assert(!std::memcmp(uncached.skinned[m].data(),cached.skinned[m].data(),cached.skinned[m].size()*sizeof(SkinnedVertex)));
    }
    std::cout<<"PASS: actual "<<model.vertexCount<<" vertices, record "<<record<<", 40 three-pass groups; uncached "
             <<uncachedMs<<" ms / "<<uncached.builds<<" builds; cached "<<cachedMs<<" ms / "<<cached.builds
             <<" builds; ratio "<<uncachedMs/cachedMs<<"x; identical skinned positions/normals and packed arrays.\n";
    std::cout<<"CPU-only workload: excludes graphics/FIFO waiting, camera/lighting setup and presentation interpolation lookup; not an FPS measurement.\n";
}

int main(int argc,char** argv) {
    protocolTests();filterTests();
    if(argc==4)actualAssetTests(argv[1],argv[2],argv[3]);
    else if(argc!=1){std::cerr<<"Usage: xion_pose_cache_test [xion.mesh xion.cloth recorded-poses.bin]\n";return 2;}
}
