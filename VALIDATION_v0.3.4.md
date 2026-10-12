# v0.3.4 validation — 2026-10-12

## Problem and scope

After reporting a significant Xion performance improvement with v0.3.3, the user supplied screenshots showing 98 FPS before drawing the Kingdom Key and 50 FPS afterward. They confirmed that the lower FPS persists while holding it after the effects fade. This is user-observed evidence, not a controlled profiler trace. The changes target substantial per-view work found in the weapon renderer.

## Completed checks

- The Windows x64 production library compiles, links and packages against the pinned Dusklight 2.0.3 SDK.
- The real weapon mesh array test checks all 156,216 triangle corners against the prior float32 positions, normals and base color bytes. Triangle count, index order, material seams and geometry precision are preserved. See `tools/KEYBLADE_ARRAYS_VALIDATION.md`.
- Main-plus-shadow vertex-stream usage falls from 1,149,764 to 313,572 bytes before alignment/command overhead. Separate attribute storage adds 1,045,240 bytes across those passes. Index uploads are unchanged; this relocates data away from the host's small vertex arena rather than reducing every buffer combined.
- A standalone CPU benchmark evaluates the actual 26,131 unique vertices under seven representative active lights over multiple view transforms. Highlight work falls from approximately 6.35 ms to 4.20 ms per full mesh/view. This excludes the game, driver, geometry submission, chain simulation and other mods. See `tools/KEYBLADE_HIGHLIGHT_VALIDATION.md` for commands, exact results and numerical comparisons.
- Highlight lookup interpolation is intentionally approximate. Across 418,096 vertex/view comparisons, the measured maximum RGB difference is below 0.001 on the 0–255 scale; nine channel bytes differ by one due to quantization boundaries. This is not a guaranteed error bound under arbitrary attenuation. Values outside the lookup domain use the original power function.
- Source lifetime review confirms immutable arrays remain resident across selector changes, pending FIFO readers are drained before storage destruction/replacement, and dynamic highlight bytes are copied into the FIFO. No new per-draw synchronization wait is used. GX cache invalidation is retained before each submission, and shadow drawing restores the native direct-position format afterward.
- Packaging checks compare all runtime assets, editable Blender files, Xion source modules, selection/voice modules, protected v0.2.3g features and the eight-platform workflow with the previous release. A focused source comparison verifies the weapon's attachment interpolation, physics, queueing, effects and gameplay hooks outside the changed render/load sections. See `tools/v034-baseline-preservation.json`.
- Runtime and source archives pass CRC checks. The packaged Windows library and every resource match the final build/project. Blender projects and development source are excluded from the runtime `.dusk`.

## Limits and installation check

No v0.3.4 in-game FPS, visual, long-duration stability or full mod-stack result is claimed. Automated live launching remains unavailable because the required host shader-cache access was not granted earlier. Source tests do not establish actual frame rate or complete compatibility.

Replace the older package, restart Dusklight, and compare stowed versus held Kingdom Key in the same location after the summon effects finish. A brief check of metallic highlights, the chain, shadows and draw/dismiss behavior will cover the renderer changes. Keep the v0.3.3 package as a rollback.

The full eight-platform Actions matrix and non-Windows runtime have not been rerun. Build configuration and public API usage remain cross-platform; the supplied prebuilt package contains only Windows x64. Previous fixed-grip/facial-animation limitations and the finite coat-collision test scope remain unchanged. The host's graphics arenas remain finite, and the weapon's existing draw-packet pool was not redesigned in this focused performance patch.
