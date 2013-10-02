Rather than make changes to the built files here, make them upstream and then
upgrade our tree's copy of the built files.

To upgrade the source-map library:

    $ git clone https://github.com/mozilla/source-map.git
    $ git co source-map
    $ git co <latest-tagged-version>
    $ npm run-script build
    $ cp dist/SourceMap.jsm /path/to/mozilla-central/toolkit/devtools/sourcemap/SourceMap.jsm
    $ cp dist/test/* /path/to/mozilla-central/toolkit/devtools/sourcemap/tests/unit/

