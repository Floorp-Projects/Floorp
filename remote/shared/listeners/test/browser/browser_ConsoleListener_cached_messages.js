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
    const { ConsoleListener } = ChromeUtils.import(
      "chrome://remote/content/shared/listeners/ConsoleListener.jsm"
    );

    const listener = new ConsoleListener(innerWindowId);

    const errors = [];
    const onError = (evtName, error) => errors.push(error);
    listener.on("error", onError);

    const waitForMessage = listener.once("error");
    listener.startListening();
    const error = await waitForMessage;
    is(error.message, "uncaught exception: error1");
    is(errors.length, 1);

    listener.stopListening();
    content.backup = { listener, errors, onError };
  });

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
  const finished = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  gBrowser.reloadTab(gBrowser.selectedTab);
  await finished;

  await createScriptNode(`(() => {throw "error3"})()`);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    const innerWindowId = content.windowGlobalChild.innerWindowId;
    const { ConsoleListener } = ChromeUtils.import(
      "chrome://remote/content/shared/listeners/ConsoleListener.jsm"
    );

    const listener = new ConsoleListener(innerWindowId);

    const errors = [];
    const onError = (evtName, error) => errors.push(error);
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

/**
 * Execute the provided script content by generating a dynamic script tag and
 * inserting it in the page for the current selected browser.
 *
 * @param {string} script
 *     The script to execute.
 * @return {Promise}
 *     A promise that resolves when the script node was added and removed from
 *     the content page.
 */
function createScriptNode(script) {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [script], function(
    _script
  ) {
    var script = content.document.createElement("script");
    script.append(content.document.createTextNode(_script));
    content.document.body.append(script);
  });
}
