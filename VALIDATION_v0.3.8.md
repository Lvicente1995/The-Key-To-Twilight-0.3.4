# v0.3.8 validation

Static validation:
- Version metadata is v0.3.8.
- Multiplayer Safe selects full `parts` / `highlightMaterials`, not the reduced LOD.
- Multiplayer Safe weapon submission uses ordinary GXBegin batches.
- KH3 summon effects remain enabled and stream indexed effect meshes in Multiplayer Safe.
- Full Detail still uses GXBeginIndexed.
- Custom weapon shadow/reflection remain suppressed in Multiplayer Safe.

Live two-client Crests testing is still required.
