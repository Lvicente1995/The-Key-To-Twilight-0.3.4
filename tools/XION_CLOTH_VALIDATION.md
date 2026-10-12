# Xion cloth validation — v0.3.1

The reviewed XIONM001 mesh remains unchanged: SHA-256 `a4851fd1d5cdbed842504fa32a6e11d5553e33d0eee4fdc5e78257cb40dc646a`. The editable Blender file also remains unchanged. The new supplement is generated from retained original cloth weights and contains 96 nodes, 653 constraints, 6 capsules and 6,174 masked vertices. It changes only coat, zipper and attached trim. No boot or leg vertices enter the mask.

The standalone C++ test executes the actual `xion_cloth.hpp` implementation. It replays 9,000 recorded pose records, advancing one 1/30-second simulation step per record. This is a deterministic test replay, not a measurement of the original recording's frame timing or a live gameplay benchmark.

Checks passed:

- Exact geometry fingerprint and per-material count validation; truncated and mismatched resources reject safely.
- Repeated `apply` calls with the same snapshot produce identical output, while an invalid preview snapshot leaves all vertices unchanged.
- Every unmasked vertex, including unmasked vertices in the same materials, remains byte-for-byte unchanged in position and normal.
- All 161,770 checked vertex samples are finite. Maximum correction in the seven output poses was 9.20945 cm.
- All final positions in those seven poses fit the renderer's relative S16 representation through `xion_compact.hpp`.
- Teleport reset, invalid-pose reset, and a collider centerline fallback are checked.
- Boot and shin/upper-boot triangle intersections are checked against the coat in seven targeted poses. The earlier running failure at record 4874 falls from 177 triangle pairs to zero. The other six poses remain at zero.

The triangle test covers 2,656 coat faces below 105 cm in the rest pose and 1,397 boot/lower-leg faces below 50 cm. It tests strict noncoplanar intersections. It is broader than the v0.3.0 foot-weight-only check, but it is not continuous collision detection, a test of every animation, or proof that all body contacts are impossible. Counts are in `xion-cloth-crossings.json`.

The final standalone Windows run measured about 3.08 ms per `apply` call averaged over seven poses, with 2.05 seconds for the full test including 9,000 simulation advances. These are local CPU timings without game rendering, and should not be presented as an in-game FPS result or an Android/Linux/macOS benchmark.

Compile `xion_cloth_test.cpp` as C++20 and pass four paths: runtime `xion.mesh`, matching `xion.cloth`, the existing recorded-pose binary, and a destination for posed geometry. The capture fixture stays in the workspace; it is not needed by normal mod builds. `model/characters/xion/generate_cloth.py` regenerates the mask using Python and NumPy without Blender or a KH3 installation.

Live secondary-motion appearance and the final integrated update timing require current-build in-game verification. The module itself has no GX calls and performs no allocation or shared-state mutation in `apply`.
