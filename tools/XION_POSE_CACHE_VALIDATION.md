# Xion pose cache: focused validation

Tested 2026-10-12 with MSVC 19.44, C++20, `/O2`, assertions enabled. No renderer changes were made during this review.

`xion_pose_cache_test.cpp` passed:

- Matching poses reuse geometry; changing a joint within one game tick or changing the compact origin causes a rebuild.
- Capture invalidation covers actor/model, cloth snapshot and preview changes even when matrices match.
- A partial rebuild failure leaves the cache invalid; retry rebuilds instead of exposing partially written data.
- A mock of the renderer's submission protocol retains pending FIFO reads on cache hits and synchronizes only before array writes. This checks protocol equivalence, not the native graphics worker itself.
- A blended image with every alpha byte zero can be excluded. Opaque/cutout images, empty images and a blended image containing even one alpha byte of 1 remain drawable.
- All actual model textures were decoded. Only `n_ex033_E_mat2_inst` (`xion_cornea.rgba`, 348 vertices) met the invisible-blend condition.

Source review confirmed that `capture()` invalidates before replacing snapshot inputs; `prepareSkin()` resolves all 40 current presentation matrices before checking the cache, invalidates before any writes, drains pending array reads only on a miss, and commits only after skinning, cloth and every array update succeed. Camera, lighting and draw masks remain outside the cache. Vertex-cache invalidation remains on each draw, including cache hits, so host storage offsets are refreshed. No blocking defect found in these changed paths.

## Bounded CPU comparison

The actual 23,110-vertex asset was tested at recorded pose 4874, with a 31-tick cloth lead-in. Forty repetitions each requested three matching-pose passes and invalidated at the start of each repetition:

| Mode | Geometry builds | Total CPU time |
| --- | ---: | ---: |
| Rebuild every pass | 120 | 396.977 ms |
| Cache matching passes | 40 | 135.556 ms |

This run was 2.93 times faster for the tested skinning, cloth application and array packing workload. Skinned positions/normals and packed arrays were identical. Timings are a single local run, exclude graphics/FIFO waits, interpolation lookup and camera/lighting setup, and do **not** predict an exact game FPS increase. No additional live-game claim is made.

## Reproduce

From this project directory in an x64 MSVC developer environment, compile with assertions enabled:

```powershell
cl /nologo /std:c++20 /EHsc /O2 /W4 tools/xion_pose_cache_test.cpp /Fe:xion_pose_cache_test.exe
.\xion_pose_cache_test.exe
```

The no-argument run covers cache protocol and transparency unit cases. To repeat the actual-asset comparison, supply the mesh, cloth resource and a local recorded-pose capture:

```powershell
.\xion_pose_cache_test.exe res/characters/xion/xion.mesh res/characters/xion/xion.cloth <recorded-poses.bin>
```

The development capture was `work/v030/xion-inspect/kh3/coat-review/live-pose-snapshot.bin`. It is private validation input, not required for the mod or included in the source package. Its headerless 1,944-byte records contain frame/procedure IDs, speed, actor position, then 40 float32 3x4 world matrices; at least 4,875 records are required for this fixed benchmark.
