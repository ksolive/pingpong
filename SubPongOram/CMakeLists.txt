cmake_minimum_required(VERSION 3.16)

# Some of the following blocks are copied from
# https://github.com/openenclave/openenclave/blob/master/samples/helloworld/CMakeLists.txt
# and licensed under SPDX-License-Identifier: MIT.
# They are marked by `## oecode`.
# The original file header is:
# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.
# The original `LICENSE` file is embedded below:
# MIT License
#
# Copyright (c) Open Enclave SDK contributors.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE

## oecode
if (LVI_MITIGATION MATCHES ControlFlow)
  # Configure the cmake to use customized compilation toolchain.
  # This package has to be added before `project()`.
  find_package(OpenEnclave-LVI-Mitigation CONFIG REQUIRED)
else ()
  # Setting the cmake compiler when LVI mitigation is not enabled. If the CC
  # environment variable has been specified or the if CMAKE_C_COMPILER cmake
  # variable has been passed to cmake, use the C compiler that has been specified.
  # Otherwise, prefer clang. Same for C++ compiler. This must be done before
  # the `project` command.
  if (UNIX)
    if (NOT DEFINED ENV{CC} AND NOT DEFINED CMAKE_C_COMPILER)
      find_program(CMAKE_C_COMPILER clang-11 clang-10 clang)
    endif ()
    if (NOT DEFINED ENV{CXX} AND NOT DEFINED CMAKE_CXX_COMPILER)
      find_program(CMAKE_CXX_COMPILER clang++-11 clang++-10 clang++)
    endif ()
  endif ()
endif ()

project(rworam-enclave LANGUAGES CXX C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

## oecode
set(OE_CRYPTO_LIB openssl_3 CACHE STRING "Crypto library used by enclaves")

find_package(OpenEnclave CONFIG REQUIRED)

add_executable(
  rworam-enclave
  api/rworam_t.c
  src/lib.cpp
  src/oht_crypto.cpp
  src/oblsort_worker.cpp
  src/oht/obl/par_obl_primitives.cpp
)
target_compile_definitions(rworam-enclave PUBLIC OE_API_VERSION=2)
target_include_directories(rworam-enclave PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(rworam-enclave PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(
  rworam-enclave PRIVATE
  OHT_OBL_OBLSORT_INCLUDEIMPL=1
  RWORAM_ENCLAVE_EXPERIMENT=1
  RWORAM_ENCLAVE_LOG=1
)

## oecode
if (LVI_MITIGATION MATCHES ControlFlow)
  # Helper to enable compiler options for LVI mitigation.
  apply_lvi_mitigation(enclave)
  # Link against LVI-mitigated libraries.
  target_link_libraries(
    rworam-enclave
    openenclave::oeenclave-lvi-cfg
    openenclave::oecrypto${OE_CRYPTO_LIB}-lvi-cfg
    openenclave::oelibcxx-lvi-cfg
  )
else ()
  target_link_libraries(
    rworam-enclave PRIVATE
    openenclave::oeenclave
    openenclave::oecrypto${OE_CRYPTO_LIB}
    openenclave::oelibcxx
  )
endif ()
