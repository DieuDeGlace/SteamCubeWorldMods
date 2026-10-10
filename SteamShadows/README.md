# SteamShadows

Made with the aid of AI (OpenAI ChatGPT/Codex), by DieuDeGlace.

D3D11 sun shadows with a 100-block default radius, loaded terrain casters and cached cloud casters. MoonLight provides nighttime shadow direction. Scene bindings are released before state restoration to allow swap-chain resizing. Clouds must have been observed before their cached models can cast.

See [root build instructions](../README.md#build-on-windows-x64). Build only this mod with `cmake --build build --target SteamShadows` from the repository root. Output: `build/SteamShadows.dll`. Configuration: `Mods/SteamShadows.ini`. Close the game to install or remove hooks; restart after binary/config changes.

## Crisp shadow edges

`SoftShadows=0` is the new default: one hardware-filtered comparison sample instead of the old nine-tap blur. A narrow antialiased edge is retained. `SoftShadows=1` restores the previous softer nine-tap filter. Settings are read at startup. Resolution remains 2048 and radius remains 100 by default; no larger map or additional render pass is introduced. Texture sampling work is reduced, but this is not a measured overall FPS guarantee.

The graphics fixture measured an edge transition of 8 pixels versus 20 with the previous filter at the same map size. Shadow coverage and scene-target release checks passed, along with the other nine tests. The fixture deliberately uses a low-resolution map to expose filter width; these pixel counts do not describe normal gameplay. In-game sharpness, aliasing and performance still depend on camera, light angle and scene complexity.
