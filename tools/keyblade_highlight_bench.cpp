// Standalone C++20 benchmark: keyblade_highlight_bench <kingdom_key.mesh>
// Uses original mesh vertices and seven synthetic native-style lights.
#include "../src/xion_model.hpp"
#include "../src/keyblade_highlight.hpp"
#include <chrono>
#include <cassert>
#include <fstream>
#include <iostream>
#include <unordered_set>
using namespace kingdom;using namespace kingdom::xion;
struct KeyVertex {V3 position,normal;float color[4],metallic,roughness;};
static_assert(sizeof(KeyVertex)==48);
using Cached=kingdom::highlight::Material;
kingdom::highlight::MaterialCache pool;
kingdom::highlight::Lights preparedLights;
struct Part {V3 pivot;std::vector<KeyVertex> vertices;std::vector<std::size_t> cached;};
struct Light {V3 position,direction,cosAtten,distAtten;std::uint8_t color[3];};
struct ActiveLight {V3 position,direction,cosAtten,distAtten;float color[3];};
template<unsigned Candidate> V3 specCalc(const KeyVertex& x,const Cached& cached,const Matrix& view,const std::array<Light,7>& lights,const std::vector<ActiveLight>& active){
    V3 n=unit(direction(view,x.normal)),p=point(view,x.position);
    if constexpr(Candidate==3)return kingdom::highlight::evaluate(n,p,cached,preparedLights);
    V3 eye=unit(p*-1.f);
    float rough=std::clamp(x.roughness,.12f,1.f);
    float exponent=Candidate?cached.exponent:std::clamp(2.f/(rough*rough)-2.f,2.f,128.f);
    float rgb[3]={};
    const unsigned count=Candidate?static_cast<unsigned>(active.size()):7;
    for(unsigned i=0;i<count;++i){
        if constexpr(!Candidate)if(!(lights[i].color[0]||lights[i].color[1]||lights[i].color[2]))continue;
        const V3 lightPosition=Candidate?active[i].position:lights[i].position;
        const V3 lightDirection=Candidate?active[i].direction:lights[i].direction;
        const V3 ca=Candidate?active[i].cosAtten:lights[i].cosAtten;
        const V3 da=Candidate?active[i].distAtten:lights[i].distAtten;
        V3 delta=lightPosition-p;float distance=length(delta);V3 l=unit(delta);
        float diffuse=std::max(0.f,dot(n,l));if(diffuse==0)continue;
        V3 half=unit(l+eye);float cosine=std::max(0.f,dot(l,lightDirection));
        float cosAttenuation=ca.x+ca.y*cosine+ca.z*cosine*cosine;
        float distAttenuation=da.x+da.y*distance+da.z*distance*distance;
        float attenuation=std::max(0.f,cosAttenuation/std::max(.000001f,distAttenuation));
        float shine=(Candidate==3?kingdom::highlight::specular(std::max(0.f,dot(n,half)),cached):std::pow(std::max(0.f,dot(n,half)),exponent))*diffuse*attenuation;
        float f=1.f-std::max(0.f,dot(eye,half));
        float fresnel;
        if constexpr(Candidate>=2){float f2=f*f;fresnel=(f2*f2)*f;}else fresnel=std::pow(f,5.f);
        for(int c=0;c<3;++c){
            float f0=Candidate?cached.f0[c]:.04f*(1-x.metallic)+x.color[c]*x.metallic;
            float oneMinus=Candidate?cached.oneMinusF0[c]:1-f0;
            float lc=Candidate?active[i].color[c]:float(lights[i].color[c]);
            rgb[c]+=shine*(f0+oneMinus*fresnel)*lc*.8f;
        }
    }
    return {rgb[0],rgb[1],rgb[2]};
}
std::uint8_t byte(float x){return static_cast<std::uint8_t>(std::clamp(x,0.f,255.f));}
int main(int argc,char** argv){
    if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(f),{}};Reader r{bytes.data(),bytes.size()};
    if(r.string(8)!="KKMESH02")return 2;const auto count=r.u32();r.offset+=12;std::vector<Part> parts;std::size_t vertices=0;
    for(unsigned p=0;p<count;++p){const auto name=r.string(r.u32());Part part;part.pivot={r.f32(),r.f32(),r.f32()};r.offset+=12;auto n=r.u32();std::unordered_set<std::string> unique;
        for(unsigned i=0;i<n;++i){KeyVertex v;std::memcpy(&v,bytes.data()+r.offset,sizeof(v));r.offset+=sizeof(v);if(unique.emplace(reinterpret_cast<const char*>(&v),sizeof(v)).second){part.vertices.push_back(v);part.cached.push_back(pool.intern(v.color,v.metallic,v.roughness));}}
        vertices+=part.vertices.size();parts.push_back(std::move(part));
    }
    if(!r.good||r.offset!=bytes.size())return 3;
    std::array<Light,7> lights;
    for(unsigned i=0;i<7;++i){const float angle=i*.8975979f;auto& l=lights[i];l.position={std::cos(angle)*250,120+float(i)*27,std::sin(angle)*160-40};l.direction=unit(l.position-V3{0,0,-180});l.cosAtten={.75f,.2f,.05f};l.distAtten={1,.0002f,.0000002f};l.color[0]=std::uint8_t(200-i*9);l.color[1]=std::uint8_t(190-i*7);l.color[2]=std::uint8_t(180+i*8);}
    std::vector<ActiveLight> active;for(const auto& l:lights)if(l.color[0]||l.color[1]||l.color[2])active.push_back({l.position,l.direction,l.cosAtten,l.distAtten,{float(l.color[0]),float(l.color[1]),float(l.color[2])}});
    for(const auto& l:lights)preparedLights.add(l.position,l.direction,l.cosAtten,l.distAtten,{l.color[0],l.color[1],l.color[2]});
    assert(pool.size()==6&&preparedLights.count==7);
    for(std::size_t i=0;i<pool.size();++i){const auto& m=pool[i];assert(m.specular.size()==4097);assert(kingdom::highlight::specular(0,m)==0&&kingdom::highlight::specular(1,m)==1);for(float x:{1.0001f,1.5f})assert(kingdom::highlight::specular(x,m)==std::pow(x,m.exponent));const float a=kingdom::highlight::specular(-.25f,m),b=std::pow(-.25f,m.exponent);assert((std::isnan(a)&&std::isnan(b))||a==b);}
    std::vector<Matrix> views;for(unsigned frame=0;frame<16;++frame){const float a=frame*.39269908f;Matrix m=identity();m.m[0][0]=std::cos(a);m.m[0][2]=std::sin(a);m.m[2][0]=-std::sin(a);m.m[2][2]=std::cos(a);m.m[2][3]=-180;views.push_back(m);}
    std::size_t exactMismatch[3]={},byteMismatch[3]={},samples=0;float maximum[3]={};unsigned maxByteError[3]={};
    for(const auto& view:views)for(const auto& part:parts){Matrix v=view;const auto offset=direction(view,part.pivot);v.m[0][3]+=offset.x;v.m[1][3]+=offset.y;v.m[2][3]+=offset.z;
        for(std::size_t i=0;i<part.vertices.size();++i){const auto& x=part.vertices[i];const auto& c=pool[part.cached[i]];V3 values[4]={specCalc<0>(x,c,v,lights,active),specCalc<1>(x,c,v,lights,active),specCalc<2>(x,c,v,lights,active),specCalc<3>(x,c,v,lights,active)};
            for(unsigned option=0;option<3;++option){if(std::memcmp(&values[0],&values[option+1],sizeof(V3)))++exactMismatch[option];const float* a=&values[0].x;const float* b=&values[option+1].x;for(unsigned k=0;k<3;++k){maximum[option]=std::max(maximum[option],std::fabs(a[k]-b[k]));const unsigned byteError=unsigned(std::abs(int(byte(a[k]))-int(byte(b[k]))));maxByteError[option]=std::max(maxByteError[option],byteError);if(byteError)++byteMismatch[option];}}++samples;
        }
    }
    assert(exactMismatch[0]==0&&maxByteError[2]<=1);
    double timings[4]={};volatile float sink=0;
    for(unsigned repeat=0;repeat<6;++repeat)for(unsigned phase=0;phase<4;++phase){const unsigned option=(phase+repeat)%4;auto start=std::chrono::steady_clock::now();float total=0;
        for(const auto& view:views)for(const auto& part:parts){Matrix v=view;const auto offset=direction(view,part.pivot);v.m[0][3]+=offset.x;v.m[1][3]+=offset.y;v.m[2][3]+=offset.z;
            for(std::size_t i=0;i<part.vertices.size();++i){V3 result;switch(option){case 0:result=specCalc<0>(part.vertices[i],pool[part.cached[i]],v,lights,active);break;case 1:result=specCalc<1>(part.vertices[i],pool[part.cached[i]],v,lights,active);break;case 2:result=specCalc<2>(part.vertices[i],pool[part.cached[i]],v,lights,active);break;default:result=specCalc<3>(part.vertices[i],pool[part.cached[i]],v,lights,active);break;}total+=result.x+result.y+result.z;}}
        timings[option]+=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();sink=total;
    }
    std::cout<<"parts "<<parts.size()<<", unique vertices "<<vertices<<", active lights "<<active.size()<<", validation samples "<<samples<<"\n";
    for(unsigned i=0;i<4;++i)std::cout<<"variant "<<i<<" mean highlight milliseconds per mesh/view "<<timings[i]/96*1000<<"\n";
    for(unsigned i=0;i<3;++i)std::cout<<"candidate "<<i+1<<" float mismatch vertices "<<exactMismatch[i]<<", max RGB delta "<<maximum[i]<<", quantized color channel mismatches "<<byteMismatch[i]<<", max final byte error "<<maxByteError[i]<<"\n";
}
