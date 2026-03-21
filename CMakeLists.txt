cmake_minimum_required(VERSION 3.20)
project(x4mcp VERSION 0.1.0)

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# X4_GAME_DIR resolution: explicit > auto-detect from Steam registry
set(X4_GAME_DIR "" CACHE PATH "X4 Foundations installation directory")

if(NOT X4_GAME_DIR AND WIN32)
    get_filename_component(_STEAM_DIR
        "[HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam;InstallPath]"
        ABSOLUTE CACHE)

    if(_STEAM_DIR AND EXISTS "${_STEAM_DIR}")
        set(_X4_CANDIDATE "${_STEAM_DIR}/steamapps/common/X4 Foundations")
        if(EXISTS "${_X4_CANDIDATE}/X4.exe")
            set(X4_GAME_DIR "${_X4_CANDIDATE}" CACHE PATH
                "X4 Foundations installation directory (auto-detected)" FORCE)
            message(STATUS "Auto-detected X4 at: ${X4_GAME_DIR}")
        endif()
    endif()
    unset(_STEAM_DIR CACHE)
    unset(_X4_CANDIDATE)
endif()

if(NOT X4_GAME_DIR)
    message(WARNING "X4_GAME_DIR not set and auto-detect failed.\n"
                    "Deploy target will be unavailable.\n"
                    "Pass -DX4_GAME_DIR=<path> or create CMakeUserPresets.json")
endif()

set(DEPLOY_DIR "${X4_GAME_DIR}/extensions/x4mcp")

# ---------------------------------------------------------------------------
# Dependencies (FetchContent)
# ---------------------------------------------------------------------------
include(FetchContent)

# cpp-httplib — single-header HTTP server
FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.3
    GIT_SHALLOW    TRUE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(httplib)

# nlohmann/json — JSON parsing
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
    EXCLUDE_FROM_ALL
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# ---------------------------------------------------------------------------
# Extension DLL
# ---------------------------------------------------------------------------
add_library(x4native_x4mcp SHARED
    src/main.cpp
    src/mcp_server.cpp
    src/game_commands.cpp
    src/config.cpp
)

target_include_directories(x4native_x4mcp PRIVATE
    "${CMAKE_SOURCE_DIR}/X4Native/sdk"
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(x4native_x4mcp PRIVATE
    httplib::httplib
    nlohmann_json::nlohmann_json
)

target_compile_definitions(x4native_x4mcp PRIVATE NOMINMAX)

set_target_properties(x4native_x4mcp PROPERTIES
    PREFIX ""
    OUTPUT_NAME "x4native_x4mcp"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/native"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/native"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/native"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/native"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/native"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/native"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/native"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/native"
)

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
install(TARGETS x4native_x4mcp RUNTIME DESTINATION native)
install(FILES extension/content.xml DESTINATION .)
install(FILES extension/x4native.json DESTINATION .)
install(FILES extension/x4mcp_config.json DESTINATION .)
install(FILES extension/ui.xml DESTINATION .)
install(DIRECTORY extension/ui/ DESTINATION ui)

# ---------------------------------------------------------------------------
# Deploy target — cmake --build build --target deploy
# ---------------------------------------------------------------------------
if(X4_GAME_DIR)
    add_custom_target(deploy
        COMMAND ${CMAKE_COMMAND} -E echo "Deploying x4mcp to ${DEPLOY_DIR}"
        COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}"
                --prefix "${DEPLOY_DIR}"
                --config $<CONFIG>
        COMMENT "Deploy x4mcp extension to game folder"
    )
    add_dependencies(deploy x4native_x4mcp)
else()
    message(STATUS "Deploy target unavailable (X4_GAME_DIR not set)")
endif()
