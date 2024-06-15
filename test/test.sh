#!/bin/bash

set -e

LLVM_PATH=${LLVM_PATH:-"/home/hzy/hzydata/softwares/llvm/clang+llvm-16.0.0-x86_64-linux-gnu-ubuntu-18.04"}
LLVM_BIN_PATH=${LLVM_PATH}/bin
LLVM_LIB_PATH=${LLVM_PATH}/lib
CLANG=${LLVM_BIN_PATH}/clang++

echo "LLVM_PATH:${LLVM_PATH}"
echo "LLVM_BIN_PATH:${LLVM_BIN_PATH}"
echo "LLVM_LIB_PATH:${LLVM_LIB_PATH}"
echo "CLANG:${CLANG}"

export LD_LIBRARY_PATH=${LLVM_LIB_PATH}

echo "captor ========== ========== ========== ========== =========="
${CLANG} -fplugin=../cmake-build-release/libcaptor.so -fplugin-arg-captor-2 -c -o $1.o $1