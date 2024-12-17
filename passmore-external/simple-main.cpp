//
// Created by Eric Passmore on 12/17/24.
//
#include <stdio.h>

extern "C" {

  // Declare the entry point function expected by wasm-ld
  void apply(unsigned long long receiver, unsigned long long code, unsigned long long action) {
    printf("Wasm apply function called with receiver: %llu, code: %llu, action: %llu\n",
           receiver, code, action);
  }
}