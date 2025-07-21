# Build Steps

1. generate buildid2 buildid2 is timestamp for checking build time, and for
   update. the buildid2 is checked on update if the base version of Firefox,
   build timestamp (buildid) and Noraneko version (version2) is same. the update
   don't affect in develop process.

2. extract binary

3. symlink source files to build /modules and /chrome do not have build script
   itself. /chrome/\* should be symlinked to loader-features. /modules should be
   symlinked to loader-modules

4. build the source do build with startup, loader-features, loader-modules
   startup is loader in browser, loading the chrome scripts that is by
   loader-features

5. create manifest the manifest is a file that makes Firefox to know there is
   source.

6. symlink built files symlink \_dist folders in startup, loader-features,
   loader-modules to /\_dist/noraneko symlink /\_dist/noraneko to
   /\_dist/bin/noraneko/noraneko-dev

7. run the browser if in development
