/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE = "https://example.com/document-builder.sjs?html=tab";

const { assertUpdate, createSessionDataUpdate, getUpdates } =
  SessionDataUpdateHelpers;

// Test session data update scenarios involving different session data item
// categories.
add_task(async function test_session_data_update_categories() {
  const tab1 = gBrowser.selectedTab;
  await loadURL(tab1.linkedBrowser, TEST_PAGE);
  const browsingContext1 = tab1.linkedBrowser.browsingContext;

  const root = createRootMessageHandler("session-data-update-categories");
  await root.updateSessionData([
    createSessionDataUpdate(["value1-1"], "add", "category1"),
    createSessionDataUpdate(["value1-2"], "add", "category1"),
  ]);

  let processedUpdates = await getUpdates(root, browsingContext1);

  is(processedUpdates.length, 1);
  assertUpdate(processedUpdates.at(-1), ["value1-1", "value1-2"], "category1");

  info("Adding a new item in category1 broadcasts all category1 items");
  await root.updateSessionData([
    createSessionDataUpdate(["value1-3"], "add", "category1"),
  ]);
  processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 2);
  assertUpdate(
    processedUpdates.at(-1),
    ["value1-1", "value1-2", "value1-3"],
    "category1"
  );

  info("Removing a new item in category1 broadcasts all category1 items");
  await root.updateSessionData([
    createSessionDataUpdate(["value1-1"], "remove", "category1"),
  ]);
  processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 3);
  assertUpdate(processedUpdates.at(-1), ["value1-2", "value1-3"], "category1");

  info("Adding a new category does not broadcast category1 items");
  await root.updateSessionData([
    createSessionDataUpdate(["value2-1"], "add", "category2"),
  ]);
  processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 4);
  assertUpdate(processedUpdates.at(-1), ["value2-1"], "category2");

  info("Adding an item in 2 categories triggers an update for each category");
  await root.updateSessionData([
    createSessionDataUpdate(["value1-4"], "add", "category1"),
    createSessionDataUpdate(["value2-2"], "add", "category2"),
  ]);
  processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 6);
  assertUpdate(
    processedUpdates.at(-2),
    ["value1-2", "value1-3", "value1-4"],
    "category1"
  );
  assertUpdate(processedUpdates.at(-1), ["value2-1", "value2-2"], "category2");

  info("Removing an item in 2 categories triggers an update for each category");
  await root.updateSessionData([
    createSessionDataUpdate(["value1-4"], "remove", "category1"),
    createSessionDataUpdate(["value2-2"], "remove", "category2"),
  ]);
  processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 8);
  assertUpdate(processedUpdates.at(-2), ["value1-2", "value1-3"], "category1");
  assertUpdate(processedUpdates.at(-1), ["value2-1"], "category2");

  info("Opening a new tab triggers an update for each category");
  const tab2 = await addTab(TEST_PAGE);
  const browsingContext2 = tab2.linkedBrowser.browsingContext;
  processedUpdates = await getUpdates(root, browsingContext2);
  is(processedUpdates.length, 2);
  assertUpdate(processedUpdates.at(-2), ["value1-2", "value1-3"], "category1");
  assertUpdate(processedUpdates.at(-1), ["value2-1"], "category2");

  root.destroy();
  gBrowser.removeTab(tab2);
});
