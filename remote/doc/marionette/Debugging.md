# Debugging

## Redirecting the Gecko output

The most common way to debug Marionette, as well as chrome code in
general, is to use `dump()` to print a string to stdout.  In Firefox,
this log output normally ends up in the gecko.log file in your current
working directory.  With Fennec it can be inspected using `adb logcat`.

`mach marionette-test` takes a `--gecko-log` option which lets
you redirect this output stream.  This is convenient if you want to
“merge” the test harness output with the stdout from the browser.
Per Unix conventions you can use `-` (dash) to have Firefox write
its log to stdout instead of file:

```shell
% ./mach marionette-test --gecko-log -
```

It is common to use this in conjunction with an option to increase
the Marionette log level:

```shell
% ./mach test --gecko-log - -vv TEST
```

A single `-v` enables debug logging, and a double `-vv` enables
trace logging.

This debugging technique can be particularly effective when combined
with using [pdb] in the Python client or the JS remote debugger
that is described below.

[pdb]: https://docs.python.org/3/library/pdb.html

## JavaScript debugger

You can attach the [Browser Toolbox] JavaScript debugger to the
Marionette server using the `--jsdebugger` flag.  This enables you
to introspect and set breakpoints in Gecko chrome code, which is a
more powerful debugging technique than using `dump()` or `console.log()`.

To automatically open the JS debugger for `Mn` tests:

```shell
% ./mach marionette-test --jsdebugger
```

It will prompt you when to start to allow you time to set your
breakpoints.  It will also prompt you between each test.

You can also use the `debugger;` statement anywhere in chrome code
to add a breakpoint.  In this example, a breakpoint will be added
whenever the `WebDriver:GetPageSource` command is called:

```javascript
    GeckoDriver.prototype.getPageSource = async function() {
      debugger;
      …
    }
```

To be prompted at the start of the test run or between tests,
you can set the `marionette.debugging.clicktostart` preference to
`true` this way:

```shell
% ./mach marionette-test --setpref='marionette.debugging.clicktostart=true' --jsdebugger
```

For reference, below is the list of preferences that enables the
chrome debugger for Marionette.  These are all set implicitly when
`--jsdebugger` is passed to mach.  In non-official builds, which
are the default when built using `./mach build`, you will find that
the chrome debugger won’t prompt for connection and will allow
remote connections.

* `devtools.browsertoolbox.panel` -> `jsdebugger`

  Selects the Debugger panel by default.

* `devtools.chrome.enabled` → true

  Enables debugging of chrome code.

* `devtools.debugger.prompt-connection` → false

  Controls the remote connection prompt.  Note that this will
  automatically expose your Firefox instance to localhost.

* `devtools.debugger.remote-enabled` → true

  Allows a remote debugger to connect, which is necessary for
  debugging chrome code.

[Browser Toolbox]: /devtools-user/browser_toolbox/index.rst
