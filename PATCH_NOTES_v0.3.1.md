# v0.3.1 — Xion stability and lower-coat motion

Based on the delivered v0.3.0 source, retaining the original mod ID, independent selectors and v0.2.3g Kingdom Key features. Requires Dusklight 2.0.3.

## Geometry-buffer pressure

The user's Windows crash reports end in a host abort while appending geometry to the fixed shared vertex buffer. The latest crashed diagnostic build had Xion voice substitution disabled, so removing her voice did not prevent that crash. Binary inspection identifies the `push_verts` path and the non-owned `ByteBuffer` capacity check; the pinned host source reserves 5 MiB for frame vertices.

Xion now submits compact signed-16-bit positions relative to her local origin and compact texture coordinates. The mesh, textures, lighting, effects and rig are unchanged. Position quantization is at most 0.0078125 game units per component. Values outside the supported range retain the original float format rather than overflowing.

For the included asset, main rendering plus one shadow decreases from 900,618 to 531,778 vertex bytes, approximately 41% less, before at most three alignment bytes per draw. This reduces the confirmed source of pressure; it does not enlarge the host buffer or guarantee that every combination of other mods fits it. See `tools/XION_COMPACT_VALIDATION.md`.

## Coat movement and boot clearance

The existing retarget and front opening are preserved. A supplemental topology-checked asset drives 96 cloth nodes, 653 constraints and 6,174 coat/zipper/trim vertices. Damped secondary movement follows Link's animated skeleton, with collision capsules around both legs and boots. It changes cosmetic vertex positions only: Link's collision, animation controller, movement and armor effects are unchanged.

The simulation advances on the native player execution step. Each draw captures an immutable snapshot, so normal, shadow and mirror passes cannot advance the simulation independently. Character changes, outfit model changes, invalid poses and large teleports reset its state. The inventory preview remains static. Missing or mismatched cloth data retains the original skinned Xion and displays a warning.

The focused regression replays 9,000 captured poses and checks 161,770 deformed vertex samples in seven selected poses. In those seven poses, the coat-versus-boot/shin triangle check reports zero crossings; the known bad running pose decreases from 177 intersecting triangle pairs to zero. All deformed vertices still fit the compact format. This is measured improvement in recorded poses, not a promise of zero clipping in every animation. See `tools/XION_CLOTH_VALIDATION.md`.

## Compatibility and validation

The normal profile also contained co-op Linkle model/voice overrides and Dawnlight equipment features. These share player/rendering systems, but the crash evidence does not establish either mod as the direct culprit. For overlapping appearance replacements, choose one character replacer at a time.

Original installations, saves, v0.3.0 deliverables and model geometry are preserved. A separate copy of the normal profile was prepared with autosave disabled. Automated launch could not complete because access to the normal shader-cache files was not granted. Live stability and final coat appearance therefore require a user-run check; offline results are recorded separately.

The unchanged universal workflow still builds all eight target libraries and merges one `.dusk`. This revision has local Windows compilation and targeted CPU tests; its full GitHub matrix has not been run.
