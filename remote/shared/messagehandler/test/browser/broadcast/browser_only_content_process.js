/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_broadcasting_only_content_process() {
  info("Navigate the initial tab to the test URL");
  const tab1 = gBrowser.selectedTab;
  await loadURL(
    tab1.linkedBrowser,
    "http://example.com/document-builder.sjs?html=tab"
  );
  const browsingContext1 = tab1.linkedBrowser.browsingContext;

  info("Open a new tab on a parent process about: page");
  await addTab("about:robots");

  info("Open a new tab on a XUL page");
  await addTab(
    getRootDirectory(gTestPath) + "doc_messagehandler_broadcasting_xul.xhtml"
  );

  const rootMessageHandler = createRootMessageHandler(
    "session-id-broadcasting_only_content_process"
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

  is(broadcastValue.length, 1, "The broadcast returned 1 value as expected");
  ok(
    broadcastValue.includes("broadcast-" + browsingContext1.id),
    "The broadcast returned the expected value from tab1"
  );

  rootMessageHandler.destroy();
});
