cmake_minimum_required(VERSION 3.15)
project(ChessBot VERSION 1.0 LANGUAGES CXX)

# ── C++ Standard ──────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Source files ──────────────────────────────────────────────
add_executable(ChessBot main.cpp)

# ── Raylib ────────────────────────────────────────────────────
# Option 1: If you installed Raylib system-wide or via vcpkg,
#           CMake will find it automatically:
find_package(raylib QUIET)

if (raylib_FOUND)
    target_link_libraries(ChessBot PRIVATE raylib)
else()
    # Option 2: Manual path — edit these to match your setup
    set(RAYLIB_INCLUDE_DIR "C:/raylib/src")
    set(RAYLIB_LIBRARY     "C:/raylib/lib/libraylib.a")

    target_include_directories(ChessBot PRIVATE ${RAYLIB_INCLUDE_DIR})
    target_link_libraries(ChessBot PRIVATE ${RAYLIB_LIBRARY})
endif()

# ── Windows-specific system libraries (required by Raylib) ────
if (WIN32)
    target_link_libraries(ChessBot PRIVATE opengl32 gdi32 winmm)
endif()

# ── Linux-specific system libraries ───────────────────────────
if (UNIX AND NOT APPLE)
    target_link_libraries(ChessBot PRIVATE GL m pthread dl rt X11)
endif()

# ── Threading (for std::thread) ───────────────────────────────
find_package(Threads REQUIRED)
target_link_libraries(ChessBot PRIVATE Threads::Threads)

# ── Compiler warnings ─────────────────────────────────────────
if (MSVC)
    target_compile_options(ChessBot PRIVATE /W4)
else()
    target_compile_options(ChessBot PRIVATE -Wall -Wextra -O2)
endif()
