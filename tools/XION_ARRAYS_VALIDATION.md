# Xion indexed arrays — 2026-10-12

The v0.3.2 renderer uses public `GXSetArray` and `GX_INDEX16` attributes with the existing `GXBeginIndexed` triangle topology. Aurora puts position, normal and UV arrays in its separate 8 MiB storage arena; the shared 5 MiB vertex arena receives attribute indices. The source mesh, cloth simulation, textures, native lighting, alpha tests and triangle indices are unchanged.

| Per-character pass | Vertex arena | Storage arena | Index arena |
|---|---:|---:|---:|
| Main, all materials | 138,660 | 300,430 | 170,220 |
| Shadow | 46,108 | 137,740 | 167,640 |
| Total | **184,768** | **438,170** | **337,860** |

Values are bytes before alignment and assume one upload per attribute/material/pass. Vertex/index draws add at most three padding bytes each; storage alignment is chosen by the host GPU limits. A mirror or inventory pass adds its own data. Compared with v0.3.1's 531,778 main/shadow vertex bytes, the vertex arena requirement falls by 347,010 bytes. These are bounded contributions, not a guarantee that arbitrary other mods and scenes cannot exhaust a shared host arena. The storage arena is also fixed-size.

Each material's encoded RGBA is checked at load. Uniform colors use the native channel material register, preserving RGB and alpha; differing colors use a fourth indexed array. The current Xion asset has uniform white vertex colors. Signed16 positions and UVs retain the v0.3.1 precision and independent F32 fallback. Varying-color materials use eight vertex bytes instead of six; F32 fallback increases storage size, without increasing attribute-index size.

`GXSetArray` queues a pointer rather than copying immediately. Each packet owns its pose arrays and marks them pending after submission. `AuroraGXSync()` drains pending FIFO reads before any rewrite or shutdown. Immutable UV/color arrays remain allocated for the module lifetime. `GXInvalidateVtxCache()` precedes resubmission because changing bytes at an unchanged pointer does not invalidate Aurora's cached storage range. No arrays are allocated or resized during drawing.

The independently supplied `PacketLeases<8>` integration prevents a still-linked J3D packet from being reused. Only the corresponding native draw buffer's `frameInit` retires a lease; rendering, selection changes and actor callbacks do not. Exhaustion preserves the original native body draw.

Validation completed:

- Windows x64 production compile and package through the existing portable MSVC build script.
- Standalone tests of byte strides/index fetches, constant material color/alpha, varying color/alpha, F32 UV/position fallback, invalid-value rejection and stable array addresses across updates.
- All 85,110 triangle corners in the actual 23,110-vertex asset resolve to the same compact position, normal, UV and encoded color as the preceding direct-attribute renderer.
- Exact unaligned budget totals above asserted by the test.

This report does not claim a new live gameplay validation. Mesh SHA-256 remains `a4851fd1d5cdbed842504fa32a6e11d5553e33d0eee4fdc5e78257cb40dc646a`.

From `tools` in a Visual Studio developer shell:

```powershell
cl /nologo /std:c++20 /EHsc /O2 /I../src xion_arrays_test.cpp
./xion_arrays_test.exe ../res/characters/xion/xion.mesh
```

Keep assertions enabled; do not define `NDEBUG`. The new helper is header-only and is included automatically by the existing production build paths.
