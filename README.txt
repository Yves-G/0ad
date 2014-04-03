 ESR31 branch information
=========================

This branch contains the changes for updating 0 A.D. to the SpiderMonkey 31 extended support release (ESR).
ESR31 is still in development and according to the release schedule (https://wiki.mozilla.org/RapidRelease/Calendar)
it's planned to be release in July 2014.


-------------------
 Testing the branch
-------------------

The ESR31 branch currently only works on Linux.

Refer to the 0 A.D. build process here:
http://trac.wildfiregames.com/wiki/BuildInstructions


In addition you need mercurial (hg) to download the current SpiderMonkey version along with the Firefox source.
The code for downloading the right version from Mozilla Central is in the build.sh script which is called from
update-workspaces.sh, so you can just run that as part of the normal build process.

Run the game by starting pyrogenesis from binaries/system after the successful build.


