#!/bin/bash

. ./buildscripts/compile-options.sh

# compile wrapper around libaom

emcc \
  $EMCC_COMMON_OPTIONS \
  $EMCC_ASMJS_OPTIONS \
  $EMCC_THREADED_OPTIONS \
  -s TOTAL_MEMORY=33554432 \
  -s EXPORT_NAME="'OGVDecoderVideoAV1MT'" \
  -s EXPORTED_FUNCTIONS="`< src/js/modules/ogv-decoder-video-exports.json`" \
  -Ibuild/js-mt/root/include \
  -Lbuild/js-mt/root/lib \
  build/js-mt/root/lib/libaom.so \
  --js-library src/js/modules/ogv-decoder-video-callbacks.js \
  --pre-js src/js/modules/ogv-module-pre.js \
  --post-js src/js/modules/ogv-decoder-video.js \
  -D OGV_AV1 \
  src/c/ogv-decoder-video-aom.c \
  -o build/ogv-decoder-video-av1-mt.js
