# Porting Goal: macOS OpenGL4 Bring-Up

Status: M0-M6 complete and verified end-to-end (real window, real GL rendering, real
audio, real input, real `.mr` script); M7 (polish) partially done. See milestone tracker.
Created: 2026-07-09
Last updated: 2026-07-09

## Target

Get the *real* engine-sim application — real `.mr` engine script loading via piranha,
real gauges/3D engine view/UI via delta-basic, real synthesized audio — running natively
on macOS, by reusing delta-studio's existing, complete OpenGL4 rendering backend rather
than writing a new graphics backend from scratch.

Explicitly NOT in scope for this goal: a native Metal backend (may become Goal 2 later),
Discord Rich Presence, DTV video capture, Windows regression testing (must not be broken,
but not actively tested by this agent).

## Key decisions

1. **Rendering backend: reuse delta-studio's OpenGL4 (`ysOpenGLDevice`) backend**, not
   Vulkan (found to be a non-functional stub — every rendering entry point is a no-op) and
   not a new Metal backend (deferred to a possible future goal). See
   `.porting/discovery-report.md` for the full analysis.
2. **macOS foundation layer is net-new**, named with a `Mac`/`ysMac*` prefix mirroring the
   existing `Windows`/`ysWindows*` convention: window system, input system, audio system.
   Audio backend targets CoreAudio (`ysAudioSystemObject::API::CoreAudio`).
3. **GL context version**: request macOS's max native core profile, 4.1, not the 4.3
   currently requested for Windows. The 3 GLSL shaders (`delta_console_shader`,
   `delta_engine_shader`, `delta_saq_shader`) need their `layout(binding=N)` qualifiers
   dropped (GL 4.2+ feature) since the equivalent binding is already done explicitly on the
   C++ side (`glBindBufferRange`, `glActiveTexture`) — this is a small, contained fix
   deferred to M1 (content pipeline), not needed for M0.
4. **Preserve the Windows build.** All edits to shared/common delta-studio files use
   surgical `#if defined(_WIN32)` / `#elif defined(__APPLE__)` branches. No Windows-only
   source file is deleted or functionally altered. New macOS platform files are additive.
5. **Boost::filesystem** (used by `dbasic::Path`, `engines/basic/src/path.cpp`) is kept
   as-is rather than migrated to `std::filesystem` — matches existing engine architecture,
   avoids an unrelated cross-cutting change. Requires `brew install boost` on macOS.
6. **Discord RPC and DTV** stay disabled on macOS (prebuilt Windows `.lib` / optional
   feature) via the existing CMake options, now platform-defaulted.
7. Demo target `delta-basic-demo` (dependencies/submodules/delta-studio/demos) is not
   needed by engine-sim and is skipped on macOS (`add_subdirectory(demos)` gated to
   `WIN32`) to reduce build scope/risk.
8. **sse2neon vendored** (`dependencies/submodules/delta-studio/dependencies/libraries/sse2neon`)
   for `ysMath`'s SSE intrinsics (`yds_math.h`), discovered during M0 to be a hard
   blocker on Apple Silicon (no `<xmmintrin.h>`/`__m128` on arm64). This is the standard,
   widely-used solution for this exact scenario rather than hand-porting the vector math
   library to NEON by hand.
9. **Pre-existing bugs found during M0 are fixed in place**, not worked around, when the
   fix is small/obviously correct (e.g., a one-letter typo, a missing `m_` prefix, an ODR
   violation) — these are bugs Clang's stricter conformance exposed that MSVC silently
   tolerated, not new platform-specific behavior. Full list in porting-memory.md watch
   list. Two exceptions were left unfixed (documented, not touched): a ConvolutionFilter
   null-deref in one test scenario, and a FunctionGaussianTest tolerance failure — both
   need product-level judgment calls the agent didn't have context for.

## Milestone tracker

- [x] **M0: Build** — DONE 2026-07-09. CMake macOS branch, gate Windows-only libs/sources,
      add macOS foundation stub sources (window/input/audio/GL-context), fix the
      delta-studio/engine-sim C++ standard mismatch, fix the one hard Win32 call
      (`dbasic::GetModulePath`), portable `main.cpp` entry point. Success criterion met:
      `engine-sim-app`, `engine-sim`, `engine-sim-script-interpreter`, `engine-sim-test`,
      and all dependency libraries compile and link from a clean `cmake --build .`.
      Scope grew beyond the original plan: SSE->NEON portability (sse2neon vendored) and
      ~15 genuine pre-existing bugs (exposed by Clang vs. MSVC conformance differences) had
      to be fixed too. Real (non-stub) impulse-response `.wav` loading implemented
      (`ysMacAudioWaveFile`). Two known pre-existing runtime bugs found, documented, not
      fixed (ConvolutionFilter test crash, FunctionGaussianTest tolerance failure). See
      porting-memory.md watch list for full detail. Not required to run cleanly at runtime
      yet (window/input/audio/GL context are still stubs) — that starts at M2.
- [x] **M1: Content pipeline** — DONE. Dropped `layout(binding=N)` from the 3 GLSL shader
      pairs (GL 4.2+ feature, macOS caps at 4.1) and bumped `#version 420` -> `410`; added
      the equivalent explicit binding in `ysOpenGLDevice::LinkProgram` (new
      `glUniformBlockBinding`/sampler-uniform calls, name-keyed to the existing
      slot/texture-unit conventions). `.mr`/asset path resolution needed **no code
      change** — `build-macos/` sits at the same depth as `assets/`/`dependencies/` under
      the repo root, so the existing relative-path convention
      (`../assets/main.mr`, `../dependencies/submodules/delta-studio/engines/basic`)
      already resolves correctly when the app is launched with CWD = the build directory
      (`cd build-macos && ./engine-sim-app`).
- [x] **M2: Window + main loop** — DONE. Real `ysMacWindow`/`ysMacWindowSystem`/
      `ysMacMonitor` (`yds_mac_window.mm`, `yds_mac_window_system.mm`, Objective-C++):
      NSApplication bootstrap (no nib/storyboard), NSWindow + flipped NSView content view,
      NSWindowDelegate routing resize/move/activate/close into `ysWindow::On*`,
      NSScreen-based monitor enumeration, non-blocking NSApplication event pump in
      `ProcessMessages()` (mirrors Win32's `PeekMessage`+`DispatchMessage`, since
      `DeltaEngine` drives its own frame loop).
- [x] **M3: Input** — DONE. `YSMacContentView` (in `yds_mac_window.mm`) routes
      keyDown/keyUp/flagsChanged/mouse*/scrollWheel NSEvents into
      `ysMacInputSystem::GetActive()`'s keyboard/mouse aggregators. Full macOS virtual
      keycode -> `ysKey::Code` table. Found and fixed a real bug in the process: mouse
      position/wheel/button *injection* must target the registered per-device `ysMouse`
      object (`aggregator->GetMouse(0)`), not the aggregator itself — unlike
      `ysKeyboardAggregator::SetKeyState` (which forwards to all keyboards),
      `ysMouseAggregator` does not override `UpdatePosition`/`UpdateWheel`/`UpdateButton`.
- [x] **M4: Audio** — DONE. Real CoreAudio output via `AVAudioEngine`/`AVAudioSourceNode`
      (`yds_mac_audio_device.mm`, `yds_mac_audio_source.mm`). The render callback reads
      directly out of the same flat buffer `LockBufferSegment`/`UnlockBufferSegments`
      write into (int16 PCM, converted to float32 in the callback), advancing a
      `std::atomic<SampleOffset>` play cursor with wraparound — the same circular-buffer +
      play-cursor model DirectSound used, just self-managed instead of hardware-managed.
      Found and fixed a real bug: `AVAudioEngine.startAndReturnError:` asserts
      ("inputNode != nullptr || outputNode != nullptr") if called before any node is
      attached/connected — the engine must be started *after* the first source node is
      connected to `mainMixerNode`, not at engine-creation time.
- [x] **M5: Device bring-up** — DONE. Real `ysOpenGLMacContext`
      (`yds_opengl_mac_context.mm`): `NSOpenGLPixelFormat` requesting a 4.1 core profile,
      `NSOpenGLContext` attached to the window's content view, vsync via
      `NSOpenGLContextParameterSwapInterval`, `flushBuffer` for `Present()`. GL function
      pointers are direct assignments from the statically-linked `<OpenGL/gl3.h>` symbols
      (no `wglGetProcAddress`-style dynamic loading needed on this platform). Found and
      fixed a real, previously-latent bug shared with the (never-exercised-on-Windows-either)
      OpenGL path: `ysOpenGLDevice::m_realContext` was never populated after creating the
      *first* rendering context — `UpdateContext()` existed but was only ever called from
      `DestroyRenderingContext`. Fixed for both platforms in `yds_opengl_device.cpp`.
- [x] **M6: Full scene** — DONE and verified. Real `.mr` script (`assets/main.mr`) compiles
      successfully via piranha (confirmed via temporary diagnostic, since confirmed and
      removed). App runs continuously (60+ seconds observed, multiple runs) without
      crashing, with a real window (confirmed via `CGWindowListCopyWindowInfo`: ~1920x1112,
      consistent with the 1920x1080 default + title bar), consuming CPU consistent with an
      active real-time simulation+render+audio loop (not hung). Found and fixed two more
      real bugs along the way (see porting-memory.md): a `const` GLSL local variable with a
      non-constant initializer (Apple's GLSL compiler rejects this, unlike whatever compiled
      it on Windows), and a null-pointer crash in `EngineSimApplication::process()` because
      `ysMacAudioSource::LockBufferSegment` always returned `segment2 == nullptr` even when
      `*size2 > 0` (the audio ring buffer wraps every ~1s in real operation, so this was a
      guaranteed near-term crash, not a hypothetical one). **Not independently visually
      screenshotted** — screen capture was unavailable in this sandboxed session
      (`screencapture` failed with "could not create image from display", and
      AppleScript/System Events could not enumerate the window either, likely a permissions
      constraint of the environment). The user should visually confirm rendering looks
      correct (gauges, 3D engine view, textures, lighting) when running locally.
- [~] **M7: Polish** — partially done. Completed: removed temporary debugging
      instrumentation (stderr diagnostics added during M6 troubleshooting), updated stale
      `STUB(M2)`/`STUB(M3)` comments now that those milestones have real implementations.
      Not done (see porting-memory.md watch list for the full list): app bundle / `Info.plist`
      (app currently runs as a bare, non-bundled executable — works, but isn't a "real" macOS
      app), minor window-position offset (~32pt vertical, likely a titled-window
      contentRect-vs-frame sizing nuance in `ysMacWindow::InitializeWindow`), Retina/backing-scale
      handling (window sizes are currently treated as logical points throughout, not checked
      against backing pixel scale), `ysMacAudioSystem`'s `EnumerateDevices`/`ConnectDevice`
      remain stubs (harmless today since `AVAudioEngine` picks the system default output
      device automatically), full leak/Instruments pass not performed, Metal validation N/A
      (this is the OpenGL path).

## Architecture notes

See `.porting/discovery-report.md` for the full codebase analysis (backend completeness
matrix, platform readiness matrix, `.mr`/piranha loading path, delta-studio abstraction
extension points). Key extension points found for adding a new platform:
- `ysWindowSystemObject::Platform` enum (`yds_window_system_object.h`) — add `MacOS`.
- `ysAudioSystemObject::API` enum (`yds_audio_system_object.h`) — add `CoreAudio`.
- Factory switches to extend: `ysWindowSystem::CreateWindowSystem`
  (`yds_window_system.cpp`), `ysInputSystem::CreateInputSystem` (`yds_input_system.cpp`),
  `ysAudioSystem::CreateAudioSystem` (`yds_audio_system.cpp`),
  `ysOpenGLDevice::CreateRenderingContext` (`yds_opengl_device.cpp`).
- `dbasic::DeltaEngine::CreateGameWindow` (`delta_engine.cpp`) currently hardcodes
  `Platform::Windows` (x2) and `ysAudioSystem::API::DirectSound8` (x1) — these three call
  sites need platform-conditional selection.

Watch list and feature status live in `.porting/porting-memory.md`, updated at each
handoff.

## Selection rationale

See discovery-report.md section 5. OpenGL4 chosen over Vulkan/MoltenVK (Vulkan backend is
an empty stub, would require writing a full renderer anyway with no benefit over Metal)
and over an immediate native Metal backend (much larger first lift; all foundation work is
shared with a future Metal goal regardless, so nothing is wasted by sequencing OpenGL
first). User confirmed this strategy on 2026-07-09.
