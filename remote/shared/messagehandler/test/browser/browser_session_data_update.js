/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE = "https://example.com/document-builder.sjs?html=tab";

const { assertUpdate, createSessionDataUpdate, getUpdates } =
  SessionDataUpdateHelpers;

// Test various session data update scenarios against a single browsing context.
add_task(async function test_session_data_update() {
  const tab1 = gBrowser.selectedTab;
  await loadURL(tab1.linkedBrowser, TEST_PAGE);
  const browsingContext1 = tab1.linkedBrowser.browsingContext;

  const root = createRootMessageHandler("session-data-update");

  info("Add a new session data item, expect one return value");
  await root.updateSessionData([
    createSessionDataUpdate(["text-1"], "add", "category1"),
  ]);
  let processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 1);
  assertUpdate(processedUpdates.at(-1), ["text-1"], "category1");

  info("Add two session data items, expect one return value with both items");
  await root.updateSessionData([
    createSessionDataUpdate(["text-2"], "add", "category1"),
    createSessionDataUpdate(["text-3"], "add", "category1"),
  ]);
  processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 2);
  assertUpdate(
    processedUpdates.at(-1),
    ["text-1", "text-2", "text-3"],
    "category1"
  );

  info("Try to add an existing data item, expect no update broadcast");
  await root.updateSessionData([
    createSessionDataUpdate(["text-1"], "add", "category1"),
  ]);
  processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 2);

  info("Add an existing and a new item");
  await root.updateSessionData([
    createSessionDataUpdate(["text-2", "text-4"], "add", "category1"),
  ]);
  processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 3);
  assertUpdate(
    processedUpdates.at(-1),
    ["text-1", "text-2", "text-3", "text-4"],
    "category1"
  );

  info("Remove an item, expect only the new item to return");
  await root.updateSessionData([
    createSessionDataUpdate(["text-3"], "remove", "category1"),
  ]);
  processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 4);
  assertUpdate(
    processedUpdates.at(-1),
    ["text-1", "text-2", "text-4"],
    "category1"
  );

  info("Remove a unknown item, expect no return value");
  await root.updateSessionData([
    createSessionDataUpdate(["text-unknown"], "remove", "category1"),
  ]);
  processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 4);
  assertUpdate(
    processedUpdates.at(-1),
    ["text-1", "text-2", "text-4"],
    "category1"
  );

  info("Remove an existing and a unknown item");
  await root.updateSessionData([
    createSessionDataUpdate(["text-2"], "remove", "category1"),
    createSessionDataUpdate(["text-unknown"], "remove", "category1"),
  ]);
  processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 5);
  assertUpdate(processedUpdates.at(-1), ["text-1", "text-4"], "category1");

  info("Add and remove at once");
  await root.updateSessionData([
    createSessionDataUpdate(["text-5"], "add", "category1"),
    createSessionDataUpdate(["text-4"], "remove", "category1"),
  ]);
  processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 6);
  assertUpdate(processedUpdates.at(-1), ["text-1", "text-5"], "category1");

  info("Adding and removing an item does not trigger any update");
  await root.updateSessionData([
    createSessionDataUpdate(["text-6"], "add", "category1"),
    createSessionDataUpdate(["text-6"], "remove", "category1"),
  ]);
  processedUpdates = await getUpdates(root, browsingContext1);
  // TODO: We could detect transactions which can't have any impact and fully
  // ignore them. See Bug 1810807.
  todo_is(processedUpdates.length, 6);
  assertUpdate(processedUpdates.at(-1), ["text-1", "text-5"], "category1");

  root.destroy();
});
