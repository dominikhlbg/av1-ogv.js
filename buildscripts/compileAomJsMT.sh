#!/bin/bash

dir=`pwd`

# set up the build directory
mkdir -p build
cd build

mkdir -p js
cd js

mkdir -p root
mkdir -p libaom
cd libaom

# finally, run configuration script
emcmake cmake ../../../libaom/ -DBUILD_SHARED_LIBS=1 \
        -DENABLE_CCACHE=1 \
	-DCONFIG_HIGHBITDEPTH=0 \
        -DAOM_TARGET_CPU=generic \
        -DENABLE_DOCS=0 \
        -DENABLE_EXAMPLES=0 \
        -DCONFIG_ACCOUNTING=0 \
        -DCONFIG_INSPECTION=0 \
        -DCONFIG_MULTITHREAD=1 \
        -DCONFIG_RUNTIME_CPU_DETECT=0 \
        -DCONFIG_UNIT_TESTS=0 \
        -DCONFIG_WEBM_IO=1 \
	-DCONFIG_AV1_DECODER=1 \
	-DCONFIG_AV1_ENCODER=0 \
	-DCMAKE_INSTALL_PREFIX:PATH="$dir/build/js-mt/root" \
	-DCMAKE_TOOLCHAIN_FILE=/home/dominik/emsdk/emscripten/1.37.36/cmake/Modules/Platform/Emscripten.cmake \
	-DCMAKE_CROSSCOMPILING_EMULATOR=/home/dominik/emsdk/node/8.9.1_64bit/bin/node

emmake make -j4
emmake make install

cd ..
cd ..
cd ..
