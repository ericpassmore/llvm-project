#!/bin/bash

CLEAN=${1:-No}

ROOT=/Users/eric/eosnetworkfoundation/repos/ericpassmore/llvm-project/
cd $ROOT || exit

if [ $CLEAN == "Y" ]; then
  [-d ./build ] && rm -rf ./build
fi
[! -d ./build] && mkdir ./build

cmake -S llvm  -B build/ -G'Unix Makefiles' \
-DCMAKE_INSTALL_PREFIX=$(pwd)/install \
-DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS="clang;lld" -DLLVM_CCACHE_BUILD=OFF

cd build

make -j 8
