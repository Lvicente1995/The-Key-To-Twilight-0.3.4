# v0.3.3 — Xion CPU rendering cost

The user reported that v0.3.2 had not crashed so far, but frame rate improved significantly when Character was set to None in the same location. This revision targets repeated character work while retaining the v0.3.2 rendering-buffer and packet-lifetime protections.

- The opaque, shadow and mirror passes reuse prepared skinning, coat collision and packed attribute arrays when all 40 resolved animation matrices and the packing origin match. Each captured packet invalidates the cache, covering changed cloth state, actor, preview mode and outfit. Different interpolated poses within the same game tick still rebuild. View, lighting and visibility remain per-pass.
- Cached geometry stays immutable, so matching passes do not need to wait for the FIFO reader. Changed poses still drain pending readers before rewriting arrays. The GX vertex-cache invalidation remains before submissions, including cache hits, because storage offsets belong to the host frame arena.
- The original mesh's blended cornea placeholder has a fully zero-alpha texture and disables depth writes. It is skipped, removing an otherwise invisible translucent packet pass. Opaque materials are never removed based on alpha; a blended texture with any nonzero alpha is retained.
- Capsule projection precomputes fixed geometry and rejects clearly outside points before expensive projection/square-root work. It keeps the same collision iterations, forces, weights and limits. Tests compare the resulting vertices and normals against v0.3.2.

No runtime assets, editable Blender models, Keyblade effects/audio/gameplay, character voices or public selection controls changed. The same mod ID, Dusklight 2.0.3 requirement and eight-platform universal workflow remain. Restart after replacing the package.

Standalone CPU results are recorded in `VALIDATION_v0.3.3.md`. They are not an in-game FPS measurement; the user's actual frame rate and continued stability need checking in the same scene.
