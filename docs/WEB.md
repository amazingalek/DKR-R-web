# The web page

This fork's `main` branch also builds with [Emscripten](https://emscripten.org) into a page, `DKR-R.html`, that draws
through WebGPU: **https://amazingalek.github.io/DKR-R-web/**. The game's code is recompiled from the decomp's ELF of US
v1.0 by DKR-R's own pipeline, and [N64Recomp-browser](https://github.com/amazingalek/N64Recomp-browser)
(`lib/N64Recomp-browser`) patches RT64, plume, hlslpp, N64ModernRuntime and N64Recomp with the web support and makes the
game a page. Everything the page needs is behind `__EMSCRIPTEN__` or `if(EMSCRIPTEN)`, so the desktop builds are
unchanged.

## What the page leaves out

- **DKR-R's launcher and overlay.** Both draw through ImGui on an SDL renderer or RT64's inspector, neither of which
  a page has. The page has a launcher of its own (`runtime-recomp/src/game/web_page.cpp`): Load ROM, which opens the
  browser's file picker, and Start Game once a ROM is kept. The ROM stays in the browser (IndexedDB, through
  N64Recomp-browser's `persistent_home.js`). The game runs on the settings it has: Original refresh (30 frames a
  second, as the desktop's default), Original water, no generated mipmaps.
- **Online play, mods and custom tracks.** Online play needs WebRTC's transport (`DKR_NETPLAY_WEBRTC` off), and mods
  are imported by a worker process, which a page can't start (`legacy_mod_process.cpp`). N64ModernRuntime's live
  recompiler (sljit) emits machine code, so the runtime's mods are off on every page too.
- **Enhanced water**, whose shaders are Vulkan's (`DKR_ENHANCED_WATER` off), the SDL3 input host (a process of its
  own: the page reads keys and gamepads through Emscripten's SDL2), the tests, and Rev A: the page holds US v1.0's code
  alone, since Rev A's would be a second copy of the game in the wasm.

## How it is built

`scripts/build_web.sh <US v1.0 ROM>` does all of it on Linux with emsdk active, naga
(`cargo install naga-cli`), spirv-opt and, for faster rebuilds, ccache on `PATH`. Its header says each step:

1. The pinned dependencies the page needs, fetched by `scripts/bootstrap_dependencies.py`.
2. DKR-R's own patches to them (`scripts/apply_dependency_patches.py`), then N64Recomp-browser's web patches on top
   (`lib/N64Recomp-browser/patches/apply.cmake`, project by project). Where the two change the same lines,
   `apply.cmake` leaves those files out and `patches/web/<project>.patch` makes the web's change on top of DKR-R's;
   where a stage of DKR-R's own (`runtime-recomp/cmake/StagedPatch.cmake`) meets the web's lines,
   `patches/web/stage/<stage>.patch` is that stage written on top of them.
3. N64Recomp built natively from the patched checkout: its `funcs.h` declares every function the recompiled code
   calls, which a WebAssembly link needs.
4. The decomp's ELF of US v1.0 and the recompiled code from it.
5. The page, with `emcmake cmake -S runtime-recomp -B build-web`, then `ninja -C build-web DKRPortGame`.

`build-web/bin/Release` then holds `DKR-R.html`, `.js`, `.wasm`, `.data` and `coi_serviceworker.js`. Serve that folder
with `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp`, or as is on a host
that can't set headers, where `coi_serviceworker.js` sets them after one reload. It needs a browser with WebGPU (Chrome
or Edge 113 and up, or Firefox 141 and up). On the page the keyboard is the desktop's (WASD the stick, Space A, Shift
B, Z, Enter Start, Q and E the shoulders, IJKL the C buttons).

## What the page does differently

- **Sound** goes to N64Recomp-browser's `audio_output.h`, an AudioWorklet on the browser's audio thread reading a ring
  in the program's memory, through an SDL audio stream from the game's rate to the page's: Emscripten's SDL2 plays its
  queue from the page's thread, where WebGPU's work runs, and breaks up whenever that thread is busy
  (`runtime_platform.cpp`).
- **Input** is read whatever SDL's window focus says: SDL's focus on a page follows the window's focus events, which
  a page opened behind another window never gets, though its tab gets keys.
- **Render to RAM is off** (`rt64_renderer.cpp`): in Chrome RT64's readback of every frame holds each display list
  for the GPU process. The game's CPU reads a frame only to hold the last one on screen while a level unloads, which
  doesn't need it (`rt64_renderer.cpp` says why).
- **The heap is the frontend's fixed 1536 MB**, without memory growth, which with threads slows the page's JavaScript:
  with growth on, it never grew past that through the shader list's recording and 27 maps in one visit.

## Prebuilt shaders

The page builds the shaders the game uses before it draws with them, so it never draws with RT64's slow ubershader in
the middle of a race: `web/rt64_shaders.txt` lists them in sections (its header says what each holds), and the build
specializes each one (`RT64_PREBUILT_SHADER_LIST` in `runtime-recomp/CMakeLists.txt`). In Chrome the whole list is
built behind the game from its first frame, and the game waits only when a frame wants 4 or more of a section's
shaders that aren't built yet, under a small "Preparing graphics" indicator in a corner. Firefox builds a pipeline in
line with the rest of its GPU work, so there those above the first section (the logos and the title) are built before
the game starts, and a section when the game first draws it, the game waiting. A shader the list misses is built
behind the game, and the page remembers it and builds it with the rest on the next visit.

The list holds 107 descriptions (134 different shader stages, 1.5 MB of WGSL) in 12 sections: the logos and the title
first, then the menus, Timber's Island, each of the five worlds (its hub, races, battle, boss race and trophy race),
the trophy podium and a race's results. A world's section holds every description its maps draw, those another world
shares too; each is built once.

`web/record_shaders.sh` records the list (its header says how): N64Recomp-browser's recorder drives the page in
headless Chrome through `web/shader_plan.json`, a page load per map, in which `web/shader_survey.js` takes the game to
the map through its own teleport and drives the player's kart toward each checkpoint. The game's AI can't drive the
player's kart instead: DKR-R's display list check then rejects every frame. A recording takes about 50 minutes.

## Played

From Pages (main 082ab50), cold, an RTX 2070, a quiet host, 2026-10-16, through `web/play_plan.json` (the title, the
menus, Timber's Island and a Dino Domain race): Chrome 155 headless waited for no shader and drew 30.0 presents a
second, the game's own rate (RT64's Original refresh presents each frame the game draws); in a window, 30.0; with the
page's thread slowed 3 times (Chrome's CPU throttling, a stand-in for a laptop's CPU), 30.0. Firefox 158 waited 2.4 s
for the boot's 23 shaders, 0.5 s for the menus and 0.6 s for the race, then drew 30.0. At 3840x2000, uncapped, 29.8
in Chrome and 30.0 in Firefox. No sound short, no WebGPU errors, in any of them.
