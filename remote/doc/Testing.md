Testing
=======

The Remote Protocol has unit- and functional tests located under different folders:

	- CDP: `remote/cdp`
	- Marionette: `remote/marionette`.
	- Shared Modules: `remote/shared/`
  - WebDriver BiDi: `remote/webdriver-bidi`

You may want to run all the tests under a particular subfolder locally like this:

	% ./mach test remote


Unit tests
----------

Because tests are run in parallel and [xpcshell] itself is quite
chatty, it can sometimes be useful to run the tests in sequence:

	% ./mach xpcshell-test --sequential remote/cdp/test/unit/test_DomainCache.js

The unit tests will appear as part of the `X` (for _xpcshell_) jobs
on Treeherder.

[xpcshell]: /testing/xpcshell/index.rst


Browser Chrome Mochitests
-------------------------

We also have a set of functional browser-chrome mochitests located
under several components, ie. _remote/shared/messagehandler/test/browser_:

	% ./mach mochitest remote/shared/messagehandler/test/browser/browser_*

The functional tests will appear under the `M` (for _mochitest_)
category in the `remote` jobs on Treeherder.

As the functional tests will sporadically pop up new Firefox
application windows, a helpful tip is to run them in headless
mode:

	% ./mach mochitest --headless remote/shared/messagehandler/test/browser

The `--headless` flag is equivalent to setting the `MOZ_HEADLESS`
environment variable.  You can additionally use `MOZ_HEADLESS_WIDTH`
and `MOZ_HEADLESS_HEIGHT` to control the dimensions of the virtual
display.

The `add_task()` function used for writing asynchronous tests is
replaced to provide some additional test setup and teardown useful
for writing tests against the Remote Agent and the targets.

There are also specific browser-chrome tests for CDP.

Before such a task is run, the `nsIRemoteAgent` listener is started
and a [CDP client] is connected.  You will use this CDP client for
interacting with the agent just as any other CDP client would.

Also target discovery is getting enabled, which means that targetCreated,
targetDestroyed, and targetInfoChanged events will be received by the client.

The task function you provide in your test will be called with the
three arguments `client`, `CDP`, and `tab`:

  - `client` is the connection to the `nsIRemoteAgent` listener,
    and it provides the a client CDP API

  - `CDP` is the CDP client class

  - `tab` is a fresh tab opened for each new test, and is automatically
    removed after the test has run

This is what it looks like all put together:

	add_task(async function testName({client, CDP, tab}) {
	  // test tab is implicitly created for us
	  info("Current URL: " + tab.linkedBrowser.currentURI.spec);

	  // manually connect to a specific target
	  const { mainProcessTarget } = RemoteAgent.cdp.targetList;
	  const target = mainProcessTarget.wsDebuggerURL;
	  const client = await CDP({ target });

	  // retrieve the Browser domain, and call getVersion() on it
	  const { Browser } = client;
	  const version = await Browser.getVersion();

           await client.close();

	  // tab is implicitly removed
	});

You can control the tab creation behaviour with the `createTab`
option to `add_task(taskFunction, options)`:

	add_task(async function testName({client}) {
	  // tab is not implicitly created
	}, { createTab: false });

If you want to write an asynchronous test _without_ this implicit
setup you may instead use `add_plain_task()`, which works exactly like the
original `add_task()`.

[CDP client]: https://github.com/cyrus-and/chrome-remote-interface


Puppeteer tests
---------------

In addition to our own Firefox-specific tests, we run the upstream
[Puppeteer test suite] against our implementation to [track progress]
towards achieving full [Puppeteer support] in Firefox. The tests are written
in the behaviour-driven testing framework [Mocha].

Check the upstream [Puppeteer test suite] documentation for instructions on
how to skip tests, run only one test or a subsuite of tests.

Puppeteer tests are vendored under _remote/test/puppeteer/_ and are
run locally like this:

	% ./mach puppeteer-test

You can also run them against Chrome as:

	% ./mach puppeteer-test --product=chrome --subset

`--subset` disables a check for missing or skipped tests in our log parsing.
This check is typically not relevant when running against Chrome.

Test expectation metadata is collected in _remote/test/puppeteer-expected.json_
via log parsing and a custom Mocha reporter under
_remote/test/puppeteer/json-mocha-reporter.js_


Testing on Try
--------------

To schedule all the Remote Protocol tests on try, you can use the
`remote-protocol` [try preset]:

	mach try --preset remote-protocol

But you can also schedule tests by selecting relevant jobs yourself:

	mach try fuzzy

[Puppeteer support]: https://bugzilla.mozilla.org/show_bug.cgi?id=puppeteer
[Mocha]: https://mochajs.org/
[Puppeteer test suite]: https://github.com/puppeteer/puppeteer/blob/master/test/README.md
[track progress]: https://puppeteer.github.io/ispuppeteerfirefoxready/
[try preset]: /tools/try/presets
