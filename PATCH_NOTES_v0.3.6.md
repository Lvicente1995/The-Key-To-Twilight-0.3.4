# v0.3.6 — Multiplayer index-pressure fallback

- Preserves the exact full-detail Kingdom Key mesh in normal gameplay.
- Detects when the previous completed frame plus a normal Kingdom Key indexed draw would approach Aurora's 2 MiB index arena.
- Latches an emergency renderer until the weapon is dismissed.
- Emergency mode submits the same Kingdom Key triangles through the regular GX FIFO, shifting pressure to the larger vertex arena instead of allocating another large index block.
- Emergency mode uses the native inexpensive sword shadow/reflection and suppresses the KH3 summon mesh packet to avoid extra geometry submissions in a nearly-full frame.
- No Xion runtime, coat, voice, Keyblade mesh, chain physics, audio, UI, or normal single-player rendering assets were changed.
- Renames the render-budget log to `Aurora render budget` because the counters describe the whole completed frame, not only Xion.

This is a targeted multiplayer stability candidate. A live multi-client Crests of Courage test is still required.
