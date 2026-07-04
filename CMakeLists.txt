cmake_minimum_required(VERSION 3.28)
project(idisplay_host VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg deps (manifest mode — see vcpkg.json). Configure with:
#   -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
find_package(nlohmann_json CONFIG REQUIRED)

# Core: everything that compiles without hardware/UI (config, network, input, token).
# Hardware/UI (dxgi capture, encoder, vdd, tray, app/main) are added per component as they land.
add_library(idisplay_core STATIC
  src/config/config.cpp
  src/input/input_injector.cpp
  src/network/secure_token.cpp
  src/network/udp_server.cpp
  src/network/input_receiver.cpp
  src/network/ctrl_server.cpp
)
target_include_directories(idisplay_core PUBLIC src)
target_link_libraries(idisplay_core PUBLIC nlohmann_json::nlohmann_json ws2_32 bcrypt)
# NOMINMAX: keep windows.h from clobbering std::min/std::max. LEAN: trim the Win32 include.
target_compile_definitions(idisplay_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)

# Media: DXGI capture + H.264 encoder + VDD stub. Needs D3D11 / DXGI / Media Foundation.
# Compile-verified here; runtime needs a display + GPU.
add_library(idisplay_media STATIC
  src/display/dxgi_capture.cpp
  src/display/vdd_manager.cpp
  src/encoder/mf_encoder.cpp
  src/encoder/encoder_factory.cpp
)
target_include_directories(idisplay_media PUBLIC src)
target_compile_definitions(idisplay_media PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(idisplay_media PUBLIC
  d3d11 dxgi mfplat mfuuid mfreadwrite wmcodecdspuuid ole32 uuid)

# Windows host executable (tray app).
add_executable(idisplay_host WIN32
  src/main.cpp
  src/app.cpp
  src/ui/tray.cpp
)
target_link_libraries(idisplay_host PRIVATE
  idisplay_core idisplay_media user32 shell32)

enable_testing()
add_subdirectory(tests)
