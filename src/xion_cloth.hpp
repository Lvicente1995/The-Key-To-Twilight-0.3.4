#pragma once
// Small cosmetic coat simulation. No game, GX, clock, allocation or shared
// mutable render state is used by apply(); all resources are validated at load.
#include "xion_model.hpp"
#include <algorithm>
#include <array>
#include <vector>

namespace kingdom::xion::cloth {
constexpr std::size_t kMaxNodes=128,kMaxEdges=1024,kMaxRecords=10000,kMaxCapsules=8;
struct Snapshot {
    std::array<V3,kMaxNodes> offsets{};
    std::uint16_t count{};
    bool valid=false;
};
struct Capsule {
    V3 a{},b{},axis{},low{},high{};float radius{},axisSquared{},rejectSquared{};
    Capsule()=default;
    Capsule(V3 start,V3 end,float r):a(start),b(end),axis(end-start),radius(r),axisSquared(dot(axis,axis)),rejectSquared(r*r*1.000002f) {
        // Expand the broad phase slightly so rounding cannot reject a point
        // near the original capsule boundary, even at large map coordinates.
        const float magnitude=std::max({std::fabs(a.x),std::fabs(a.y),std::fabs(a.z),std::fabs(b.x),std::fabs(b.y),std::fabs(b.z)});
        const float extent=r+magnitude*(8.f*std::numeric_limits<float>::epsilon())+.0001f;
        low={std::min(a.x,b.x)-extent,std::min(a.y,b.y)-extent,std::min(a.z,b.z)-extent};
        high={std::max(a.x,b.x)+extent,std::max(a.y,b.y)+extent,std::max(a.z,b.z)+extent};
    }
};
inline V3 limited(V3 v,float maximum) {const float d=length(v);return d>maximum?v*(maximum/d):v;}
inline V3 project(V3 p,const Capsule& c,V3 fallback) {
    if(p.x<c.low.x||p.x>c.high.x||p.y<c.low.y||p.y>c.high.y||p.z<c.low.z||p.z>c.high.z)return p;
    const V3 axis=c.axis;const float q=c.axisSquared;
    const V3 center=c.a+axis*(q>1.e-8f?std::clamp(dot(p-c.a,axis)/q,0.f,1.f):0.f);
    const V3 d=p-center;const float squared=dot(d,d);
    if(squared>c.rejectSquared)return p;
    const float l=std::sqrt(squared);
    if(l>=c.radius)return p;
    // The original normalization uses its fallback only on the centerline.
    // Avoid constructing and normalizing that unused fallback for other hits.
    if(l>.00001f)return center+(d/l)*c.radius;
    if(q>1.e-8f)fallback-=axis*(dot(fallback,axis)/q);
    const V3 perpendicular=unit(cross(axis,std::fabs(axis.x)<std::fabs(axis.y)?V3{1,0,0}:V3{0,1,0}),{0,0,-1});
    return center+unit(d,unit(fallback,perpendicular))*c.radius;
}
inline std::uint32_t fingerprint(const Model& model) {
    std::uint32_t h=2166136261u;
    auto byte=[&](std::uint8_t x){h=(h^x)*16777619u;};
    auto number=[&](std::uint32_t x,unsigned count){for(unsigned k=0;k<count;++k)byte(static_cast<std::uint8_t>(x>>(k*8)));};
    for(const auto& m:model.materials) {
        for(unsigned char x:m.name)byte(x);number(static_cast<std::uint32_t>(m.vertices.size()),4);number(static_cast<std::uint32_t>(m.indices.size()),4);
        for(const auto& v:m.vertices)for(float x:{v.position.x,v.position.y,v.position.z})number(std::bit_cast<std::uint32_t>(x),4);
        for(auto i:m.indices)number(i,2);
    }
    return h;
}
class System {
    struct Node {Vertex target;float freedom{};};
    struct Edge {std::uint16_t a,b;};
    struct Record {std::uint16_t material,vertex;std::array<std::uint16_t,4> nodes;std::array<float,4> weights;float freedom;};
    struct Collider {std::uint16_t boneA,boneB;V3 a,b;float radius;};
    std::vector<Node> nodes_;
    std::vector<Edge> edges_;
    std::vector<Record> records_;
    std::vector<Collider> colliders_;
    std::array<std::vector<std::uint8_t>,kMaxMaterials> masks_;
    std::array<bool,kMaxMaterials> activeMaterials_{};
    std::array<V3,kMaxNodes> positions_{},previous_{};
    V3 lastRoot_{};
    bool ready_=false,started_=false;
    static V3 target(const Vertex& vertex,const std::array<Matrix,kBoneCount>& transforms) {
        V3 result{};for(unsigned k=0;k<4;++k)if(vertex.weights[k]>0)result+=point(transforms[vertex.bones[k]],vertex.position)*vertex.weights[k];return result;
    }
    std::array<Capsule,kMaxCapsules> capsules(const std::array<Matrix,kBoneCount>& world) const {
        std::array<Capsule,kMaxCapsules> result{};
        for(std::size_t i=0;i<colliders_.size();++i) {
            const auto& c=colliders_[i];
            // The native actor can be uniformly scaled in previews/cutscenes.
            const float scale=std::max({length(direction(world[c.boneA],{1,0,0})),length(direction(world[c.boneA],{0,1,0})),length(direction(world[c.boneA],{0,0,1}))});
            result[i]={point(world[c.boneA],c.a),point(world[c.boneB],c.b),c.radius*scale};
        }
        return result;
    }
public:
    bool initialize(const Model& model,const std::vector<std::uint8_t>& bytes,std::string& error) {
        ready_=false;reset();nodes_.clear();edges_.clear();records_.clear();colliders_.clear();activeMaterials_={};
        for(auto& mask:masks_)mask.clear();
        auto fail=[&](const char* why){error=why;return false;};
        Reader r{bytes.data(),bytes.size()};
        if(r.string(8)!="XICLOTH1"||r.u32()!=fingerprint(model))return fail("Xion cloth supplement does not match the runtime geometry.");
        const auto mc=r.u32(),nc=r.u32(),ec=r.u32(),rc=r.u32(),cc=r.u32();
        if(mc!=model.materials.size()||!nc||nc>kMaxNodes||ec>kMaxEdges||!rc||rc>kMaxRecords||!cc||cc>kMaxCapsules)return fail("Invalid Xion cloth resource counts.");
        for(std::size_t m=0;m<mc;++m) {
            if(r.u32()!=model.materials[m].vertices.size())return fail("Xion cloth vertex count mismatch.");
            masks_[m].resize(model.materials[m].vertices.size());
        }
        auto vector=[&](){return V3{r.f32(),r.f32(),r.f32()};};
        for(std::size_t i=0;i<nc;++i) {
            Node n;n.target.position=vector();for(auto& b:n.target.bones)b=r.u16();for(auto& w:n.target.weights)w=r.f32();n.freedom=r.f32();
            float sum=0;for(unsigned k=0;k<4;++k){if(n.target.bones[k]>=kBoneCount||!std::isfinite(n.target.weights[k])||n.target.weights[k]<0||n.target.weights[k]>1)return fail("Invalid cloth node weights.");sum+=n.target.weights[k];}
            if(!finite(n.target.position)||length(n.target.position)>1000||std::fabs(sum-1)>.001f||!std::isfinite(n.freedom)||n.freedom<0||n.freedom>1)return fail("Invalid cloth node.");
            nodes_.push_back(n);
        }
        for(std::size_t i=0;i<ec;++i){Edge e{r.u16(),r.u16()};if(e.a>=nc||e.b>=nc||e.a==e.b)return fail("Invalid cloth constraint.");edges_.push_back(e);}
        for(std::size_t i=0;i<rc;++i) {
            Record v;v.material=r.u16();v.vertex=r.u16();for(auto& n:v.nodes)n=r.u16();for(auto& w:v.weights)w=r.f32();v.freedom=r.f32();
            if(v.material>=mc||v.vertex>=masks_[v.material].size()||masks_[v.material][v.vertex])return fail("Invalid or duplicate cloth vertex.");
            float sum=0;for(unsigned k=0;k<4;++k){if(v.nodes[k]>=nc||!std::isfinite(v.weights[k])||v.weights[k]<0||v.weights[k]>1)return fail("Invalid cloth vertex weights.");sum+=v.weights[k];}
            if(std::fabs(sum-1)>.001f||!std::isfinite(v.freedom)||v.freedom<0||v.freedom>1)return fail("Invalid cloth vertex freedom.");
            masks_[v.material][v.vertex]=1;activeMaterials_[v.material]=true;records_.push_back(v);
        }
        for(std::size_t i=0;i<cc;++i){Collider c;c.boneA=r.u16();c.boneB=r.u16();c.a=vector();c.b=vector();c.radius=r.f32();if(c.boneA>=kBoneCount||c.boneB>=kBoneCount||!finite(c.a)||!finite(c.b)||length(c.a)>200||length(c.b)>200||!std::isfinite(c.radius)||c.radius<1||c.radius>25)return fail("Invalid cloth collider.");colliders_.push_back(c);}
        if(!r.good||r.offset!=r.size)return fail("Truncated or trailing Xion cloth payload.");
        ready_=true;error.clear();return true;
    }
    void reset(){started_=false;positions_={};previous_={};lastRoot_={};}
    Snapshot advance(const Model& model,const std::array<Matrix,kBoneCount>& world,float dt=1.f/30.f) {
        Snapshot result;if(!ready_)return result;
        for(const auto& matrix:world)if(!finite(matrix)){reset();return result;}
        std::array<Matrix,kBoneCount> transforms;for(std::size_t b=0;b<kBoneCount;++b)transforms[b]=multiply(world[b],model.inverseBind[b]);
        std::array<V3,kMaxNodes> targets;for(std::size_t i=0;i<nodes_.size();++i)targets[i]=target(nodes_[i].target,transforms);
        const V3 root=point(world[0],{});
        const bool restart=!started_||!std::isfinite(dt)||dt<=0||dt>.1f||length(root-lastRoot_)>180.f;
        lastRoot_=root;
        if(restart){positions_=previous_=targets;started_=true;dt=1.f/30.f;}
        const auto colliders=capsules(world);
        // Two short, damped steps. The guide is animated cloth, so motion stays
        // bounded even when a cinematic pose cannot be represented as cloth.
        const float h=std::clamp(dt,.001f,.05f)*.5f;
        for(unsigned step=0;step<2;++step) {
            for(std::size_t i=0;i<nodes_.size();++i) {
                const float free=nodes_[i].freedom;const V3 old=positions_[i];
                if(free<.001f){positions_[i]=previous_[i]=targets[i];continue;}
                positions_[i]+=limited((positions_[i]-previous_[i])*.80f,8.f)+V3{0,-180.f*h*h*free,0};
                positions_[i]+=(targets[i]-positions_[i])*(.28f+.3f*(1-free));previous_[i]=old;
            }
            for(unsigned iteration=0;iteration<4;++iteration) {
                for(const auto& e:edges_) {
                    V3 delta=positions_[e.b]-positions_[e.a];const float d=length(delta),rest=length(targets[e.b]-targets[e.a]);
                    const float a=nodes_[e.a].freedom,b=nodes_[e.b].freedom;
                    if(d>1.e-5f&&a+b>.001f){const V3 correction=delta*((d-rest)/d*.38f);positions_[e.a]+=correction*(a/(a+b));positions_[e.b]-=correction*(b/(a+b));}
                }
                for(std::size_t i=0;i<nodes_.size();++i) {
                    const float free=nodes_[i].freedom;
                    positions_[i]=targets[i]+limited(positions_[i]-targets[i],18.f*free);
                    if(free>.02f)for(std::size_t c=0;c<colliders_.size();++c)positions_[i]=mix(positions_[i],project(positions_[i],colliders[c],targets[i]-root),free);
                }
            }
        }
        result.count=static_cast<std::uint16_t>(nodes_.size());result.valid=true;
        for(std::size_t i=0;i<nodes_.size();++i){result.offsets[i]=limited(positions_[i]-targets[i],24.f);if(!finite(result.offsets[i])){reset();return {};}}
        return result;
    }
    void apply(const Model& model,const std::array<Matrix,kBoneCount>& world,const Snapshot& snapshot,std::array<std::vector<SkinnedVertex>,kMaxMaterials>& output) const {
        if(!ready_||!snapshot.valid||snapshot.count!=nodes_.size())return;
        for(std::size_t m=0;m<model.materials.size();++m)if(output[m].size()!=model.materials[m].vertices.size())return;
        for(const auto& matrix:world)if(!finite(matrix))return;
        const auto colliders=capsules(world);const V3 root=point(world[0],{});
        for(const auto& record:records_) {
            auto& vertex=output[record.material][record.vertex];V3 delta{};
            for(unsigned k=0;k<4;++k)delta+=snapshot.offsets[record.nodes[k]]*record.weights[k];
            const V3 base=vertex.position;vertex.position+=limited(delta,24.f)*record.freedom;
            // The coarse rig provides motion. Dense final projection handles
            // the shin and upper boot as well as the foot, including vertices
            // whose native foot weight is zero. It never moves the legs.
            if(record.freedom>.02f)for(unsigned pass=0;pass<3;++pass)for(std::size_t c=0;c<colliders_.size();++c)
                vertex.position=mix(vertex.position,project(vertex.position,colliders[c],base-root),record.freedom);
            if(!finite(vertex.position)||length(vertex.position-base)>60.f)vertex.position=base;
            vertex.normal=vertex.normal*.001f;
        }
        // Rebuild only masked normals. Shared original cloth controls keep the
        // zipper on its panel; ordinary body/face normals remain unchanged.
        for(std::size_t m=0;m<model.materials.size();++m)if(activeMaterials_[m]) {
            const auto& indices=model.materials[m].indices;auto& out=output[m];const auto& mask=masks_[m];
            for(std::size_t i=0;i<indices.size();i+=3){auto a=indices[i],b=indices[i+1],c=indices[i+2];if(!(mask[a]||mask[b]||mask[c]))continue;const V3 n=cross(out[b].position-out[a].position,out[c].position-out[a].position);if(mask[a])out[a].normal+=n;if(mask[b])out[b].normal+=n;if(mask[c])out[c].normal+=n;}
        }
        for(const auto& record:records_)output[record.material][record.vertex].normal=unit(output[record.material][record.vertex].normal);
    }
    std::size_t nodeCount()const{return nodes_.size();}
    std::size_t vertexCount()const{return records_.size();}
};
} // namespace kingdom::xion::cloth
