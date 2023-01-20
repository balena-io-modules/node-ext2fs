#!/usr/bin/env sh

set -ex

TOOLCHAIN_VERSION=3.1.5

# On macOS, make sure you have Xcode Command Line Tools installed.
#
# On Linux, install first Python with:
#
#  $ apt-get install python3
#
# For detailed instructions check:
#
# https://emscripten.org/docs/getting_started/downloads.html#installation-instructions-using-the-emsdk-recommended

git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

./emsdk install ${TOOLCHAIN_VERSION}
./emsdk activate ${TOOLCHAIN_VERSION}
