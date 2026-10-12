# v0.3.1 validation — 2026-10-12

This revision is based on the delivered v0.3.0 project, retaining the supplied v0.2.3g weapon implementation. Historical v0.3.0 gameplay checks remain in `VALIDATION_v0.3.0.md`; they are not new live tests of this revision.

## Crash evidence and change

The six available Windows crash dumps report a fatal host abort. The latest crash stack contains the `push_verts` path and a return address at the non-owned `ByteBuffer` capacity-abort branch in the installed executable. The pinned Dusklight 2.0.3 source allocates a fixed 5 MiB shared frame vertex arena. The latest diagnostic log explicitly disabled Xion voice substitution while retaining the renderer; that version also crashed.

Xion's main-plus-shadow vertex submission is reduced from 900,618 to 531,778 bytes (40.95%, excluding small per-draw alignment). The mesh, weights, textures and voices are unchanged. This addresses the demonstrated pressure source; the host buffer remains finite and the rest of the scene and other mods still share it. No particular other mod is proven to cause the overflow.

## Completed checks

- Local Windows x64 compile and `.dusk` packaging against the pinned 2.0.3 SDK.
- Compact position/UV boundary and invalid-value tests, independent float fallback, 10,000 translated/reflected view calculations, and 184,880 samples across eight captured native poses. See `tools/XION_COMPACT_VALIDATION.md`.
- Cloth replay through 9,000 captured pose records, with 161,770 vertex checks across seven selected poses. Tests cover immutable repeated application, invalid/mismatched resource rejection, unchanged body/leg vertices, teleport reset and invalid-pose handling. See `tools/XION_CLOTH_VALIDATION.md`.
- Seven selected poses pass the lower-coat versus boot/shin triangle-intersection check, including the old running failure (177 pairs before, zero after). The check is broader than v0.3.0's foot-weighted-only audit, but it still covers a finite pose sample.
- All 161,770 post-cloth samples fit the compact position range; the reduced vertex budget remains applicable.
- Protected v0.2.3g files, all existing v0.3.0 runtime resources, weapon integration, voice modules, editable Blender projects and universal workflow are checked byte-for-byte during source packaging. See `tools/v031-baseline-preservation.json`.
- Source archive CRC and runtime archive contents checked during packaging. Blender/source assets are kept outside the runtime `.dusk`.

## Live test still needed

An isolated copy of the user's normal profile and mod set was prepared with autosave disabled. Automated launch stopped at access-denied shader-cache database initialization; requested access was not granted. There is therefore **no new live stability, visual or frame-rate result for v0.3.1**. Original installations, saves and settings were preserved.

Install the Windows test package in place of the previous package with the same mod ID, fully restart Dusklight, and run Xion in the usual setup for several minutes. Check walking/running, rolling, attacks and both selectors. If a crash persists, retain its new log/dump: reducing vertex traffic is not equivalent to proving that every scene/mod combination fits the host arena.

## Limits

- The eight-platform workflow is unchanged, but its full matrix and non-Windows runtime have not been rerun for this revision.
- The custom coat solver is cosmetic and approximate; it does not reproduce KH3's cloth solver, environment collision or cloth self-collision. Unrecorded extreme poses may still clip, and lower-powered devices need a frame-rate check.
- Inventory coat movement is intentionally static. Re-exporting the mesh requires rebuilding the topology-matched supplementary cloth asset; otherwise the renderer falls back to static skinning with a warning.
- Existing fixed-finger and incomplete facial animation limitations remain. Character/voice replacements from other mods can overlap; coexistence has not been established.
- A separate conditional issue in the existing eight-slot render-packet pool was identified: more than eight outstanding captures before draw-buffer drain can reuse a queued packet. It was not the demonstrated buffer-capacity abort and is outside this focused change. Remote co-op avatars and unusual repeated draw-hook combinations remain unvalidated.
