# Breakpad

Breakpad is a set of client and server components which implement a
crash-reporting system.

* [Homepage](https://chromium.googlesource.com/breakpad/breakpad/)
* [Documentation](https://chromium.googlesource.com/breakpad/breakpad/+/master/docs/)
* [Bugs](https://bugs.chromium.org/p/google-breakpad/)
* Discussion/Questions: [google-breakpad-discuss@googlegroups.com](https://groups.google.com/d/forum/google-breakpad-discuss)
* Developer/Reviews: [google-breakpad-dev@googlegroups.com](https://groups.google.com/d/forum/google-breakpad-dev)
* Tests: [![Build Status](https://travis-ci.org/google/breakpad.svg?branch=master)](https://travis-ci.org/google/breakpad)
* Coverage [![Coverity Status](https://scan.coverity.com/projects/9215/badge.svg)](https://scan.coverity.com/projects/google-breakpad)

## Getting started (from master)

1.  First, [download depot_tools](http://dev.chromium.org/developers/how-tos/install-depot-tools)
    and ensure that they’re in your `PATH`.

2.  Create a new directory for checking out the source code (it must be named
    breakpad).

    ```sh
    mkdir breakpad && cd breakpad
    ```

3.  Run the `fetch` tool from depot_tools to download all the source repos.

    ```sh
    fetch breakpad
    cd src
    ```

4.  Build the source.

    ```sh
    ./configure && make
    ```

    You can also cd to another directory and run configure from there to build
    outside the source tree.

    This will build the processor tools (`src/processor/minidump_stackwalk`,
    `src/processor/minidump_dump`, etc), and when building on Linux it will
    also build the client libraries and some tools
    (`src/tools/linux/dump_syms/dump_syms`,
    `src/tools/linux/md2core/minidump-2-core`, etc).

5.  Optionally, run tests.

    ```sh
    make check
    ```

6.  Optionally, install the built libraries

    ```sh
    make install
    ```

If you need to reconfigure your build be sure to run `make distclean` first.

To update an existing checkout to a newer revision, you can
`git pull` as usual, but then you should run `gclient sync` to ensure that the
dependent repos are up-to-date.

## To request change review

1.  Follow the steps above to get the source and build it.

2.  Make changes. Build and test your changes.
    For core code like processor use methods above.
    For linux/mac/windows, there are test targets in each project file.

3.  Commit your changes to your local repo and upload them to the server.
    http://dev.chromium.org/developers/contributing-code
    e.g. `git commit ... && git cl upload ...`
    You will be prompted for credential and a description.

4.  At https://chromium-review.googlesource.com/ you'll find your issue listed;
    click on it, then “Add reviewer”, and enter in the code reviewer. Depending
    on your settings, you may not see an email, but the reviewer has been
    notified with google-breakpad-dev@googlegroups.com always CC’d.
