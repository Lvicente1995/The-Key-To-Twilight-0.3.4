#pragma once

// Included after native hook/service declarations and both renderer modules.
// All selection changes are applied by mod_update at the host frame boundary.
namespace kingdom::customization {

selections::System settings;
xion::System character;
character_voice::System characterVoice;
bool keybladeAssetsReady=false;
bool characterAssetsReady=false;
bool audioAttempted=false;
std::string keybladeError;
std::string keybladeWarning;
std::string characterError;
std::string characterVoiceWarning;
selections::Selection applied{KeybladeSelection::None,CharacterSelection::None,KeybladeRenderMode::Auto};

void initializeAssets() {
    namespace kk=keyblade_runtime;
    try {
        if(!kk::loadMesh())keybladeError="Kingdom Key model is missing or invalid.";
        else if(!kh3fx::loadAssets(svc_resource,mod_ctx,keybladeError)) {}
        else {
            keybladeAssetsReady=true;
            // Keep native sound resource registrations resident across choices;
            // never append new banks or tables during a runtime switch.
            audioAttempted=true;
            unsigned loaded=0;
            try {loaded=kk::keybladeAudio.initialize(mod_ctx,svc_audio_res,svc_hook);}
            catch(...) {}
            if(loaded!=audio::kCueCount) {
                kk::keybladeAudio.shutdown();
                keybladeWarning="Kingdom Key custom audio is unavailable; using native sword sounds.";
                svc_log->warn(mod_ctx,keybladeWarning.c_str());
            }
        }
    } catch(const std::exception&) {
        keybladeError="Kingdom Key assets could not be loaded.";
    } catch(...) {
        keybladeError="Kingdom Key assets could not be loaded.";
    }
    if(!keybladeAssetsReady) {
        kk::enabled=false;
        if(audioAttempted)kk::keybladeAudio.shutdown();
        kk::finishRenderingReads();
        kh3fx::clearAssets();
        kk::parts.clear();
        kk::safeParts.clear();
        svc_log->warn(mod_ctx,keybladeError.c_str());
    }
    try {
        characterAssetsReady=character.initialize(svc_resource,mod_ctx,svc_interp,characterError);
    } catch(...) {
        characterError="Xion assets could not be loaded.";
        character.shutdown();
        characterAssetsReady=false;
    }
    if(!characterAssetsReady)svc_log->warn(mod_ctx,characterError.c_str());
    else {
        if(!character.clothWarning().empty())svc_log->warn(mod_ctx,character.clothWarning().c_str());
        unsigned loaded=0;
        try {
            loaded=characterVoice.initialize(mod_ctx,svc_audio_res,svc_hook,character_voice::kXionClips);
        } catch(...) {characterVoice.shutdown();}
        if(loaded!=std::size(character_voice::kXionClips)) {
            characterVoiceWarning="Some Xion vocal samples are unavailable; missing sounds use Link's original voice.";
            svc_log->warn(mod_ctx,characterVoiceWarning.c_str());
        }
    }
}

void apply(const selections::Selection& requested) {
    namespace kk=keyblade_runtime;
    if(requested.rendering!=applied.rendering) {
        kk::setRenderMode(requested.rendering);
        applied.rendering=requested.rendering;
    }
    if(requested.keyblade!=applied.keyblade) {
        kk::deactivate();
        applied.keyblade=KeybladeSelection::None;
        if(requested.keyblade==KeybladeSelection::KingdomKey&&keybladeAssetsReady) {
            try {
                if(kk::activate(keybladeError))applied.keyblade=KeybladeSelection::KingdomKey;
            } catch(...) {
                kk::deactivate();
                keybladeError="Kingdom Key could not be activated.";
            }
        }
    }
    if(requested.character!=applied.character) {
        characterVoice.setEnabled(false);
        character.setEnabled(false);
        applied.character=CharacterSelection::None;
        if(requested.character==CharacterSelection::Xion&&characterAssetsReady) {
            if(character.setEnabled(true))applied.character=CharacterSelection::Xion;
            else characterError="Xion could not be activated.";
        }
        characterVoice.setEnabled(applied.character==CharacterSelection::Xion);
    }
    std::string status;
    if(requested.keyblade!=applied.keyblade)
        status=keybladeError+" Using the original Ordon Sword. Repair the assets and reload the mod.";
    if(requested.character!=applied.character) {
        if(!status.empty())status+='\n';
        status+=characterError+" Using Link. Repair the assets and reload the mod.";
    }
    if(status.empty()) {
        status="Selections apply immediately. Armor abilities stay unchanged.";
        if(applied.keyblade==KeybladeSelection::KingdomKey&&applied.rendering==KeybladeRenderMode::MultiplayerSafe)
            status+=" Multiplayer Safe uses the compact Kingdom Key and omits extra weapon shadow/reflection/summon mesh passes.";
    }
    if(applied.keyblade==KeybladeSelection::KingdomKey&&!keybladeWarning.empty())
        status+='\n'+keybladeWarning;
    if(applied.character==CharacterSelection::Xion&&!characterVoiceWarning.empty())
        status+='\n'+characterVoiceWarning;
    if(applied.character==CharacterSelection::Xion&&!character.clothWarning().empty())
        status+='\n'+character.clothWarning();
    settings.setStatus(status);
}

HookAction onPrepareCustomSound(ModContext* context,void* args,void* result,void* user) {
    if(characterVoice.prepareSequence(mods::arg<JAISe*>(args,0))) {
        *static_cast<bool*>(result)=true;
        return HOOK_SKIP_ORIGINAL;
    }
    return keyblade_runtime::onPrepareKeybladeSound(context,args,result,user);
}

void update() {
    selections::Selection requested;
    if(settings.takePending(requested))apply(requested);
}

void shutdown() {
    settings.shutdown();
    keyblade_runtime::deactivate();
    characterVoice.shutdown();
    character.shutdown();
    if(audioAttempted)keyblade_runtime::keybladeAudio.shutdown();
    audioAttempted=false;
    keyblade_runtime::finishRenderingReads();
    kh3fx::clearAssets();
    keyblade_runtime::parts.clear();
    keyblade_runtime::safeParts.clear();
    keybladeAssetsReady=characterAssetsReady=false;
    applied={KeybladeSelection::None,CharacterSelection::None,KeybladeRenderMode::Auto};
    keybladeWarning.clear();
    characterVoiceWarning.clear();
}

} // namespace kingdom::customization
