/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE = "http://example.com/document-builder.sjs?html=tab";

add_task(async function test_broadcasting_two_windows_command() {
  const window1Browser = gBrowser.selectedTab.linkedBrowser;
  await loadURL(window1Browser, TEST_PAGE);
  const browsingContext1 = window1Browser.browsingContext;

  const window2 = await BrowserTestUtils.openNewBrowserWindow();
  registerCleanupFunction(() => BrowserTestUtils.closeWindow(window2));

  const window2Browser = window2.gBrowser.selectedBrowser;
  await loadURL(window2Browser, TEST_PAGE);
  const browsingContext2 = window2Browser.browsingContext;

  const rootMessageHandler = createRootMessageHandler(
    "session-id-broadcasting_two_windows_command"
  );
  const broadcastValue = await sendTestBroadcastCommand(
    "commandwindowglobalonly",
    "testBroadcast",
    {},
    rootMessageHandler
  );

  ok(
    Array.isArray(broadcastValue),
    "The broadcast returned an array of values"
  );
  is(broadcastValue.length, 2, "The broadcast returned 2 values as expected");

  ok(
    broadcastValue.includes("broadcast-" + browsingContext1.id),
    "The broadcast returned the expected value from tab1"
  );
  ok(
    broadcastValue.includes("broadcast-" + browsingContext2.id),
    "The broadcast returned the expected value from tab2"
  );

  rootMessageHandler.destroy();
});
