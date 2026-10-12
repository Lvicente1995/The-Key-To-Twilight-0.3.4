# The Key to Twilight v0.3.5

## Crests of Courage / multiplayer stability

This revision isolates The Key to Twilight's local character and weapon replacement state from remote-player rendering.

Changes:
- Kingdom Key simulation, draw ownership, equip/dismiss hooks and render replacement are restricted to the authoritative local Link actor.
- Remote `daAlink_c`/puppet-adjacent calls no longer reset or take ownership of the local Keyblade chain/presence state.
- The native sword `modelDraw` replacement now runs only inside the local player's actual Link draw, or the real status-window preview. Calls made externally by another mod's remote held-item renderer fall through to the original game renderer.
- Remote Link draw calls no longer clear the local `gameplaySword`, `gameplaySheath` or `gameplayPacket` state used by the later shadow/reflection passes.
- Xion body/cloth rendering is likewise restricted to the authoritative local Link actor so remote Crests players stay owned by Crests.
- Kingdom Key shadow handling now validates the shadow model count before reading slot zero and leaves unrelated remote shadow models untouched.
- Kingdom Key mirror handling now fails through safely on invalid mirror/view inputs.

No Kingdom Key geometry, materials, chain physics, audio, KH3 effects, Xion mesh, Xion coat tuning or v0.3.4 highlight optimization was changed.
