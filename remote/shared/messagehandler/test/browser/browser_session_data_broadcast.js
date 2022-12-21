/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RootMessageHandler } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs"
);

const TEST_PAGE = "https://example.com/document-builder.sjs?html=tab";

add_task(async function test_session_data_broadcast() {
  const tab1 = gBrowser.selectedTab;
  await loadURL(tab1.linkedBrowser, TEST_PAGE);
  const browsingContext1 = tab1.linkedBrowser.browsingContext;

  const root = createRootMessageHandler("session-id-event");

  info("Add a new session data item, expect one return value");
  const value1 = await addSessionData(root, ["text-1"]);
  is(value1.length, 1);
  is(value1[0].addedData, "text-1");
  is(value1[0].removedData, "");
  is(value1[0].sessionData, "text-1");
  is(value1[0].contextId, browsingContext1.id);

  info("Add two session data items, expect one return value with both items");
  const value2 = await addSessionData(root, ["text-2", "text-3"]);
  is(value2.length, 1);
  is(value2[0].addedData, "text-2, text-3");
  is(value2[0].removedData, "");
  is(value2[0].sessionData, "text-1, text-2, text-3");
  is(value2[0].contextId, browsingContext1.id);

  info("Try to add an existing data item, expect no return value");
  const value3 = await addSessionData(root, ["text-1"]);
  is(value3.length, 0);

  info("Add an existing and a new item, expect only the new item to return");
  const value4 = await addSessionData(root, ["text-1", "text-4"]);
  is(value4.length, 1);
  is(value4[0].addedData, "text-4");
  is(value4[0].removedData, "");
  is(value4[0].sessionData, "text-1, text-2, text-3, text-4");
  is(value4[0].contextId, browsingContext1.id);

  info("Remove an item, expect only the new item to return");
  const value5 = await removeSessionData(root, ["text-3"]);
  is(value5.length, 1);
  is(value5[0].addedData, "");
  is(value5[0].removedData, "text-3");
  is(value5[0].sessionData, "text-1, text-2, text-4");
  is(value5[0].contextId, browsingContext1.id);

  info("Open a new tab on the same test URL");
  const tab2 = await addTab(TEST_PAGE);
  const browsingContext2 = tab2.linkedBrowser.browsingContext;

  info("Add a new session data item, check both contexts have the same data.");
  const value6 = await addSessionData(root, ["text-5"]);
  is(value6.length, 2);
  is(value6[0].addedData, "text-5");
  is(value6[0].removedData, "");
  is(value6[0].sessionData, "text-1, text-2, text-4, text-5");
  is(value6[0].contextId, browsingContext1.id);
  is(value6[1].addedData, "text-5");
  // "text-1, text-2, text-4" were added as initial session data and
  // "text-5" was added afterwards.
  is(value6[1].sessionData, "text-1, text-2, text-4, text-5");
  is(value6[1].contextId, browsingContext2.id);

  info("Remove a missing item, expect no return value");
  const value7 = await removeSessionData(root, ["text-missing"]);
  is(value7.length, 0);

  info("Remove an existing and a missing item");
  const value8 = await removeSessionData(root, ["text-2", "text-missing"]);
  is(value8.length, 2);
  is(value8[0].addedData, "");
  is(value8[0].removedData, "text-2");
  is(value8[0].sessionData, "text-1, text-4, text-5");
  is(value8[0].contextId, browsingContext1.id);
  is(value8[1].addedData, "");
  is(value8[1].removedData, "text-2");
  is(value8[1].sessionData, "text-1, text-4, text-5");
  is(value8[1].contextId, browsingContext2.id);

  info("Add multiple items at once");
  const value9 = await updateSessionData(root, [
    {
      method: "add",
      values: ["text-6"],
      moduleName: "command",
      category: "testCategory",
      contextDescriptor: {
        type: ContextDescriptorType.All,
      },
    },
    {
      method: "add",
      values: ["text-7"],
      moduleName: "command",
      category: "testCategory",
      contextDescriptor: {
        type: ContextDescriptorType.TopBrowsingContext,
        id: browsingContext2.browserId,
      },
    },
  ]);
  is(value9.length, 2);
  is(value9[0].addedData, "text-6");
  is(value9[0].removedData, "");
  is(value9[0].sessionData, "text-1, text-4, text-5, text-6");
  is(value9[0].contextId, browsingContext1.id);
  is(value9[1].addedData, "text-6, text-7");
  is(value9[1].removedData, "");
  is(value9[1].sessionData, "text-1, text-4, text-5, text-6, text-7");
  is(value9[1].contextId, browsingContext2.id);

  info("Remove multiple items at once");
  const value10 = await updateSessionData(root, [
    {
      method: "remove",
      values: ["text-5"],
      moduleName: "command",
      category: "testCategory",
      contextDescriptor: {
        type: ContextDescriptorType.All,
      },
    },
    {
      method: "remove",
      values: ["text-7"],
      moduleName: "command",
      category: "testCategory",
      contextDescriptor: {
        type: ContextDescriptorType.TopBrowsingContext,
        id: browsingContext2.browserId,
      },
    },
  ]);
  is(value10.length, 2);
  is(value10[0].addedData, "");
  is(value10[0].removedData, "text-5");
  is(value10[0].sessionData, "text-1, text-4, text-6");
  is(value10[0].contextId, browsingContext1.id);
  is(value10[1].addedData, "");
  is(value10[1].removedData, "text-5, text-7");
  is(value10[1].sessionData, "text-1, text-4, text-6");
  is(value10[1].contextId, browsingContext2.id);

  info("Add and remove at once");
  const value11 = await updateSessionData(root, [
    {
      method: "add",
      values: ["text-8"],
      moduleName: "command",
      category: "testCategory",
      contextDescriptor: {
        type: ContextDescriptorType.All,
      },
    },
    {
      method: "remove",
      values: ["text-6"],
      moduleName: "command",
      category: "testCategory",
      contextDescriptor: {
        type: ContextDescriptorType.All,
      },
    },
  ]);
  is(value11.length, 2);
  is(value11[0].addedData, "text-8");
  is(value11[0].removedData, "text-6");
  is(value11[0].sessionData, "text-1, text-4, text-8");
  is(value11[0].contextId, browsingContext1.id);
  is(value11[1].addedData, "text-8");
  is(value11[1].removedData, "text-6");
  is(value11[1].sessionData, "text-1, text-4, text-8");
  is(value11[1].contextId, browsingContext2.id);

  info(
    "Add session data item to all contexts and remove this event for one context"
  );
  // Add first an event for one context.
  const value12 = await updateSessionData(root, [
    {
      method: "add",
      values: ["text-9"],
      moduleName: "command",
      category: "testCategory",
      contextDescriptor: {
        type: ContextDescriptorType.TopBrowsingContext,
        id: browsingContext1.browserId,
      },
    },
  ]);
  is(value12.length, 1);
  is(value12[0].addedData, "text-9");
  is(value12[0].removedData, "");
  is(value12[0].sessionData, "text-1, text-4, text-8, text-9");
  is(value12[0].contextId, browsingContext1.id);

  // Remove the item for one context and add the item for all contexts.
  const value13 = await updateSessionData(root, [
    {
      method: "remove",
      values: ["text-9"],
      moduleName: "command",
      category: "testCategory",
      contextDescriptor: {
        type: ContextDescriptorType.TopBrowsingContext,
        id: browsingContext1.browserId,
      },
    },
    {
      method: "add",
      values: ["text-9"],
      moduleName: "command",
      category: "testCategory",
      contextDescriptor: {
        type: ContextDescriptorType.All,
      },
    },
  ]);
  is(value13.length, 2);
  is(value13[0].addedData, "");
  // Make sure that nothing is removed.
  is(value13[0].removedData, "");
  is(value13[0].sessionData, "text-1, text-4, text-8, text-9");
  is(value13[0].contextId, browsingContext1.id);
  is(value13[1].addedData, "text-9");
  is(value13[1].removedData, "");
  is(value13[1].sessionData, "text-1, text-4, text-8, text-9");
  is(value13[1].contextId, browsingContext2.id);

  info(
    "Remove the event, which has also an individual subscription, for all contexts."
  );
  const value14 = await updateSessionData(root, [
    {
      method: "add",
      values: ["text-10"],
      moduleName: "command",
      category: "testCategory",
      contextDescriptor: {
        type: ContextDescriptorType.All,
      },
    },
    {
      method: "add",
      values: ["text-10"],
      moduleName: "command",
      category: "testCategory",
      contextDescriptor: {
        type: ContextDescriptorType.TopBrowsingContext,
        id: browsingContext1.browserId,
      },
    },
  ]);
  is(value14.length, 2);
  is(value14[0].addedData, "text-10");
  is(value14[0].removedData, "");
  is(value14[0].sessionData, "text-1, text-4, text-8, text-9, text-10");
  is(value14[0].contextId, browsingContext1.id);
  is(value14[1].addedData, "text-10");
  is(value14[1].removedData, "");
  is(value14[1].sessionData, "text-1, text-4, text-8, text-9, text-10");
  is(value14[1].contextId, browsingContext2.id);

  const value15 = await updateSessionData(root, [
    {
      method: "remove",
      values: ["text-10"],
      moduleName: "command",
      category: "testCategory",
      contextDescriptor: {
        type: ContextDescriptorType.All,
      },
    },
  ]);

  is(value15.length, 2);
  is(value15[0].addedData, "");
  // Make sure that nothing is removed for the first context
  is(value15[0].removedData, "");
  is(value15[0].sessionData, "text-1, text-4, text-8, text-9, text-10");
  is(value15[0].contextId, browsingContext1.id);
  is(value15[1].addedData, "");
  is(value15[1].removedData, "text-10");
  is(value15[1].sessionData, "text-1, text-4, text-8, text-9");
  is(value15[1].contextId, browsingContext2.id);

  root.destroy();

  gBrowser.removeTab(tab2);
});

function addSessionData(rootMessageHandler, values) {
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

function removeSessionData(rootMessageHandler, values) {
  return rootMessageHandler.handleCommand({
    moduleName: "command",
    commandName: "testRemoveSessionData",
    destination: {
      type: RootMessageHandler.type,
    },
    params: {
      values,
    },
  });
}

function updateSessionData(rootMessageHandler, params) {
  return rootMessageHandler.handleCommand({
    moduleName: "command",
    commandName: "testUpdateSessionData",
    destination: {
      type: RootMessageHandler.type,
    },
    params,
  });
}
