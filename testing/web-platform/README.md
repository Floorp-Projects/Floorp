web-platform-tests
==================

This directory contains the W3C
[web-platform-tests](http://github.com/w3c/web-platform-tests). They
can be run using `mach`:

    mach wpt

To run only certain tests, pass these as additional arguments to the
command. For example to include all tests in the dom directory:

    mach wpt testing/web-platform/tests/dom

Tests may also be passed by id; this is the path plus any query or
fragment part of a url and is suitable for copying directly from logs
e.g. on treeherder:

    mach wpt /web-nfc/idlharness.https.window.html

A single file can produce multiple tests, so passing test ids rather
than paths is sometimes necessary to run exactly one test.

The testsuite contains a mix of various test types including
javascript (`testharness`) tests, reftests and wdspec tests. To limit
the type of tests that get run, use `--test-type=<type>` e.g.
`--test-type=reftest` for reftests.

Note that if only a single testharness test is run the browser will
stay open by default (matching the behaviour of mochitest). To prevent
this pass `--no-pause-after-test` to `mach wpt`.

Tests can be run in headless mode using the `--headless` command line
argument.

Running in Android (GeckoView)
------------------------------

You can run the tests against a Gecko-based browser (GeckoView) on an
Android emulator. As shown below, to do so you must start an emulator,
build Firefox for Android and then run mach wpt with the
`org.mozilla.geckoview.test` package. The package will be installed
interactively by `mach` and tests will run against TestRunnerActivity.

    ./mach android-emulator --version x86-7.0
    ./mach build
    ./mach wpt --package=org.mozilla.geckoview.test

FAQ
---

* I fixed a bug and some tests have started to pass. How do I fix the
  UNEXPECTED-PASS messages when web-platform-tests is run?

  You need to update the expectation data for those tests. See the
  section on expectations below.

* I want to write some new tests for the web-platform-tests
  testsuite. How do I do that?

  See the section on writing tests below. You can commit the tests
  directly to the Mozilla repository under
  `testing/web-platform/tests` and they will be automatically
  upstreamed when the patch lands. For this reason please ensure that
  any tests you write are testing correct-per-spec behaviour even if
  we don't yet pass, get proper review, and have a commit message that
  makes sense outside of the Mozilla context. If you are writing tests
  that should not be upstreamed yet for some reason they must be
  located under `testing/web-platform/mozilla/tests`.

* How do I write a test that requires the use of a Mozilla-specific
  feature?

  Tests in the `mozilla/tests/` directory use the same harness but are
  not synced with any upstream. Be aware that these tests run on the
  server with a `/_mozilla/` prefix to their URLs.

* How do I write tests that require user interaction or other features
  not accessible from content js?

  For testharness tests this is possible using the
  [testdriver](https://web-platform-tests.org/writing-tests/testdriver.html)
  API.

  For Gecko-specific testharness tests, the specialPowers extension is
  available. Note that this should only be used when no other approach
  works; such tests can't be shared with other browsers. If you're
  using specialPowers for something that could be tested in other
  browsers if we extended testdriver or added test-only APIs, please
  file a bug.

Writing tests
-------------

Documentation for writing tests, and everything else that isn't
specific to the gecko integration can be found at
[https://web-platform-tests.org]().

Directories
-----------

`tests/` contains the tests themselves. This is a copy of a certain
revision of web-platform-tests. Any patches modifying this directory
will be upstreamed next time the tests are imported.

`meta/` contains Gecko-specific expectation data. This is explained in
the following section.

`mozilla/tests` contains tests that will not be upstreamed and may
make use of Mozilla-specific features.

`mozilla/meta` contains metadata for the Mozilla-specific tests.

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
        [Subtest name for failing test]
            expected: FAIL

        [Subtest name for erroring test]
            expected: ERROR

Expectations can also be made platform-specific using a simple
python-like conditional syntax e.g. for a test that times out on linux
but otherwise fails:

    [filename.html]
        expected:
            if os == "linux": TIMEOUT
            FAIL

The available variables for the conditions are those provided by
[mozinfo](https://firefox-source-docs.mozilla.org/mozbase/mozinfo.html).

Tests that are intermittent may be marked with multiple statuses using
a list of possibilities e.g. for a test that usually passes, but
intermittently fails:

    [filename.html]
        [Subtest name for intermittent test]
            expected: [PASS, FAIL]


For more information on manifest files, see the [wptrunner
documentation](https://web-platform-tests.org/tools/wptrunner/docs/expectation.html).

Autogenerating Expectation Data
-------------------------------

After changing some code it may be necessary to update the expectation
data for the relevant tests. This can of course be done manually, but
tools are available to automate much of the process.

First one must run the tests that have changed status, and save the
raw log output to a file:

    mach wpt /url/of/test.html --log-wptreport new_results.json

Then the `wpt-update` command may be run using this log data to update
the expectation files:

    mach wpt-update new_results.json

Disabling Tests
---------------

Tests are disabled using the same manifest files used to set
expectation values. For example, if a test is unstable on Windows, it
can be disabled using an ini file with the contents:

    [filename.html]
        disabled:
            if os == "win": https://bugzilla.mozilla.org/show_bug.cgi?id=1234567

For intermittents it's generally preferable to give the test multiple
expectations rather than disable it.

Fuzzy Reftests
--------------

Reftests where the test doesn't exactly match the reference can be
marked as fuzzy. If the difference is inherent to the test, it should
be encoded in a [meta
element](https://web-platform-tests.org/writing-tests/reftests.html#fuzzy-matching),
but where it's a Gecko-specific difference it can be added to the
metadata file, using the same syntax e.g.

    [filename.html]
        fuzzy: maxDifference=10-15;totalPixels=200-300

In this case we specify that we expect between 200 and 300 pixels,
inclusive, to be different, and the maximum difference in any colour
channel to be between 10 and 15.


Enabling Prefs
--------------

Some tests require specific prefs to be enabled before running. These
prefs can be set in the expectation data using a `prefs` key with a
comma-seperate list of `pref.name:value` items to set e.g.

    [filename.html]
        prefs: [dom.serviceWorkers.enabled:true,
                dom.serviceWorkers.exemptFromPerDomainMax:true,
                dom.caches.enabled:true]

Disabling Leak Checks
----------------------

When a test is imported that leaks, it may be necessary to temporarily
disable leak checking for that test in order to allow the import to
proceed. This works in basically the same way as disabling a test, but
with the key 'leaks' e.g.

    [filename.html]
        leaks:
            if os == "linux": https://bugzilla.mozilla.org/show_bug.cgi?id=1234567

Setting per-Directory Metadata
------------------------------

Occasionally it is useful to set metadata for an entire directory of
tests e.g. to disable then all, or to enable prefs for every test. In
that case it is possible to create a `__dir__.ini` file in the
metadata directory corresponding to the tests for which you want to
set this metadata e.g. to disable all the tests in
`tests/feature/unsupported/`, one might create
`meta/feature/unsupported/__dir__.ini` with the contents:

    disabled: Feature is unsupported

Settings set in this way are inherited into subdirectories. It is
possible to unset a value that has been set in a parent using the
special token `@Reset` (usually used with prefs), or to force a value
to true or false using `@True` and `@False`.  For example to enable
the tests in `meta/feature/unsupported/subfeature-supported` one might
create an ini file
`meta/feature/unsupported/subfeature-supported/__dir__.ini` like:

    disabled: @False

Setting Metadata for Release Branches
-------------------------------------

Run info properties can be used to set metadata for release branches
that differs from nightly (e.g. for when a feature depends on prefs
that are only set on nightly), for example:

    [filename.html]
      expected:
        if release_or_beta: FAIL

Note that in general the automatic metadata update will work better if
the nonstandard configuration is used explicitly in the conditional,
and placed at the top of the set of conditions, i.e. the following
would cause problems later:

    [filename.html]
      expected:
        if nightly_build: PASS
        FAIL

This is because on import the automatic metadata updates are run
against the results of nightly builds, and we remove any existing
conditions that match that configuration to avoid building up stale
configuration options.

Test Manifest
-------------

web-platform-tests use a large auto-generated JSON file as their
manifest. This stores data about the type of tests, their references,
if any, and their timeout, gathered by inspecting the filenames and
the contents of the test files. It it not necessary to manually add
new tests to the manifest; it is automatically kept up to date when
running `mach wpt`.


Running Tests In Other Browsers
-------------------------------

web-platform-tests is cross browser, and the runner is compatible with
multiple browsers. Therefore it's possible to check the behaviour of
tests in other browsers. By default Chrome, Edge and Servo are
supported. In order to run the tests in these browsers use the
`--product` argument to wptrunner:

    mach wpt --product chrome dom/historical.html

By default these browsers run without expectation metadata, but it can
be added in the `testing/web-platform/products/<product>`
directory. To run with the same metadata as for Firefox (so that
differences are reported as unexpected results), pass `--meta
testing/web-platform/meta` to the mach command.

Results from the upstream CI for many browser, including Chrome and
Safari, are available on [wpt.fyi](https://wpt.fyi). There is also a
[gecko dashboard](https://jgraham.github.io/wptdash/) which by default
shows tests that are failing in Gecko but not in Chrome and Safari,
organised by component, based on the wpt.fyi data.
