# v0.3.9 validation

Validated in this environment:

- v0.3.9 metadata is consistent in CMake, `mod.json`, README and startup logging.
- Safe LOD generator produced 5,676 triangles from the 52,072-triangle source.
- Safe LOD keeps the same part count, names, pivots and chain anchor contract required by the runtime loader.
- Multiplayer Safe selects `safeParts` / `safeHighlightMaterials` and uses `GXBeginIndexed` for the weapon.
- The v0.3.8 full-weapon ordinary-GX streaming branch has been removed.
- KH3 summon effects remain queued in Multiplayer Safe and their mesh packet receives streamed-FX mode.
- Custom weapon shadow/reflection remain suppressed in Multiplayer Safe.
- Existing portable indexed-mesh and source checks pass.
- ZIP CRC validation passes.

Not verified here:

- Native eight-platform build.
- Live Dusklight runtime behavior.
- Live multi-client Crests of Courage behavior.
