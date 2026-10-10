# MoonLight

Made with the aid of AI (OpenAI ChatGPT/Codex), by DieuDeGlace.

A procedural bluish-white moon opposite the sun, subtle night lighting, dimmer night clouds and a moon-shadow direction shared with SteamShadows. Requires SteamShadows enabled for moon shadows.

See [root build instructions](../README.md#build-on-windows-x64). Build only this mod with `cmake --build build --target MoonLight` from the repository root. Output: `build/MoonLight.dll`. Configuration: `Mods/MoonLight.ini`. Close the game to install or remove hooks; restart after binary/config changes.

Sun/moon shadow angles now follow the world clock rather than the fixed native lighting vector. Directional strength fades near the horizon at dusk/dawn; moon rise is at 18:00 and set at 06:00. The moon disk shares this clock-driven trajectory. No additional shadow map or pass is added. Native daytime material lighting is unchanged. Live visual verification remains required.
