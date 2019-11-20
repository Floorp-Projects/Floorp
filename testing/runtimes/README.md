Test Runtimes
=============

These files contain runtimes for test manifests in the tree. They are of the form:

    { '<path to manifest>': <average runtime in seconds> }

They are being used to normalize chunk durations so all chunks take roughly
the same length of time.

Generating a Test Runtime File
------------------------------

The ``writeruntimes`` script can be used to generate this file:

    $ ./writeruntimes

It will take awhile. You can optionally specify platforms or suites on the
command line, but these should only be used for debugging purposes (not for
committing an update to the data). For more info, see:

    $ ./writeruntimes -- --help
