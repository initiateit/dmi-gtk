#!/bin/bash

try_flags=(
  "-march=znver4"
  "-march=znver3"
  "-march=x86-64-v4"
  "-march=x86-64-v3"
  "-march=native"
)

if ! grep -q avx512 /proc/cpuinfo; then
  echo "AVX-512 not supported — skipping x86-64-v4"
  try_flags=( "${try_flags[@]/-march=x86-64-v4}" )
else
  echo "AVX-512 supported"
fi

for flag in "${try_flags[@]}"; do
  if echo | gcc $flag -x c - -o /dev/null >/dev/null 2>&1; then
    ARCH_FLAGS=$flag
    echo "Using $ARCH_FLAGS"
    break
  fi
done

gcc $ARCH_FLAGS -O2 -pipe -fomit-frame-pointer main.c dmi-api.c -o dmi-gtk `pkg-config --cflags --libs gtk4` -lddcutil