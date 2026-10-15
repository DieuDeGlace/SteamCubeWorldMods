# DualHair — two-tone hair

Gives chosen player characters Omega-style two-colour hair. The top of the hair stays the normal in-game hair colour and fades down into a second colour toward the tips. Targets CubeModLoader API 7.3 and the inspected Steam executable. It also runs on the GameNative copy of that build, which has the same code with the SteamStub `.bind` section stripped.

## How it works

The native creature renderer queues the hair model from one call site, with a material colour of base × hair colour. For a configured character, DualHair queues a recoloured copy of that hair model instead:

- The copy is made with the game's own `Sprite` copy constructor.
- Its voxels get a top-to-bottom gradient, from the character's hair colour into the second colour.
- The game's own mesher rebuilds the copy's vertex and index buffers.
- The hair tint is divided back out of the queued colour, so the top of the hair matches vanilla.

Everything after the queue (lighting, shadows, TAA, other mods) sees an ordinary native model draw. The mod adds no shaders, D3D hooks or per-frame scans.

Each hairstyle and colour pair is built once and cached. A copy that has gone unused for 10 seconds is freed through the native destructor; the cache holds at most 64 copies. Recoloured voxels never become empty or a pure marker colour. The hook checks the bytes at the call site, the hair-sprite read, and the copy, mesh and allocator prologues. If any of them differ (another build, or another mod changed them), DualHair stays off and says so in `Mods/DualHair.log`.

## Use

Install `DualHair.dll` and `config/DualHair.ini` into `Mods`, then type in chat:

| Command | Effect |
|---|---|
| `/hair2 R G B` or `/hair2 #RRGGBB` | Set your character's second hair colour |
| `/hair2` | Show the current setting |
| `/hair2 off` | Back to one colour |
| `/hair2 start N` | Where the second colour begins, as a percentage of the way down the hair (default 40) |
| `/hair2 blend N` | Length of the fade between the colours (default 35; 0 = hard split) |

Commands take effect immediately and are saved by character name under `[Characters]` in `DualHair.ini`. Other players' characters get two-tone hair when their name is listed in your ini (it is a local visual only; nothing is sent over the network). Change the first colour as normal in game.

## Build

```bat
cmake --build build --target DualHair DualHairRulesTest
ctest --test-dir build -R DualHairRules --output-on-failure
```

`DualHairRules` covers colour parsing, the gradient shape, voxel shading (the top matches the native tint, and the result is never empty or a marker colour) and tint removal. The in-game look, the gradient direction on every hairstyle and the interaction with installed shadow/TAA mods still need a live check.
