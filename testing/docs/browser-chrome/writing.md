# Writing new browser mochitests

After [creating a new empty test file](index.md#adding-new-tests), you will
have an empty `add_task` into which you can write your test.

## General guidance

The test can use `ok`, `is`, `isnot`, as well as all the regular
[CommonJS standard assertions](http://wiki.commonjs.org/wiki/Unit_Testing/1.1),
to make test assertions.

The test can use `info` to log strings into the test output.
``console.log`` will work for local runs of individual tests, but aren't
normally used for checked-in tests.

The test will run in a separate scope inside the browser window.
`gBrowser`, `gURLBar`, `document`, and various other globals are thus
accessible just as they are for non-test code in the same window. However,
variables declared in the test file will not outlive the test.

## Test architecture

It is the responsibility of individual tests to leave the browser as they
found it. If the test changes prefs, opens tabs, customizes the UI, or makes
other changes, it should revert those when it is done.

To help do this, a number of useful primitives are available:

- `add_setup` allows you to add setup tasks that run before any `add_task` tasks.
- `SpecialPowers.pushPrefEnv` ([see below](#changing-preferences)) allows you to set prefs that will be automatically
  reverted when the test file has finished running.
- [`BrowserTestUtils.withNewTab`](browsertestutils.rst#BrowserTestUtils.withNewTab), allows you to easily run async code
  talking to a tab that you open and close it when done.
- `registerCleanupFunction` takes an async callback function that you can use
  to do any other cleanup your test might need.

## Common operations

### Opening new tabs and new windows, and closing them

Should be done using the relevant methods in `BrowserTestUtils` (which
is available without any additional work).

Typical would be something like:

```js
add_task(async function() {
  await BrowserTestUtils.withNewTab("https://example.com/mypage", async (browser) {
    // `browser` will have finished loading the passed URL when this code runs.
    // Do stuff with `browser` in here. When the async function exits,
    // the test framework will clean up the tab.
  });
});
```

### Executing code in the content process associated with a tab or its subframes

Should be done using `SpecialPowers.spawn`:

```js
let result = await SpecialPowers.spawn(browser, [42, 100], async (val, val2) => {
  // Replaces the document body with '42':
  content.document.body.textContent = val;
  // Optionally, return a result. Has to be serializable to make it back to
  // the parent process (so DOM nodes or similar won't work!).
  return Promise.resolve(val2 * 2);
});
```

You can pass a BrowsingContext reference instead of `browser` to directly execute
code in subframes.

Inside the function argument passed to `SpecialPowers.spawn`, `content` refers
to the `window` of the web content in that browser/BrowsingContext.

For some operations, like mouse clicks, convenience helpers are available on
`BrowserTestUtils`:

```js
await BrowserTestUtils.synthesizeMouseAtCenter("#my.css.selector", {accelKey: true}, browser);
```

### Changing preferences

Use `SpecialPowers.pushPrefEnv`:

```js
await SpecialPowers.pushPrefEnv({
  set: [["accessibility.tabfocus", 7]]
});
```
This example sets the pref allowing buttons and other controls to receive tab focus -
this is the default on Windows and Linux but not on macOS, so it can be necessary in
order for your test to pass reliably on macOS if it uses keyboard focus.

### Wait for an observer service notification topic or DOM event

Use the utilities for this on [`TestUtils`](../testutils.rst#TestUtils.topicObserved):

```js
await TestUtils.topicObserved("sync-pane-loaded");
```

and [`BrowserTestUtils`](browsertestutils.rst#BrowserTestUtils.waitForEvent), respectively:

```js
await BrowserTestUtils.waitForEvent(domElement, "click");
```

### Wait for some DOM to update.

Use [`BrowserTestUtils.waitForMutationCondition`](browsertestutils.rst#BrowserTestUtils.waitForMutationCondition).
Do **not** use `waitForCondition`, which uses a timeout loop and often
leads to intermittent failures.

### Mocking code not under test

The [`Sinon`](https://sinonjs.org/) mocking framework is available. You can import it
using something like:

```js
const { sinon } = ChromeUtils.importESModule("resource://testing-common/Sinon.sys.mjs");
```

More details on how to do mocking are available on the Sinon website.

## Additional files

You can use extra files (e.g. webpages to load) by adding them to a `support-files`
property using the `browser.toml` file:

```toml
["browser_foo.js"]
support-files = [
  "bar.html",
  "baz.js",
]
```

## Reusing code across tests

For operations that are common to a specific set of tests, you can use the `head.js`
file to share JS code.

Where code is needed across various directories of tests, you should consider if it's
common enough to warrant being in `BrowserTestUtils.sys.mjs`, or if not, setting up
a separate `jsm` module containing your test helpers. You can add these to
`TESTING_JS_MODULES` in `moz.build` to avoid packaging them with Firefox. They
will be available in `resource://testing-common/` to all tests.
