To build SpiderMonkey for use in 0 A.D. on Linux or OS X, run ./build.sh

To build on Windows (if you don't want to use the precompiled binaries in SVN),
get https://developer.mozilla.org/en/Windows_Build_Prerequisites#MozillaBuild
then run start-msvc8.bat and run ./build.sh here.

This version of SpiderMonkey comes from
https://ftp.mozilla.org/pub/mozilla.org/js/mozjs-24.2.0.tar.bz2

The game must be compiled with precisely this version since SpiderMonkey 
does not guarantee API stability and may have behavioural changes that 
cause subtle bugs or network out-of-sync errors.
A standard system-provided version of the library may only be used if it's
exactly the same version or if it's another minor release that does not 
change the behaviour of the scripts executed by SpiderMonkey.
