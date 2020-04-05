##
## Copyright (c) 2016, Lawrence Livermore National Security, LLC.
##
## Produced at the Lawrence Livermore National Laboratory.
##
## All rights reserved.
##
##

set(RAJA_COMPILER "RAJA_COMPILER_XLC" CACHE STRING "")

set(CMAKE_C_COMPILER   "/usr/tce/packages/spectrum-mpi/spectrum-mpi-rolling-release-xl-2020.03.18/bin/mpicc" CACHE PATH "")
set(CMAKE_CXX_COMPILER "/usr/tce/packages/spectrum-mpi/spectrum-mpi-rolling-release-xl-2020.03.18/bin/mpicxx" CACHE PATH "")

set(CMAKE_CXX_FLAGS "-I/g/g90/hkhetaw/scorep/install/include" CACHE STRING "")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ffast-math -I/g/g90/hkhetaw/scorep/install/include" CACHE STRING "")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -ffast-math -I/g/g90/hkhetaw/scorep/install/include" CACHE STRING "")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -I/g/g90/hkhetaw/scorep/install/include" CACHE STRING "")

set(ENABLE_CHAI On CACHE BOOL "")
set(ENABLE_CUDA On CACHE BOOL "")
set(ENABLE_MPI On CACHE BOOL "")
set(ENABLE_OPENMP Off CACHE BOOL "")
set(ENABLE_MPI_WRAPPER On CACHE BOOL "")

set(CMAKE_CUDA_FLAGS "-restrict -gencode=arch=compute_70,code=sm_70 -I/g/g90/hkhetaw/scorep/install/include" CACHE STRING "")
set(CMAKE_CUDA_FLAGS_RELEASE "-O3 --expt-extended-lambda -I/g/g90/hkhetaw/scorep/install/include" CACHE STRING "")
set(CMAKE_CUDA_FLAGS_RELWITHDEBINFO "-O3 -lineinfo --expt-extended-lambda -I/g/g90/hkhetaw/scorep/install/include" CACHE STRING "")
set(CMAKE_CUDA_FLAGS_DEBUG "-O0 -g -G --expt-extended-lambda -I/g/g90/hkhetaw/scorep/install/include" CACHE STRING "")
set(CMAKE_CUDA_HOST_COMPILER "${CMAKE_CXX_COMPILER}" CACHE STRING "")

set(CMAKE_CXX_LINK_EXECUTABLE "scorep --user --nocompiler --noopenmp --nopomp --cuda --noopenacc --noopencl --nomemory mpicxx <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>" CACHE STRING "")
set(CMAKE_C_LINK_EXECUTABLE "scorep --user --nocompiler --noopenmp --nopomp --cuda --noopenacc --noopencl --nomemory mpicxx <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>" CACHE STRING "")

