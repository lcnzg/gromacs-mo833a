#!/usr/bin/env bash

cd ../..

if [[ ! -f "build-release/bin/gmx" ]]; then
    mkdir -p build-release && cd build-release
    cmake .. -DGMX_BUILD_OWN_FFTW=ON -DREGRESSIONTEST_DOWNLOAD=ON
    make -j`nproc`
    make check -j`nproc`
    cd ..
fi

if [[ ! -f "build-debug/bin/gmx" ]]; then
    mkdir -p build-debug && cd build-debug
    cmake .. -DGMX_BUILD_OWN_FFTW=ON -DREGRESSIONTEST_DOWNLOAD=ON -DCMAKE_BUILD_TYPE=Debug
    make -j`nproc`
    make check -j`nproc`
    cd ..
fi

