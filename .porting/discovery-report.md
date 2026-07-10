# Discovery Report: engine-sim -> macOS

Date: 2026-07-09
Repo: /Users/tera/Documents/GitHub/engine-sim (branch master @ 85f7c3b)

## 1. What engine-sim actually is

- `engine-sim` (static lib): pure-C++ physics/audio simulation (gas dynamics, crankshaft,
  combustion, synthesizer, etc.) — src/*.cpp, include/*.h. No Windows dependency found.
- `engine-sim-script-interpreter` (static lib): compiles `.mr` files (assets/main.mr,
  es/**/*.mr, assets/engines/**/*.mr) via the `piranha` submodule into the C++ simulation
  objects (scripting/src/compiler.cpp, engine_context.cpp, language_rules.cpp). Pure C++,
  no Windows dependency.
- `engine-sim-app` (WIN32 executable): the real UI — gauges, 3D piston/crank rendering,
  oscilloscopes, engine_view, etc. (src/engine_sim_application.cpp + ~25 UI/render source
  files). This is what must run natively on macOS to have "real functionality," not a
  reimplementation.
- Entry point `src/main.cpp` is a 20-line `WinMain` wrapper: constructs
  `EngineSimApplication`, calls `initialize(hInstance, ysContextObject::DeviceAPI::DirectX11)`,
  `run()`, `destroy()`. Trivial to replace with a `main()`.
- Dependencies confirmed portable, pure C++, no Win32 code found: `piranha`, `csv-io`,
  `simple-2d-constraint-solver`, `direct-to-video` (DTV, default OFF anyway).
- `dependencies/discord` (Discord Rich Presence) links a prebuilt Windows `.lib`
  (`discord-rpc.lib`) — must be disabled for macOS (`DISCORD_ENABLED=OFF`, option already
  exists in top CMakeLists.txt).

## 2. delta-studio: the actual rendering/windowing/audio engine

engine-sim links against `delta-basic` (engines/basic), which sits on top of `delta-core`
(dependencies/submodules/delta-studio). delta-studio is **already a multi-backend engine**,
not DirectX-only:

- Graphics device abstraction: `ysDevice` (yds_device.h), factory `ysDevice::CreateDevice(api)`
  with `DeviceAPI { DirectX10, DirectX11, OpenGL4_0, Vulkan }`.
- Window abstraction: `ysWindowSystem` / `ysWindow`, factory keyed by
  `ysWindowSystemObject::Platform { Windows, ... }`. Only a Windows implementation exists
  (`yds_windows_window*.cpp`, raw Win32 `CreateWindow`/`WndProc`, ~480 lines total).
- Input abstraction: `ysInputSystem` / keyboard+mouse aggregators, only a Windows
  implementation exists (`yds_windows_input_*.cpp`, raw Win32 raw-input, ~735 lines
  equivalent to replace, plus a key-map table).
- Audio abstraction: `ysAudioSystem` / `ysAudioDevice` / `ysAudioBuffer` / `ysAudioSource`,
  only a DirectSound8 implementation exists (`yds_ds8_*.cpp`, ~466 lines). Note:
  `DeltaEngine::CreateGameWindow` **hardcodes** `ysAudioSystem::API::DirectSound8` (not
  parameterized like the graphics API is) — this is a spot that needs a small signature
  change, not just a new subclass.
- SDL2 is already linked by delta-core, but is **only** used for `SDL_image` texture
  loading in the OpenGL backend (`yds_opengl_device.cpp`), not for windowing/input.

### Per-backend completeness (this was the key finding driving the strategy below)

| Backend | Files / LOC | Status |
|---|---|---|
| DirectX11 (`yds_d3d11_*.cpp`) | ~1873 LOC | Full, in-use reference implementation (what engine-sim ships with today) |
| OpenGL4 (`yds_opengl_*.cpp`) | ~1305 LOC device + 322 LOC Windows GL-context | **Fully implemented**, real GL calls throughout (buffers, shaders, textures, render targets, draw). Only 1 stub-like empty-body function in the whole device. |
| Vulkan (`yds_vulkan_*.cpp`) | ~527 LOC total | **Non-functional skeleton.** Instance/device creation only; every single rendering entry point (buffers, shaders, textures, render targets, `ClearBuffers`, `Present`, `Draw`) is `return ysError();` — an empty no-op. This is unimplemented, not a working renderer. |

Implication: "retarget via MoltenVK" is **not** a shortcut — it would mean writing an entire
Vulkan renderer from scratch (the same order of work as writing a new Metal backend) *and*
adding a MoltenVK translation layer on top, *and* still writing all of window/input/audio.
It has no advantage over writing a native Metal backend directly, and is worse for
debuggability (extra translation layer) and dependencies (MoltenVK is not a first-party
Apple framework).

The OpenGL4 backend, by contrast, is a complete, working renderer today. GLSL sources
already exist and mirror the HLSL set 1:1 (both are tiny — 3 shader programs total:
`delta_console_shader`, `delta_engine_shader`, `delta_saq_shader` — console/UI, 3D
model+gauges, and a screen-aligned-quad respectively). Switching engine-sim's device API
from `DirectX11` to `OpenGL4_0` is a one-line change at the two call sites
(`src/main.cpp`, and the API is threaded through `DeltaEngine::GameEngineSettings::API`).

**Caveat found:** the existing GLSL shaders are `#version 420` and
`ysOpenGLDevice::CreateRenderingContext` requests a **4.3** context
(`yds_opengl_device.cpp:59`, `CreateRenderingContext(this, window, 4, 3)`). macOS's native
OpenGL implementation caps at **4.1 core** — it will never give you 4.2 or 4.3. The shaders
use `layout(binding=N)` on UBOs and samplers, which needs GL 4.2
(`ARB_shading_language_420pack`)/4.5 semantics. However, the C++ side already does the
binding explicitly at runtime (`glBindBufferRange(GL_UNIFORM_BUFFER, slot, ...)` at
yds_opengl_device.cpp:599, `glActiveTexture(slot + GL_TEXTURE0)` at ~1078-1101), so the
`layout(binding=)` qualifiers in the shaders are redundant with the C++-side binding, not
load-bearing. Fix is small and well-scoped: request a 4.1 core context on macOS, drop the
`layout(binding=)` qualifiers from the 3 shader pairs, and add the two corresponding
GL-3.1-era calls that are 4.1-safe (`glUniformBlockBinding` for UBOs is core since 3.1;
samplers need one `glUniform1i(loc, slot)` per sampler after linking, which may already
happen — needs to be verified/added during that milestone). This is a handful of files,
not an unknown.

## 3. `.mr` script loading path (piranha)

`EngineSimApplication::compile()` calls `compiler.compile("../assets/main.mr")`
(src/engine_sim_application.cpp:626) — a **relative path**, assuming CWD is set up as it
is on Windows dev builds (exe runs from a build subdir with assets one level up). Piranha
and the `.mr` grammar/compiler themselves are pure, portable C++ (no Win32 found anywhere
in `piranha/`). The only macOS-specific concern is **working-directory / resource-path
resolution** — needs a real decision (dev-time CWD vs. app-bundle `Resources/` layout) but
is not an architectural blocker. This is the exact system the previous (discarded) attempt
bypassed by hardcoding a fake engine in C++ instead of loading real `.mr` content — this
port must go through this real path.

## 4. Platform readiness matrix (foundation)

| Subsystem | Windows impl | macOS impl | Work needed |
|---|---|---|---|
| Build system | CMake, but delta-studio's CMakeLists.txt unconditionally links D3DX/DXGI/DirectSound/Vulkan Windows .lib files and assumes MSVC | none | Gate Windows-only link lines behind `if (WIN32)`, add macOS branch (frameworks, SDL2/SDL2_image via Homebrew, no D3DX/DXGI/DirectSound/Vulkan libs) |
| Window system | Win32 `CreateWindow`/`WndProc` (~480 LOC) | none | New `ysWindow`/`ysWindowSystem` subclass, Cocoa/AppKit-based |
| Input | Win32 raw input (~735 LOC incl. key maps) | none | New keyboard/mouse subclass, AppKit `NSEvent`-based (+ optionally GameController for controller support later) |
| Audio | DirectSound8 (~466 LOC) | none | New `ysAudioDevice`/`ysAudioBuffer`/`ysAudioSource`, CoreAudio-based (needs the `CreateAudioSystem` hardcoded-API fix mentioned above) |
| Graphics device | D3D11 (shipping), D3D10, OpenGL4 (complete, unused today), Vulkan (stub) | none | **Recommended: switch to the existing OpenGL4 backend** + macOS GL context (NSOpenGLContext/CGL) + the 4.1/shader fix above. Defer a native Metal backend to a later goal. |
| Discord RPC | prebuilt Windows .lib | n/a | Disable via existing `DISCORD_ENABLED=OFF` option |
| DTV video capture | optional, default OFF | n/a | Leave OFF, not a blocker |
| Scripting (.mr/piranha) | portable C++ | portable C++ | Should build as-is; only the asset path resolution needs attention |

## 5. Strategy recommendation

Two viable paths were evaluated:

1. **Native Metal backend for delta-studio** (`ysMetalDevice` etc., mirroring the D3D11
   backend's ~1800 LOC feature set) — the "proper," modern, non-deprecated long-term
   answer, and what the loaded Metal skill set is built for. Large first lift: full
   GPU backend from scratch (buffers, pipelines, textures, render targets, draw/present)
   plus HLSL->MSL shader rewrite (small: 3 shaders) plus all foundation work below.
2. **Bring up on delta-studio's existing, complete OpenGL4 backend** — reuses ~1300 LOC of
   already-working rendering code and the already-existing GLSL shaders (small fix needed
   for the 4.1 cap, see above). Only net-new code is the foundation layer (window/input/
   audio/GL-context) that *either* strategy needs regardless, since delta-studio has no
   macOS platform layer of any kind today.

Recommendation: **Goal 1 = bring up on OpenGL4** to get the *real* engine-sim (real `.mr`
loading, real gauges/3D rendering/UI, real audio) running natively and verifiably as fast
as possible with the lowest amount of net-new/novel code, deferring the higher-risk, larger
Metal rendering backend to an optional **Goal 2** once the app is demonstrably working end
to end. This follows the methodology's goal-ordering principle (smallest feature delta,
simplest viable target first) and decouples "get it working" from "make it modern," since
all foundation work (window/input/audio) is shared between both goals and isn't wasted
either way.

Caveat to flag to the user explicitly: OpenGL is deprecated by Apple (since macOS 10.14)
but still present and functional on current macOS. This is a real trade-off the user should
confirm before we commit to it.
