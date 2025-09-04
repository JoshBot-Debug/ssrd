cmake_minimum_required(VERSION 3.22)
project(ssrd)

# === Options ===
option(ENABLE_AVX256 "Enable AVX-256 instructions" OFF)
option(ENABLE_OPEN_MP "Enable multithreaded with OpenMP" OFF)
option(ENABLE_STL_DEBUG "Enable STL debug mode and DEBUG macro." OFF)
option(ENABLE_THREAD_SANITIZER "Enable ThreadSanitizer (disables AddressSanitizer)" OFF)

# === Compiler Flags ===
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# === Optional AVX ===
if(ENABLE_AVX256)
  add_compile_options(-mavx2)
  message(STATUS "AVX-256 instructions ENABLED.")
else()
  message(STATUS "AVX-256 is DISABLED.")
endif()

# === Source Files ===
file(GLOB_RECURSE SRC_FILES
  ${CMAKE_SOURCE_DIR}/src/*.cpp
)
add_executable(ssrd ${SRC_FILES})

# === Static/Shared Object Files ===
file(GLOB_RECURSE SHARED_LIB ${CMAKE_SOURCE_DIR}/bin/*.o)
file(GLOB_RECURSE STATIC_LIB ${CMAKE_SOURCE_DIR}/bin/*.a)
target_sources(ssrd PRIVATE ${SHARED_LIB})
target_link_libraries(ssrd PRIVATE ${STATIC_LIB})

# === OpenMP ===
if(ENABLE_OPEN_MP)
  find_package(OpenMP REQUIRED)
  if(OpenMP_FOUND)
    target_link_libraries(ssrd PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(ssrd PRIVATE ENABLE_OPEN_MP)
    message(STATUS "OpenMP multithreading is ENABLED.")
  else()
    message(WARNING "OpenMP requested but NOT FOUND.")
  endif()
else()
  message(STATUS "OpenMP is DISABLED.")
endif()

# === Libraries ===
find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
pkg_check_modules(TBB REQUIRED tbb)
pkg_check_modules(PORTAL REQUIRED libportal)
pkg_check_modules(PORTAL_GTK REQUIRED libportal-gtk4)
pkg_check_modules(JPEG REQUIRED libjpeg)

message(STATUS "TBB_LIBRARIES = ${TBB_LIBRARIES}")
message(STATUS "PIPEWIRE_INCLUDE_DIRS = ${PIPEWIRE_INCLUDE_DIRS}")
message(STATUS "PIPEWIRE_LIBRARIES = ${PIPEWIRE_LIBRARIES}")
message(STATUS "PORTAL_LIBRARIES = ${PORTAL_LIBRARIES}")
message(STATUS "PORTAL_INCLUDE_DIRS = ${PORTAL_INCLUDE_DIRS}")
message(STATUS "PORTAL_GTK_LIBRARIES = ${PORTAL_GTK_LIBRARIES}")
message(STATUS "PORTAL_GTK_INCLUDE_DIRS = ${PORTAL_GTK_INCLUDE_DIRS}")

target_link_libraries(ssrd PRIVATE
  ${TBB_LIBRARIES}
  ${PIPEWIRE_LIBRARIES}
  ${PORTAL_LIBRARIES}
  ${PORTAL_GTK_LIBRARIES}
)

# === Include Directories ===
target_include_directories(ssrd PRIVATE 
  ${CMAKE_SOURCE_DIR}/src
  ${PIPEWIRE_INCLUDE_DIRS}
  ${PORTAL_INCLUDE_DIRS}
  ${PORTAL_GTK_INCLUDE_DIRS}
)

# === Optional: ccache (speed up rebuilds) ===
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_PROGRAM}")
  message(STATUS "ccache is ENABLED.")
endif()

# === Debug Configuration ===
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DDEBUG -D_GLIBCXX_DEBUG)

  if(ENABLE_STL_DEBUG)
    target_compile_options(ssrd PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wconversion
      -Wsign-conversion
      -Wnull-dereference
      -Wdouble-promotion
      -Wformat=2
      -Wunused
    )

    message(STATUS "STL debug mode and DEBUG macro is ENABLED.")

    if(ENABLE_THREAD_SANITIZER)
      set(SANITIZE_FLAGS "-fsanitize=thread")
      message(STATUS "Using ThreadSanitizer (no ASan/UBSan).")
    else()
      set(SANITIZE_FLAGS "-fsanitize=undefined,address -fno-omit-frame-pointer")
      message(STATUS "Using AddressSanitizer + UndefinedBehaviorSanitizer.")
    endif()

    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 ${SANITIZE_FLAGS}")
    set(CMAKE_LINKER_FLAGS_DEBUG "${SANITIZE_FLAGS}")
  else()
    message(STATUS "STL debug mode and DEBUG macro is DISABLED.")
  endif()
endif()

# === Release Configuration ===
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto=auto -funroll-loops -fomit-frame-pointer -g0 -s -ffast-math")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto=auto")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "-flto=auto")