# v0.3.7 validation

## Source evidence from the reported v0.3.6 crash

- v0.3.6 was loaded successfully.
- The expected runtime message `Kingdom Key: index-pressure fallback enabled` did not occur before the crash.
- The last completed Aurora budget reported before peer model mounting/drawing was 983,504 / 2,097,152 index bytes, demonstrating why the previous-frame threshold did not predict the same-frame spike.
- Crests was actively mounting and drawing remote player models immediately before the log terminated.

## Offline checks

- Original full-detail Kingdom Key: 52,072 triangles; 312,432 index bytes per complete indexed pass.
- Multiplayer Safe Kingdom Key: 3,076 triangles; 18,456 index bytes per complete indexed pass.
- Triangle reduction: 94.1%.
- Safe mesh uses the same 16 runtime parts, pivot layout and chain/charm transform structure as the full mesh.
- `tools/check_indexed_mesh.py` still passes for the untouched full-detail mesh.
- `tools/generate_keyblade_safe_lod.py` regenerates `res/kingdom_key_safe.mesh` with standard Python only.

## Not claimed

A native eight-platform build and a live two/three-player Crests of Courage test cannot be run in this environment. For the first multiplayer test, explicitly choose **Keyblade Rendering → Multiplayer Safe** before summoning the Keyblade so the result does not depend on Auto detection.
