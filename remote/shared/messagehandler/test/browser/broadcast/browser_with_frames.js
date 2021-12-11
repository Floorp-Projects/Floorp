/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_broadcasting_with_frames() {
  info("Navigate the initial tab to the test URL");
  const tab = gBrowser.selectedTab;
  await loadURL(tab.linkedBrowser, createTestMarkupWithFrames());

  const contexts = tab.linkedBrowser.browsingContext.getAllBrowsingContextsInSubtree();
  is(contexts.length, 4, "Test tab has 3 children contexts (4 in total)");

  const rootMessageHandler = createRootMessageHandler(
    "session-id-broadcasting_with_frames"
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
  is(broadcastValue.length, 4, "The broadcast returned 4 values as expected");

  for (const context of contexts) {
    ok(
      broadcastValue.includes("broadcast-" + context.id),
      "The broadcast contains the value for browsing context " + context.id
    );
  }

  rootMessageHandler.destroy();
});
