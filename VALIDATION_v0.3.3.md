# v0.3.3 validation — 2026-10-12

## User observation and scope

The user reported no crash so far with v0.3.2, but lower frame rate when Xion was active; changing Character to None significantly improved it in the same spot. No numerical FPS was supplied. This revision optimizes Xion's CPU work without reverting the indexed-array or packet-lifetime fixes.

## Completed checks

- Windows x64 compilation and runtime packaging against the pinned Dusklight 2.0.3 SDK.
- Exact old/new coat comparison across 9,000 recorded snapshots and 97 selected full poses: 2,241,670 vertex positions and normals remain byte-identical. A further 100,000 boundary, degenerate and random capsule cases match. The original focused collision regression also passes, with identical seven-pose output. See `tools/XION_CLOTH_OPTIMIZATION.md`.
- Alternating standalone coat-apply benchmark, 448 calls per implementation: approximately 3.115 ms before and 1.566 ms after. This is about half the CPU time for that operation on the local machine, not a game FPS result.
- Pose-cache and transparency tests are recorded in `tools/XION_POSE_CACHE_VALIDATION.md`. The cache only removes redundant preparation for identical resolved poses. Changed interpolation matrices, changed origin and new captures still rebuild; pending array reads are preserved and drained before writes.
- The current all-zero-alpha blended cornea material contributes neither color nor depth and no longer queues a translucent pass. Texture/material data and mesh indices remain unchanged on disk. Opaque zero-alpha and partially visible blended materials are retained.
- Source packaging verifies all runtime assets, editable Blender projects, protected v0.2.3g modules, weapon integration, voices and universal workflow against the prior revision. See `tools/v033-baseline-preservation.json`.
- Runtime/source archive CRC checks and final runtime-library/resource comparisons complete during packaging.

## Retained safety and limits

The public indexed attribute path, FIFO drain before array mutation, per-submission GX cache invalidation, packet leases and conservative native-model fallback remain. No host buffer sizes or executable code are patched. Exact matrix comparison is used rather than a game-tick-only cache, so high-refresh interpolation remains represented.

There is no new live FPS or long-duration stability result for v0.3.3. Automated game launching previously lacked the required shader-cache access. Install this package in place of the older package, restart, and compare Xion versus Character None in the same scene. Source/CPU tests cannot measure the complete game, other mods, graphics-driver cost or slower devices.

The eight-platform build workflow is unchanged; its full matrix and non-Windows runtime have not been rerun. Previous model/facial-animation limitations and the finite scope of the coat collision tests still apply.
