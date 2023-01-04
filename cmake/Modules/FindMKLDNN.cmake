# - Try to find MKLDNN
#
# The following variables are optionally searched for defaults
#  MKL_FOUND             : set to true if a library implementing the CBLAS interface is found
#
# The following are set after configuration is done:
#  MKLDNN_FOUND          : set to true if mkl-dnn is found.
#  MKLDNN_INCLUDE_DIR    : path to mkl-dnn include dir.
#  MKLDNN_LIBRARIES      : list of libraries for mkl-dnn
#
# The following variables are used:
#  MKLDNN_USE_NATIVE_ARCH : Whether native CPU instructions should be used in MKLDNN. This should be turned off for
#  general packaging to avoid incompatible CPU instructions. Default: OFF.

IF(NOT MKLDNN_FOUND)
  SET(MKLDNN_LIBRARIES)
  SET(MKLDNN_INCLUDE_DIR)

  SET(IDEEP_ROOT "${PROJECT_SOURCE_DIR}/third_party/ideep")
  SET(MKLDNN_ROOT "${PROJECT_SOURCE_DIR}/third_party/ideep/mkl-dnn/third_party/oneDNN")
  IF(NOT APPLE AND NOT WIN32 AND NOT BUILD_LITE_INTERPRETER)
    MESSAGE("-- Will build oneDNN Graph")
    SET(LLGA_ROOT "${PROJECT_SOURCE_DIR}/third_party/ideep/mkl-dnn")
    SET(BUILD_ONEDNN_GRAPH ON)
  ENDIF(NOT APPLE AND NOT WIN32 AND NOT BUILD_LITE_INTERPRETER)

  FIND_PACKAGE(BLAS)
  FIND_PATH(IDEEP_INCLUDE_DIR ideep.hpp PATHS ${IDEEP_ROOT} PATH_SUFFIXES include)
  FIND_PATH(MKLDNN_INCLUDE_DIR dnnl.hpp dnnl.h PATHS ${MKLDNN_ROOT} PATH_SUFFIXES include)
  IF(NOT MKLDNN_INCLUDE_DIR)
    EXECUTE_PROCESS(COMMAND git${CMAKE_EXECUTABLE_SUFFIX} submodule update --init mkl-dnn WORKING_DIRECTORY ${IDEEP_ROOT})
    FIND_PATH(MKLDNN_INCLUDE_DIR dnnl.hpp dnnl.h PATHS ${MKLDNN_ROOT} PATH_SUFFIXES include)
  ENDIF(NOT MKLDNN_INCLUDE_DIR)
  IF(BUILD_ONEDNN_GRAPH)
    FIND_PATH(LLGA_INCLUDE_DIR oneapi/dnnl/dnnl_graph.hpp PATHS ${LLGA_ROOT} PATH_SUFFIXES include)
  ENDIF(BUILD_ONEDNN_GRAPH)

  IF(NOT IDEEP_INCLUDE_DIR OR NOT MKLDNN_INCLUDE_DIR)
    MESSAGE(STATUS "MKLDNN source files not found!")
    RETURN()
  ENDIF(NOT IDEEP_INCLUDE_DIR OR NOT MKLDNN_INCLUDE_DIR)
  LIST(APPEND MKLDNN_INCLUDE_DIR ${IDEEP_INCLUDE_DIR})
  IF(BUILD_ONEDNN_GRAPH)
    LIST(APPEND MKLDNN_INCLUDE_DIR ${LLGA_INCLUDE_DIR})
  ENDIF(BUILD_ONEDNN_GRAPH)
  IF(MKL_FOUND)
    ADD_DEFINITIONS(-DIDEEP_USE_MKL)
    # Append to mkldnn dependencies
    LIST(APPEND MKLDNN_LIBRARIES ${MKL_LIBRARIES})
    LIST(APPEND MKLDNN_INCLUDE_DIR ${MKL_INCLUDE_DIR})
  ELSE(MKL_FOUND)
    SET(MKLDNN_USE_MKL "NONE" CACHE STRING "" FORCE)
  ENDIF(MKL_FOUND)

  SET(MKL_cmake_included TRUE)
  IF(NOT MKLDNN_CPU_RUNTIME)
    SET(MKLDNN_CPU_RUNTIME "OMP" CACHE STRING "")
  ELSEIF(MKLDNN_CPU_RUNTIME STREQUAL "TBB")
    IF(USE_TBB)
      MESSAGE(STATUS "MKL-DNN is using TBB")

      SET(TBB_cmake_included TRUE)
      SET(Threading_cmake_included TRUE)

      SET(DNNL_CPU_THREADING_RUNTIME ${MKLDNN_CPU_RUNTIME})
      INCLUDE_DIRECTORIES(${TBB_INCLUDE_DIR})
      LIST(APPEND EXTRA_SHARED_LIBS TBB::tbb)
    ELSE()
      MESSAGE(FATAL_ERROR "MKLDNN_CPU_RUNTIME is set to TBB but TBB is not used")
    ENDIF()
  ENDIF()
  MESSAGE(STATUS "MKLDNN_CPU_RUNTIME = ${MKLDNN_CPU_RUNTIME}")

  SET(MKLDNN_CPU_RUNTIME ${MKLDNN_CPU_RUNTIME} CACHE STRING "" FORCE)
  SET(DNNL_BUILD_TESTS FALSE CACHE BOOL "" FORCE)
  SET(DNNL_BUILD_EXAMPLES FALSE CACHE BOOL "" FORCE)
  SET(DNNL_LIBRARY_TYPE STATIC CACHE STRING "" FORCE)
  SET(DNNL_ENABLE_PRIMITIVE_CACHE TRUE CACHE BOOL "" FORCE)
  SET(DNNL_GRAPH_CPU_RUNTIME ${MKLDNN_CPU_RUNTIME} CACHE STRING "" FORCE)

  IF(BUILD_ONEDNN_GRAPH)
    SET(DNNL_GRAPH_LIBRARY_TYPE STATIC CACHE STRING "" FORCE)
  ENDIF(BUILD_ONEDNN_GRAPH)
  IF(MKLDNN_USE_NATIVE_ARCH)  # Disable HostOpts in MKLDNN unless MKLDNN_USE_NATIVE_ARCH is set.
    SET(DNNL_ARCH_OPT_FLAGS "HostOpts" CACHE STRING "" FORCE)
  ELSE()
    IF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      IF(CPU_INTEL)
        SET(DNNL_ARCH_OPT_FLAGS "-msse4" CACHE STRING "" FORCE)
      ENDIF()
    ELSE()
      SET(DNNL_ARCH_OPT_FLAGS "" CACHE STRING "" FORCE)
    ENDIF()
  ENDIF()

  IF(BUILD_ONEDNN_GRAPH)
    ADD_SUBDIRECTORY(${LLGA_ROOT})
    IF(NOT TARGET dnnl_graph)
      MESSAGE("Failed to include LLGA target")
      RETURN()
    ENDIF(NOT TARGET dnnl_graph)

    IF(CMAKE_COMPILER_IS_GNUCC)
      TARGET_COMPILE_OPTIONS(dnnl_graph PRIVATE -Wno-maybe-uninitialized)
      TARGET_COMPILE_OPTIONS(dnnl_graph PRIVATE -Wno-strict-overflow)
      TARGET_COMPILE_OPTIONS(dnnl_graph PRIVATE -Wno-error=strict-overflow)
    ENDIF(CMAKE_COMPILER_IS_GNUCC)
  ELSE(BUILD_ONEDNN_GRAPH)
    ADD_SUBDIRECTORY(${MKLDNN_ROOT})
  ENDIF(BUILD_ONEDNN_GRAPH)

  IF(NOT TARGET dnnl)
    MESSAGE("Failed to include MKL-DNN target")
    RETURN()
  ENDIF(NOT TARGET dnnl)

  IF(NOT APPLE AND CMAKE_COMPILER_IS_GNUCC)
    TARGET_COMPILE_OPTIONS(dnnl PRIVATE -Wno-maybe-uninitialized)
    TARGET_COMPILE_OPTIONS(dnnl PRIVATE -Wno-strict-overflow)
    TARGET_COMPILE_OPTIONS(dnnl PRIVATE -Wno-error=strict-overflow)
  ENDIF(NOT APPLE AND CMAKE_COMPILER_IS_GNUCC)
  LIST(APPEND MKLDNN_LIBRARIES ${MKL_OPENMP_LIBRARY})
  IF(BUILD_ONEDNN_GRAPH)
    LIST(APPEND MKLDNN_LIBRARIES "$<TARGET_FILE:dnnl_graph>")
  ENDIF(BUILD_ONEDNN_GRAPH)
  LIST(APPEND MKLDNN_LIBRARIES dnnl)

  SET(MKLDNN_FOUND TRUE)
  MESSAGE(STATUS "Found MKL-DNN: TRUE")

ENDIF(NOT MKLDNN_FOUND)
