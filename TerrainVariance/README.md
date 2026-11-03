# TerrainVariance

Made with the aid of AI (OpenAI ChatGPT/Codex), by DieuDeGlace.

Smooth added height variation, stronger hills, and up to 80-block mountain displacement. Underwater and low shore columns retain their native heights so water surfaces remain visible. Displacement fades in between native heights 16 and 80; higher inland terrain retains the full configured variation. Columns and six separately placed object arrays move together; placement indexes are adjusted. PC and mobile use identical terrain implementation, rules and regression tests. Collision/placement behavior has not been exhaustively tested for every object or world.

See [root build instructions](../README.md#build-on-windows-x64). Build only this mod with `cmake --build build --target TerrainVariance` from the repository root. Output: `build/TerrainVariance.dll`. Configuration: `Mods/TerrainVariance.ini`. Close the game to install or remove hooks; restart after binary/config changes.


## Alpha terrain integration groundwork

`AlphaCoordinates.h` is an original MIT-licensed integer mapping helper shared with
CubeVulkan. It keeps Alpha's absolute finite 24-bit grid separate from Steam's signed
coordinates, and splits a 64-column Steam tile across its 256-column Alpha parents.
It handles partial world-edge coverage without wrapping. `AlphaCoordinatesTest`
checks full tile coverage, parent boundaries and clipped edges.

This helper is preparation, not an enabled Alpha terrain mode. The current DLL's
height, shoreline, water and object-placement behavior is unchanged. Full Alpha
surface generation cannot be replaced by simply translating Steam columns. Further
height/profile integration and native-game validation are still needed.
