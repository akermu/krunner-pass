#!/bin/bash

set -e

mkdir -p build
cd build

cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_INSTALL_PREFIX=`kf5-config --prefix` -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install

set +e

kquitapp5 krunner
kstart5 --windowclass krunner krunner
