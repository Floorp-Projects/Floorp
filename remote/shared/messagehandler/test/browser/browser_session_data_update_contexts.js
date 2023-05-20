/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE = "https://example.com/document-builder.sjs?html=tab";

const { assertUpdate, createSessionDataUpdate, getUpdates } =
  SessionDataUpdateHelpers;

// Test session data update scenarios involving 2 browsing contexts, and using
// the TopBrowsingContext ContextDescriptor type.
add_task(async function test_session_data_update_contexts() {
  const tab1 = gBrowser.selectedTab;
  await loadURL(tab1.linkedBrowser, TEST_PAGE);
  const browsingContext1 = tab1.linkedBrowser.browsingContext;

  const root = createRootMessageHandler("session-data-update-contexts");

  info("Add several items over 2 separate updates for all contexts");
  await root.updateSessionData([
    createSessionDataUpdate(["text-1"], "add", "category1"),
  ]);
  await root.updateSessionData([
    createSessionDataUpdate(["text-2"], "add", "category1"),
    createSessionDataUpdate(["text-3"], "add", "category1"),
  ]);

  info("Check we processed two distinct updates in browsingContext 1");
  let processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 2);
  assertUpdate(
    processedUpdates.at(-1),
    ["text-1", "text-2", "text-3"],
    "category1"
  );

  info("Open a new tab on the same test URL");
  const tab2 = await addTab(TEST_PAGE);
  const browsingContext2 = tab2.linkedBrowser.browsingContext;

  processedUpdates = await getUpdates(root, browsingContext2);
  is(processedUpdates.length, 1);
  assertUpdate(
    processedUpdates.at(-1),
    ["text-1", "text-2", "text-3"],
    "category1"
  );

  info("Add two items: one globally and one in a single context");
  await root.updateSessionData([
    createSessionDataUpdate(["text-4"], "add", "category1"),
    createSessionDataUpdate(["text-5"], "add", "category1", {
      type: ContextDescriptorType.TopBrowsingContext,
      id: browsingContext2.browserId,
    }),
  ]);

  processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 3);
  assertUpdate(
    processedUpdates.at(-1),
    ["text-1", "text-2", "text-3", "text-4"],
    "category1"
  );

  processedUpdates = await getUpdates(root, browsingContext2);
  is(processedUpdates.length, 2);
  assertUpdate(
    processedUpdates.at(-1),
    ["text-1", "text-2", "text-3", "text-4", "text-5"],
    "category1"
  );

  info("Remove two items: one globally and one in a single context");
  await root.updateSessionData([
    createSessionDataUpdate(["text-1"], "remove", "category1"),
    createSessionDataUpdate(["text-5"], "remove", "category1", {
      type: ContextDescriptorType.TopBrowsingContext,
      id: browsingContext2.browserId,
    }),
  ]);

  processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 4);
  assertUpdate(
    processedUpdates.at(-1),
    ["text-2", "text-3", "text-4"],
    "category1"
  );

  processedUpdates = await getUpdates(root, browsingContext2);
  is(processedUpdates.length, 3);
  assertUpdate(
    processedUpdates.at(-1),
    ["text-2", "text-3", "text-4"],
    "category1"
  );

  info(
    "Add session data item to all contexts and remove this event for one context (2 steps)"
  );

  info("First step: add an item to browsingContext1");
  await root.updateSessionData([
    createSessionDataUpdate(["text-6"], "add", "category1", {
      type: ContextDescriptorType.TopBrowsingContext,
      id: browsingContext1.browserId,
    }),
  ]);

  info(
    "Second step: remove the item from browsingContext1, and add it globally"
  );
  await root.updateSessionData([
    createSessionDataUpdate(["text-6"], "remove", "category1", {
      type: ContextDescriptorType.TopBrowsingContext,
      id: browsingContext1.browserId,
    }),
    createSessionDataUpdate(["text-6"], "add", "category1"),
  ]);

  processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 6);
  assertUpdate(
    processedUpdates.at(-1),
    ["text-2", "text-3", "text-4", "text-6"],
    "category1"
  );

  processedUpdates = await getUpdates(root, browsingContext2);
  is(processedUpdates.length, 4);
  assertUpdate(
    processedUpdates.at(-1),
    ["text-2", "text-3", "text-4", "text-6"],
    "category1"
  );

  info(
    "Remove the event, which has also an individual subscription, for all contexts (2 steps)"
  );

  info("First step: Add the same item for browsingContext1 and globally");
  await root.updateSessionData([
    createSessionDataUpdate(["text-7"], "add", "category1", {
      type: ContextDescriptorType.TopBrowsingContext,
      id: browsingContext1.browserId,
    }),
    createSessionDataUpdate(["text-7"], "add", "category1"),
  ]);
  processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 7);
  // We will find text-7 twice here, the module is responsible for not applying
  // the same session data item twice. Each item corresponds to a different
  // descriptor which matched browsingContext1.
  assertUpdate(
    processedUpdates.at(-1),
    ["text-2", "text-3", "text-4", "text-6", "text-7", "text-7"],
    "category1"
  );

  processedUpdates = await getUpdates(root, browsingContext2);
  is(processedUpdates.length, 5);
  assertUpdate(
    processedUpdates.at(-1),
    ["text-2", "text-3", "text-4", "text-6", "text-7"],
    "category1"
  );

  info("Second step: Remove the item globally");
  await root.updateSessionData([
    createSessionDataUpdate(["text-7"], "remove", "category1"),
  ]);

  processedUpdates = await getUpdates(root, browsingContext1);
  is(processedUpdates.length, 8);
  assertUpdate(
    processedUpdates.at(-1),
    ["text-2", "text-3", "text-4", "text-6", "text-7"],
    "category1"
  );

  processedUpdates = await getUpdates(root, browsingContext2);
  is(processedUpdates.length, 6);
  assertUpdate(
    processedUpdates.at(-1),
    ["text-2", "text-3", "text-4", "text-6"],
    "category1"
  );

  root.destroy();

  gBrowser.removeTab(tab2);
});
