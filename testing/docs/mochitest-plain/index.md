# Mochitest

## DISCLAIMER

If you are testing web platform code, prefer using use a [wpt
test](/web-platform/index.rst) (preferably upstreamable ones).

## Introduction

Mochitest is an automated testing framework built on top of the
[MochiKit](https://mochi.github.io/mochikit/) JavaScript libraries.

Only things that can be tested using JavaScript (with chrome privileges!) can be
tested with this framework. Given some creativity, that's actually much more
than you might first think, but it's not possible to write Mochitest tests to
directly test a non-scripted C++ component, for example. (Use a compiled-code
test like [GTest](/gtest/index.rst) to do that.)

## Running tests

To run a single test (perhaps a new test you just added) or a subset of the
entire Mochitest suite, pass a path parameter to the `mach` command.

For example, to run only the test `test_CrossSiteXHR.html` in the Mozilla source
tree, you would run this command:

```
./mach test dom/security/test/cors/test_CrossSiteXHR.html
```

To run all the tests in `dom/svg/`, this command would work:

```
./mach test dom/svg/
```

You can also pass a manifest path to run all tests on that manifest:

```
./mach test dom/base/test/mochitest.ini
```

## Running flavors and subsuites

Flavors are variations of the default configuration used to run Mochitest. For
example, a flavor might have a slightly different set of prefs set for it, a
custom extension installed or even run in a completely different scope.

The Mochitest flavors are:

  * **plain** - The most basic and common Mochitest. They run in content scope,
    but can access certain privileged APIs with SpecialPowers.

  * **browser** - These often test the browser UI itself and run in browser
    window scope.

  * **chrome** - These run in chrome scope and are typically used for testing
    privileged JavaScript APIs. More information can be found
    [here](../chrome-tests/index.rst).

  * **a11y** - These test the accessibility interfaces. They can be found under
    the top `accessible` directory and run in chrome scope. Note that these run
    without e10s / fission.

A subsuite is similar to a flavor, except that it has an identical
configuration. It is just logically separated from the "default" subsuite for
display purposes. For example, devtools is a subsuite of the browser flavor.
There is no difference in how these two jobs are run. It exists so that the
devtools team can easily see and run their tests.

**Note**: There are also tags, which are similar to subsuites. Although they
both are used to logically group related sets of tests, they behave
differently. For example, applying a subsuite to a test removes that test from
the default set, whereas, a tag does not remove it.

By default, mach finds and runs every test in the given subdirectory no matter
which flavor or subsuite it belongs to. But sometimes, you might only want to
run a specific flavor or subsuite. This can be accomplished using the `--flavor`
(or `-f`) and `--subsuite` options respectively. For example:


```
./mach mochitest -f plain                        # runs all plain tests
./mach mochitest -f browser --subsuite devtools  # runs all browser tests in the devtools subsuite
./mach mochitest -f chrome dom/indexedDB         # runs all chrome tests in the dom/indexedDB subdirectory
```

In many cases, it won't be necessary to filter by flavor or subsuite as running
specific directories will do it implicitly. For example running:

```
./mach mochitest devtools/
```

Is a rough equivalent to running the `devtools` subsuite. There might be
situations where you might want to run tests that don't belong to any subsuite.
To do this, use:

```
./mach mochitest --subsuite default
```

## Debugging individual tests

If you need to debug an individual test, you could reload the page containing
the test with the debugger attached. If attaching a debugger before the problem
shows up is hard (for example, if the browser crashes as the test is loading),
you can specify a debugger when you run mochitest:

```
./mach mochitest --debugger=gdb ...
```

See also the `--debugger-args` and `--debugger-interactive` arguments. You can
also use the `--jsdebugger` argument to debug JavaScript.

## Finding errors

Search for the string `TEST-UNEXPECTED-FAIL` to find unexpected failures. You
can also search for `SimpleTest FINISHED` to see the final test summary.
## Logging results

The output from a test run can be sent to the console and/or a file (by default
the results are only displayed in the browser). There are several levels of
detail to choose from. The levels are `DEBUG`, `INFO`, `WARNING`, `ERROR` and
`CRITICAL`, where `DEBUG` produces the highest detail (everything), and
`CRITICAL` produces the least.

Mochitest uses structured logging. This means that you can use a set of command
line arguments to configure the log output. To log to stdout using the mach
formatter and log to a file in JSON format, you can use `--log-mach=-`
`--log-raw=mochitest.log`. By default the file logging level for all your
formatters is `INFO` but you can change this using `--log-mach-level=<level>`.

To turn on logging to the console use `--console-level=<level>`.

For example, to log test run output with the default (tbpl) formatter to the
file `~/mochitest.log` at `DEBUG` level detail you would use:

```
./mach mochitest --log-tbpl=~/mochitest.log --log-tbpl-level=DEBUG
```

## Headless mode

The tests must run in a focused window, which effectively prevents any other
user activity on the engaged computer. You can avoid this by using the
`--headless` argument or `MOZ_HEADLESS=1` environment variable.

```
./mach mochitest --headless ...
```

## Writing tests

A Mochitest plain test is simply an HTML or XHTML file that contains some
JavaScript to test for some condition.

### Asynchronous Tests

Sometimes tests involve asynchronous patterns, such as waiting for events or
observers. In these cases, you need to use `add_task`:

```js
add_task(async function my_test() {
  let keypress = new Promise(...);
  // .. simulate keypress
  await keypress;
  // .. run test
});
```

Use `add_setup()` when asynchronous test task is meant to prepare test for run.
All setup tasks are executed once in order they appear prior to any test tasks.

```js
add_setup(async () => {
  await clearStorage();
});
```

Or alternatively, manually call `waitForExplicitFinish` and `finish`:

```js
SimpleTest.waitForExplicitFinish();
addEventListener("keypress", function() {
  // ... run test ...
  SimpleTest.finish();
}, false);
// ... simulate key press ...
```


If you need more time, `requestLongerTimeout(number)` can be quite useful.
`requestLongerTimeout()` takes an integer factor that is a multiplier for the
default 45 seconds timeout. So a factor of 2 means: "Wait for at last 90s
(2*45s)". This is really useful if you want to pause execution to do a little
debugging.

### Test functions

Each test must contain some JavaScript that will run and tell Mochitest whether
the test has passed or failed. `SimpleTest.js` provides a number of functions
for the test to use, to communicate the results back to Mochitest. These
include:


 * `ok(expressionThatShouldBeTrue, "Description of the check")` -- tests a value for its truthfulness
 * `is(actualValue, expectedValue, "Description of the check")` -- compares two values (using Object.is)
 * `isnot(actualValue, unexpectedValue, "Description of the check")` -- opposite of is()

If you want to include a test for something that currently fails, don't just
comment it out! Instead, use one of the "todo" equivalents so we notice if it
suddenly starts passing (at which point the test can be re-enabled):

 * `todo(falseButShouldBeTrue, "Description of the check")`
 * `todo_is(actualValue, expectedValue, "Description of the check")`
 * `todo_isnot(actualValue, unexpectedValue, "Description of the check")`

Tests can call a function `info("Message string")` to write a message to the
test log.

In addition to mochitest assertions, mochitest supports the
[CommonJS standard assertions](http://wiki.commonjs.org/wiki/Unit_Testing/1.1),
like [nodejs' assert module](https://nodejs.org/api/assert.html#assert) but
implemented in `Assert.jsm`. These are auto-imported in the browser flavor, but
need to be imported manually in other flavors.

### Helper functions

Right now, useful helpers derived from MochiKit are available in
[`testing/mochitest/tests/SimpleTest/SimpleTest.js`](https://searchfox.org/mozilla-central/source/testing/mochitest/tests/SimpleTest/SimpleTest.js).

Although all of Mochikit is available at `testing/mochitest/MochiKit`, only
include files that you require to minimize test load times. Bug 367569 added
`sendChar`, `sendKey`, and `sendString` helpers.
These are available in [`testing/mochitest/tests/SimpleTest/EventUtils.js`](https://searchfox.org/mozilla-central/source/testing/mochitest/tests/SimpleTest/EventUtils.js).

If you need to access some data files from your Mochitest, you can get an URI
for them by using `SimpleTest.getTestFileURL("relative/path/to/data.file")`.
Then you can eventually fetch their content by using `XMLHttpRequest` or so.

### Adding tests to the tree

`mach addtest` is the preferred way to add a test to the tree:

```
./mach addtest --suite mochitest-{plain,chrome,browser-chrome} path/to/new/test
```

That will add the manifest entry to the relevant manifest (`mochitest.ini`,
`chrome.ini`, etc. depending on the flavor) to tell the build system about your
new test, as well as creating the file based on a template.

```ini
[test_new_feature.html]
```

Optionally, you can specify metadata for your test, like whether to skip the
test on certain platforms:

```ini
[test_new_feature.html]
skip-if = os == 'win'
```

The [mochitest.ini format](/build/buildsystem/test_manifests.rst), which is
recognized by the parser, defines a long list of metadata.

### Adding a new mochitest.ini or chrome.ini file

If a `mochitest.ini` or `chrome.ini` file does not exist in the test directory
where you want to add a test, add them and update the moz.build file in the
directory for your test. For example, in `gfx/layers/moz.build`, we add
these two manifest files:

```python
MOCHITEST_MANIFESTS += ['apz/test/mochitest.ini']
MOCHITEST_CHROME_MANIFESTS += ['apz/test/chrome.ini']
```

<!--  TODO: This might be outdated.*

## Getting Stack Traces


To get stack when Mochitest crashes:

 * Get a minidump_stackwalk binary for your platform from http://hg.mozilla.org/build/tools/file/tip/breakpad/
 * Set the MINIDUMP_STACKWALK environment variable to point to the absolute path of the binary.

If the resulting stack trace doesn't have line numbers, run `mach buildsymbols`
to generate the requisite symbol files.

-->

## FAQ

See the [Mochitest FAQ page](faq.md) for other features and such that you may
want to use, such as SSL-enabled tests, custom http headers, async tests, leak
debugging, prefs...
