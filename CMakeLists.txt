# Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
#
# You may use, distribute and modify this code under the
# terms of the Apache 2.0 license.
# If you don't have a copy of this license, please visit:
#     https://github.com/gillesdegottex/phaseshift

cmake_minimum_required(VERSION 3.22)

project(phaseshift)

# Compilation options ---------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # To generate compile_commands.json to allow clangd do the code navigation

# By default, all options are building the smallest and faster library
option(PHASESHIFT_DEV_ASSERT "If ON, standard C asserts will be enabled by removing -DNDEBUG compilation flag (no matter the build type, release or not)." OFF)
option(PHASESHIFT_DEV_PROFILING "Enable profiling measures (ex. processing time in phaseshift::audio_block)" OFF)
# option(PHASESHIFT_BENCHMARKS "Build the benchmarks" OFF)
option(PHASESHIFT_DEV_TESTS "Build the tests" OFF)
option(PHASESHIFT_SUPPORT_SNDFILE "Support libsndfile" OFF)
option(PHASESHIFT_SUPPORT_SNDFILE_LOCAL "Support libsndfile using local build" OFF)

set(PHASESHIFT_BUILD_ARCH "undefined" CACHE STRING "The string identifying the architecture of the build, in a broad sense: it often contains os, machine arch, interpreter version, etc. For python SDK it is the SOABI tag.")

message(STATUS "  CMAKE_SYSTEM_NAME: " ${CMAKE_SYSTEM_NAME})
message(STATUS "  CMAKE_SYSTEM_VERSION: " ${CMAKE_SYSTEM_VERSION})
message(STATUS "  CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})

# The lib itself --------------------------------------------------------------

add_library(${PROJECT_NAME} STATIC ${PROJECT_NAME}/utils.cpp ${PROJECT_NAME}/dev/time_elapsed_summary.cpp ${PROJECT_NAME}/audio_block/audio_block.cpp ${PROJECT_NAME}/audio_block/ol.cpp ${PROJECT_NAME}/audio_block/ola.cpp ${PROJECT_NAME}/audio_block/ola_decoupled.cpp ${PROJECT_NAME}/audio_block/tinywavfile.cpp ${PROJECT_NAME}/lookup_table.cpp ${PROJECT_NAME}/sigproc/clipper.cpp ${CMAKE_CURRENT_SOURCE_DIR}/ext/tinywav/tinywav.c)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Defines ---------------------------------------------------------------------

# https://mcyoung.xyz/2025/04/14/target-triples/
set(PHASESHIFT_BUILD_DEVICE_ARCH "undefined")
set(PHASESHIFT_BUILD_VENDOR "unknown")
set(PHASESHIFT_BUILD_OS "undefined")
set(PHASESHIFT_BUILD_OS_VERSION "undefined")

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  message(STATUS "  CMAKE_OSX_DEPLOYMENT_TARGET: " ${CMAKE_OSX_DEPLOYMENT_TARGET})
  message(STATUS "  CMAKE_OSX_ARCHITECTURES: " ${CMAKE_OSX_ARCHITECTURES})
  message(STATUS "  CMAKE_OSX_SYSROOT: " ${CMAKE_OSX_SYSROOT})
  message(STATUS "  MACOSX_DEPLOYMENT_TARGET: " ${MACOSX_DEPLOYMENT_TARGET})
  message(STATUS "  OSX_ARCHITECTURES: " ${OSX_ARCHITECTURES})
  set(PHASESHIFT_BUILD_DEVICE_ARCH "${CMAKE_OSX_ARCHITECTURES}")
  set(PHASESHIFT_BUILD_VENDOR "apple")
  set(PHASESHIFT_BUILD_OS "macos")
  if (CMAKE_OSX_DEPLOYMENT_TARGET)
    set(PHASESHIFT_BUILD_ARCH_DEFAULT "macos-${CMAKE_OSX_DEPLOYMENT_TARGET}")
  else()
    set(PHASESHIFT_BUILD_ARCH_DEFAULT "macos-unknown_version")
  endif()
  set(PHASESHIFT_BUILD_OS_VERSION "${CMAKE_OSX_DEPLOYMENT_TARGET}")

  if (CMAKE_OSX_ARCHITECTURES)
    set(PHASESHIFT_BUILD_ARCH_DEFAULT "${PHASESHIFT_BUILD_ARCH_DEFAULT}-${CMAKE_OSX_ARCHITECTURES}")
  else()
    set(PHASESHIFT_BUILD_ARCH_DEFAULT "${PHASESHIFT_BUILD_ARCH_DEFAULT}-unknown_arch")
  endif()

elseif (CMAKE_SYSTEM_NAME STREQUAL "iOS")
  message(STATUS "  CMAKE_OSX_DEPLOYMENT_TARGET: " ${CMAKE_OSX_DEPLOYMENT_TARGET})
  message(STATUS "  CMAKE_OSX_ARCHITECTURES: " ${CMAKE_OSX_ARCHITECTURES})
  message(STATUS "  CMAKE_OSX_SYSROOT: " ${CMAKE_OSX_SYSROOT})
  message(STATUS "  MACOSX_DEPLOYMENT_TARGET: " ${MACOSX_DEPLOYMENT_TARGET})
  message(STATUS "  OSX_ARCHITECTURES: " ${OSX_ARCHITECTURES})
  set(PHASESHIFT_BUILD_DEVICE_ARCH "${CMAKE_OSX_ARCHITECTURES}")
  set(PHASESHIFT_BUILD_VENDOR "apple")
  set(PHASESHIFT_BUILD_OS "ios")
  set(PHASESHIFT_BUILD_OS_VERSION "${CMAKE_OSX_DEPLOYMENT_TARGET}")
  set(PHASESHIFT_BUILD_ARCH_DEFAULT "ios-${CMAKE_OSX_DEPLOYMENT_TARGET}-${CMAKE_OSX_ARCHITECTURES}")  # TODO Standard apple triplet is arm64-apple-ios13.0 ?

elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(PHASESHIFT_BUILD_DEVICE_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
  set(PHASESHIFT_BUILD_VENDOR "unknown")
  set(PHASESHIFT_BUILD_OS "linux-gnu")
  # set(PHASESHIFT_BUILD_OS_VERSION "${TODO}")
  set(PHASESHIFT_BUILD_ARCH_DEFAULT "linux-${CMAKE_SYSTEM_PROCESSOR}")

elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
  message(STATUS "  CMAKE_ANDROID_ARCH_ABI: " ${CMAKE_ANDROID_ARCH_ABI})
  message(STATUS "  ANDROID_PLATFORM: " ${ANDROID_PLATFORM})
  set(PHASESHIFT_BUILD_DEVICE_ARCH "${CMAKE_ANDROID_ARCH_ABI}")
  set(PHASESHIFT_BUILD_VENDOR "google")
  set(PHASESHIFT_BUILD_OS "android")
  set(PHASESHIFT_BUILD_OS_VERSION "${ANDROID_PLATFORM}")
  set(PHASESHIFT_BUILD_ARCH_DEFAULT "android-${ANDROID_PLATFORM}-${CMAKE_ANDROID_ARCH_ABI}")
  target_link_libraries(${PROJECT_NAME} PUBLIC log)  # For __android_log_print

elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(PHASESHIFT_BUILD_DEVICE_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
  set(PHASESHIFT_BUILD_VENDOR "pc")
  set(PHASESHIFT_BUILD_OS "windows")
  # set(PHASESHIFT_BUILD_OS_VERSION "${TODO}")
  set(PHASESHIFT_BUILD_ARCH_DEFAULT "win-${CMAKE_SYSTEM_PROCESSOR}")  # TODO

else()
  message(FATAL_ERROR "Unknown system by phaseshift: ${CMAKE_SYSTEM_NAME}. Please add a default architecture description in CMakeLists.txt")
endif()

if (${PHASESHIFT_BUILD_ARCH} STREQUAL "undefined")
  set(PHASESHIFT_BUILD_ARCH ${PHASESHIFT_BUILD_ARCH_DEFAULT})
else()
  message(STATUS "  PHASESHIFT_BUILD_ARCH was given during cmake configuration.")
endif()

message(STATUS "  PHASESHIFT_BUILD_ARCH_DEFAULT: " ${PHASESHIFT_BUILD_ARCH_DEFAULT})
target_compile_definitions(${PROJECT_NAME} PUBLIC -DPHASESHIFT_BUILD_ARCH_DEFAULT=\"${PHASESHIFT_BUILD_ARCH_DEFAULT}\")
message(STATUS "  PHASESHIFT_BUILD_ARCH: " ${PHASESHIFT_BUILD_ARCH} " (-DPHASESHIFT_BUILD_ARCH=<build_arch>)")
target_compile_definitions(${PROJECT_NAME} PUBLIC -DPHASESHIFT_BUILD_ARCH=\"${PHASESHIFT_BUILD_ARCH}\")
message(STATUS "  PHASESHIFT_BUILD_DEVICE_ARCH: " ${PHASESHIFT_BUILD_DEVICE_ARCH})
target_compile_definitions(${PROJECT_NAME} PUBLIC -DPHASESHIFT_BUILD_DEVICE_ARCH=\"${PHASESHIFT_BUILD_DEVICE_ARCH}\")
message(STATUS "  PHASESHIFT_BUILD_VENDOR: " ${PHASESHIFT_BUILD_VENDOR})
target_compile_definitions(${PROJECT_NAME} PUBLIC -DPHASESHIFT_BUILD_VENDOR=\"${PHASESHIFT_BUILD_VENDOR}\")
message(STATUS "  PHASESHIFT_BUILD_OS: " ${PHASESHIFT_BUILD_OS})
target_compile_definitions(${PROJECT_NAME} PUBLIC -DPHASESHIFT_BUILD_OS=\"${PHASESHIFT_BUILD_OS}\")
message(STATUS "  PHASESHIFT_BUILD_OS_VERSION: " ${PHASESHIFT_BUILD_OS_VERSION})
target_compile_definitions(${PROJECT_NAME} PUBLIC -DPHASESHIFT_BUILD_OS_VERSION=\"${PHASESHIFT_BUILD_OS_VERSION}\")

set(PHASESHIFT_BUILD_TRIPLE "${PHASESHIFT_BUILD_DEVICE_ARCH}-${PHASESHIFT_BUILD_VENDOR}-${PHASESHIFT_BUILD_OS}")
message(STATUS "  PHASESHIFT_BUILD_TRIPLE: " ${PHASESHIFT_BUILD_TRIPLE})
target_compile_definitions(${PROJECT_NAME} PUBLIC -DPHASESHIFT_BUILD_TRIPLE=\"${PHASESHIFT_BUILD_TRIPLE}\")


# Optimisation ----------------------------------------------------------------

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Specifies the build type." FORCE)
endif()

if (WIN32)
  add_compile_options(/W4)
else()
  add_compile_options(-Wpedantic)
endif()

# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "${PROJECT_NAME} library options:")

# Keep control of asserts behavior independently of CMAKE_BUILD_TYPE
if(PHASESHIFT_DEV_ASSERT)
  message(WARNING "  Removed -DNDEBUG from compilation flags in order to enable C asserts. Should be used for testing or prototyping only. (PHASESHIFT_DEV_ASSERT=ON)")
  string(REGEX REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
  string(REGEX REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
  string(REGEX REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  string(REGEX REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_ASSERTIONS")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -D_GLIBCXX_ASSERTIONS")
else()
  message(STATUS "  C asserts are disabled. (PHASESHIFT_DEV_ASSERT=OFF)")
endif()

if(PHASESHIFT_DEV_PROFILING)
  message(WARNING "  Enabled profiling measures (ex. processing time in phaseshift::audio_block) (PHASESHIFT_DEV_PROFILING=ON)")
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DPHASESHIFT_DEV_PROFILING)
else()
  message(STATUS "  Profiling functions disabled. (PHASESHIFT_DEV_PROFILING=OFF)")
endif()


# The necessary external libs -------------------------------------------------

find_package(PkgConfig REQUIRED)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ext/)

# This variable exposes to any parent project sources that might be necessary to build tests
# (it is _not_ included in the phaseshift lib!)
set(PHASESHIFT_SOURCES_TESTS ${PHASESHIFT_SOURCES_TESTS})

  # ACBench -------------------------------------------------------------------
  message(STATUS)
  message(STATUS "* ACBench")
  target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ext/acbench)

  # FFTScarf ------------------------------------------------------------------
  message(STATUS)
  message(STATUS "* FFTScarf")
  set(FFTSCARF_FFT_FFTREAL ON CACHE STRING "docstring")
  set(FFTSCARF_PLAN_PROTECTACCESS ON CACHE STRING "docstring")
  add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/ext/fftscarf ${CMAKE_CURRENT_BINARY_DIR}/fftscarf_build)
  target_link_libraries(${PROJECT_NAME} PUBLIC fftscarf)

  # Eigen ---------------------------------------------------------------------
  message(STATUS)
  message(STATUS "* Eigen")
  # add_definitions(-DEIGEN_DEFAULT_TO_ROW_MAJOR) # Eigen is tested/developped with column major order. But row major make it compatible with numpy I think. TODO TODO TODO
  target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ext/eigen)
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DEIGEN_MPL2_ONLY)

  # libsndfile ----------------------------------------------------------------
  if (PHASESHIFT_SUPPORT_SNDFILE_LOCAL)
    message(STATUS)
    message(STATUS "* libsndfile (local build)")
    target_compile_definitions(${PROJECT_NAME} PUBLIC -DPHASESHIFT_SUPPORT_SNDFILE)

    # Can't do that as it breaks some configurations about testing
    # add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ext/libsndfile ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_build)

    # TODO(GD) Use custom_command(.) and custom_target(.)
    include(ExternalProject)
    ExternalProject_Add(libsndfile_local
      URL ${CMAKE_CURRENT_SOURCE_DIR}/ext/libsndfile
      SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_src
      # CONFIGURE_COMMAND ./config -w no-hw no-shared no-egd --prefix=${CMAKE_CURRENT_BINARY_DIR}/openssl_install --openssldir="" "-DNDEBUG -O3"
      CONFIGURE_COMMAND cmake -DBUILD_SHARED_LIBS=ON -DINSTALL_MANPAGES=OFF -DBUILD_TESTING=OFF -DBUILD_REGTEST=OFF -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DENABLE_BOW_DOCS=OFF -DENABLE_EXTERNAL_LIBS=OFF -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install -DCMAKE_INSTALL_LIBDIR=lib .
      BUILD_COMMAND make -j4
      BUILD_IN_SOURCE TRUE
      INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install
      INSTALL_COMMAND make install
      LOG_CONFIGURE FALSE
      LOG_BUILD TRUE
      LOG_INSTALL FALSE
      LOG_MERGED_STDOUTERR TRUE
      LOG_OUTPUT_ON_FAILURE TRUE
    )

    target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install/include)
    if (WIN32)
      target_link_libraries(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install/lib/libsndfile.dll)
      set(PHASESHIFT_INSTALL_FILES ${PHASESHIFT_INSTALL_FILES} ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install/lib/libsndfile.dll)
    elseif(APPLE)
      target_link_libraries(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install/lib/libsndfile.1.dylib)
      set(PHASESHIFT_INSTALL_FILES ${PHASESHIFT_INSTALL_FILES} ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install/lib/libsndfile.1.dylib)
    else()
      target_link_libraries(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install/lib/libsndfile.so)
      set(PHASESHIFT_INSTALL_FILES ${PHASESHIFT_INSTALL_FILES} ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install/lib/libsndfile.so)
      target_link_libraries(${PROJECT_NAME} PUBLIC mpg123 mp3lame)
    endif()

  elseif (PHASESHIFT_SUPPORT_SNDFILE)
    message(STATUS)
    message(STATUS "libsndfile")
    target_compile_definitions(${PROJECT_NAME} PUBLIC -DPHASESHIFT_SUPPORT_SNDFILE)

    if (WIN32)
      find_path(LIBSNDFILE_INCLUDE_DIRS sndfile.h REQUIRED PATHS "C:/Program Files/libsndfile/include")
      find_library(LIBSNDFILE_LIBRARIES sndfile REQUIRED PATHS "C:/Program Files/libsndfile/lib")
    else()
      # find_path(LIBSNDFILE_INCLUDE_DIRS sndfile.h REQUIRED)
      # find_library(LIBSNDFILE_LIBRARIES sndfile REQUIRED)

      pkg_check_modules(LIBSNDFILE REQUIRED sndfile)
      message(STATUS "  LIBSNDFILE_VERSION: " ${LIBSNDFILE_VERSION})
      message(STATUS "  LIBSNDFILE_INCLUDE_DIRS: " ${LIBSNDFILE_INCLUDE_DIRS})
      message(STATUS "  LIBSNDFILE_LIBRARIES: " ${LIBSNDFILE_LIBRARIES})
      message(STATUS "  LIBSNDFILE_LDFLAGS: " ${LIBSNDFILE_LDFLAGS})
      # message(STATUS "  LIBSNDFILE_CFLAGS: " ${LIBSNDFILE_CFLAGS})
    endif()

    target_include_directories(${PROJECT_NAME} PUBLIC ${LIBSNDFILE_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PUBLIC ${LIBSNDFILE_LDFLAGS} ${LIBSNDFILE_LIBRARIES})
  endif()

# Include sndfile support only if explicitly requested
if(PHASESHIFT_SUPPORT_SNDFILE_LOCAL)
    message(STATUS "  Including libsndfile (local build)")
    target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}/audio_block/sndfile.cpp)
elseif(PHASESHIFT_SUPPORT_SNDFILE)
    message(STATUS "  Including libsndfile (system)")
    target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}/audio_block/sndfile.cpp)
endif()

# Wait for sndfile to be built before building anything with phaseshift
if (PHASESHIFT_SUPPORT_SNDFILE_LOCAL)
  add_dependencies(${PROJECT_NAME} libsndfile_local )
endif()

# Set test source file (catch2_extra.cpp is always needed for tests)
set(PHASESHIFT_SOURCES_TESTS ${PHASESHIFT_SOURCES_TESTS} ${CMAKE_CURRENT_SOURCE_DIR}/phaseshift/dev/catch2_extra.cpp)


set_target_properties(phaseshift
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/"
)

# Projects depending on the lib -----------------------------------------------

  # Tests ---------------------------------------------------------------------

  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test_data/totest)
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DPHASESHIFT_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

  if(PHASESHIFT_DEV_TESTS)
    message(STATUS "Compiling tests")
    set(CMAKE_CTEST_ARGUMENTS "--output-on-failure")
    include(CTest)

    # Note: Tests can be built without libsndfile support since they don't use it
    # libsndfile is only needed for actual functionality that requires it

    # Tests link against phaseshift (which carries all includes and libs transitively)
    if (NOT WIN32)
      set(PHASESHIFT_TEST_EXTRA_LIBS -lpthread)
    endif()

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ext/Catch2)

    add_executable(phaseshift_utils_test phaseshift/utils_test.cpp)
    target_link_libraries(phaseshift_utils_test PRIVATE phaseshift ${PHASESHIFT_TEST_EXTRA_LIBS} Catch2::Catch2WithMain)
    add_test(NAME phaseshift_utils_test COMMAND phaseshift_utils_test)

    add_executable(phaseshift_ringbuffer_test phaseshift/containers/ringbuffer_test.cpp)
    target_link_libraries(phaseshift_ringbuffer_test PRIVATE phaseshift ${PHASESHIFT_TEST_EXTRA_LIBS} Catch2::Catch2WithMain)
    add_test(NAME phaseshift_ringbuffer_test COMMAND phaseshift_ringbuffer_test)

    add_executable(phaseshift_audio_block_ol_test phaseshift/audio_block/ol_test.cpp ${PHASESHIFT_SOURCES_TESTS})
    target_link_libraries(phaseshift_audio_block_ol_test PRIVATE phaseshift ${PHASESHIFT_TEST_EXTRA_LIBS} Catch2::Catch2WithMain)
    add_test(NAME phaseshift_audio_block_ol_test COMMAND phaseshift_audio_block_ol_test)

    add_executable(phaseshift_audio_block_ola_test phaseshift/audio_block/ola_test.cpp ${PHASESHIFT_SOURCES_TESTS})
    target_link_libraries(phaseshift_audio_block_ola_test PRIVATE phaseshift ${PHASESHIFT_TEST_EXTRA_LIBS} Catch2::Catch2WithMain)
    add_test(NAME phaseshift_audio_block_ola_test COMMAND phaseshift_audio_block_ola_test)

    add_executable(phaseshift_audio_block_ola_decoupled_test phaseshift/audio_block/ola_decoupled_test.cpp ${PHASESHIFT_SOURCES_TESTS})
    target_include_directories(phaseshift_audio_block_ola_decoupled_test PUBLIC phaseshift ${PHASESHIFT_TEST_EXTRA_LIBS} Catch2::Catch2WithMain)
    target_link_libraries(phaseshift_audio_block_ola_decoupled_test PRIVATE phaseshift ${PHASESHIFT_TEST_EXTRA_LIBS} Catch2::Catch2WithMain)
    add_test(NAME phaseshift_audio_block_ola_decoupled_test COMMAND phaseshift_audio_block_ola_decoupled_test)

    add_executable(phaseshift_lookup_table_test phaseshift/lookup_table_test.cpp)
    target_link_libraries(phaseshift_lookup_table_test PRIVATE phaseshift ${PHASESHIFT_TEST_EXTRA_LIBS} Catch2::Catch2WithMain)
    add_test(NAME phaseshift_lookup_table_test COMMAND phaseshift_lookup_table_test)

    add_executable(phaseshift_lookup_table_clipper_test phaseshift/sigproc/clipper_test.cpp)
    target_link_libraries(phaseshift_lookup_table_clipper_test PRIVATE phaseshift ${PHASESHIFT_TEST_EXTRA_LIBS} Catch2::Catch2WithMain)
    add_test(NAME phaseshift_lookup_table_clipper_test COMMAND phaseshift_lookup_table_clipper_test)

    # libsndfile test
    if((PHASESHIFT_SUPPORT_SNDFILE) OR (PHASESHIFT_SUPPORT_SNDFILE_LOCAL))
        add_executable(phaseshift_libsndfile_test phaseshift/libsndfile_test.cpp)
        target_link_libraries(phaseshift_libsndfile_test PRIVATE phaseshift ${PHASESHIFT_TEST_EXTRA_LIBS} Catch2::Catch2WithMain)
        add_test(NAME phaseshift_libsndfile_test COMMAND phaseshift_libsndfile_test)
    endif()
  endif()

# Some info -------------------------------------------------------------------
message(STATUS)
message(STATUS "  PHASESHIFT_INSTALL_FILES=" )
foreach(dir ${PHASESHIFT_INSTALL_FILES})
  message(STATUS "       ${dir}")
endforeach()

message(STATUS "  CMAKE_C_FLAGS=${CMAKE_C_FLAGS}")
message(STATUS "  CMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}")
message(STATUS "  CMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}")
message(STATUS "  CMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}")

if (NOT "${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
  set(PHASESHIFT_SOURCES_TESTS ${PHASESHIFT_SOURCES_TESTS} PARENT_SCOPE)
  set(PHASESHIFT_INSTALL_FILES ${PHASESHIFT_INSTALL_FILES} PARENT_SCOPE)
  set(PHASESHIFT_BUILD_ARCH ${PHASESHIFT_BUILD_ARCH} PARENT_SCOPE)
  set(PHASESHIFT_BUILD_DEVICE_ARCH ${PHASESHIFT_BUILD_DEVICE_ARCH} PARENT_SCOPE)
  set(PHASESHIFT_BUILD_VENDOR ${PHASESHIFT_BUILD_VENDOR} PARENT_SCOPE)
  set(PHASESHIFT_BUILD_OS ${PHASESHIFT_BUILD_OS} PARENT_SCOPE)
  set(PHASESHIFT_BUILD_OS_VERSION ${PHASESHIFT_BUILD_OS_VERSION} PARENT_SCOPE)
  set(PHASESHIFT_BUILD_TRIPLE ${PHASESHIFT_BUILD_TRIPLE} PARENT_SCOPE)
endif()
