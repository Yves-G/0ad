#!/bin/sh

set -e

JOBS=${JOBS:="-j2"}

echo "Building tinygettext…"
echo

cd src/

scons ${JOBS}

cd ../

mkdir -p lib/

if [ "`uname -s`" = "Darwin" ]
then
  extension=dylib
else
  extension=so
fi

filepath=src/libtinygettext.${extension}
cp $filepath lib/
cp $filepath ../../../binaries/system/
