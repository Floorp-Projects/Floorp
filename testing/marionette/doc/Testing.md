# Running Tests

We verify and test Marionette in a couple of different ways.
While for the server component XPCShell tests are used, the
client and harness packages both use Python.

For debugging test failures which are happing in CI, a one-click loaner from
[Taskcluster](Taskcluster.html) can be used.

## Setting up the tests

Marionette-based tests can be run in-tree with a locally built
or downloaded Firefox through `mach`, as well as with out-of-tree
tests with `marionette`.

Running in-tree tests, `mach` will automatically manage the Python
virtual environment in which your tests are run.  The Marionette
client that is picked up is the one that is in-tree at
_testing/marionette/client_.

If you want to run tests from a downloaded test archive, you will
need to download the `target.common.tests.zip` artifact as attached to
Treeherder [build jobs] `B` for your system.  Extract that file and set up
the Python Marionette client and harness by executing the following
command:

    % pip install -r config/marionette_requirements.txt

The tests can then be found under
*marionette/tests/testing/marionette/harness/marionette_harness/tests* and
can be executed with the command `marionette`.  It supports the same options as
described below for `mach`.

[build jobs]: https://treeherder.mozilla.org/#/jobs?repo=mozilla-central&filter-searchStr=build


## XPCShell tests

Marionette has a set of [xpcshell] unit tests located in
_testing/marionette/test*.js_.  These can be run this way:

    % ./mach test testing/marionette/test_*.js

Because tests are run in parallel and xpcshell itself is quite
chatty, it can sometimes be useful to run the tests sequentially:

    % ./mach test --sequential testing/marionette/test_error.js

These unit tests run as part of the `X` jobs on Treeherder.

[xpcshell]: https://developer.mozilla.org/en-US/docs/Mozilla/QA/Writing_xpcshell-based_unit_tests


## Python functional tests

We also have a set of functional tests that make use of the Marionette
Python client.  These start a Firefox process and tests the Marionette
protocol input and output.  The following command will run all tests:

    % ./mach marionette-test

But you can also run individual tests:

    % ./mach marionette-test testing/marionette/harness/marionette_harness/tests/unit/test_navigation.py

In case you want to run the tests with eg. a Nightly build of Firefox,
mach let you do this with the --binary option:

    % ./mach marionette-test --binary=/path/to/firefox-executable TEST

When working on Marionette code it is often useful to surface the
stdout from Firefox, which can be achived with the `--gecko-log` option.
See [Debugging](Debugging.html) for usage instructions.

As these are functional integration tests and pop up Firefox windows
sporadically, a helpful tip is to surpress the window whilst you
are running them by using Firefox’ [headless mode]:

    % ./mach marionette-test -z TEST

`-z` is an alias for `--headless` and equivalent to setting the
`MOZ_HEADLESS` output variable.  In addition to `MOZ_HEADLESS`
there is also `MOZ_HEADLESS_WIDTH` and `MOZ_HEADLESS_HEIGHT` for
controlling the dimensions of the no-op virtual display.  This is
similar to using xvfb(1) which you may know from the X windowing system,
but has the additional benefit of also working on macOS and Windows.

These functional tests will run as part of the `Mn` job on Treeherder.

In addition to these two test types that specifically test the
Marionette protocol, Marionette is used as the backend for the
[geckodriver] WebDriver implementation.  It is served by a WPT test
suite which effectively tests conformance to the W3C specification.

[headless mode]: https://developer.mozilla.org/en-US/Firefox/Headless_mode
[geckodriver]: ../geckodriver/README.md


## Python harness tests

The Marionette harness Python package has a set of unit tests, which
are written by using the [pytest] framework. The following command will
run all tests:

    % ./mach python-test testing/marionette/

To run a specific test specify the full path to the module:

    % ./mach python-test testing/marionette/harness/marionette_harness/tests/harness_unit/test_serve.py

[pytest]: https://docs.pytest.org/en/latest/
