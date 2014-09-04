web-platform-tests
==================

This directory contains the W3C
[web-platform-tests](http://github.com/w3c/web-platform-tests). They
can be run using `mach`:

    mach web-platform-tests

To limit the testrun to certain directories use the `--include` option;
for example:

    mach web-platform-tests --include=dom

The testsuite contains a mix of javascript tests and reftests. To
limit the type of tests that get run, use `--test-type=testharness` for
javascript tests or `--test-type=reftest` for reftests.

FAQ
---

* I fixed a bug and some tests have started to pass. How do I fix the
  UNEXPECTED-PASS messages when web-platform-tests is run?

  You need to update the expectation data for those tests. See the
  section on expectations below.

* I want to write some new tests for the web-platform-tests
  testsuite. How do I do that?

  See the section on tests below. At the moment you will have to
  submit the tests directly to the W3C and wait for them to be
  imported into the Mozilla tree.

* A test is unstable; how do I disable it?

  See the section on disabling tests.

Directories
-----------

`tests/` contains the tests themselves. This is a copy of a certain
revision of web-platform-tests. The contents of this directory must
not be modified locally as any modifications will be overwritten
whenever a new upstream sync is performed.

`harness/` contains the [wptrunner](http://github.com/w3c/wptrunner)
test runner. Again the contents of this directory will be overwritten
on update.

`meta/` contains Gecko-specific expectation data. This is explained in
the following section.

Expectation Data
----------------

With the tests coming from upstream, it is not guaranteed that they
all pass in Gecko-based browsers. For this reason it is necessary to
provide metadata about the expected results of each test. This is
provided in a set of manifest files in the `meta/` subdirectories.

There is one manifest file per test with "non-default"
expectations. By default tests are expected to PASS, and tests with
subtests are expected to have an overall status of OK. The manifest
file of a test has the same path as the test file but under the `meta`
directory rather than the `tests` directory and has the suffix `.ini`.

The format of these files is similar to `ini` files, but with a couple
of important differences; sections can be nested using indentation,
and only `:` is permitted as a key-value separator. For example the
expectation file for a test with one failing subtest and one erroring
subtest might look like:

    [filename.html]
        type: testharness

        [Subtest name for failing test]
            expected: FAIL

        [Subtest name for erroring test]
            expected: ERROR

Expectations can also be made platform-specific using a simple
python-like conditional syntax e.g. for a test that times out on linux
but otherwise fails:

    [filename.html]
        type: reftest
        expected:
            if os == "linux": TIMEOUT
            FAIL

The available variables for the conditions are those provided by
[mozinfo](http://mozbase.readthedocs.org/en/latest/mozinfo.html).

For more information on manifest files, see the
[wptrunner documentation](http://wptrunner.readthedocs.org/en/latest/expectation.html).

Autogenerating Expectation Data
-------------------------------

After changing some code it may be necessary to update the expectation
data for the relevant tests. This can of course be done manually, but
tools are available to automate much of the process.

First one must run the tests that have changed status, and save the
raw log output to a file:

    mach web-platform-tests --include=url/of/test.html --log-raw=new_results.log

Then the `web-platform-tests-update` command may be run using this log
data to update the expectation files:

    mach web-platform-tests-update --no-check-clean new_results.log

By default this only updates the results data for the current
platform. To forcibly overwrite all existing result data, use the
`--ignore-existing` option to the update command.

Disabling Tests
---------------

Tests are disabled using the same manifest files used to set
expectation values. For example, if a test is unstable on Windows, it
can be disabled using an ini file with the contents:

    [filename.html]
        type: testharness
        disabled:
            if os == "win": https://bugzilla.mozilla.org/show_bug.cgi?id=1234567

Test Format
-----------

Javascript tests are written using
[testharness.js](http://github.com/w3c/testharness.js/). Reftests are
similar to standard Gecko reftests without an explicit manifest file,
but with in-test or filename conventions for identifying the
reference.

New tests must presently be submitted upstream before they can be run
on Mozilla infrastructure. This situation is expected to be temporary.

Full documentation on test authoring and submission can be found on
[testthewebforward.org](http://testthewebforward.org/docs).

Running Tests In Other Browsers
-------------------------------

web-platform-tests is cross browser, and the runner is compatible with
multiple browsers. Therefore it's possible to check the behaviour of
tests in other browsers. This is somewhat more involved than running
them in Firefox since extra dependencies may be required. For example
to test in Chrome:

1. Download the chromedriver binary and place it somewhere sensible
   e.g. `~/bin`

2. In your gecko source tree activate the virtualenv created by mach,
   since this has most dependencies already installed. This is typically
   in objdir/_virtualenv and is activated via e.g.

        source objdir/_virtualenv/bin/activate

3. Install the extra requirements:

        cd testing/web-platform/harness
        pip install -r requirements_chrome.txt

4. Edit the config file `testing/web-platform/wptrunner.ini` so that
   Chrome support is enabled by changing the section that reads:

        [products]
        firefox =

   to read

        [products]
        firefox =
        chrome =

   (alternatively create a new config file elsewhere and use the
   `--config` option to `runtests.py` to point wptrunner at this config
   file).

5. Run `runtests.py` using the location of chromedriver as
   the binary:

        cd testing/web-platform
        python runtests.py --product=chrome --binary=~/bin/chromedriver --log-mach=-

By default this will use the same test checkout and metadata as are in
the Gecko tree, so it's easy to compare behaviour relative to Firefox.
