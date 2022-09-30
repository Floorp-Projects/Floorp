/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_PAGE =
  "https://example.com/document-builder.sjs?html=<meta charset=utf8></meta>";

add_task(async function test_cached_javascript_errors() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, TEST_PAGE);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  await createScriptNode(`(() => {throw "error1"})()`);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    const innerWindowId = content.windowGlobalChild.innerWindowId;
    const { ConsoleListener } = ChromeUtils.importESModule(
      "chrome://remote/content/shared/listeners/ConsoleListener.sys.mjs"
    );

    const listener = new ConsoleListener(innerWindowId);

    const errors = [];
    // Do not push the whole error object in the array. It would create a strong
    // reference preventing from reproducing GC-related bugs.
    const onError = (evtName, error) => errors.push(error.message);
    listener.on("error", onError);

    const waitForMessage = listener.once("error");
    listener.startListening();
    const error = await waitForMessage;
    is(error.message, "uncaught exception: error1");
    is(errors.length, 1);

    listener.stopListening();
    content.backup = { listener, errors, onError };
  });

  // Force a GC to check that old cached messages which have been garbage
  // collected are not re-displayed.
  await doGC();
  await createScriptNode(`(() => {throw "error2"})()`);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    const { listener, errors, onError } = content.backup;

    const waitForMessage = listener.once("error");
    listener.startListening();
    const { message } = await waitForMessage;
    is(message, "uncaught exception: error2");
    is(errors.length, 2);

    listener.off("error", onError);
    listener.destroy();
  });

  info("Reload the current tab and check only new messages are emitted");
  await BrowserTestUtils.reloadTab(gBrowser.selectedTab);

  await createScriptNode(`(() => {throw "error3"})()`);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    const innerWindowId = content.windowGlobalChild.innerWindowId;
    const { ConsoleListener } = ChromeUtils.importESModule(
      "chrome://remote/content/shared/listeners/ConsoleListener.sys.mjs"
    );

    const listener = new ConsoleListener(innerWindowId);

    const errors = [];
    const onError = (evtName, error) => errors.push(error.message);
    listener.on("error", onError);

    const waitForMessage = listener.once("error");
    listener.startListening();
    const error = await waitForMessage;
    is(error.message, "uncaught exception: error3");
    is(errors.length, 1);

    listener.off("error", onError);
    listener.destroy();
  });

  gBrowser.removeTab(gBrowser.selectedTab);
});
