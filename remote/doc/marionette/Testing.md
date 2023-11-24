# Testing

We verify and test Marionette in a couple of different ways, using
a combination of unit tests and functional tests.  There are three
distinct components that we test:

* the Marionette **server**, using a combination of xpcshell
unit tests and functional tests written in Python spread across
Marionette- and WPT tests;

* the Python **client** is tested with the same body of functional
Marionette tests;

* and the **harness** that backs the Marionette, or `Mn` job on
try, tests is verified using separate mock-styled unit tests.

All these tests can be run by using [mach].

[mach]: https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/mach

## xpcshell unit tests

Marionette has a set of [xpcshell] unit tests located in
_remote/marionette/test/xpcshell.  These can be run this way:

```shell
% ./mach test remote/marionette/test/unit
```

Because tests are run in parallel and xpcshell itself is quite
chatty, it can sometimes be useful to run the tests sequentially:

```shell
% ./mach test --sequential remote/marionette/test/xpcshell/test_error.js
```

These unit tests run as part of the `X` jobs on Treeherder.

[xpcshell]: https://developer.mozilla.org/en-US/docs/Mozilla/QA/Writing_xpcshell-based_unit_tests

## Marionette functional tests

We also have a set of [functional tests] that make use of the Marionette
Python client.  These start a Firefox process and tests the Marionette
protocol input and output, and will appear as `Mn` on Treeherder.
The following command will run all tests locally:

```shell
% ./mach marionette-test
```

But you can also run individual tests:

```shell
% ./mach marionette-test testing/marionette/harness/marionette_harness/tests/unit/test_navigation.py
```

In case you want to run the tests with another binary like [Firefox Nightly]:

```shell
% ./mach marionette-test --binary /path/to/nightly/firefox TEST
```

When working on Marionette it is often useful to surface the stdout
from Gecko, which can be achieved using the `--gecko-log` option.
See [Debugging](Debugging.md) for usage instructions, but the gist is that
you can redirect all Gecko output to stdout:

```shell
% ./mach marionette-test --gecko-log - TEST
```

Our functional integration tests pop up Firefox windows sporadically,
and a helpful tip is to suppress the window can be to use Firefox’
headless mode:

```shell
% ./mach marionette-test -z TEST
```

`-z` is an alias for the `--headless` flag and equivalent to setting
the `MOZ_HEADLESS` output variable.  In addition to `MOZ_HEADLESS`
there is also `MOZ_HEADLESS_WIDTH` and `MOZ_HEADLESS_HEIGHT` for
controlling the dimensions of the no-op virtual display.  This is
similar to using Xvfb(1) which you may know from the X windowing system,
but has the additional benefit of also working on macOS and Windows.

[functional tests]: PythonTests.md
[Firefox Nightly]: https://nightly.mozilla.org/

### Android

Prerequisites:

* You have [built Fennec](https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Simple_Firefox_for_Android_build).

* You can run an Android [emulator](https://wiki.mozilla.org/Mobile/Fennec/Android/Testing#Running_tests_on_the_Android_emulator),
    which means you have the AVD you need.

When running tests on Fennec, you can have Marionette runner take care of
starting Fennec and an emulator, as shown below.

```shell
% ./mach marionette-test --emulator --app fennec
    --avd-home /path/to/.mozbuild/android-device/avd
    --emulator-binary /path/to/.mozbuild/android-sdk/emulator/emulator
    --avd=mozemulator-x86
```

For Fennec tests, if the appropriate `emulator` command is in your `PATH`, you may omit the `--emulator-binary` argument.  See `./mach marionette-test -h`
for additional options.

Alternately, you can start an emulator yourself and have the Marionette runner
start Fennec for you:

```shell
% ./mach marionette-test --emulator --app='fennec' --address=127.0.0.1:2828
```

To connect to an already-running Fennec in an emulator or on a device,
you will need to have it started with the `-marionette` command line argument,
or by setting the environment variable `MOZ_MARIONETTE=1` for the process.

Make sure port 2828 is forwarded:

```shell
% adb forward tcp:2828 tcp:2828
```

If Fennec is already started:

```shell
% ./mach marionette-test --app='fennec' --address=127.0.0.1:2828
```

If Fennec is not already started on the emulator/device, add the `--emulator`
option. Marionette Test Runner will take care of forwarding the port and
starting Fennec with the correct prefs. (You may need to run
`adb forward --remove-all` to allow the runner to start.)

```shell
% ./mach marionette-test --emulator --app='fennec' --address=127.0.0.1:2828 --startup-timeout=300
```

If you need to troubleshoot the Marionette connection, the most basic check is
to start Fennec with `-marionette` or the environment variable `MOZ_MARIONETTE=1`,
make sure that the port 2828 is forwarded, and then see if you get any response from
Marionette when you connect manually:

```shell
% telnet 127.0.0.1:2828
```

You should see output like `{"applicationType":"gecko","marionetteProtocol":3}`

[geckodriver]: /testing/geckodriver/index.rst

## WPT functional tests

Marionette is also indirectly tested through [geckodriver] with WPT
(`Wd` on Treeherder).  To run them:

```shell
% ./mach wpt testing/web-platform/tests/webdriver
```

WPT tests conformance to the [WebDriver] standard and uses
[geckodriver].  Together with the Marionette remote protocol in
Gecko, they make up Mozilla’s WebDriver implementation.

This command supports a `--webdriver-arg='-vv'` argument that
enables more detailed logging, as well as `--jsdebugger` for opening
the Browser Toolbox.

A particularly useful trick is to combine this with the headless
mode for Firefox:

```shell
% ./mach wpt --webdriver-arg='-vv' --headless testing/web-platform/tests/webdriver
```

[WebDriver]: https://w3c.github.io/webdriver/

## Harness tests

The Marionette harness Python package has a set of mock-styled unit
tests that uses the [pytest] framework.  The following command will
run all tests:

```shell
% ./mach python-test testing/marionette
```

To run a specific test specify the full path to the module:

```shell
% ./mach python-test testing/marionette/harness/marionette_harness/tests/harness_unit/test_serve.py
```

[pytest]: https://docs.pytest.org/en/latest/

## One-click loaners

Additionally, for debugging hard-to-reproduce test failures in CI,
one-click loaners from [Taskcluster](Taskcluster.md) can be particularly useful.

## Out-of-tree testing

All the above examples show tests running _in-tree_, with a local
checkout of _central_ and a local build of Firefox.  It is also
possibly to run the Marionette tests _without_ a local build and
with a downloaded test archive from [Taskcluster](Taskcluster.md)

If you want to run tests from a downloaded test archive, you will
need to download the `target.common.tests.tar.gz` artifact attached to
Treeherder [build jobs] `B` for your system.  Extract the archive
and set up the Python Marionette client and harness by executing
the following command in a virtual environment:

```shell
% pip install -r config/marionette_requirements.txt
```

The tests can then be found under
_marionette/tests/testing/marionette/harness/marionette_harness/tests_
and can be executed with the command `marionette`.  It supports
the same options as described above for `mach`.

[build jobs]: https://treeherder.mozilla.org/#/jobs?repo=mozilla-central&filter-searchStr=build
