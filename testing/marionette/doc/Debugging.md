Debugging
=========

Redirecting the Gecko output
----------------------------

The most common way to debug Marionette, as well as chrome code in
general, is to use `dump()` to print a string to stdout.  In Firefox,
this log output normally ends up in the gecko.log file in your current
working directory.  With Fennec it can be inspected using `adb logcat`.

`mach marionette test` takes a `--gecko-log` option which lets
you redirect this output stream.  This is convenient if you want to
“merge” the test harness output with the stdout from the browser.
Per Unix conventions you can use `-` (dash) to have Firefox write
its log to stdout instead of file:

	% ./mach marionette test --gecko-log -

This debugging technique can be particularly effective when combined
with using [pdb] in the Python client or the JS remote debugger
that is described below.

[pdb]: https://docs.python.org/2/library/pdb.html


JavaScript debugger
-------------------

You can attach a JavaScript debugger to the Marionette server
through the [Browser Toolbox].  This enables you to introspect and
set breakpoints in Gecko chrome code, which is a far more powerful
debuggin technique than using `dump()` or `console.log()`.

The browser toolbox can be opened automatically when running Mn
tests by passing `--jsdebugger` to the mach command:

	% ./mach marionette test --jsdebugger

It will prompt you when to start the test run to allow you time to
set your breakpoints.  It will also prompt you between each test.

For reference, below is the list of preferences that enables the
chrome debugger for Marionette.  These are all set implicitly when
`--jsdebugger` is passed to mach.  In non-official builds, which
are the default when built using `./mach build`, you will find that
the chrome debugger won’t prompt for connection and will allow
remote connections.

  * `devtools.debugger.prompt-connection` → true

    Controls the remote connection prompt.  Note that this will
    automatically expose your Firefox instance to the network.

  * `devtools.chrome.enabled` → true

    Enables debugging of chrome code.

  * `devtools.debugger.remote-enabled` → true

    Allows a remote debugger to connect, which is necessary for
    debugging chrome code.

[Browser Toolbox]: https://developer.mozilla.org/en-US/docs/Tools/Browser_Toolbox
