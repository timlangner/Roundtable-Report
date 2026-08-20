cmake_minimum_required(VERSION 3.24)
project(EldenRing_StatsShare LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "EldenRing_StatsShare must be built as 64-bit.")
endif()

include(FetchContent)

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)

FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)

FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.4
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.9
)

FetchContent_MakeAvailable(nlohmann_json tomlplusplus catch2 minhook imgui)

add_library(imgui_dx12 STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
)
target_include_directories(imgui_dx12 PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_dx12 PUBLIC d3d12 dxgi)

set(CORE_SOURCES
    src/config.cpp
    src/util.cpp
    src/game/aob.cpp
    src/game/gamedata.cpp
    src/game/offsets.cpp
    src/game/location.cpp
    src/game/progress.cpp
    src/game/item_names.cpp
    src/game/boss_combat.cpp
    src/game/chr_call.cpp
    src/run/identity.cpp
    src/run/save_headers.cpp
    src/run/session_post.cpp
    src/run/store.cpp
    src/discord/payload.cpp
    src/discord/map_image.cpp
)

add_library(stats_share_core STATIC ${CORE_SOURCES})
target_include_directories(stats_share_core PUBLIC src "${CMAKE_SOURCE_DIR}/third_party")
target_link_libraries(stats_share_core PUBLIC
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
)
target_link_libraries(stats_share_core PUBLIC bcrypt)
target_compile_definitions(stats_share_core PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

if(MSVC)
    set_source_files_properties(src/game/chr_call.cpp PROPERTIES COMPILE_FLAGS "/EHa")
endif()

add_library(EldenRing_StatsShare SHARED
    src/dllmain.cpp
    src/shutdown.cpp
    src/discord/webhook.cpp
    src/overlay/dx12_hook.cpp
    src/overlay/hud.cpp
)
target_include_directories(EldenRing_StatsShare PRIVATE src)
target_link_libraries(EldenRing_StatsShare PRIVATE
    stats_share_core
    minhook
    imgui_dx12
    winhttp
    d3d12
    dxgi
    dxguid
)
target_compile_definitions(EldenRing_StatsShare PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_target_properties(EldenRing_StatsShare PROPERTIES
    OUTPUT_NAME EldenRing_StatsShare
    PREFIX ""
)

add_executable(stats_share_tests
    tests/test_aob.cpp
    tests/test_save_headers.cpp
    tests/test_identity.cpp
    tests/test_discord_payload.cpp
    tests/test_store.cpp
    tests/test_offsets.cpp
    tests/test_config.cpp
    tests/test_location.cpp
    tests/test_progress.cpp
    tests/test_boss_combat.cpp
    tests/test_map_image.cpp
    tests/test_session_post.cpp
)
target_link_libraries(stats_share_tests PRIVATE stats_share_core Catch2::Catch2WithMain)
target_compile_definitions(stats_share_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

include(CTest)
include(Catch)
catch_discover_tests(stats_share_tests)

add_custom_command(TARGET EldenRing_StatsShare POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/dist/natives"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:EldenRing_StatsShare>
        "${CMAKE_SOURCE_DIR}/dist/natives/EldenRing_StatsShare.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/dist/EldenRing_StatsShare.toml"
        "${CMAKE_SOURCE_DIR}/dist/natives/EldenRing_StatsShare.toml"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/maps"
        "$<TARGET_FILE_DIR:EldenRing_StatsShare>/maps"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/maps"
        "${CMAKE_SOURCE_DIR}/dist/natives/maps"
)

add_custom_command(TARGET stats_share_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/maps"
        "$<TARGET_FILE_DIR:stats_share_tests>/maps"
)
