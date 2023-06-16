/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const COM_TEST_PAGE = "https://example.com/document-builder.sjs?html=COM";
const FRAME_TEST_PAGE = createTestMarkupWithFrames();

add_task(async function test_broadcasting_filter_top_browsing_context() {
  info("Navigate the initial tab to the COM test URL");
  const tab1 = gBrowser.selectedTab;
  await loadURL(tab1.linkedBrowser, COM_TEST_PAGE);
  const browsingContext1 = tab1.linkedBrowser.browsingContext;

  info("Open a second tab on the frame test URL");
  const tab2 = await addTab(FRAME_TEST_PAGE);
  const browsingContext2 = tab2.linkedBrowser.browsingContext;

  const contextsForTab2 =
    tab2.linkedBrowser.browsingContext.getAllBrowsingContextsInSubtree();
  is(
    contextsForTab2.length,
    4,
    "Frame test tab has 3 children contexts (4 in total)"
  );

  const rootMessageHandler = createRootMessageHandler(
    "session-id-broadcasting_filter_top_browsing_context"
  );

  const broadcastValue1 = await sendBroadcastForTopBrowsingContext(
    browsingContext1,
    rootMessageHandler
  );

  ok(
    Array.isArray(broadcastValue1),
    "The broadcast returned an array of values"
  );

  is(broadcastValue1.length, 1, "The broadcast returned one value as expected");

  ok(
    broadcastValue1.includes("broadcast-" + browsingContext1.id),
    "The broadcast returned the expected value from tab1"
  );

  const broadcastValue2 = await sendBroadcastForTopBrowsingContext(
    browsingContext2,
    rootMessageHandler
  );

  ok(
    Array.isArray(broadcastValue2),
    "The broadcast returned an array of values"
  );

  is(broadcastValue2.length, 4, "The broadcast returned 4 values as expected");

  for (const context of contextsForTab2) {
    ok(
      broadcastValue2.includes("broadcast-" + context.id),
      "The broadcast contains the value for browsing context " + context.id
    );
  }

  rootMessageHandler.destroy();
});

function sendBroadcastForTopBrowsingContext(
  topBrowsingContext,
  rootMessageHandler
) {
  return sendTestBroadcastCommand(
    "commandwindowglobalonly",
    "testBroadcast",
    {},
    {
      type: ContextDescriptorType.TopBrowsingContext,
      id: topBrowsingContext.browserId,
    },
    rootMessageHandler
  );
}
