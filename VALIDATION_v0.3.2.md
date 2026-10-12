# v0.3.2 validation — 2026-10-12

## Crash evidence

The user tested v0.3.1 in their normal Windows Dusklight setup and reported another crash after a couple of minutes. The matching log confirms v0.3.1 was active. The new dump (`c62756b3-1850-4dc4-b689-010c906d2e2c`) reaches the same non-owned vertex-buffer capacity abort as earlier reports.

Using the installed executable's unwind metadata, the recovered host append had:

- Previous vertex arena length: `0x4FFFF0` (5,242,864 bytes).
- Append size: `0x50` (80 bytes).
- Requested end: `0x500040` (5,242,944 bytes), 64 bytes above 5 MiB.
- Primitive: ordinary `GX_QUADS`, four vertices, 20-byte stride.

This supports cumulative arena exhaustion. It does not identify the original producer of the final quad in the deferred stream or prove that another particular mod caused it. The buffer object's heap page was absent from the minidump; the fixed 5 MiB capacity is corroborated by the pinned host source and installed abort branch.

## Completed checks

- Windows x64 compilation and package against the pinned Dusklight 2.0.3 SDK, including the new public GX calls and `J3DDrawBuffer::frameInit` hook.
- Indexed attribute tests: constant RGB/alpha registers, varying-color fallback, F32 position/UV fallback, invalid values, fixed array addresses and all 85,110 triangle corners of the actual asset. Attribute values match the preceding direct renderer. See `tools/XION_ARRAYS_VALIDATION.md` and `tools/xion-arrays-results.txt`.
- Main-plus-shadow vertex arena contribution: 184,768 bytes versus v0.3.1's 531,778 bytes (65.3% lower). Additional storage contribution: 438,170 bytes; index contribution remains 337,860 bytes. These figures exclude alignment and extra mirror/preview passes; the storage arena is separately limited to 8 MiB.
- Packet lease regression: eight queued slots, safe ninth-capture fallback, 100 repeated list traversals, unrelated buffer resets, separate opaque/translucent retirement, failed-capture cancellation and shared buffers. See `tools/xion-packet-leases-results.txt`.
- Independent source review of FIFO pointer lifetime, cache invalidation, attribute order/endianness, material color/alpha, shadow state restoration and lease integration found no concrete defect.
- Source packaging verifies all existing runtime resources, editable Blender projects, weapon integration, voice modules, coat implementation and universal workflow against the preceding revision. See `tools/v032-baseline-preservation.json`.
- Packaged source/runtime archives receive CRC validation; the runtime library/resources are checked against the final build and source tree.

The renderer logs completed-frame vertex/storage/index sizes when they cross a new 128 KiB high-water band. This uses the public Aurora statistics API. It is diagnostic information, not a current-frame reservation or an automatic proof of free space.

## Still unverified

No new live game run is claimed. Earlier automated launches could not obtain the required normal shader-cache access; this update has compilation, byte-equivalence and source-lifetime validation only. Install the v0.3.2 package in place of the old package, fully restart, and revisit the area/action where the user observed the crash. Test Xion with the normal mod set for several minutes. Preserve the new log/dump if it still fails.

The host arenas remain finite; storage use has shifted, not disappeared. Final frame rate, visual parity and sustained stability in the user's actual mod combination remain to be checked. The eight-platform workflow is unchanged but its full matrix has not been rerun. The v0.3.1 coat behavior and its recorded-pose results are retained; this rendering change does not broaden those animation guarantees.
