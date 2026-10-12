# Xion compact vertex validation — 2026-10-12

The v0.3.1 renderer reduces data sent to Dusklight's shared 5 MiB vertex arena. The XIONM001 mesh, weights, indices, animation palette and materials are unchanged. Final mesh SHA-256: `a4851fd1d5cdbed842504fa32a6e11d5553e33d0eee4fdc5e78257cb40dc646a`.

Positions use signed 16-bit fixed point with six fractional bits, relative to the captured character base translation. The draw position matrix includes that translation; the normal and lighting matrices retain the native view. This applies to normal, shadow and mirror rendering. UVs use signed 16-bit fixed point with fourteen fractional bits. All source UVs are within [0,1]. Each material independently checks both attributes before drawing and uses the original F32 representation for any attribute that does not fit. No clamping or position wrapping occurs.

| Pass | v0.3.0 vertex bytes | v0.3.1 vertex bytes |
|---|---:|---:|
| Main, including translucent material | 623,970 | 392,870 |
| Shadow, including alpha cutout | 276,648 | 138,908 |
| Main + shadow | 900,618 | 531,778 |

This is a 40.95% reduction for one main rendering and its shadow. Values exclude at most three alignment bytes per material draw. Index bytes are unchanged. An additional mirror or preview pass incurs its own vertex data. The rest of the scene and other mods still share the same host arena, so this reduction is not a guarantee against every combination exceeding the host limit. Exceptional poses using F32 fallback also consume more data.

The standalone C++20 test passed:

- Signed range boundaries, NaN/infinity rejection, and independent position/UV F32 fallback selection.
- 10,000 translated and reflected view cases. Maximum local position rounding error: 0.0078125 cm. Maximum UV rounding error: 0.0000305176. Worst comparison against the original F32 world/view calculation: 0.024704 cm, including F32 cancellation at large coordinates.
- All 23,110 source vertices in 12 materials fit the compact format.
- 184,880 skinned samples across eight private captured native poses (records 0, 2303, 3080, 3131, 4874, 4975, 6291 and 8789), with no F32 fallback required.

These are CPU representation and budget checks, not a claim of a new live gameplay test. They do not validate all possible other-mod combinations or resolve the separately identified conditional eight-packet reuse issue.

From `tools` in an x64 Visual Studio developer shell:

```powershell
cl /nologo /std:c++20 /EHsc /O2 /I../src xion_compact_test.cpp
./xion_compact_test.exe ../res/characters/xion/xion.mesh
```

Do not define `NDEBUG`: the tests use assertions. An optional second argument accepts the private native pose capture (1,944-byte records), which is not distributed. The helper is header-only and included by `xion_renderer.hpp`; the existing CMake and direct MSVC build paths compile it automatically.
