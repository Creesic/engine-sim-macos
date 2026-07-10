cmake_minimum_required(VERSION 3.10)

# Some vendored dependencies (googletest, etc., pulled in via FetchContent)
# declare an old cmake_minimum_required that a sufficiently new CMake (4.x)
# refuses to honor outright. Only take effect if the invoker hasn't already
# picked a policy floor.
if (NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif ()

# Default to an optimized build. With a single-config generator (Makefiles /
# Ninja) an unset CMAKE_BUILD_TYPE means NO optimization flags at all (-O0),
# which for this real-time physics simulator is ~5-15x slower -- the
# difference between ~6 fps and full refresh rate. The Visual Studio build
# this was ported from always ships an optimized Release config; match that
# by default here while still allowing an explicit -DCMAKE_BUILD_TYPE=Debug.
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif ()

# On Apple Silicon, an x86_64 CMake/toolchain resolved via PATH (e.g. Intel
# Homebrew at /usr/local, still present after a Rosetta-based Homebrew
# install) silently produces an x86_64-via-Rosetta build instead of a native
# arm64 one. That is more than a performance issue for this project: the
# native macOS windowing/rendering backend has been observed to create a
# window that exists (correct geometry, real GPU work) but never actually
# becomes visible on screen when run under Rosetta. Prefer native arm64
# Homebrew so a plain `cmake -S . -B build` does the right thing by default.
#
# `uname -m` is not a reliable way to detect this: if the `cmake` binary
# itself is x86_64 and running under Rosetta, `uname -m` reports the
# *translated* architecture (x86_64), not the real hardware. `sysctl
# hw.optional.arm64` reports true hardware capability regardless of
# translation, matching the check the discarded first-draft macos.cmake used.
#
# CMAKE_OSX_ARCHITECTURES must be set before project()/enable_language() to
# take effect, but the ordinary `APPLE` variable isn't populated until after
# that call -- use CMAKE_HOST_APPLE (available immediately, reflects the
# host running CMake) instead.
if (CMAKE_HOST_APPLE)
    execute_process(COMMAND sysctl -n hw.optional.arm64
        OUTPUT_VARIABLE ENGINE_SIM_HAS_ARM64 OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if (ENGINE_SIM_HAS_ARM64 STREQUAL "1")
        # A plain set() here is not enough: project()'s own Darwin platform
        # setup creates a CMAKE_OSX_ARCHITECTURES cache entry defaulted to
        # empty and that shadows/overwrites a same-named normal variable.
        # Write directly to the cache (without FORCE) so it takes effect but
        # an explicit -DCMAKE_OSX_ARCHITECTURES=... from the invoker still wins.
        if (NOT DEFINED CACHE{CMAKE_OSX_ARCHITECTURES} AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
            set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architecture")
        endif ()
        if (NOT DEFINED CMAKE_PREFIX_PATH AND EXISTS "/opt/homebrew")
            set(CMAKE_PREFIX_PATH "/opt/homebrew")
        endif ()
    endif ()
endif ()

option(DTV "Enable video output" OFF)
option(PIRANHA_ENABLED "Enable scripting input" ON)

# Discord Rich Presence links a prebuilt Windows-only static library
# (dependencies/discord/lib/discord-rpc.lib) and has no macOS build; default
# it off there. Still overridable via -DDISCORD_ENABLED=ON/OFF.
if (APPLE)
    option(DISCORD_ENABLED "Enable Discord Rich Presence" OFF)
else ()
    option(DISCORD_ENABLED "Enable Discord Rich Presence" ON)
endif ()

if (DTV)
    add_compile_definitions(ATG_ENGINE_SIM_VIDEO_CAPTURE)
endif (DTV)

if (PIRANHA_ENABLED)
    add_compile_definitions(ATG_ENGINE_SIM_PIRANHA_ENABLED)
endif (PIRANHA_ENABLED)

if (DISCORD_ENABLED)
    add_compile_definitions(ATG_ENGINE_SIM_DISCORD_ENABLED)
endif (DISCORD_ENABLED)

# Enable group projects in folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "cmake")

project(engine-sim)

set(CMAKE_CXX_STANDARD 17)

# ========================================================
# GTEST

include(FetchContent)
FetchContent_Declare(
    googletest
    URL
    https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set_property(TARGET gmock PROPERTY FOLDER "gtest")
set_property(TARGET gmock_main PROPERTY FOLDER "gtest")
set_property(TARGET gtest PROPERTY FOLDER "gtest")
set_property(TARGET gtest_main PROPERTY FOLDER "gtest")

# ========================================================

add_library(engine-sim STATIC
    # Source files
    src/audio_buffer.cpp
    src/camshaft.cpp
    src/crankshaft.cpp
    src/combustion_chamber.cpp
    src/connecting_rod.cpp
    src/convolution_filter.cpp
    src/cylinder_bank.cpp
    src/cylinder_head.cpp
    src/delay_filter.cpp
    src/derivative_filter.cpp
    src/direct_throttle_linkage.cpp
    src/dynamometer.cpp
    src/engine.cpp
    src/exhaust_system.cpp
    src/feedback_comb_filter.cpp
    src/filter.cpp
    src/fuel.cpp
    src/function.cpp
    src/gas_system.cpp
    src/gaussian_filter.cpp
    src/governor.cpp
    src/ignition_module.cpp
    src/impulse_response.cpp
    src/intake.cpp
    src/jitter_filter.cpp
    src/leveling_filter.cpp
    src/low_pass_filter.cpp
    src/part.cpp
    src/piston.cpp
    src/piston_engine_simulator.cpp
    src/simulator.cpp
    src/standard_valvetrain.cpp
    src/starter_motor.cpp
    src/synthesizer.cpp
    src/throttle.cpp
    src/transmission.cpp
    src/utilities.cpp
    src/valvetrain.cpp
    src/vehicle.cpp
    src/vehicle_drag_constraint.cpp
    src/vtec_valvetrain.cpp

    # Include files
    include/audio_buffer.h
    include/application_settings.h
    include/camshaft.h
    include/crankshaft.h
    include/combustion_chamber.h
    include/connecting_rod.h
    include/convolution_filter.h
    include/cylinder_bank.h
    include/cylinder_head.h
    include/delay_filter.h
    include/derivative_filter.h
    include/direct_throttle_linkage.h
    include/dynamometer.h
    include/engine.h
    include/exhaust_system.h
    include/feedback_comb_filter.h
    include/filter.h
    include/fuel.h
    include/function.h
    include/gas_system.h
    include/gaussian_filter.h
    include/governor.h
    include/ignition_module.h
    include/impulse_response.h
    include/intake.h
    include/jitter_filter.h
    include/leveling_filter.h
    include/low_pass_filter.h
    include/part.h
    include/piston.h
    include/piston_engine_simulator.h
    include/simulator.h
    include/standard_valvetrain.h
    include/starter_motor.h
    include/synthesizer.h
    include/throttle.h
    include/transmission.h
    include/units.h
    include/utilities.h
    include/valvetrain.h
    include/vehicle.h
    include/vehicle_drag_constraint.h
    include/vtec_valvetrain.h
)

target_link_libraries(engine-sim
    simple-2d-constraint-solver
    csv-io
    delta-basic)

target_include_directories(engine-sim
    PUBLIC dependencies/submodules)

if (PIRANHA_ENABLED)
    add_library(engine-sim-script-interpreter STATIC
        # Source files
        scripting/src/channel_types.cpp
        scripting/src/compiler.cpp
        scripting/src/engine_context.cpp
        scripting/src/language_rules.cpp

        # Include files
        scripting/include/actions.h
        scripting/include/camshaft_node.h
        scripting/include/channel_types.h
        scripting/include/compiler.h
        scripting/include/connecting_rod_node.h
        scripting/include/crankshaft_node.h
        scripting/include/cylinder_bank_node.h
        scripting/include/cylinder_head_node.h
        scripting/include/engine_context.h
        scripting/include/engine_node.h
        scripting/include/engine_sim.h
        scripting/include/exhaust_system_node.h
        scripting/include/intake_node.h
        scripting/include/function_node.h
        scripting/include/ignition_module_node.h
        scripting/include/ignition_wire_node.h
        scripting/include/impulse_response_node.h
        scripting/include/intake_node.h
        scripting/include/language_rules.h
        scripting/include/node.h
        scripting/include/object_reference_node.h
        scripting/include/object_reference_node_output.h
        scripting/include/piranha.h
        scripting/include/piston_node.h
        scripting/include/rod_journal_node.h
        scripting/include/standard_valvetrain_node.h
        scripting/include/transmission_node.h
        scripting/include/valvetrain_node.h
        scripting/include/vtec_valvetrain_node.h
		scripting/include/vehicle_node.h
    )

    target_include_directories(engine-sim-script-interpreter
        PUBLIC dependencies/submodules)

    target_link_libraries(engine-sim-script-interpreter
        csv-io
        piranha)
endif (PIRANHA_ENABLED)

if (DISCORD_ENABLED)
    add_library(discord STATIC
        # Source files
        dependencies/discord/Discord.cpp

        # Include files
        dependencies/discord/Discord.h
        dependencies/discord/discord_register.h
        dependencies/discord/discord_rpc.h
    )
endif (DISCORD_ENABLED)

add_executable(engine-sim-app WIN32
    # Source files
    src/main.cpp
    src/engine_sim_application.cpp
    src/ecu_bridge.cpp
    src/geometry_generator.cpp
    src/simulation_object.cpp
    src/piston_object.cpp
    src/connecting_rod_object.cpp
    src/ui_element.cpp
    src/ui_manager.cpp
    src/cylinder_pressure_gauge.cpp
    src/ui_math.cpp
    src/gauge.cpp
    src/crankshaft_object.cpp
    src/cylinder_bank_object.cpp
    src/cylinder_head_object.cpp
    src/ui_button.cpp
    src/ui_utilities.cpp
    src/combustion_chamber_object.cpp
    src/oscilloscope.cpp
    src/shaders.cpp
    src/engine_view.cpp
    src/right_gauge_cluster.cpp
    src/cylinder_temperature_gauge.cpp
    src/labeled_gauge.cpp
    src/throttle_display.cpp
    src/afr_cluster.cpp
    src/fuel_cluster.cpp
    src/oscilloscope_cluster.cpp
    src/performance_cluster.cpp
    src/firing_order_display.cpp
    src/load_simulation_cluster.cpp
    src/mixer_cluster.cpp
    src/info_cluster.cpp

    # Include files
    include/delta.h
    include/dtv.h
    include/engine_sim_application.h
    include/geometry_generator.h
    include/simulation_object.h
    include/piston_object.h
    include/connecting_rod_object.h
    include/ui_element.h
    include/ui_manager.h
    include/cylinder_pressure_gauge.h
    include/ui_math.h
    include/units.h
    include/crankshaft_object.h
    include/cylinder_bank_object.h
    include/cylinder_head_object.h
    include/ui_button.h
    include/ui_utilities.h
    include/combustion_chamber_object.h
    include/oscilloscope.h
    include/shaders.h
    include/engine_view.h
    include/right_gauge_cluster.h
    include/cylinder_temperature_gauge.h
    include/labeled_gauge.h
    include/throttle_display.h
    include/afr_cluster.h
    include/fuel_cluster.h
    include/oscilloscope_cluster.h
    include/performance_cluster.h
    include/firing_order_display.h
    include/load_simulation_cluster.h
    include/mixer_cluster.h
    include/info_cluster.h
)

target_link_libraries(engine-sim-app
    engine-sim
)

if (DTV)
    target_link_libraries(engine-sim-app
        direct-to-video)
endif (DTV)

if (PIRANHA_ENABLED)
    target_link_libraries(engine-sim-app
        engine-sim-script-interpreter)
endif (PIRANHA_ENABLED)

if (DISCORD_ENABLED)
    add_library(discord-rpc STATIC IMPORTED)
    set_property(TARGET discord-rpc PROPERTY IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/dependencies/discord/lib/discord-rpc.lib)
    target_link_libraries(engine-sim-app
    	discord
	discord-rpc)
endif (DISCORD_ENABLED)

target_include_directories(engine-sim-app
    PUBLIC dependencies/submodules)

add_subdirectory(dependencies)

# GTEST

enable_testing()

add_executable(engine-sim-test
    # Source files
    test/gas_system_tests.cpp
    test/function_test.cpp
    test/synthesizer_tests.cpp
)

target_link_libraries(engine-sim-test
    gtest_main
    engine-sim
)

include(GoogleTest)
gtest_discover_tests(engine-sim-test)
