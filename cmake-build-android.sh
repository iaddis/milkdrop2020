#!/bin/bash 
set -e
cmake   -S . \
        -B build.android \
        -DCMAKE_TOOLCHAIN_FILE=~/Library/Android/sdk/ndk-bundle/build/cmake/android.toolchain.cmake \
        -DANDROID_ABI=arm64-v8a \
        -DANDROID_PLATFORM=android-24 \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo
make -C build.android
