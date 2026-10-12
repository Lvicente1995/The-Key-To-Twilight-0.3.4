# v0.3.4 — Kingdom Key rendering cost

The user reported a sustained 98-to-50 FPS drop when drawing the Kingdom Key, continuing after the summon effects faded. This revision targets the held weapon renderer. The FPS figures describe the user's previous build; this update has no new live FPS result yet.

- Immutable positions, normals and base colors now use the public GX indexed-array path. The main and shadow passes consume 313,572 vertex-stream bytes together instead of 1,149,764, excluding alignment and command overhead. The attributes move to the separate storage arena; this is a reduction in vertex-arena pressure, not a reduction in total GPU upload bytes. All 52,072 triangles and the original float32 geometry are retained.
- Dynamic metallic highlight colors remain FIFO-owned direct data. Geometry is never rewritten during rendering, so this change introduces no per-frame or per-view synchronization wait. Pending array readers are drained before the storage is freed or replaced.
- Material constants and six fine specular lookup tables are prepared at load. Per-view light data is prepared once, preserving all seven eligible native lights. The fifth-power Fresnel term uses multiplication. Positions, normals, camera response, attenuation, fog and lighting controls are retained.
- The CPU lighting benchmark improves from approximately 6.35 ms to 4.20 ms per complete weapon view. The lookup is a numerical approximation: in 418,096 tested vertex/view samples, nine final color channels differ by one byte, with maximum pre-quantization error below 0.001 on the 0–255 scale. These are measured fixture results, not a universal bound for every possible light configuration or platform.

The complete v0.3.3 Xion rendering, coat, voice and crash protections are unchanged. All runtime assets, editable Blender files, chain physics, summon/dismiss and hit effects, effect orientation, sounds, shield behavior, UI icons/text, selection controls and gameplay hooks are preserved. No model detail, reflections, shadows or lights were disabled.

The mod ID remains `local.kingdom_key.ordon`; Dusklight 2.0.3 and the eight-platform combined workflow remain unchanged. Replace the previous installed package and restart Dusklight. Do not install two versions simultaneously. The supplied runtime build is Windows x64; use the source workflow to build the combined universal package.

See `VALIDATION_v0.3.4.md` for completed checks and remaining limits.
