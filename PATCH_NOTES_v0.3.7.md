# v0.3.7 — Preemptive multiplayer-safe Kingdom Key

The v0.3.6 crash log showed that the adaptive fallback was too late: its decision used the previous completed frame, while Crests of Courage can mount and draw peer models in the same frame that the local Kingdom Key is summoned. The fallback marker never appeared before the crash.

v0.3.7 adds a persistent **Keyblade Rendering** setting:

- **Auto** — full detail at ordinary load; latches Multiplayer Safe when the previous completed frame reaches 768 KiB of Aurora index data.
- **Full Detail** — always uses the original 52,072-triangle Kingdom Key and all existing weapon render passes.
- **Multiplayer Safe** — preemptively uses a compact 3,076-triangle runtime Kingdom Key and omits the custom Kingdom Key shadow, mirror/reflection and KH3 summon mesh pass. Audio, chain simulation, gameplay, hit effects/audio, UI and selection behavior remain active.

The editable full-detail Blender project and the original `res/kingdom_key.mesh` are unchanged. The compact runtime asset is `res/kingdom_key_safe.mesh` and can be regenerated with `tools/generate_keyblade_safe_lod.py`.

This is intended to reduce the held weapon's indexed triangle submission by about 94% before Crests begins drawing remote players, rather than reacting after the frame is already crowded.
