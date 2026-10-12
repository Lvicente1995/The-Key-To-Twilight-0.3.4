# v0.3.9 — Quality multiplayer-safe LOD

v0.3.8 proved that streaming the complete 52,072-triangle Kingdom Key through the ordinary GX vertex path is not viable: Multiplayer Safe could crash even with no peer nearby. The full mesh expands to 156,216 triangle vertex references, moving too much work/data to the ordinary vertex submission path.

v0.3.9 removes that experiment.

## Multiplayer Safe

- Uses an indexed 5,676-triangle quality LOD instead of the v0.3.7 3,076-triangle emergency mesh.
- The key teeth, guard, collars, pommel and charm receive substantially more geometry than v0.3.7 so the silhouette stays recognizable.
- Repeated chain links remain aggressively simplified because they account for a large part of the original 52,072-triangle model.
- Main weapon index payload is about 34,056 bytes per pass instead of 312,432 bytes for Full Detail.
- KH3 summon mesh effects are enabled again. In Multiplayer Safe only the effect meshes use the lightweight streamed FX path; the weapon itself does not.
- Custom Kingdom Key shadow and mirror/reflection passes remain omitted in Multiplayer Safe for additional buffer headroom.

## Full Detail

Full Detail is unchanged and continues to use the original 52,072-triangle indexed Kingdom Key and complete extra render passes.
