/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(setupMailHandler);

add_task(async function test_open_without_user_interaction() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.disable_open_during_load", true],
      ["dom.block_external_protocol_in_iframes", true],
      ["dom.delay.block_external_protocol_in_iframes.enabled", false],
    ],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  let dialogWindowPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    true
  );

  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    TEST_PATH + "file_external_protocol_iframe.html"
  );

  let dialog = await dialogWindowPromise;
  ok(dialog, "Should show the dialog even without user interaction");

  let dialogClosedPromise = waitForProtocolAppChooserDialog(
    tab.linkedBrowser,
    false
  );

  // Adding another iframe without user interaction should be blocked.
  let blockedWarning = new Promise(resolve => {
    Services.console.registerListener(function onMessage(msg) {
      let { message, logLevel } = msg;
      if (logLevel != Ci.nsIConsoleMessage.warn) {
        return;
      }
      if (!message.includes("Iframe with external protocol was blocked")) {
        return;
      }
      Services.console.unregisterListener(onMessage);
      resolve();
    });
  });

  info("Adding another frame without user interaction");

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    let frame = content.document.createElement("iframe");
    frame.src = "mailto:foo@baz.com";
    content.document.body.appendChild(frame);
  });

  await blockedWarning;

  info("Removing tab to close the dialog.");
  gBrowser.removeTab(tab);
  await dialogClosedPromise;
});
