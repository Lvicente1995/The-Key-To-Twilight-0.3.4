# v0.3.3 capsule calculation optimization

Only collision calculation cost changed. Capsule axes, squared lengths and conservative bounds are prepared once per pose. Points clearly outside a capsule skip the projection or square root. Ordinary inside points skip construction of the unused centerline fallback. The original exact projection remains at the boundary and centerline.

Simulation steps, constraints, damping, weights, radii, time step, collision passes, runtime mesh and cloth supplement are unchanged.

Windows MSVC C++20 `/O2` comparison on 2026-10-12:

- 100,000 random, boundary and degenerate capsule queries: byte-identical results to v0.3.2.
- All 9,000 captured pose records: byte-identical simulated node offsets.
- 97 full poses, including the seven original collision fixtures: 2,241,670 skinned vertex positions **and normals** byte-identical to v0.3.2.
- 448 alternating timed applies per implementation: v0.3.2 mean 3.11501 ms; v0.3.3 mean 1.56582 ms (1.99× faster).
- Existing cloth regression passes: 161,770 vertex samples; maximum correction 9.20945 cm; unchanged unmasked vertices, immutable snapshots, resource rejection, reset behavior and S16 bounds. Its separate apply mean was 1.54379 ms.

These are standalone CPU timings, not an in-game FPS measurement. Exact equality on the collision fixtures preserves the previous triangle-intersection results without rerunning the intersection search. Broader live visual and performance checks remain separate.

`xion_cloth_compare.cpp` accepts the runtime mesh, cloth supplement and captured pose fixture. To reproduce against v0.3.2, copy that release's `src/xion_cloth.hpp` to a temporary include directory as `xion_cloth_baseline.hpp`, change only its namespace from `kingdom::xion::cloth` to `kingdom::xion::cloth_baseline`, and compile the comparison with the temporary directory and current `src` on the include path. Keep assertions enabled. The 1944-byte-per-record capture is a local validation fixture, not a runtime requirement.
