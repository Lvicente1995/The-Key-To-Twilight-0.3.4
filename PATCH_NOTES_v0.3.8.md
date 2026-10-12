# v0.3.8 — Full-quality multiplayer-safe streaming

v0.3.7 proved that reducing indexed Kingdom Key rendering prevents the immediate Crests crash, but its emergency LOD was visually unacceptable and its KH3 summon mesh was intentionally disabled.

- Multiplayer Safe now renders the normal full-detail 52,072-triangle Kingdom Key.
- The weapon uses ordinary GXBegin triangle batches instead of GXBeginIndexed in Multiplayer Safe.
- Dynamic metallic highlights remain once-per-unique-vertex.
- KH3 summon effect meshes are restored and also use streamed triangle batches in Multiplayer Safe.
- Custom weapon shadow and mirror/reflection remain omitted in Multiplayer Safe for extra headroom.
- Full Detail mode remains unchanged.
- The v0.3.7 safe LOD asset remains only for provenance and is not selected for rendering.
