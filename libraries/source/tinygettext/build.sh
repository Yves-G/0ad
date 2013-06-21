#!/bin/sh

set -e

JOBS=${JOBS:="-j2"}

echo "Building tinygettext…"
echo

scons ${JOBS}

mkdir -p lib/

if [ "`uname -s`" = "Darwin" ]
then
  extension=dylib
else
  extension=so
fi

filepath=tinygettext/libtinygettext.${extension}
cp $filepath lib/
cp $filepath ../../../binaries/system/
