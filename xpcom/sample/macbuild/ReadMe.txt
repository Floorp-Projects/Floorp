
Because this sample is not part of the build system, you
have to do some things by hand. Build it like this:

1. Have a built tree.

2. Build SampleIDL.mcp, headers target. This should make an alias
   to nsISample.h in dist:xpconnect (actual location in dist
   does not matter).
   
3. Build SampleIDL.mcp, sample.xpt target. This makes sample.xpt.
   Put an alias to sample.xpt in your Components folder.
   
4. Build a target of Sample.mcp. Make an alias to the resulting
   shared lib in the components folder.
   
5. Run viewer or apprunner, and load "xpconnect-sample.html".
   Things should work.
   
sfraser@netscape.com
