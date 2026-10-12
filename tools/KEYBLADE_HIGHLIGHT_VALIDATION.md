# v0.3.4 highlight calculation

The CPU highlight path retains every vertex, all seven native light slots, camera dependence, metalness, roughness and attenuation. Six immutable source material tuples cache the roughness exponent, Fresnel base color and diffuse bytes. Black lights are omitted from the temporary active list; every nonblack light keeps its original order and coefficients.

Each material owns a 4097-float table over cosine `[0,1]`; linear interpolation replaces the variable-exponent power calculation. Six tables use 98,328 bytes. Cosines outside `[0,1]` and missing tables use `std::pow`. Fresnel's fifth power uses three multiplications. This is a floating-point approximation, not fewer lights, reduced mesh detail or a quality setting.

The standalone MSVC C++20 `/O2` benchmark reads the actual `KKMESH02` asset and deduplicates complete vertices exactly as the runtime. It uses 26,131 unique vertices, 16 parts, 16 view rotations and seven nonblack synthetic native-style lights. It does **not** capture the user's current scene or measure GX submission/GPU work.

Measured on 2026-10-12, 96 full-mesh/view evaluations per variant:

| Calculation | Mean CPU time |
|---|---:|
| Original calculation | 6.35460 ms |
| Cached material/light values, original powers | 6.08243 ms |
| Cached values plus multiplication Fresnel | 5.31268 ms |
| Production lookup plus multiplication Fresnel | 4.20366 ms |

The production helper reduced this workload by 33.8% (1.51× faster). These are standalone CPU timings, not an in-game FPS promise.

Across 418,096 vertex/view samples (1,254,288 output channels):

- Cached values with original powers: bit-identical RGB output.
- Multiplication Fresnel alone: maximum RGB difference `0.0000305176` on the 0–255 scale; zero final-byte differences.
- Production lookup and Fresnel: maximum RGB difference `0.000839233` on the 0–255 scale. Nine output channels differed after quantization; the maximum difference was **one byte level**, or `1/255`. These are measured fixture results, not a universal error bound for arbitrary lighting.
- The test checks six shared materials, all seven active lights, table sizes, exact zero/one endpoints, and `std::pow` fallback for negative and above-one inputs.

Material tables and indices are prepared at mesh load and remain immutable during drawing. `MaterialCache::intern` returns `size_t`; the runtime validates the index before storing it as `u16`. No references to material entries are retained while the pool grows. Drawing reads the finalized pool, so vector relocation during loading cannot invalidate a draw reference.

Reproduce with `tools/keyblade_highlight_bench.cpp` and the release's `res/kingdom_key.mesh`, compiling as C++20 with optimization and assertions enabled. The original baseline arithmetic remains in the test for direct comparison. Meshes, materials, textures and light counts were not edited by this optimization.
