cmake_minimum_required(VERSION 3.19)
project(bandwidth-test VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
include(CMakePrintHelpers)

option(BT_BUILD_TESTS "Build tests" OFF)

# ---------------------------------------------------------------------------
# clipp (header-only)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    clipp
    GIT_REPOSITORY https://github.com/muellan/clipp.git
    GIT_TAG v1.2.3
)
FetchContent_MakeAvailable(clipp)

# ---------------------------------------------------------------------------
# MPI
# ---------------------------------------------------------------------------
set(HAVE_MPI OFF)
if(DEFINED ENV{MPI_HOME})
    set(MPI_HOME $ENV{MPI_HOME})
endif()

find_package(MPI)
if(MPI_FOUND)
    set(HAVE_MPI ON)
    message(STATUS "MPI found: ${MPI_CXX_COMPILER}")
    message(STATUS "MPI include: ${MPI_CXX_INCLUDE_DIRS}")
    message(STATUS "MPI libs: ${MPI_CXX_LIBRARIES}")
endif()

# ---------------------------------------------------------------------------
# ZeroMQ + cppzmq
# ---------------------------------------------------------------------------
set(HAVE_ZMQ OFF)
set(ZMQ_SYSTEM_FOUND OFF)

# Try system ZMQ first
find_package(ZMQ QUIET)
if(ZMQ_FOUND)
    set(HAVE_ZMQ ON)
    set(ZMQ_SYSTEM_FOUND ON)
    message(STATUS "ZMQ found via find_package")
endif()

if(NOT HAVE_ZMQ)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(ZMQ QUIET libzmq)
        if(ZMQ_FOUND)
            set(HAVE_ZMQ ON)
            set(ZMQ_SYSTEM_FOUND ON)
            message(STATUS "ZMQ found via pkg-config")
        endif()
    endif()
endif()

# Always fetch cppzmq for C++ bindings (it's header-only)
FetchContent_Declare(
    cppzmq
    GIT_REPOSITORY https://github.com/zeromq/cppzmq.git
    GIT_TAG v4.11.0
)
FetchContent_MakeAvailable(cppzmq)

# Fetch libzmq only if not found on system
if(NOT HAVE_ZMQ)
    if(HAVE_MPI)
        message(STATUS "ZMQ not found on system; fetching libzmq + cppzmq")
    else()
        message(WARNING "ZMQ not found on system and MPI not available; cannot build any backend.")
        message(WARNING "Install ZMQ (libzmq3-dev / zeromq-devel) or MPI (OpenMPI/MPICH).")
    endif()

    FetchContent_Declare(
        libzmq
        GIT_REPOSITORY https://github.com/zeromq/libzmq.git
        GIT_TAG v4.3.5
    )
    set(BUILD_SHARED_LIBS OFF CACHE STRING "" FORCE)
    set(ZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CPPZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WITH_PERF_TOOL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(libzmq)
    set(HAVE_ZMQ ON)
endif()

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
set(SOURCES
    src/main.cpp
    src/cli.cpp
    src/benchmark.cpp
    src/output.cpp
    src/bandwidth_tester.cpp
)

if(HAVE_ZMQ)
    list(APPEND SOURCES src/zmq_bandwidth_tester.cpp)
endif()

if(HAVE_MPI)
    list(APPEND SOURCES src/mpi_bandwidth_tester.cpp)
endif()

add_executable(bandwidth-test ${SOURCES})

target_include_directories(bandwidth-test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${clipp_SOURCE_DIR}/include
        ${cppzmq_SOURCE_DIR}
)

if(HAVE_MPI)
    target_compile_definitions(bandwidth-test PRIVATE HAVE_MPI=1)
    target_link_libraries(bandwidth-test PRIVATE MPI::MPI_CXX)
endif()

if(HAVE_ZMQ)
    target_compile_definitions(bandwidth-test PRIVATE HAVE_ZMQ=1)
    if(ZMQ_SYSTEM_FOUND)
        target_link_libraries(bandwidth-test PRIVATE ${ZMQ_LIBRARIES})
        target_include_directories(bandwidth-test PRIVATE ${ZMQ_INCLUDE_DIRS} ${ZMQ_LIBRARY_DIRS})
        target_link_directories(bandwidth-test PRIVATE ${ZMQ_LIBRARY_DIRS})
    elseif(TARGET libzmq)
        target_link_libraries(bandwidth-test PRIVATE libzmq)
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BT_BUILD_TESTS)
    # Fetch Catch2
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0
    )
    FetchContent_MakeAvailable(Catch2)

    enable_testing()

    add_executable(bandwidth-test-tests tests/test_benchmark.cpp src/benchmark.cpp)
    target_include_directories(bandwidth-test-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(bandwidth-test-tests PRIVATE Catch2::Catch2WithMain)

    message(STATUS "Tests enabled: bandwidth-test-tests")
endif()

message(STATUS "========== bandwidth-test config ==========")
message(STATUS "  HAVE_MPI : ${HAVE_MPI}")
message(STATUS "  HAVE_ZMQ : ${HAVE_ZMQ}")
message(STATUS "  CXX      : ${CMAKE_CXX_COMPILER} (${CMAKE_CXX_COMPILER_ID})")
message(STATUS "  C++ std  : ${CMAKE_CXX_STANDARD}")
message(STATUS "============================================")
