The Firefox remote agent is a low-level debugging interface based
on the CDP protocol.

With it, you can inspect the state and control execution of documents
running in web content, instrument Gecko in interesting ways,
simulate user interaction for automation purposes, and debug
JavaScript execution.

This component provides an experimental and partial implementation
of a remote devtools interface using the CDP protocol and transport
layer.

See https://firefox-source-docs.mozilla.org/remote/ for documentation.

It is available in Firefox and is started this way:

	% ./mach run --remote-debugging-port


Puppeteer
=========
Puppeteer is a Node library which provides a high-level API to control Chrome,
Chromium, and Firefox over the Chrome DevTools Protocol. Puppeteer runs headless
by default, but can be configured to run full (non-headless) browsers.

To verify that our implementation of the CDP protocol is valid we do not only
run xpcshell and browser-chrome mochitests in Firefox CI but also the Puppeteer
unit tests.

Expectation Data
----------------

With the tests coming from upstream, it is not guaranteed that they
all pass in Gecko-based browsers. For this reason it is necessary to
provide metadata about the expected results of each test. This is
provided in a manifest file under `test/puppeteer-expected.json`.

For each test of the Puppeteer unit test suite an equivalent entry will exist
in this manifest file. By default tests are expected to `PASS`.

Tests that are intermittent may be marked with multiple statuses using
a list of possibilities e.g. for a test that usually passes, but
intermittently fails:

    "Page.click should click the button (click.spec.ts)": [
      "PASS", "FAIL"
    ],

Disabling Tests
---------------

Tests are disabled by using the manifest file `test/puppeteer-expected.json`.
For example, if a test is unstable, it can be disabled using `SKIP`:

    "Workers Page.workers (worker.spec.ts)": [
      "SKIP"
    ],

For intermittents it's generally preferable to give the test multiple
expectations rather than disable it.

Autogenerating Expectation Data
-------------------------------

After changing some code it may be necessary to update the expectation
data for the relevant tests. This can of course be done manually, but
`mach` is able to automate the process:

    mach puppeteer-test --write-results

By default it writes the output to `test/puppeteer-expected.json`.

Given that the unit tests run in Firefox CI only for Linux it is advised to
download the expectation data (available as artifact) from the TaskCluster job.
