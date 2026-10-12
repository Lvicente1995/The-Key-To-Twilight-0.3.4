# v0.3.2 — Xion rendering-buffer follow-up

The user's new crash report confirms that v0.3.1 still exhausted Dusklight's fixed 5 MiB vertex buffer. The recovered append was an ordinary four-vertex quad: 80 bytes requested when only 16 bytes remained. This was cumulative frame-buffer use, not an oversized malformed Xion mesh. The deferred command stream does not identify which native effect or mod originally emitted that last quad.

Xion now uses the public GX indexed-attribute array path. Position, normal and texture-coordinate data are read from the separate storage buffer instead of being repeated as direct attributes in the smaller vertex arena. Constant material colors use the normal GX material register; nonconstant vertex colors retain an indexed-array fallback. Existing fixed-point precision, float fallback, lighting, textures, transparency, shadows, mirrors and interpolation remain supported.

Array memory remains allocated while the FIFO reader uses it. The renderer drains pending readers before rewriting dynamic array data, invalidates the vertex cache for updated attributes, and drains outstanding readers during shutdown. No native executable, private renderer structures or platform-specific memory offsets are patched.

The eight character packet slots now remain reserved until their owning J3D draw buffers reset. Drawing a packet does not release it because interpolation can replay the same list. If all slots are occupied, that capture retains the native model instead of overwriting a packet already linked into a list. This closes a separately identified conditional queue-cycle risk; the dump does not prove that it caused the reported overflow.

Main-plus-shadow vertex traffic is now 184,768 bytes, down from v0.3.1's 531,778 bytes. The separate storage buffer receives approximately 438,170 bytes; both figures exclude alignment and additional views. Low-volume local log entries record completed-frame buffer usage when it reaches a new 128 KiB high-water band, to make any further failure easier to diagnose.

The v0.3.1 coat assets/solver and all existing weapon resources, effects, audio and gameplay modules remain unchanged. The mod ID and eight-platform combined-package workflow are unchanged. A full restart is required after replacing the package; selectors still apply at runtime.

The host's vertex and storage buffers are still finite. See `VALIDATION_v0.3.2.md` for measured budgets, completed tests and live-test limits. A successful build is not a claim that every other-mod combination is crash-free.
