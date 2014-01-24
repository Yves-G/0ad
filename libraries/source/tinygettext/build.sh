#!/bin/sh

set -e

JOBS=${JOBS:="-j2"}
MAKE=${MAKE:="make"}

echo "Building tinygettext..."
echo

cd src/

cmake -G "Unix Makefiles"

${MAKE} ${JOBS}

cd ../

mkdir -p lib/

filepath=src/libtinygettext.so
cp $filepath lib/
cp $filepath ../../../binaries/system/
