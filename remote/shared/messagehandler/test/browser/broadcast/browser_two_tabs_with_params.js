/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE = "http://example.com/document-builder.sjs?html=tab";

add_task(async function test_broadcasting_two_tabs_with_params_command() {
  info("Navigate the initial tab to the test URL");
  const tab1 = gBrowser.selectedTab;
  await loadURL(tab1.linkedBrowser, TEST_PAGE);
  const browsingContext1 = tab1.linkedBrowser.browsingContext;

  info("Open a new tab on the same test URL");
  const tab2 = await addTab(TEST_PAGE);
  const browsingContext2 = tab2.linkedBrowser.browsingContext;

  const rootMessageHandler = createRootMessageHandler(
    "session-id-broadcasting_two_tabs_command"
  );

  const broadcastValue = await sendTestBroadcastCommand(
    "commandwindowglobalonly",
    "testBroadcastWithParameter",
    {
      value: "some-value",
    },
    rootMessageHandler
  );
  ok(
    Array.isArray(broadcastValue),
    "The broadcast returned an array of values"
  );

  is(broadcastValue.length, 2, "The broadcast returned 2 values as expected");

  ok(
    broadcastValue.includes("broadcast-" + browsingContext1.id + "-some-value"),
    "The broadcast returned the expected value from tab1"
  );
  ok(
    broadcastValue.includes("broadcast-" + browsingContext2.id + "-some-value"),
    "The broadcast returned the expected value from tab2"
  );
  rootMessageHandler.destroy();
});
