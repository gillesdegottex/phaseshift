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
message(STATUS "  MACOSX_DEPLOYMENT_TARGET: " ${MACOSX_DEPLOYMENT_TARGET})
message(STATUS "  CMAKE_OSX_DEPLOYMENT_TARGET: " ${CMAKE_OSX_DEPLOYMENT_TARGET})
message(STATUS "  OSX_ARCHITECTURES: " ${OSX_ARCHITECTURES})
message(STATUS "  CMAKE_OSX_ARCHITECTURES: " ${CMAKE_OSX_ARCHITECTURES})


if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(PHASESHIFT_BUILD_ARCH_DEFAULT "macos-${CMAKE_SYSTEM_VERSION}")  # TODO CMAKE_SYSTEM_VERSION is Darwin version, should something else
elseif (CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(PHASESHIFT_BUILD_ARCH_DEFAULT "ios-${CMAKE_OSX_DEPLOYMENT_TARGET}-${CMAKE_SYSTEM_PROCESSOR}")  # TODO Should be arm64-apple-ios13.0
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(PHASESHIFT_BUILD_ARCH_DEFAULT "linux-${CMAKE_SYSTEM_PROCESSOR}")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(PHASESHIFT_BUILD_ARCH_DEFAULT "win-${CMAKE_SYSTEM_PROCESSOR}")  # TODO
else()
  message(WARNING "  CMAKE_SYSTEM_NAME: " ${CMAKE_SYSTEM_NAME})
  message(WARNING "  CMAKE_SYSTEM_VERSION: " ${CMAKE_SYSTEM_VERSION})
  message(WARNING "  CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})
  message(FATAL_ERROR "Unknown system by phaseshift: ${CMAKE_SYSTEM_NAME}. Please add a default architecture description in CMakeLists.txt")
endif()

if (${PHASESHIFT_BUILD_ARCH} STREQUAL "undefined")
  set(PHASESHIFT_BUILD_ARCH ${PHASESHIFT_BUILD_ARCH_DEFAULT})
else()
  message(STATUS "  PHASESHIFT_BUILD_ARCH was given during cmake configuration.")
endif()
message(STATUS "  Default Architecture: " ${PHASESHIFT_BUILD_ARCH_DEFAULT})
message(STATUS "  Architecture: " ${PHASESHIFT_BUILD_ARCH} " (-DPHASESHIFT_BUILD_ARCH=<build_arch>)")
set(PHASESHIFT_DEFINITIONS ${PHASESHIFT_DEFINITIONS} -DPHASESHIFT_BUILD_ARCH=\"${PHASESHIFT_BUILD_ARCH}\")


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
  add_definitions(-DPHASESHIFT_DEV_PROFILING)
  set(PHASESHIFT_DEFINITIONS ${PHASESHIFT_DEFINITIONS} -DPHASESHIFT_DEV_PROFILING)
else()
  message(STATUS "  Profiling functions disabled. (PHASESHIFT_DEV_PROFILING=OFF)")
endif()


# The necessary external libs -------------------------------------------------

find_package(PkgConfig REQUIRED)

# This variable will add any extra sources that needs to be included in the phaseshift lib
# No matter if it is for production or testing
set(PHASESHIFT_SOURCES_SUPPORT ${PHASESHIFT_SOURCES_SUPPORT})

# This variable exposes to any parent project sources that might be necessary to build tests
# (it is _not_ included in the phaseshift lib!)
set(PHASESHIFT_SOURCES_TESTS ${PHASESHIFT_SOURCES_TESTS})

# # If conda environment is present, make the corresponding directories available
# # Not so great as it might be better to use `find_package` or `pkg-config` or something like that
# if(NOT "$ENV{CONDA_PREFIX}" STREQUAL "")
#   message(STATUS "Conda environment detected. Adding include directory...")
#   set(PHASESHIFT_INCLUDES ${PHASESHIFT_INCLUDES} $ENV{CONDA_PREFIX}/include)
# endif()

  # ACBench -------------------------------------------------------------------
  message(STATUS)
  message(STATUS "ACBench")
  set(PHASESHIFT_INCLUDES ${PHASESHIFT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/ext/acbench)

  # FFTScarf ------------------------------------------------------------------
  set(FFTSCARF_FFT_OOURA ON CACHE STRING "docstring")
  set(FFTSCARF_FFT_FFTREAL ON CACHE STRING "docstring")
  set(FFTSCARF_PLAN_PROTECTACCESS ON CACHE STRING "docstring")
  add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/ext/fftscarf ${CMAKE_CURRENT_BINARY_DIR}/fftscarf_build)

  message(STATUS "  FFTSCARF_INCLUDES: " ${FFTSCARF_INCLUDES})
  message(STATUS "  FFTSCARF_LIBRARIES: " ${FFTSCARF_LIBRARIES})
  message(STATUS "  FFTSCARF_FFT_IPP_LIBDIR=" ${FFTSCARF_FFT_IPP_LIBDIR})

  set(PHASESHIFT_INCLUDES ${PHASESHIFT_INCLUDES} ${FFTSCARF_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR}/fftscarf_build)
  set(PHASESHIFT_LIBRARIES ${PHASESHIFT_LIBRARIES} ${FFTSCARF_LIBRARIES})

  # Eigen ---------------------------------------------------------------------
  message(STATUS)
  message(STATUS "Eigen")
  # add_definitions(-DEIGEN_DEFAULT_TO_ROW_MAJOR) # Eigen is tested/developped with column major order. But row major make it compatible with numpy I think.
  include_directories(ext/eigen)
  set(PHASESHIFT_INCLUDES ${PHASESHIFT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/ext/eigen)
  set(PHASESHIFT_DEFINITIONS ${PHASESHIFT_DEFINITIONS} -DEIGEN_MPL2_ONLY)
  message(STATUS "  Include directory: " ${CMAKE_CURRENT_SOURCE_DIR}/ext/eigen)

  # libsndfile ----------------------------------------------------------------
  if (PHASESHIFT_SUPPORT_SNDFILE_LOCAL)
    message(STATUS)
    message(STATUS "libsndfile (local build)")
    set(PHASESHIFT_SOURCES_SUPPORT ${PHASESHIFT_SOURCES_SUPPORT} ${PROJECT_NAME}/audio_block/sndfile.cpp)

    # Can't do that as it breaks some configurations about testing
    # add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ext/libsndfile ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_build)

    # TODO(GD) Use custom_command(.) and custom_target(.)
    include(ExternalProject)
    ExternalProject_Add(libsndfile
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

    set(PHASESHIFT_INCLUDES ${PHASESHIFT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install/include)
    if (WIN32)
      set(PHASESHIFT_LIBRARIES ${PHASESHIFT_LIBRARIES} ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install/lib/libsndfile.dll)
      set(PHASESHIFT_INSTALL_FILES ${PHASESHIFT_INSTALL_FILES} ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install/lib/libsndfile.dll)
    elseif(APPLE)
      set(PHASESHIFT_LIBRARIES ${PHASESHIFT_LIBRARIES} ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install/lib/libsndfile.1.dylib)
      set(PHASESHIFT_INSTALL_FILES ${PHASESHIFT_INSTALL_FILES} ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install/lib/libsndfile.1.dylib)
    else()
      set(PHASESHIFT_LIBRARIES ${PHASESHIFT_LIBRARIES} ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install/lib/libsndfile.so)
      set(PHASESHIFT_INSTALL_FILES ${PHASESHIFT_INSTALL_FILES} ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install/lib/libsndfile.so)
      # set(PHASESHIFT_LIBRARIES ${PHASESHIFT_LIBRARIES} ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install/lib/libsndfile.a)
      # set(PHASESHIFT_INSTALL_FILES ${PHASESHIFT_INSTALL_FILES} ${CMAKE_CURRENT_BINARY_DIR}/libsndfile_install/lib/libsndfile.a)
    endif()

  elseif (PHASESHIFT_SUPPORT_SNDFILE)
    message(STATUS)
    message(STATUS "libsndfile")

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

    set(PHASESHIFT_INCLUDES ${PHASESHIFT_INCLUDES} ${LIBSNDFILE_INCLUDE_DIRS})
    set(PHASESHIFT_LIBRARIES ${PHASESHIFT_LIBRARIES} ${LIBSNDFILE_LDFLAGS} ${LIBSNDFILE_LIBRARIES})

    set(PHASESHIFT_SOURCES_SUPPORT ${PHASESHIFT_SOURCES_SUPPORT} ${PROJECT_NAME}/audio_block/sndfile.cpp)
  endif()


# The lib itself --------------------------------------------------------------
message(STATUS)
message(STATUS "The ${PROJECT_NAME} library itself:")

add_library(${PROJECT_NAME} STATIC ${PROJECT_NAME}/utils.cpp ${PROJECT_NAME}/dev/time_elapsed_summary.cpp ${PROJECT_NAME}/audio_block/ol.cpp ${PROJECT_NAME}/audio_block/ola.cpp ${PROJECT_NAME}/lookup_table.cpp ${PROJECT_NAME}/sigproc/clipper.cpp ${PHASESHIFT_SOURCES_SUPPORT})

# Wait for sndfile to be built before building anything with phaseshift
if (PHASESHIFT_SUPPORT_SNDFILE_LOCAL)
  add_dependencies(${PROJECT_NAME} libsndfile )
endif()

set(PHASESHIFT_SOURCES_TESTS ${PHASESHIFT_SOURCES_TESTS} ${CMAKE_CURRENT_SOURCE_DIR}/phaseshift/audio_block/tester.cpp ${CMAKE_CURRENT_SOURCE_DIR}/phaseshift/dev/catch2_extra.cpp)

set(PHASESHIFT_INCLUDES ${PHASESHIFT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR})
set(PHASESHIFT_INCLUDES ${PHASESHIFT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/ext/)
target_include_directories(${PROJECT_NAME} PUBLIC ${PHASESHIFT_INCLUDES})
set(PHASESHIFT_LIBRARIES phaseshift ${PHASESHIFT_LIBRARIES} ${PHASESHIFT_POST_LIBRARIES})

# Projects depending on the lib -----------------------------------------------

  # Tests ---------------------------------------------------------------------

  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test_data/totest)
  add_compile_definitions(PHASESHIFT_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  set(PHASESHIFT_DEFINITIONS ${PHASESHIFT_DEFINITIONS} -DPHASESHIFT_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

  if(PHASESHIFT_DEV_TESTS)
    message(STATUS "Compiling tests")
    set(CMAKE_CTEST_ARGUMENTS "--output-on-failure")
    include(CTest)

    if ((NOT PHASESHIFT_SUPPORT_SNDFILE_LOCAL) AND (NOT PHASESHIFT_SUPPORT_SNDFILE))
      message(FATAL_ERROR "When activating tests (PHASESHIFT_DEV_TESTS), you need to either activate PHASESHIFT_SUPPORT_SNDFILE (use system libsndfile library) or PHASESHIFT_SUPPORT_SNDFILE_LOCAL (rebuild libsndfile in the build directory).")
    endif()

    if (WIN32)
    else()
      set(PHASESHIFT_LIBRARIES ${PHASESHIFT_LIBRARIES} -lpthread)
    endif()

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ext/Catch2)

    add_executable(phaseshift_utils_test phaseshift/utils_test.cpp)
    target_include_directories(phaseshift_utils_test PUBLIC ${PHASESHIFT_INCLUDES})
    target_link_libraries(phaseshift_utils_test PRIVATE ${PHASESHIFT_LIBRARIES} Catch2::Catch2WithMain)
    add_test(NAME phaseshift_utils_test COMMAND phaseshift_utils_test)

    add_executable(phaseshift_ringbuffer_test phaseshift/containers/ringbuffer_test.cpp)
    target_include_directories(phaseshift_ringbuffer_test PUBLIC ${PHASESHIFT_INCLUDES})
    target_link_libraries(phaseshift_ringbuffer_test PRIVATE ${PHASESHIFT_LIBRARIES} Catch2::Catch2WithMain)
    add_test(NAME phaseshift_ringbuffer_test COMMAND phaseshift_ringbuffer_test)

    add_executable(phaseshift_audio_block_ola_test phaseshift/audio_block/ola_test.cpp ${PHASESHIFT_SOURCES_TESTS})
    target_include_directories(phaseshift_audio_block_ola_test PUBLIC ${PHASESHIFT_INCLUDES})
    target_link_libraries(phaseshift_audio_block_ola_test PRIVATE ${PHASESHIFT_LIBRARIES} Catch2::Catch2WithMain)
    add_test(NAME phaseshift_audio_block_ola_test COMMAND phaseshift_audio_block_ola_test)

    add_executable(phaseshift_lookup_table_test phaseshift/lookup_table_test.cpp)
    target_include_directories(phaseshift_lookup_table_test PUBLIC ${PHASESHIFT_INCLUDES})
    target_link_libraries(phaseshift_lookup_table_test PRIVATE ${PHASESHIFT_LIBRARIES} Catch2::Catch2WithMain)
    add_test(NAME phaseshift_lookup_table_test COMMAND phaseshift_lookup_table_test)

    add_executable(phaseshift_lookup_table_clipper_test phaseshift/sigproc/clipper_test.cpp)
    target_include_directories(phaseshift_lookup_table_clipper_test PUBLIC ${PHASESHIFT_INCLUDES})
    target_link_libraries(phaseshift_lookup_table_clipper_test PRIVATE ${PHASESHIFT_LIBRARIES} Catch2::Catch2WithMain)
    add_test(NAME phaseshift_lookup_table_clipper_test COMMAND phaseshift_lookup_table_clipper_test)
    endif()

  # Benchmarks ----------------------------------------------------------------
  # if(PHASESHIFT_BENCHMARKS)
  #   add_subdirectory(benchmarks/ola)
  #   add_dependencies(benchmark_ola ${PROJECT_NAME})

  #   add_subdirectory(benchmarks/interpolation)
  #   add_dependencies(benchmark_interpolation ${PROJECT_NAME})

  #   # TODO(GD) Add one for lookup tables
  # endif()


# Some info -------------------------------------------------------------------
message(STATUS "  PHASESHIFT_DEFINITIONS=" )
foreach(dir ${PHASESHIFT_DEFINITIONS})
  message(STATUS "       ${dir}")
endforeach()
message(STATUS "  PHASESHIFT_INCLUDES=" )
foreach(dir ${PHASESHIFT_INCLUDES})
  message(STATUS "       ${dir}")
endforeach()
message(STATUS "  PHASESHIFT_LIBRARIES=" )
foreach(dir ${PHASESHIFT_LIBRARIES})
  message(STATUS "       ${dir}")
endforeach()
message(STATUS "  PHASESHIFT_INSTALL_FILES=" )
foreach(dir ${PHASESHIFT_INSTALL_FILES})
  message(STATUS "       ${dir}")
endforeach()

message(STATUS "  CMAKE_C_FLAGS=${CMAKE_C_FLAGS}")
message(STATUS "  CMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}")
message(STATUS "  CMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}")
message(STATUS "  CMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}")

if (NOT "${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
  set(PHASESHIFT_DEFINITIONS ${PHASESHIFT_DEFINITIONS} PARENT_SCOPE)
  set(PHASESHIFT_INCLUDES ${PHASESHIFT_INCLUDES} PARENT_SCOPE)
  set(PHASESHIFT_LIBRARIES ${PHASESHIFT_LIBRARIES} PARENT_SCOPE)
  set(PHASESHIFT_SOURCES_TESTS ${PHASESHIFT_SOURCES_TESTS} PARENT_SCOPE)
  set(PHASESHIFT_INSTALL_FILES ${PHASESHIFT_INSTALL_FILES} PARENT_SCOPE)
  set(PHASESHIFT_BUILD_ARCH ${PHASESHIFT_BUILD_ARCH} PARENT_SCOPE)
  set(FFTSCARF_FFT_IPP_LIBDIR ${FFTSCARF_FFT_IPP_LIBDIR} PARENT_SCOPE)
endif()
