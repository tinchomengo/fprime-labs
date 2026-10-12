####
# This sets up the build system for the 'IssTracker' project, including
# components and deployments from project.cmake. In addition, it imports the core F Prime components.
####

cmake_minimum_required(VERSION 3.24.2)
project(IssTracker C CXX)

###
# F' Core Setup
# This includes all of the F prime core components, and imports the make-system.
###
include("${CMAKE_CURRENT_LIST_DIR}/lib/fprime/cmake/FPrime.cmake")
# NOTE: register custom targets between these two lines

###
# SGP4 library (dnwrnr/sgp4, git submodule in lib/sgp4)
# Only the library sources are compiled here: the repository's own top-level
# CMakeLists.txt also downloads googletest and builds example programs.
###
set(SGP4_ROOT "${CMAKE_CURRENT_LIST_DIR}/lib/sgp4")
add_library(sgp4 STATIC
    "${SGP4_ROOT}/libsgp4/CoordGeodetic.cpp"
    "${SGP4_ROOT}/libsgp4/CoordTopocentric.cpp"
    "${SGP4_ROOT}/libsgp4/DateTime.cpp"
    "${SGP4_ROOT}/libsgp4/DecayedException.cpp"
    "${SGP4_ROOT}/libsgp4/Eci.cpp"
    "${SGP4_ROOT}/libsgp4/Globals.cpp"
    "${SGP4_ROOT}/libsgp4/Observer.cpp"
    "${SGP4_ROOT}/libsgp4/OrbitalElements.cpp"
    "${SGP4_ROOT}/libsgp4/SGP4.cpp"
    "${SGP4_ROOT}/libsgp4/SatelliteException.cpp"
    "${SGP4_ROOT}/libsgp4/SolarPosition.cpp"
    "${SGP4_ROOT}/libsgp4/TimeSpan.cpp"
    "${SGP4_ROOT}/libsgp4/Tle.cpp"
    "${SGP4_ROOT}/libsgp4/TleException.cpp"
    "${SGP4_ROOT}/libsgp4/Util.cpp"
    "${SGP4_ROOT}/libsgp4/Vector.cpp"
)
# The library sources need C++17; its headers also compile as C++14, which F´ uses
set_target_properties(sgp4 PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_include_directories(sgp4 PUBLIC "${SGP4_ROOT}")

fprime_setup_included_code()


# This includes project-wide objects
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/IssTracker")
