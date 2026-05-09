cmake_minimum_required(VERSION 3.14)
project(erebus C)
set(CMAKE_C_STANDARD 99)

# ── Raylib via FetchContent ──────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# ── Source files ─────────────────────────────────────────────────────
set(SOURCES
    src/main.c
    src/engine/raycaster.c
    src/engine/texgen.c
    src/engine/postfx.c
    src/world/map.c
    src/game/player.c
)

# ── Build the executable ────────────────────────────────────────────
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

# Link math lib on non-MSVC
if(NOT MSVC)
    target_link_libraries(${PROJECT_NAME} PRIVATE m)
endif()

# ── Install ─────────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    install(DIRECTORY assets/ DESTINATION assets)
endif()
