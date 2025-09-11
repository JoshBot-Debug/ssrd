cmake_minimum_required(VERSION 3.22)
project(ssrd)

# === Options ===
option(ENABLE_AVX256 "Enable AVX-256 instructions" OFF)
option(ENABLE_OPEN_MP "Enable multithreaded with OpenMP" OFF)
option(ENABLE_STL_DEBUG "Enable STL debug mode and DEBUG macro." OFF)
option(ENABLE_THREAD_SANITIZER "Enable ThreadSanitizer (disables AddressSanitizer)" OFF)
option(ENABLE_OPENGL_VALIDATION "Enable OPENGL validation & debug extension" OFF)
option(ENABLE_VMA_STATS_LOG "Log out OPENGL memory allocator stats when the application exits" OFF)

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

# === OPENGL validation ===
if(ENABLE_OPENGL_VALIDATION)
    add_compile_definitions(ENABLE_OPENGL_VALIDATION)
endif()

# === Log VMA stats ===
if(ENABLE_VMA_STATS_LOG)
    add_compile_definitions(ENABLE_VMA_STATS_LOG)
endif()

# === Source Files ===
file(GLOB_RECURSE COMMON_SRC ${CMAKE_SOURCE_DIR}/common/*.cpp)
file(GLOB_RECURSE CLIENT_SRC ${CMAKE_SOURCE_DIR}/client/*.cpp)
file(GLOB_RECURSE SERVER_SRC ${CMAKE_SOURCE_DIR}/server/*.cpp)
file(GLOB_RECURSE GLAD_SRC ${CMAKE_SOURCE_DIR}/glad/*.c)

add_executable(ssrd-client ${CLIENT_SRC} ${COMMON_SRC} ${GLAD_SRC})
add_executable(ssrd-server ${SERVER_SRC} ${COMMON_SRC})

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
pkg_check_modules(PORTAL REQUIRED libportal)
pkg_check_modules(PORTAL_GTK REQUIRED libportal-gtk4)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(OPENGL REQUIRED opengl)
pkg_check_modules(GLFW3 REQUIRED glfw3)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWSCALE REQUIRED libswscale)

message(STATUS "PIPEWIRE_LIBRARIES = ${PIPEWIRE_LIBRARIES}")
message(STATUS "PIPEWIRE_INCLUDE_DIRS = ${PIPEWIRE_INCLUDE_DIRS}")
message(STATUS "PORTAL_LIBRARIES = ${PORTAL_LIBRARIES}")
message(STATUS "PORTAL_INCLUDE_DIRS = ${PORTAL_INCLUDE_DIRS}")
message(STATUS "PORTAL_GTK_LIBRARIES = ${PORTAL_GTK_LIBRARIES}")
message(STATUS "PORTAL_GTK_INCLUDE_DIRS = ${PORTAL_GTK_INCLUDE_DIRS}")
message(STATUS "OPENSSL_LIBRARIES = ${OPENSSL_LIBRARIES}")
message(STATUS "OPENGL_LIBRARIES = ${OPENGL_LIBRARIES}")
message(STATUS "GLFW3_LIBRARIES = ${GLFW3_LIBRARIES}")
message(STATUS "AVCODEC_LIBRARIES = ${AVCODEC_LIBRARIES}")
message(STATUS "AVCODEC_INCLUDE_DIRS = ${AVCODEC_INCLUDE_DIRS}")
message(STATUS "AVUTIL_LIBRARIES = ${AVUTIL_LIBRARIES}")
message(STATUS "AVUTIL_INCLUDE_DIRS = ${AVUTIL_INCLUDE_DIRS}")
message(STATUS "SWSCALE_LIBRARIES = ${SWSCALE_LIBRARIES}")
message(STATUS "SWSCALE_INCLUDE_DIRS = ${SWSCALE_INCLUDE_DIRS}")

# === Libraries ===
# Common
foreach(target ssrd-client ssrd-server)
# === Common Libraries ===
target_link_libraries(${target} PRIVATE
  ${OPENSSL_LIBRARIES}
)

# === Common Include Directories ===
target_include_directories(${target} PRIVATE 
  ${CMAKE_SOURCE_DIR}/common
  ${PIPEWIRE_INCLUDE_DIRS}
)
endforeach()

# Client
# === Include Directories ===
target_link_libraries(ssrd-client PRIVATE
  ${OPENGL_LIBRARIES}
  ${GLFW3_LIBRARIES}
  ${AVCODEC_LIBRARIES}
  ${SWSCALE_LIBRARIES}
  ${AVUTIL_LIBRARIES}
)

target_include_directories(ssrd-client PRIVATE 
  ${CMAKE_SOURCE_DIR}/client
  ${CMAKE_SOURCE_DIR}/glad/include
)

# Server
target_link_libraries(ssrd-server PRIVATE
  ${PIPEWIRE_LIBRARIES}
  ${PORTAL_LIBRARIES}
  ${PORTAL_GTK_LIBRARIES}
  ${AVCODEC_LIBRARIES}
  ${SWSCALE_LIBRARIES}
  ${AVUTIL_LIBRARIES}
)

# === Include Directories ===
target_include_directories(ssrd-server PRIVATE 
  ${CMAKE_SOURCE_DIR}/server
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