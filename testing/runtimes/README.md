Test Runtimes
=============

These files contain test runtimes for various suites across different platforms. Each JSON file
corresponds to a single test job in production and has the following format:

    { '<test id>': <average runtime> }

These files are being used to normalize chunk durations so all chunks take roughly the same length
of time. They are still experimental and their format and/or file structure are subject to change
without notice.

Generating a Test Runtime File
------------------------------

The writeruntimes.py script can be used to generate a runtime file. You must
specify the suite for which the runtimes are to be generated, e.g.

    writeruntimes.py -s mochitest-media
