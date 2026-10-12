# Kingdom Key static attribute arrays

Validated 2026-10-12 with MSVC 19.44, C++20, `/O2 /W4`, assertions enabled. The standalone test compiled without warnings and passed.

The held weapon contains 16 rigidly transformed parts: its body, 14 chain links and charm. Exact complete-vertex deduplication produces 26,131 vertices and 156,216 triangle indices (52,072 triangles). The mesh and index order are unchanged.

The renderer now submits positions, normals and base colors through the public `GXSetArray`/`GX_INDEX16` path. Positions and normals retain their original float32 precision; base colors retain the previous clamp/truncation and solid alpha. Camera-dependent highlights remain direct RGBA8 data copied into the FIFO. They do not require a mutable array or a new per-pass synchronization wait.

## Focused checks

`keyblade_arrays_test.cpp` verifies:

- All 156,216 original triangle corners resolve to exactly the previous float32 positions, normals and base color bytes.
- Signed zero and large coordinates remain exact; base color clamping, truncation and alpha match the previous renderer.
- The main vertex layout is POS IDX16, NRM IDX16, CLR0 IDX16, then direct CLR1 RGBA8; shadow vertices contain only POS IDX16.
- Array addresses remain stable through repeated read-only submissions.

The native source review also checked that main and shadow passes invalidate host vertex-cache ranges before submitting arrays; shadow drawing restores the native direct float32 position format afterward. Static arrays remain immutable while resident. `finishRenderingReads()` drains pending host FIFO reads before shutdown frees their storage. This is a source-level lifetime check, not a new live-game stability claim. The existing packet pool is unchanged by this focused patch.

## Per main-plus-shadow submission

Bytes below exclude small per-draw alignment and command overhead. Triangle-index uploads are unchanged.

| Buffer | Previous | New |
| --- | ---: | ---: |
| Main vertex arena | 836,192 | 261,310 |
| Shadow vertex arena | 313,572 | 52,262 |
| Combined vertex arena | 1,149,764 | 313,572 |
| Added attribute storage arena | 0 | 1,045,240 |

This reduces the weapon's use of the host's 5 MiB vertex arena by 72.7%, moving immutable attributes into its separate storage arena. It does not remove geometry or promise a particular FPS change. The CPU highlight calculation is validated separately.

## Reproduce

From this project directory in an x64 MSVC developer environment:

```powershell
cl /nologo /std:c++20 /EHsc /O2 /W4 tools/keyblade_arrays_test.cpp /Fe:keyblade_arrays_test.exe
.\keyblade_arrays_test.exe res/kingdom_key.mesh
```

No game process or private pose capture is required. Without the mesh argument, the test runs only its synthetic layout/attribute cases.
