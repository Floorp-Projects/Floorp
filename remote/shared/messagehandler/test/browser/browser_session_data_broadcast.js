/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RootMessageHandler } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.jsm"
);

const TEST_PAGE = "https://example.com/document-builder.sjs?html=tab";

add_task(async function test_session_data_broadcast() {
  const tab1 = BrowserTestUtils.addTab(gBrowser, TEST_PAGE);
  await BrowserTestUtils.browserLoaded(tab1.linkedBrowser);
  const browsingContext1 = tab1.linkedBrowser.browsingContext;

  const root = createRootMessageHandler("session-id-event");

  info("Add a new session data item, expect one return value");
  const value1 = await sendAddSessionData(root, ["text-1"]);
  is(value1.length, 1);
  is(value1[0].newData, "text-1");
  is(value1[0].sessionData, "text-1");
  is(value1[0].contextId, browsingContext1.id);

  info("Add two session data items, expect one return value with both items");
  const value2 = await sendAddSessionData(root, ["text-2", "text-3"]);
  is(value2.length, 1);
  is(value2[0].newData, "text-2, text-3");
  is(value2[0].sessionData, "text-1, text-2, text-3");
  is(value2[0].contextId, browsingContext1.id);

  info("Try to add an existing data item, expect no return value");
  const value3 = await sendAddSessionData(root, ["text-1"]);
  is(value3.length, 0);

  info("Add an existing and a new item, expect only the new item to return");
  const value4 = await sendAddSessionData(root, ["text-1", "text-4"]);
  is(value4.length, 1);
  is(value4[0].newData, "text-4");
  is(value4[0].sessionData, "text-1, text-2, text-3, text-4");
  is(value4[0].contextId, browsingContext1.id);

  info("Open a new tab on the same test URL");
  const tab2 = await addTab(TEST_PAGE);
  const browsingContext2 = tab2.linkedBrowser.browsingContext;

  info("Add a new session data item, check both contexts have the same data.");
  const value5 = await sendAddSessionData(root, ["text-5"]);
  is(value5.length, 2);
  is(value5[0].newData, "text-5");
  is(value5[0].sessionData, "text-1, text-2, text-3, text-4, text-5");
  is(value5[0].contextId, browsingContext1.id);
  is(value5[1].newData, "text-5");
  // "text-1, text-2, text-3, text-4" were added as initial session data and
  // "text-5" was added afterwards.
  is(value5[1].sessionData, "text-1, text-2, text-3, text-4, text-5");
  is(value5[1].contextId, browsingContext2.id);

  root.destroy();

  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);
});

function sendAddSessionData(rootMessageHandler, values) {
  return rootMessageHandler.handleCommand({
    moduleName: "command",
    commandName: "testAddSessionData",
    destination: {
      type: RootMessageHandler.type,
    },
    params: {
      values,
    },
  });
}
