# SteamShadows

Made with the aid of AI (OpenAI ChatGPT/Codex), by DieuDeGlace.

D3D11 sun shadows with a 100-block default radius, loaded terrain casters and cached cloud casters. MoonLight provides nighttime shadow direction. Scene bindings are released before state restoration to allow swap-chain resizing. Clouds must have been observed before their cached models can cast.

See [root build instructions](../README.md#build-on-windows-x64). Build only this mod with `cmake --build build --target SteamShadows` from the repository root. Output: `build/SteamShadows.dll`. Configuration: `Mods/SteamShadows.ini`. Close the game to install or remove hooks; restart after binary/config changes.
