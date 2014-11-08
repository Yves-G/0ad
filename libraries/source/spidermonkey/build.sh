#!/bin/sh

set -e

# Since this script is called by update-workspaces.sh, we want to quickly
# avoid doing any work if SpiderMonkey is already built and up-to-date.
# Running SM's Makefile is a bit slow and noisy, so instead we'll make a
# special file and only rebuild if it's older than SVN.
# README.txt should be updated whenever we update SM, so use that as
# a time comparison.
if [ -e .already-built -a .already-built -nt README.txt ]
then
    echo "SpiderMonkey is already up to date"
    exit
fi

echo "Building SpiderMonkey..."
echo

MAKE=${MAKE:="make"}

MAKE_OPTS="${JOBS}"

CONF_OPTS="--enable-shared-js --enable-gcgenerational --disable-tests --without-intl-api" # --enable-trace-logging"

# Modify TRACE_LOG_DIR to define where the tracelogger should store its output (if enabled with --enable-trace-logging)
TLCXXFLAGS='-DTRACE_LOG_DIR="\"/tmp/traces/\""'

# We bundle prebuilt binaries for Windows and the .libs for nspr aren't included.
# If you want to build on Windows, check README.txt and edit the absolute paths 
# to match your enviroment.
# TODO: Test on Windows and update NSPR
if [ "${OS}" = "Windows_NT" ]
then
  NSPR_INCLUDES="-IC:/Projects/0ad/libraries/source/spidermonkey/nspr-4.10.3/nspr/dist/include/nspr"
  NSPR_LIBS=" \
  C:/Projects/0ad/libraries/source/spidermonkey/nspr-4.10.3/nspr/dist/lib/nspr4.lib \
  C:/Projects/0ad/libraries/source/spidermonkey/nspr-4.10.3/nspr/dist/lib/plds4.lib \
  C:/Projects/0ad/libraries/source/spidermonkey/nspr-4.10.3/nspr/dist/lib/plc4.lib"
else
  NSPR_INCLUDES="`pkg-config nspr --cflags`"
  NSPR_LIBS="`pkg-config nspr --libs`"
fi

# If Valgrind looks like it's installed, then set up SM to support it
# (else the JITs will interact poorly with it)
if [ -e /usr/include/valgrind/valgrind.h ]
then
  CONF_OPTS="${CONF_OPTS} --enable-valgrind"
fi

# We need to be able to override CHOST in case it is 32bit userland on 64bit kernel
CONF_OPTS="${CONF_OPTS} \
  ${CBUILD:+--build=${CBUILD}} \
  ${CHOST:+--host=${CHOST}} \
  ${CTARGET:+--target=${CTARGET}}"

echo "SpiderMonkey build options: ${CONF_OPTS}"
echo ${CONF_OPTS}

# Delete the existing directory to avoid conflicts and extract the tarball
rm -rf mozjs31
tar xjf mozjs-31.2.0.rc0.tar.bz2
mv mozjs-31.2.0 mozjs31

# Apply patches if needed
cd mozjs31
# Tracelogger patches (the Tracelogger is a tool for developers and these
# patches are only needed if the tracelogger is used):
patch -p1 -i ../FixTraceLoggerBuild.diff
patch -p1 -i ../FixTraceLoggerFlushing.diff
cd ..

# Clean up header files that may be left over by earlier versions of SpiderMonkey
rm -rf include-unix-*

cd mozjs31/js/src

# Clean up data generated by previous builds that could cause problems
rm -rf build-debug
rm -rf build-release

# Run autoconf to create the configure script
autoconf2.13

# We want separate debug/release versions of the library, so we have to change
# the LIBRARY_NAME for each build.
# (We use perl instead of sed so that it works with MozillaBuild on Windows,
# which has an ancient sed.)
perl -i.bak -pe 's/(LIBRARY_NAME\s+=).*/$1 '\''mozjs31-ps-debug'\''/' moz.build
mkdir -p build-debug
cd build-debug
CXXFLAGS="${TLCXXFLAGS}" ../configure ${CONF_OPTS} --with-nspr-libs="$NSPR_LIBS" --with-nspr-cflags="$NSPR_INCLUDES" --enable-debug --disable-optimize --enable-js-diagnostics --enable-gczeal # --enable-root-analysis
${MAKE} ${MAKE_OPTS}
cd ..

perl -i.bak -pe 's/(LIBRARY_NAME\s+=).*/$1 '\''mozjs31-ps-release'\''/' moz.build
mkdir -p build-release
cd build-release
CXXFLAGS="${TLCXXFLAGS}" ../configure ${CONF_OPTS} --with-nspr-libs="$NSPR_LIBS" --with-nspr-cflags="$NSPR_INCLUDES" --enable-optimize  # --enable-gczeal --enable-debug-symbols
${MAKE} ${MAKE_OPTS}
cd ..

cd ../../..

if [ "${OS}" = "Windows_NT" ]
then
  INCLUDE_DIR_DEBUG=include-win32-debug
  INCLUDE_DIR_RELEASE=include-win32-release
  DLL_SRC_SUFFIX=.dll
  DLL_DST_SUFFIX=.dll
  LIB_PREFIX=
  LIB_SRC_SUFFIX=.lib
  LIB_DST_SUFFIX=.lib
else
  INCLUDE_DIR_DEBUG=include-unix-debug
  INCLUDE_DIR_RELEASE=include-unix-release
  DLL_SRC_SUFFIX=.so
  DLL_DST_SUFFIX=.so
  LIB_PREFIX=lib
  LIB_SRC_SUFFIX=.so
  LIB_DST_SUFFIX=.so
  if [ "`uname -s`" = "OpenBSD" ]
  then
    DLL_SRC_SUFFIX=.so.1.0
    DLL_DST_SUFFIX=.so.1.0
    LIB_SRC_SUFFIX=.so.1.0
    LIB_DST_SUFFIX=:so.1.0
  fi
fi

# Copy files into the necessary locations for building and running the game

# js-config.h is different for debug and release builds, so we need different include directories for both
mkdir -p ${INCLUDE_DIR_DEBUG}
mkdir -p ${INCLUDE_DIR_RELEASE}
cp -R -L mozjs31/js/src/build-release/dist/include/* ${INCLUDE_DIR_RELEASE}/
cp -R -L mozjs31/js/src/build-debug/dist/include/* ${INCLUDE_DIR_DEBUG}/

mkdir -p lib/
cp -L mozjs31/js/src/build-debug/dist/lib/${LIB_PREFIX}mozjs31-ps-debug${LIB_SRC_SUFFIX} lib/${LIB_PREFIX}mozjs31-ps-debug${LIB_DST_SUFFIX}
cp -L mozjs31/js/src/build-release/dist/lib/${LIB_PREFIX}mozjs31-ps-release${LIB_SRC_SUFFIX} lib/${LIB_PREFIX}mozjs31-ps-release${LIB_DST_SUFFIX}
cp -L mozjs31/js/src/build-debug/dist/bin/${LIB_PREFIX}mozjs31-ps-debug${DLL_SRC_SUFFIX} ../../../binaries/system/${LIB_PREFIX}mozjs31-ps-debug${DLL_DST_SUFFIX}
cp -L mozjs31/js/src/build-release/dist/bin/${LIB_PREFIX}mozjs31-ps-release${DLL_SRC_SUFFIX} ../../../binaries/system/${LIB_PREFIX}mozjs31-ps-release${DLL_DST_SUFFIX}

# Flag that it's already been built successfully so we can skip it next time
touch .already-built
