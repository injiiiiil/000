# HIP_PATH
IF(NOT DEFINED $ENV{HIP_PATH})
  SET(HIP_PATH /opt/rocm/hip)
ELSE()
  SET(HIP_PATH $ENV{HIP_PATH})
ENDIF()

# HCC_PATH
IF(NOT DEFINED $ENV{HCC_PATH})
  SET(HCC_PATH /opt/rocm/hcc)
ELSE()
  SET(HCC_PATH $ENV{HCC_PATH})
ENDIF()

# HIPBLAS_PATH
IF(NOT DEFINED $ENV{HIPBLAS_PATH})
  SET(HIPBLAS_PATH /opt/rocm/hipblas)
ELSE()
  SET(HIPBLAS_PATH $ENV{HIPBLAS_PATH})
ENDIF()

# HIPRNG_PATH
IF(NOT DEFINED $ENV{HIPRNG_PATH})
  SET(HIPRNG_PATH /opt/rocm/hcrng)
ELSE()
  SET(HIPRNG_PATH $ENV{HIPRNG_PATH})
ENDIF()

# HIPSPARSE_PATH
IF(NOT DEFINED $ENV{HIPSPARSE_PATH})
  SET(HIPSPARSE_PATH /opt/rocm/hcsparse)
ELSE()
  SET(HIPSPARSE_PATH $ENV{HIPSPARSE_PATH})
ENDIF()

SET(THRUST_PATH "/root/Thrust")

# Add HIP to the CMAKE Module Path
set(CMAKE_MODULE_PATH ${HIP_PATH}/cmake ${CMAKE_MODULE_PATH})

# Disable Asserts In Code
ADD_DEFINITIONS(-DNDEBUG)

# Find the HIP Package
FIND_PACKAGE(HIP 1.0 REQUIRED)
