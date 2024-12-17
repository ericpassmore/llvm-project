#!/bin/bash

SOURCE=${1:-simple-main}

ROOT=/Users/eric/eosnetworkfoundation/repos/ericpassmore/llvm-project/
cd ${ROOT}/passmore-external || exit

if [ ! -s "${SOURCE}".cpp ]; then
  echo "Could not file ${SOURCE}.cpp file; exiting "
  exit 127
fi

clang --target=wasm32 -nostdlib -Wl,--no-entry -Wl,--export=apply -o "${SOURCE}".o -c "${SOURCE}".cpp
wasm-ld "${SOURCE}".o --no-entry --export=apply --allow-undefined -o "${SOURCE}".wasm
