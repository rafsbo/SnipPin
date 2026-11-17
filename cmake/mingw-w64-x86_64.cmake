# Cross-compiling SnipPin from Linux with MinGW-w64 (useful for CI and for
# contributors without Visual Studio). Usage:
#   cmake -S . -B build-mingw-x86_64 -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DSNIPPIN_BUILD_TESTS=OFF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

# Prefer the POSIX threading flavour when available (Debian/Ubuntu packages).
find_program(_snippin_cxx NAMES ${TOOLCHAIN_PREFIX}-g++-posix ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_CXX_COMPILER ${_snippin_cxx})
set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
