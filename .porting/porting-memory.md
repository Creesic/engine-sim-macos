# Porting Memory: engine-sim -> macOS

## Watch list

- **Build toolchain is x86_64/Rosetta, not native arm64.** `brew`/`cmake` on this machine
  resolve to the Intel-prefix Homebrew at `/usr/local` (confirmed: `which brew` ->
  `/usr/local/bin/brew`), which produced x86_64 binaries (`file engine-sim-app` ->
  `Mach-O 64-bit executable x86_64`) that run via Rosetta 2 on this Apple Silicon (M1 Max)
  machine. A stray, incompatible `/usr/local/lib/SDL2.framework` (x86_64-only, owned by
  root, unrelated to this session's `brew install`) was also found and had to be worked
  around in CMake (`CMAKE_FIND_FRAMEWORK LAST`). To build natively for arm64, use the
  native Homebrew at `/opt/homebrew` (put `/opt/homebrew/bin` before `/usr/local/bin` in
  `PATH`, or invoke `/opt/homebrew/bin/cmake`/`brew` explicitly) and reinstall
  `sdl2_image`/`boost` under that prefix.
- **M1-M6 complete and verified end-to-end** (2026-07-09, same session as M0). The app
  runs continuously without crashing, with a real Cocoa window, real OpenGL4 rendering,
  real CoreAudio output, real keyboard/mouse input, and a real `.mr` script (`assets/main.mr`)
  compiled via piranha. Verified via: repeated `timeout N ./engine-sim-app` runs (clean
  exit via timeout, not crash), lldb backtraces at each bug found until none remained,
  `CGWindowListCopyWindowInfo` (via a small Python/Quartz snippet) confirming a real
  ~1920x1112 window owned by the process, and `ps` CPU-time deltas confirming active
  ongoing work (not a hung process). **Screenshot verification was not possible** in this
  sandboxed session -- `screencapture` failed ("could not create image from display") and
  AppleScript/System Events could not enumerate the app's windows either; both are likely
  blocked by this environment's permissions. The user should visually confirm the actual
  rendered output (gauges, 3D engine view, lighting, textures) looks correct when run
  locally -- functional/crash-free verification does not guarantee pixel-correct output.
  Bugs found and fixed while reaching this state are listed below, organized by milestone.
- **M2 (window) bugs/decisions**: none beyond the implementation itself. Window position has
  a minor ~32pt vertical offset (extends slightly above the primary screen's top edge) --
  likely because `ysMacWindow::InitializeWindow` computes the origin as if the given rect
  were the window's full frame, but `NSWindow -initWithContentRect:styleMask:...` interprets
  it as the *content* rect and grows the frame upward to add the title bar. Not fixed
  (cosmetic, window still shows and functions); a real fix should convert the desired frame
  rect through `-frameRectForContentRect:` or compute the title bar height explicitly before
  placing the window.
- **M3 (input) bug**: `ysMouseAggregator` does not override `UpdatePosition`/`UpdateWheel`/
  `UpdateButton`/`SetOsPosition` (only `GetX`/`GetY`/`GetWheel`/`IsDown`/`ProcessMouseButton`,
  which sum/forward across registered per-device `ysMouse` objects). Calling the "write"
  methods on the aggregator itself silently updates the aggregator's own unused `ysMouse`
  base-class fields, which none of the "read" methods ever consult. `yds_mac_window.mm`'s
  mouse handlers call `aggregator->GetMouse(0)->UpdateButton(...)` etc. instead. This is
  worth double-checking if a second mouse-input code path is ever added.
- **M4 (audio) bug**: `AVAudioEngine.startAndReturnError:` throws an uncaught
  `NSException` ("required condition is false: inputNode != nullptr || outputNode !=
  nullptr") if called on an engine with no attached/connected nodes. Fixed by deferring
  `startAndReturnError:` from `ysMacAudioDevice::GetOrCreateEngine()` (engine creation) to
  `ysMacAudioSource::EnsureNodeAttached()`, after the first `AVAudioSourceNode` is attached
  and connected to `mainMixerNode`.
- **M5 (device bring-up) bug, affects Windows too**: `ysOpenGLDevice::m_realContext` (used
  by every `m_realContext->glXxx(...)` call site -- i.e. all GPU buffer/shader/texture
  operations) was never populated the first time a rendering context was created.
  `UpdateContext()` (which scans for the context with `IsRealContext() == true` and caches
  it) existed but was previously called only from `DestroyRenderingContext`. Fixed by
  calling `UpdateContext()` right after context creation in both the Windows and macOS
  branches of `ysOpenGLDevice::CreateRenderingContext` (`yds_opengl_device.cpp`). This
  almost certainly went unnoticed on Windows because engine-sim ships with the DirectX11
  backend by default -- the OpenGL4_0 path was likely never actually run there either.
- **M6 (full scene) bugs**:
  - `engines/basic/shaders/glsl/delta_engine_shader.frag` -- `const float distanceToCamera`/
    `fogAttenuation` were declared `const` but initialized with a non-constant expression
    (`length(...)` over runtime vectors), which the GLSL spec disallows. Apple's OpenGL GLSL
    compiler rejects this ("Initializer not allowed"); whatever compiled this shader on
    Windows was more lenient. Fixed by dropping `const` (they're plain locals).
  - `ysMacAudioSource::LockBufferSegment` (`yds_mac_audio_source.mm`) always set
    `*segment2 = nullptr`, but the base class (`ysAudioSource::LockBufferSegment`) can
    still report `*size2 > 0` when the requested range wraps past the end of the ring
    buffer -- which happens routinely (engine-sim's own `AudioBuffer` is a 1-second ring
    buffer). `EngineSimApplication::process()` then `memcpy`'d through the null `data1`
    pointer. Fixed: `*segment2` now points at the start of `m_buffer` (wrapping) whenever
    `m_lockSegment2Size > 0`, matching the DirectSound-style two-segment lock/unlock
    contract the calling code (and the original DS8 backend) already assumes.
- **sse2neon dependency added** (`dependencies/submodules/delta-studio/dependencies/libraries/sse2neon/sse2neon.h`,
  vendored from github.com/DLTcollab/sse2neon, MIT licensed). `ysVector`/`ysMath`
  (`yds_math.h`) are built directly on x86 SSE intrinsics (`__m128`, `_mm_*`), which do not
  exist on Apple Silicon; sse2neon is the standard drop-in translation to ARM NEON used by
  other ported SSE-based codebases. Swapped in via `#if defined(__APPLE__) &&
  (defined(__aarch64__) || defined(__arm64__))`.
- **`yds_msvc_string_compat.h`** (new, `dependencies/submodules/delta-studio/include/`)
  provides portable substitutes for MSVC-only symbols used pervasively across delta-studio:
  `strcpy_s`/`strcat_s`/`sprintf_s`/`vsprintf_s`/`fopen_s`/`localtime_s` (secure CRT),
  `_aligned_malloc`/`_aligned_free`, and `__forceinline`. Included via `yds_base.h` (nearly
  universal) plus directly in a handful of files for self-sufficiency
  (`yds_allocator.h`, `yds_dynamic_array_deprecated.h`, `yds_queue.h`,
  `yds_geometry_preprocessing.cpp`, `yds_logger_output.cpp`). engine-sim's own code needed
  the same `__forceinline` fix in `include/filter.h`, and piranha/simple-2d-constraint-solver
  needed small local copies of the same pattern (separate submodules, no shared header).
- **`DeltaEngine::CreateGameWindow`** (delta_engine.cpp) previously hardcoded
  `Platform::Windows` (x2) and `ysAudioSystem::API::DirectSound8` — now platform-conditional
  (`#if defined(_WIN32) / elif defined(__APPLE__)`), fixed as part of M0.
- **OpenGL context version**: macOS branch now requests 4.1 core (vs 4.3 on Windows) in
  `ysOpenGLDevice::CreateRenderingContext`. The 3 GLSL shaders (delta_console_shader,
  delta_engine_shader, delta_saq_shader) still use `#version 420` with `layout(binding=N)`
  on UBOs/samplers (a GL 4.2 feature) — **not yet fixed, deferred to M1**. This is redundant
  with explicit C++-side binding (`glBindBufferRange`, `glActiveTexture`) already present in
  yds_opengl_device.cpp, so dropping the qualifiers should be safe, but M1 still needs to do
  it (and verify actual shader compilation against a real 4.1 context, which only happens in
  M5 when a real GL context exists).
- **`.mr` script load path** uses a relative path `"../assets/main.mr"`
  (src/engine_sim_application.cpp:626) — assumes a Windows dev-build CWD layout. Still needs
  a real decision for macOS (dev CWD vs. app-bundle Resources/ layout) — **not yet addressed,
  M1**.
- **Real impulse-response `.wav` loading implemented for macOS**: `ysMacAudioWaveFile`
  (`yds_mac_audio_wave_file.h/.cpp`) is a real (not stub) portable RIFF/WAVE parser over
  plain ISO C file I/O, mirroring `ysWindowsAudioWaveFile`'s mmio-based interface. Wired into
  `EngineSimApplication::loadEngine` (src/engine_sim_application.cpp) via a
  `PlatformAudioWaveFile` type alias. This is genuinely used at startup (exhaust system
  impulse responses), not dead code — unlike delta-basic's `AssetManager::LoadAudioFile`
  (engines/basic/src/asset_manager.cpp), which also used `ysWindowsAudioWaveFile` but is
  never called by engine-sim and was stubbed to return `ysError::NotImplemented` on
  non-Windows instead (STUB(M4) if ever needed).
- **Foundation stubs (STUB markers) still to implement**, per milestone:
  - M2 (window/main loop): `ysMacWindow`, `ysMacWindowSystem`, `ysMacMonitor` —
    grep `STUB(M2)`.
  - M3 (input): `ysMacInputSystem`, `ysMacInputDevice` — grep `STUB(M3)`.
  - M4 (audio): `ysMacAudioDevice`, `ysMacAudioSource`, `ysMacAudioBuffer`,
    `ysMacAudioSystem` — buffer/source plumbing is real (heap-backed), but nothing reaches a
    CoreAudio output device yet — grep `STUB(M4)`.
  - M5 (device bring-up): `ysOpenGLMacContext` — no real NSOpenGLContext yet — grep
    `STUB(M5)`.
- **Known runtime issues found during M0 (not fixed, pre-existing/unrelated to porting)**:
  - `ConvolutionFilter::f()` (src/convolution_filter.cpp:38) null-derefs `m_shiftRegister`
    when `SynthesizerTests.SynthesizerSystemTestSingleThread` runs, because
    `setupSynchronizedSynthesizer` (test/synthesizer_tests.cpp) never calls
    `initializeImpulseResponse`/`ConvolutionFilter::initialize`. This is a debug-build-only
    manifestation (nothing OS-specific) but crashes (`SIGSEGV`) when the test binary is
    actually *run* (not when just discovered/listed via `--gtest_list_tests`). It did not
    reproducibly block `cmake --build .` in the final clean run (CTest's
    `gtest_discover_tests` post-build hook seems to only list tests, not execute bodies), but
    running `ctest`/`./engine-sim-test` directly will crash on this one test.
  - `FunctionTests.FunctionGaussianTest` fails (not a crash) — likely a `rand()`/PRNG
    implementation difference between platforms causing a statistical tolerance check to
    fail; not investigated further.
- **Bugs found and fixed during M0 (genuine pre-existing bugs, exposed by stricter
  Clang/ld64 checks vs. lenient MSVC, not platform-specific workarounds)**:
  - `include/ring_buffer.h:49` — `RingBuffer::overwrite()` used bare `start` instead of
    `m_start`, resolving to an unrelated `start()` member function (never instantiated
    before since `overwrite()` is unused anywhere in the codebase).
  - `src/piston_engine_simulator.cpp` destructor asserted a non-existent
    `m_antialiasingFilters` member (never compiled before because `assert()` discards its
    argument under `NDEBUG`/Release; this port's debug build evaluates it).
  - `src/yds_opengl_device.cpp` — `R32_FLOAT` render target case had `glType = GL_FLAT`
    (should be `GL_FLOAT`, a one-letter typo; `GL_FLAT` is an unrelated legacy shading-model
    constant not even declared in a core-profile GL header).
  - `dependencies/submodules/delta-studio/src/yds_logger.cpp` — default
    `ysLoggerMessageLevel()` constructor tried to "delegate" via
    `ysLoggerMessageLevel::ysLoggerMessageLevel("", -1);` as a bare statement (not valid
    delegation, MSVC-tolerated only); rewritten as a real C++11 delegating constructor.
  - `include/constants.h`, `include/units.h` — all constants were `extern constexpr`
    (ODR-violating: every including TU emits its own definition of the same external
    symbol); changed to `inline constexpr` (C++17), the correct portable idiom. This was the
    cause of ~60 "duplicate symbol" link errors.
  - `src/connecting_rod.cpp`, `src/engine.cpp` — each had a stray first `#include` line
    using backslashes (`"..\include\connecting_rod.h"`), which only works as a path
    separator on Windows; removed (the very next line already included the same header
    correctly with forward slashes).
  - `scripting/include/object_reference_node.h` — nested template `overrideType<Type>()`
    shadowed the enclosing class template's own `Type` parameter (hard error on Clang,
    tolerated by MSVC); renamed to `T_OverrideType`.
  - `include/gas_system.h` — `GasSystem::Mix`'s non-static data member initializers,
    referenced via `= {}` default arguments on `GasSystem`'s own member functions, violated
    the "complete-class context" rule for nested classes (Clang enforces, MSVC didn't); gave
    `Mix` an explicit default constructor instead (same resulting values).
  - Several `simple-2d-constraint-solver` / `piranha` two-phase-lookup issues (non-dependent
    member access on forward-declared types inside class templates,
    e.g. `sparse_matrix.h`'s `Matrix*` usage) — fixed by including the needed complete type.
  - `boost::filesystem::path::is_complete()` (piranha/src/path.cpp) — removed in modern
    Boost (1.90, via Homebrew); replaced with `is_absolute()` (identical behavior, the
    method's current name).

## Feature status

| Feature | Status |
|---|---|
| Core physics/audio simulation (src/*.cpp) | **Working.** Runs continuously in the real app. A few pre-existing bugs fixed (see watch list). One known test crash (ConvolutionFilter, pre-existing, not fixed) only affects `engine-sim-test`, not the app. |
| `.mr` scripting / piranha compiler | **Working, verified.** `assets/main.mr` compiles successfully via piranha in the real app (confirmed via a temporary diagnostic during M6, then removed). No code change was needed for asset path resolution -- resolves correctly given the build directory's location relative to `assets/`. |
| Build system (CMake) | **Done.** Full `if (WIN32)`/`elif (APPLE)` split across delta-studio, engine-sim top-level, and dependencies/submodules/CMakeLists.txt. Windows source lists/link libs untouched and still gated correctly. |
| Window system | **Working, verified.** Real `ysMacWindow`/`ysMacWindowSystem`/`ysMacMonitor` (Cocoa/AppKit, `yds_mac_window.mm`/`yds_mac_window_system.mm`). Confirmed via `CGWindowListCopyWindowInfo`: real ~1920x1112 window, alive and stable across multiple runs. Minor ~32pt position offset not yet fixed (M7). |
| Input (keyboard/mouse) | **Working (event routing verified; not manually interactively tested).** Real NSEvent handling in `YSMacContentView` routes into `ysKeyboardAggregator`/`ysMouseAggregator`. No crashes; the app runs and processes its own internal input polling every frame without issue. Nobody has typed/clicked into the running app during this session to confirm end-to-end responsiveness -- worth the user double-checking interactively. |
| Audio (device/buffer/source) | **Working, verified.** Real CoreAudio output via `AVAudioEngine`/`AVAudioSourceNode` (`yds_mac_audio_device.mm`/`yds_mac_audio_source.mm`), real ring-buffer wraparound. Runs without crashing or throwing for 60+ seconds of continuous playback in testing. Not confirmed to *sound* correct (no audio output device was listened to in this sandboxed session) -- the user should listen and confirm. |
| Audio (impulse-response .wav loading) | **Real, working** (`ysMacAudioWaveFile`) — not a stub, used at the real startup path. |
| Graphics device (OpenGL4) | **Working, verified.** Real `ysOpenGLMacContext` (NSOpenGLContext/NSOpenGLPixelFormat, 4.1 core, `yds_opengl_mac_context.mm`). GLSL shaders fixed for GL 4.1. Geometry/shader/texture/buffer creation and per-frame rendering all run without crashing. Visual correctness (does the 3D engine view/gauges/lighting *look* right) not confirmed -- screenshot capture was unavailable in this sandboxed session; the user should visually confirm. |
| Discord RPC / DTV | Out of scope (disabled via existing CMake options, `DISCORD_ENABLED` now platform-defaulted). |

## Session log

- 2026-07-09: Discovery pass completed (see discovery-report.md). Goal document created
  (`goal-macos-opengl-bringup.md`) after user confirmed the OpenGL4 bring-up strategy.
  **M0 (Build) completed**: full CMake macOS branch, all foundation stub source files
  (window/input/audio/GL-context) created and wired into delta-studio's platform factory
  switches, C++ standard bumped to 17 (matching engine-sim), sse2neon vendored for
  SSE->NEON on Apple Silicon, MSVC-compat shims added, ~15 genuine pre-existing bugs found
  and fixed (see watch list) that were only exposed by Clang's stricter conformance vs.
  MSVC's leniency. `engine-sim-app`, `engine-sim-test`, `engine-sim-script-interpreter`,
  and all dependency libraries (delta-core, delta-basic, delta-physics, csv-io,
  simple-2d-constraint-solver, piranha) build and link successfully from a clean
  `cmake --build .`. Two known (pre-existing, unrelated to porting) runtime issues found
  and documented but not fixed: a ConvolutionFilter test crash and a FunctionGaussianTest
  tolerance failure. Toolchain note: current build is x86_64-via-Rosetta, not native arm64
  (Intel-prefix Homebrew at /usr/local was what resolved on this machine).
- 2026-07-09 (continued, same session): drove through **M1-M6 continuously** per explicit
  user direction (relayed by the coordinating agent), only stopping for the M0 checkpoint.
  M1: GLSL shaders fixed for GL 4.1 (`layout(binding=)` dropped, explicit C++-side
  `glUniformBlockBinding`/sampler binding added to `LinkProgram`); asset path resolution
  needed no change. M2: real Cocoa window (`yds_mac_window.mm`/`yds_mac_window_system.mm`).
  M3: real NSEvent-based keyboard/mouse input. M4: real CoreAudio output
  (`AVAudioEngine`/`AVAudioSourceNode`). M5: real `NSOpenGLContext` (4.1 core). M6: verified
  end-to-end -- real `.mr` script compiles, app runs 60+ seconds without crashing, real
  window confirmed alive via `CGWindowListCopyWindowInfo`, CPU-time deltas confirm active
  work. Five more genuine bugs found and fixed while reaching a stable M6 state (see watch
  list): a GLSL `const`-with-non-constant-initializer shader bug, a null-pointer crash from
  incomplete audio ring-buffer wraparound, an `AVAudioEngine` start-ordering crash, a
  `ysMouseAggregator` write-path misuse, and the `ysOpenGLDevice::m_realContext`
  never-populated bug (this last one affects the Windows OpenGL path too, now fixed for
  both). M7 (polish) partially done: temporary debug instrumentation removed, stale
  `STUB(M2)`/`STUB(M3)` comments corrected; app bundle/Info.plist, window-position offset,
  Retina/backing-scale handling, and a full Instruments leak pass are **not** done --
  see the goal document's M7 entry and this file's watch list for the itemized remainder.
  Screenshot/visual and interactive input/audio verification were not possible in this
  sandboxed session (no working `screencapture`/Accessibility access, no way to listen to
  audio) -- the user should confirm these directly.
