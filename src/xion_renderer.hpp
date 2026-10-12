#pragma once
// Cosmetic player rendering only. Native Link models, joints, animation and
// equipment state remain owned and updated by the game.
#include "xion_model.hpp"
#include "xion_compact.hpp"
#include "xion_arrays.hpp"
#include "xion_packet_leases.hpp"
#include "xion_pose_cache.hpp"
#include "xion_cloth.hpp"
#include "mods/svc/resource.h"
#include "mods/svc/interp.h"
#include "mods/svc/hook.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_mirror.h"
#include "local_player.hpp"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "SSystem/SComponent/c_counter.h"
#include "JSystem/J3DGraphBase/J3DPacket.h"
#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"
#include "JSystem/J3DGraphBase/J3DShape.h"
#include <dolphin/gx/GXAurora.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>

namespace kingdom::xion {
inline Matrix fromMtx(const Mtx m) {Matrix out;std::memcpy(out.m,m,sizeof(Mtx));return out;}
inline u8 byte(float x) {return static_cast<u8>(std::clamp(x,0.0f,255.0f));}
inline s8 normalByte(float x) {return static_cast<s8>(std::clamp(std::lround(x*64.0f),-64l,64l));}
inline void loadLight(const J3DLightInfo& info,unsigned index) {
    GXLightObj light;
    GXInitLightPos(&light,info.mLightPosition.x,info.mLightPosition.y,info.mLightPosition.z);
    GXInitLightDir(&light,info.mLightDirection.x,info.mLightDirection.y,info.mLightDirection.z);
    GXInitLightColor(&light,info.mColor);
    GXInitLightAttn(&light,info.mCosAtten.x,info.mCosAtten.y,info.mCosAtten.z,
                   info.mDistAtten.x,info.mDistAtten.y,info.mDistAtten.z);
    GXLoadLightObjImm(&light,static_cast<GXLightID>(1u<<index));
}
class System {
    struct Texture {std::vector<u8> pixels;GXTexObj object{};bool initialized=false;};
    struct MeshMasks {std::array<std::vector<u16>,4> indices;};
    struct Packet {
        System* owner{};
        std::array<Matrix,kBoneCount> joints{};
        std::array<const void*,kBoneCount> keys{};
        std::array<J3DModel*,8> bodyModels{};
        dKy_tevstr_c lighting{};
        Matrix lightViewInverse{},baseMatrix{};
        cloth::Snapshot clothPose{};
        daAlink_c* actor{};
        bool valid=false,preview=false,interpolate=false,hideHead=false,hideFeet=false;
        // Scratch owns skinned vertices; rendering does not alter actor/simulation state.
        std::array<std::vector<SkinnedVertex>,kMaxMaterials> skinned;
        std::array<arrays::PoseData,kMaxMaterials> vertexArrays;
        bool arraysPending=false;
        PoseCache poseCache;

        bool matches(J3DModel* model) const {
            return model&&std::find(bodyModels.begin(),bodyModels.end(),model)!=bodyModels.end();
        }
        V3 compactOrigin() const {return {baseMatrix.m[0][3],baseMatrix.m[1][3],baseMatrix.m[2][3]};}
        void finishArrayReads() {
            // GXSetArray queues CPU pointers; the FIFO worker copies their
            // contents later. Drain it before rewriting this packet's arrays.
            if(arraysPending){AuroraGXSync();arraysPending=false;}
        }
        void bindArrays(std::size_t m,bool normal,bool texture,bool color) {
            const auto& pose=vertexArrays[m];const auto& material=owner->arrayMaterials_[m];
            GXSetArray(GX_VA_POS,pose.positionData(),static_cast<u32>(pose.positionBytes()),pose.positionStride(),arrays::kLittleEndian);
            if(normal)GXSetArray(GX_VA_NRM,pose.normals.data(),static_cast<u32>(pose.normals.size()*sizeof(arrays::Normal)),sizeof(arrays::Normal),arrays::kLittleEndian);
            if(texture)GXSetArray(GX_VA_TEX0,material.uvData(),static_cast<u32>(material.uvBytes()),material.uvStride(),arrays::kLittleEndian);
            if(color)GXSetArray(GX_VA_CLR0,material.colors.data(),static_cast<u32>(material.colors.size()*sizeof(arrays::Color)),sizeof(arrays::Color),arrays::kLittleEndian);
        }
        bool prepareSkin() {
            if(!valid||!owner||!owner->ready_)return false;
            std::array<Matrix,kBoneCount> world=joints,transforms,normals;
            for(std::size_t j=0;j<kBoneCount;++j) {
                Matrix replacement;
                if(interpolate&&keys[j]&&owner->interp_&&owner->interp_->lookup_replacement_mtx(keys[j],replacement.m)&&finite(replacement))world[j]=replacement;
            }
            const V3 origin=compactOrigin();
            if(poseCache.matches(world,origin))return true;
            // A cache hit never writes the arrays and needs no FIFO wait.
            // On a miss, invalidate BEFORE partial writes can occur.
            poseCache.invalidate();
            finishArrayReads();
            for(std::size_t j=0;j<kBoneCount;++j) {
                transforms[j]=multiply(world[j],owner->model_.inverseBind[j]);
                if(!normalTransform(transforms[j],normals[j])) {
                    // A discontinuity/degenerate interpolated scale must not
                    // blank an actor after its original packet was suppressed.
                    transforms[j]=multiply(joints[j],owner->model_.inverseBind[j]);
                    if(!normalTransform(transforms[j],normals[j]))return false;
                }
            }
            for(std::size_t m=0;m<owner->model_.materials.size();++m) {
                const auto& vertices=owner->model_.materials[m].vertices;
                auto& out=skinned[m];
                // Sizes are fixed during initialize, before any original model is hidden.
                if(out.size()!=vertices.size())return false;
                for(std::size_t v=0;v<vertices.size();++v)out[v]=skin(vertices[v],transforms,normals);
            }
            if(!preview&&owner->clothReady_)owner->cloth_.apply(owner->model_,world,clothPose,skinned);
            for(std::size_t m=0;m<owner->model_.materials.size();++m)
                if(!vertexArrays[m].update(skinned[m],origin))return false;
            poseCache.commit(world,origin);
            return true;
        }
        void lightingInView(const Mtx view) {
            const Matrix drawView=fromMtx(view);
            const Matrix conversion=multiply(drawView,lightViewInverse);
            J3DLightInfo first=lighting.mLightObj.mInfo;
            const auto position=point(drawView,{lighting.field_0x32c.x,lighting.field_0x32c.y,lighting.field_0x32c.z});
            first.mLightPosition={position.x,position.y,position.z};
            dKy_setLight_again();loadLight(first,0);
            for(unsigned i=0;i<6;++i) {
                auto light=lighting.mLights[i].mInfo;
                const auto p=point(conversion,{light.mLightPosition.x,light.mLightPosition.y,light.mLightPosition.z});
                const auto d=direction(conversion,{light.mLightDirection.x,light.mLightDirection.y,light.mLightDirection.z});
                light.mLightPosition={p.x,p.y,p.z};light.mLightDirection={d.x,d.y,d.z};loadLight(light,i+2);
            }
            GXSetChanAmbColor(GX_COLOR0A0,{byte(lighting.AmbCol.r),byte(lighting.AmbCol.g),byte(lighting.AmbCol.b),255});
            dKy_GxFog_tevstr_set(&lighting);
        }
        void drawInView(const Mtx view,bool transparent,bool reflection=false) {
            if(!prepareSkin())return;
            j3dSys.reinitGX();J3DShape::resetVcdVatCache();
            GXInvalidateVtxCache(); // Contents changed even if array addresses did not.
            GXClearVtxDesc();GXSetVtxDesc(GX_VA_POS,GX_INDEX16);GXSetVtxDesc(GX_VA_NRM,GX_INDEX16);
            GXSetVtxDesc(GX_VA_CLR0,GX_NONE);GXSetVtxDesc(GX_VA_TEX0,GX_INDEX16);
            GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
            // Indexed attributes live in Aurora's separate storage arena;
            // the vertex arena receives only 6 bytes of indices per vertex
            // when a material has uniform color (8 for varying vertex colors).
            GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_NRM,GX_NRM_XYZ,GX_S8,6);
            GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
            GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
            GXSetNumChans(1);GXSetChanCtrl(GX_ALPHA0,GX_DISABLE,GX_SRC_REG,GX_SRC_VTX,GX_LIGHT_NULL,GX_DF_NONE,GX_AF_NONE);
            lightingInView(view);
            GXSetNumTexGens(1);GXSetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
            GXSetNumIndStages(0);GXSetNumTevStages(1);GXSetTevDirect(GX_TEVSTAGE0);
            GXSetTevSwapMode(GX_TEVSTAGE0,GX_TEV_SWAP0,GX_TEV_SWAP0);
            GXSetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLOR0A0);GXSetTevOp(GX_TEVSTAGE0,GX_MODULATE);
            GXSetColorUpdate(GX_TRUE);GXSetAlphaUpdate(GX_TRUE);GXSetCoPlanar(GX_FALSE);GXSetClipMode(GX_CLIP_ENABLE);
            GXSetCurrentMtx(GX_PNMTX0);GXLoadPosMtxImm(view,GX_PNMTX0);GXLoadNrmMtxImm(view,GX_PNMTX0);
            // Relative S16 positions keep large map coordinates out of the
            // fixed-point range. Translation belongs only in the position
            // matrix; world-space normals and lighting retain the original view.
            const V3 origin=compactOrigin();
            const Matrix localView=compact::translatedView(fromMtx(view),origin);
            const unsigned mask=(hideHead&&!reflection?1u:0u)|(hideFeet?2u:0u);
            for(std::size_t m=0;m<owner->model_.materials.size();++m) {
                const auto& material=owner->model_.materials[m];
                if(!owner->drawable_[m])continue;
                if(bool(material.flags&AlphaBlend)!=transparent)continue;
                const auto& indices=owner->masks_[m].indices[mask];if(indices.empty())continue;
                const auto& pose=vertexArrays[m];const auto& attributes=owner->arrayMaterials_[m];
                GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,pose.positionPacked?GX_S16:GX_F32,pose.positionPacked?compact::kPositionFraction:0);
                GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,attributes.uvPacked?GX_S16:GX_F32,attributes.uvPacked?compact::kUvFraction:0);
                GXLoadPosMtxImm(pose.positionPacked?localView.m:view,GX_PNMTX0);
                GXSetVtxDesc(GX_VA_CLR0,attributes.constantColor?GX_NONE:GX_INDEX16);
                if(attributes.constantColor) {
                    const auto& c=attributes.colors[0];GXSetChanMatColor(GX_COLOR0A0,{c[0],c[1],c[2],c[3]});
                }
                bindArrays(m,true,true,!attributes.constantColor);
                const bool unlit=material.flags&Unlit;
                GXSetChanCtrl(GX_COLOR0,unlit?GX_DISABLE:GX_ENABLE,GX_SRC_REG,attributes.constantColor?GX_SRC_REG:GX_SRC_VTX,0xff,GX_DF_CLAMP,GX_AF_SPOT);
                GXSetChanCtrl(GX_ALPHA0,GX_DISABLE,GX_SRC_REG,attributes.constantColor?GX_SRC_REG:GX_SRC_VTX,GX_LIGHT_NULL,GX_DF_NONE,GX_AF_NONE);
                GXSetTevColorOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,unlit?GX_CS_SCALE_1:GX_CS_SCALE_2,GX_TRUE,GX_TEVPREV);
                GXSetTevAlphaOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
                GXSetZMode(GX_ENABLE,GX_LEQUAL,transparent?GX_DISABLE:GX_ENABLE);
                GXSetZCompLoc(material.flags&AlphaTest?GX_FALSE:GX_TRUE);
                GXSetBlendMode(transparent?GX_BM_BLEND:GX_BM_NONE,GX_BL_SRCALPHA,GX_BL_INVSRCALPHA,GX_LO_COPY);
                GXSetAlphaCompare(material.flags&AlphaTest?GX_GEQUAL:GX_ALWAYS,byte(material.alphaCutoff*255),GX_AOP_AND,GX_ALWAYS,0);
                // XIONM001 keeps outward CCW editor triangles; Aurora's GX
                // pipeline treats CW as front and keeps indexed order unchanged.
                GXSetCullMode(material.flags&TwoSided?GX_CULL_NONE:(reflection?GX_CULL_BACK:GX_CULL_FRONT));
                GXLoadTexObj(&owner->textures_[m].object,GX_TEXMAP0);
                arraysPending=true;
                GXBeginIndexed(GX_VTXFMT0,static_cast<u16>(material.vertices.size()),indices.data(),static_cast<u32>(indices.size()));
                for(std::size_t v=0;v<material.vertices.size();++v) {
                    GXPosition1x16(static_cast<u16>(v));GXNormal1x16(static_cast<u16>(v));
                    if(!attributes.constantColor)GXColor1x16(static_cast<u16>(v));
                    GXTexCoord1x16(static_cast<u16>(v));
                }
                GXEnd();
            }
            J3DShape::resetVcdVatCache();
        }
        void silhouette(const Mtx view) {
            if(!prepareSkin())return;
            GXCullMode oldCull;GXGetCullMode(&oldCull);J3DShape::resetVcdVatCache();
            GXInvalidateVtxCache();
            GXClearVtxDesc();GXSetVtxDesc(GX_VA_POS,GX_INDEX16);
            GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
            GXSetNumChans(0);GXSetNumTexGens(0);GXSetNumIndStages(0);GXSetNumTevStages(1);
            GXSetTevDirect(GX_TEVSTAGE0);GXSetTevSwapMode(GX_TEVSTAGE0,GX_TEV_SWAP0,GX_TEV_SWAP0);
            GXSetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD_NULL,GX_TEXMAP_NULL,GX_COLOR_NULL);
            // The real-shadow pass owns projection, channel write mask,
            // additive blend and TEVREG0; leave those native settings intact.
            GXSetTevColorIn(GX_TEVSTAGE0,GX_CC_ZERO,GX_CC_ZERO,GX_CC_ZERO,GX_CC_C0);
            GXSetTevColorOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
            GXSetTevAlphaIn(GX_TEVSTAGE0,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_A0);
            GXSetTevAlphaOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
            GXSetCurrentMtx(GX_PNMTX0);GXLoadPosMtxImm(view,GX_PNMTX0);GXSetCullMode(GX_CULL_NONE);
            const V3 origin=compactOrigin();
            const Matrix localView=compact::translatedView(fromMtx(view),origin);
            GXSetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
            // First-person head hiding applies to the camera, not the shadow.
            const unsigned mask=hideFeet?2u:0u;
            for(std::size_t m=0;m<owner->model_.materials.size();++m) {
                const auto& material=owner->model_.materials[m];const auto& indices=owner->masks_[m].indices[mask];
                if(indices.empty()||(material.flags&AlphaBlend))continue;
                const bool cutout=material.flags&AlphaTest;
                const auto& pose=vertexArrays[m];const auto& attributes=owner->arrayMaterials_[m];
                GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,pose.positionPacked?GX_S16:GX_F32,pose.positionPacked?compact::kPositionFraction:0);
                GXLoadPosMtxImm(pose.positionPacked?localView.m:view,GX_PNMTX0);
                GXSetVtxDesc(GX_VA_CLR0,cutout&&!attributes.constantColor?GX_INDEX16:GX_NONE);
                GXSetVtxDesc(GX_VA_TEX0,cutout?GX_INDEX16:GX_NONE);
                bindArrays(m,false,cutout,cutout&&!attributes.constantColor);
                GXSetNumChans(cutout?1:0);GXSetNumTexGens(cutout?1:0);
                if(cutout) {
                    GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
                    GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,attributes.uvPacked?GX_S16:GX_F32,attributes.uvPacked?compact::kUvFraction:0);
                    if(attributes.constantColor)GXSetChanMatColor(GX_COLOR0A0,{255,255,255,attributes.colors[0][3]});
                    GXSetChanCtrl(GX_ALPHA0,GX_DISABLE,GX_SRC_REG,attributes.constantColor?GX_SRC_REG:GX_SRC_VTX,GX_LIGHT_NULL,GX_DF_NONE,GX_AF_NONE);
                    GXSetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
                    GXLoadTexObj(&owner->textures_[m].object,GX_TEXMAP0);
                    GXSetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLOR0A0);
                    GXSetTevAlphaIn(GX_TEVSTAGE0,GX_CA_ZERO,GX_CA_TEXA,GX_CA_RASA,GX_CA_ZERO);
                    GXSetAlphaCompare(GX_GEQUAL,byte(material.alphaCutoff*255),GX_AOP_AND,GX_ALWAYS,0);
                    // Native RGB shadow channels disable alpha writes; the
                    // fourth channel enables only alpha. Keeping C0 untouched
                    // while using the cutout alpha respects both arrangements.
                } else {
                    GXSetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD_NULL,GX_TEXMAP_NULL,GX_COLOR_NULL);
                    GXSetTevAlphaIn(GX_TEVSTAGE0,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_A0);
                    GXSetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
                }
                arraysPending=true;
                GXBeginIndexed(GX_VTXFMT0,static_cast<u16>(material.vertices.size()),indices.data(),static_cast<u32>(indices.size()));
                for(std::size_t v=0;v<material.vertices.size();++v) {
                    GXPosition1x16(static_cast<u16>(v));
                    if(cutout) {
                        if(!attributes.constantColor)GXColor1x16(static_cast<u16>(v));
                        GXTexCoord1x16(static_cast<u16>(v));
                    }
                }
                GXEnd();
            }
            // The next native caster does not reload this shared material.
            GXSetVtxDesc(GX_VA_CLR0,GX_NONE);GXSetVtxDesc(GX_VA_TEX0,GX_NONE);
            GXSetVtxDesc(GX_VA_POS,GX_DIRECT);
            // Subsequent native casters use F32 positions in the native view.
            GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
            GXLoadPosMtxImm(view,GX_PNMTX0);
            GXSetNumChans(0);GXSetNumTexGens(0);
            GXSetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD_NULL,GX_TEXMAP_NULL,GX_COLOR_NULL);
            GXSetTevAlphaIn(GX_TEVSTAGE0,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_A0);
            GXSetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
            GXSetCullMode(oldCull);J3DShape::resetVcdVatCache();
        }
    };
    struct DrawPass final:J3DPacket {
        Packet* packet{};bool transparent{};
        void draw() override {if(packet)packet->drawInView(j3dSys.getViewMtx(),transparent);}
    };
    struct Slot {
        Packet packet;DrawPass opaque,translucent;
        Slot(){opaque.packet=&packet;translucent.packet=&packet;translucent.transparent=true;}
    };
    Model model_;
    std::array<arrays::MaterialData,kMaxMaterials> arrayMaterials_;
    cloth::System cloth_;
    cloth::Snapshot clothPose_{};
    daAlink_c* clothOwner_{};
    J3DModel* clothBody_{};
    u32 clothTick_{};
    bool clothReady_=false,clothTickValid_=false;
    std::string clothWarning_;
    std::array<Texture,kMaxMaterials> textures_;
    std::array<MeshMasks,kMaxMaterials> masks_;
    std::array<bool,kMaxMaterials> drawable_{};
    std::array<Slot,8> slots_;
    const ResourceService* resource_{};ModContext* context_{};const InterpService* interp_{};
    Packet* current_{};Packet* gameplay_{};
    PacketLeases<8> leases_;
    bool ready_=false,enabled_=false,hasTransparent_=false,attempted_=false;

    void resetCloth() {
        cloth_.reset();clothPose_={};clothTickValid_=false;
        clothOwner_=nullptr;clothBody_=nullptr;
    }

    bool read(const std::string& path,std::vector<u8>& bytes,std::string& error) {
        ResourceBuffer buffer=RESOURCE_BUFFER_INIT;
        if(resource_->load(context_,path.c_str(),&buffer)!=MOD_OK){error="Missing Xion resource: "+path;return false;}
        struct Release{const ResourceService* service;ModContext* context;ResourceBuffer* buffer;~Release(){service->free(context,buffer);}} release{resource_,context_,&buffer};
        if(!buffer.data||!buffer.size||buffer.size>64*1024*1024){error="Invalid Xion resource size: "+path;return false;}
        bytes.assign(static_cast<const u8*>(buffer.data),static_cast<const u8*>(buffer.data)+buffer.size);return true;
    }
    bool compatible(daAlink_c* link) const {
        if(!enabled_||!ready_||!local_player::is(link)||link->checkWolf()||link->mClothesChangeWaitTimer||!link->mpLinkModel||!link->mpLinkFaceModel)return false;
        auto* data=link->mpLinkModel->getModelData();auto* face=link->mpLinkFaceModel->getModelData();
        if(!data||data->getJointNum()!=kBodyBones||!face||face->getJointNum()!=kFaceBones)return false;
        // Outfit reloads and other model mods must not silently change the
        // animation palette assumed by this retargeted mesh.
        static constexpr const char* names[kBodyBones]={"center","backbone1","backbone2","neck","head","pod","shoulderL","armL1","armL2","handL","weaponL","shoulderR","armR1","armR2","handR","weaponR","waist","clotchL","legL1","legL2","footL","toeL","clotchR","legR1","legR2","footR","toeR","fskirtL1","fskirtL2","fskirtR1","fskirtR2","rskirtL1","rskirtL2","rskirtR1","rskirtR2"};
        auto* table=data->getJointName();if(!table)return false;
        for(std::size_t i=0;i<kBodyBones;++i){const char* actual=table->getName(static_cast<u16>(i));if(!actual||std::strcmp(actual,names[i]))return false;}
        return true;
    }
    bool capture(Packet& packet,daAlink_c* link,bool preview) {
        packet.poseCache.invalidate();
        packet.valid=false;packet.owner=this;packet.actor=link;packet.preview=preview;packet.interpolate=!preview;
        for(std::size_t j=0;j<kBoneCount;++j) {
            MtxP matrix=j<kBodyBones?link->mpLinkModel->getAnmMtx(static_cast<int>(j)):link->mpLinkFaceModel->getAnmMtx(static_cast<int>(j-kBodyBones));
            packet.joints[j]=fromMtx(matrix);packet.keys[j]=matrix;
            Matrix inverseCheck;if(!finite(packet.joints[j])||!inverse(packet.joints[j],inverseCheck))return false;
        }
        packet.bodyModels={link->mpLinkModel,link->mpLinkHandModel,link->mpLinkHatModel,link->mpLinkFaceModel,
                           link->mpDemoFCBlendModel,link->mpDemoFCTongueModel,link->mpDemoHLTmpModel,link->mpDemoHRTmpModel};
        packet.baseMatrix=fromMtx(link->mpLinkModel->getBaseTRMtx());
        packet.clothPose={};
        // Actor drawing also runs during interpolated presentation. Only copy
        // the execute-post snapshot here; repeated render passes never step it.
        if(!preview&&clothReady_&&clothTickValid_&&clothOwner_==link&&clothBody_==link->mpLinkModel)
            packet.clothPose=clothPose_;
        packet.hideHead=!preview&&dComIfGp_checkCameraAttentionStatus(link->field_0x317c,0x20);
        packet.hideFeet=link->checkEquipHeavyBoots();
        g_env_light.setLightTevColorType_MAJI(link->mpLinkModel,&link->tevStr);
        packet.lighting=link->tevStr;
        if(!inverse(fromMtx(j3dSys.getViewMtx()),packet.lightViewInverse))packet.lightViewInverse=identity();
        packet.valid=true;return true;
    }
public:
    System()=default;System(const System&)=delete;System& operator=(const System&)=delete;
    bool initialize(const ResourceService* resources,ModContext* context,const InterpService* interpolation,std::string& error) {
        if(ready_)return true;
        if(attempted_){error="Reload the mod before reinitializing Xion resources.";return false;}
        if(!resources||!context||!interpolation){error="Xion requires resource and interpolation services.";return false;}
        attempted_=true;
        resource_=resources;context_=context;interp_=interpolation;
        std::vector<u8> bytes;if(!read("characters/xion/xion.mesh",bytes,error)||!decodeModel(bytes,model_,error))return false;
        std::size_t textureBytes=0;
        for(std::size_t m=0;m<model_.materials.size();++m) {
            const auto& material=model_.materials[m];RgbaImage image;
            if(!read(material.texturePath,bytes,error)||!decodeImage(bytes,image,error)){shutdown();return false;}
            drawable_[m]=!invisibleBlendedMaterial(material,image);
            const std::size_t tiledSize=std::size_t((image.width+3)/4)*((image.height+3)/4)*64;
            if(textureBytes+tiledSize>64*1024*1024){error="Xion textures exceed the 64 MiB memory budget.";shutdown();return false;}
            textureBytes+=tiledSize;
            auto& texture=textures_[m];texture.pixels=tileRgba(image);
            GXInitTexObj(&texture.object,texture.pixels.data(),static_cast<u16>(image.width),static_cast<u16>(image.height),GX_TF_RGBA8,GX_REPEAT,GX_REPEAT,GX_FALSE);
            GXInitTexObjLOD(&texture.object,GX_LINEAR,GX_LINEAR,0,0,0,GX_FALSE,GX_FALSE,GX_ANISO_1);texture.initialized=true;
            hasTransparent_|=drawable_[m]&&bool(material.flags&AlphaBlend);
            for(std::size_t i=0;i<material.indices.size();i+=3) {
                bool head=false,feet=false;
                for(unsigned k=0;k<3;++k) {
                    const auto& vertex=material.vertices[material.indices[i+k]];float headWeight=0,feetWeight=0;
                    for(unsigned w=0;w<4;++w) {
                        const auto bone=vertex.bones[w];
                        if(bone==4||bone>=kBodyBones)headWeight+=vertex.weights[w];
                        if(bone==20||bone==21||bone==25||bone==26)feetWeight+=vertex.weights[w];
                    }
                    head|=headWeight>0.3f;feet|=feetWeight>0.5f;
                }
                for(unsigned mask=0;mask<4;++mask)if(!((mask&1)&&head)&&!((mask&2)&&feet))
                    masks_[m].indices[mask].insert(masks_[m].indices[mask].end(),material.indices.begin()+i,material.indices.begin()+i+3);
            }
            arrayMaterials_[m].initialize(material);
            for(auto& slot:slots_) {
                slot.packet.skinned[m].resize(material.vertices.size());
                slot.packet.vertexArrays[m].initialize(material.vertices.size());
            }
        }
        // A missing/stale optional coat map retains the original skinned Xion.
        std::string clothError;
        try {
            clothReady_=read("characters/xion/xion.cloth",bytes,clothError)&&cloth_.initialize(model_,bytes,clothError);
        } catch(...) {clothReady_=false;}
        if(!clothReady_)clothWarning_="Xion coat motion is unavailable; using the original skinned coat. Reload after repairing its asset.";
        ready_=true;error.clear();return true;
    }
    void reset() {current_=nullptr;gameplay_=nullptr;}
    void onExecute(daAlink_c* link) {
        // Remote Crests actors/puppets must never reset or advance the local
        // Xion cloth simulation.
        if(!local_player::is(link))return;
        if(!clothReady_||!compatible(link)){resetCloth();return;}
        const u32 tick=g_Counter.mCounter0;
        if(clothOwner_!=link||clothBody_!=link->mpLinkModel||
           (clothTickValid_&&tick-clothTick_>1))resetCloth();
        if(clothTickValid_&&tick==clothTick_)return;
        // Native execute has calculated both the body and attached face by
        // this point (modelCalc / setItemMatrix). No render state is sampled.
        std::array<Matrix,kBoneCount> joints;
        for(std::size_t j=0;j<kBoneCount;++j) {
            const MtxP matrix=j<kBodyBones?link->mpLinkModel->getAnmMtx(static_cast<int>(j)):
                link->mpLinkFaceModel->getAnmMtx(static_cast<int>(j-kBodyBones));
            joints[j]=fromMtx(matrix);Matrix inverseCheck;
            if(!finite(joints[j])||!inverse(joints[j],inverseCheck)){resetCloth();return;}
        }
        clothPose_=cloth_.advance(model_,joints);
        if(!clothPose_.valid){resetCloth();return;}
        clothTick_=tick;clothTickValid_=true;clothOwner_=link;clothBody_=link->mpLinkModel;
    }
    bool setEnabled(bool value) {
        enabled_=value&&ready_;reset();resetCloth();
        return enabled_==value;
    }
    void shutdown() {
        enabled_=false;ready_=false;reset();resetCloth();
        for(auto& slot:slots_){slot.packet.finishArrayReads();slot.packet.valid=false;}
        for(auto& texture:textures_)if(texture.initialized){GXDestroyTexObj(&texture.object);texture.initialized=false;}
        // Texture pixels and model arrays stay allocated through host FIFO
        // retirement. The module's final destruction releases them after detach.
    }
    void beginFrame(daAlink_c* link) {if(local_player::is(link))reset();}
    void onDrawBufferReset(J3DDrawBuffer* buffer) {leases_.onBufferReset(buffer);}
    bool hasGameplayPacket() const {return enabled_&&ready_&&gameplay_&&gameplay_->valid;}
    bool ownsShadowModel(J3DModel* model) const {return hasGameplayPacket()&&gameplay_->matches(model);}
    HookAction queueModel(daAlink_c* link,J3DModel* model,bool hidden,bool preview) {
        if(!compatible(link)||!model)return HOOK_CONTINUE;
        if(model!=link->mpLinkModel) {
            return current_&&current_->valid&&current_->actor==link&&current_->preview==preview&&current_->matches(model)?HOOK_SKIP_ORIGINAL:HOOK_CONTINUE;
        }
        current_=nullptr;
        auto* opaque=j3dSys.getDrawBuffer(0);auto* translucent=j3dSys.getDrawBuffer(1);
        if(!opaque||(!hidden&&hasTransparent_&&!translucent))return HOOK_CONTINUE;
        // Native draw() does not unlink packets; only frameInit retires a
        // lease. A hidden body still needs its snapshot until shadow drawing.
        const auto slotIndex=leases_.acquire(opaque,!hidden&&hasTransparent_?translucent:nullptr);
        if(slotIndex==PacketLeases<8>::unavailable)return HOOK_CONTINUE;
        auto& slot=slots_[slotIndex];slot.opaque.drawClear();slot.translucent.drawClear();
        if(!capture(slot.packet,link,preview)){leases_.cancel(slotIndex);return HOOK_CONTINUE;}
        current_=&slot.packet;
        if(!preview){gameplay_=current_;daMirror_c::entry(model);}
        if(!hidden){opaque->entryImm(&slot.opaque,0);if(hasTransparent_)translucent->entryImm(&slot.translucent,0);}
        return HOOK_SKIP_ORIGINAL;
    }
    bool drawMirror(J3DModel* model,dMirror_packet_c* mirror,const Mtx view) {
        if(!ownsShadowModel(model))return false;
        // Only the body was registered in the mirror list; suppress a stray
        // auxiliary entry without drawing the character a second time.
        if(model!=gameplay_->bodyModels[0])return true;
        const auto& base=gameplay_->baseMatrix;const V3 origin{base.m[0][3],base.m[1][3],base.m[2][3]};
        if(!mirror||mirror->mViewScale.y<=0||point(fromMtx(view),origin).z<=point(fromMtx(j3dSys.getViewMtx()),origin).z) {
            gameplay_->drawInView(view,false,true);if(hasTransparent_)gameplay_->drawInView(view,true,true);
        }
        return true;
    }
    void drawSilhouette(const Mtx shadowView) {if(hasGameplayPacket())gameplay_->silhouette(shadowView);}
    bool ready() const {return ready_;}
    const std::string& clothWarning() const {return clothWarning_;}
    std::size_t vertexCount() const {return model_.vertexCount;}
};
} // namespace kingdom::xion
