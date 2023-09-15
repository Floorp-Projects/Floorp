/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MessageHandlerRegistry } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/MessageHandlerRegistry.sys.mjs"
);
const { RootMessageHandler } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs"
);
const { SessionData } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/sessiondata/SessionData.sys.mjs"
);

const TEST_PAGE = "http://example.com/document-builder.sjs?html=tab";

add_task(async function test_sessionData() {
  info("Navigate the initial tab to the test URL");
  const tab1 = gBrowser.selectedTab;
  await loadURL(tab1.linkedBrowser, TEST_PAGE);

  const sessionId = "sessionData-test";

  const rootMessageHandlerRegistry = new MessageHandlerRegistry(
    RootMessageHandler.type
  );

  const rootMessageHandler =
    rootMessageHandlerRegistry.getOrCreateMessageHandler(sessionId);
  ok(rootMessageHandler, "Valid ROOT MessageHandler created");

  const sessionData = rootMessageHandler.sessionData;
  ok(
    sessionData instanceof SessionData,
    "ROOT MessageHandler has a valid sessionData"
  );

  let sessionDataSnapshot = await getSessionDataFromContent();
  is(sessionDataSnapshot.size, 0, "session data is empty");

  info("Store a string value in session data");
  sessionData.updateSessionData([
    {
      method: "add",
      moduleName: "fakemodule",
      category: "testCategory",
      contextDescriptor: contextDescriptorAll,
      values: ["value-1"],
    },
  ]);

  sessionDataSnapshot = await getSessionDataFromContent();
  is(sessionDataSnapshot.size, 1, "session data contains 1 session");
  ok(sessionDataSnapshot.has(sessionId));
  let snapshot = sessionDataSnapshot.get(sessionId);
  ok(Array.isArray(snapshot));
  is(snapshot.length, 1);

  const stringDataItem = snapshot[0];
  checkSessionDataItem(
    stringDataItem,
    "fakemodule",
    "testCategory",
    ContextDescriptorType.All,
    "value-1"
  );

  info("Store a number value in session data");
  sessionData.updateSessionData([
    {
      method: "add",
      moduleName: "fakemodule",
      category: "testCategory",
      contextDescriptor: contextDescriptorAll,
      values: [12],
    },
  ]);
  snapshot = (await getSessionDataFromContent()).get(sessionId);
  is(snapshot.length, 2);

  const numberDataItem = snapshot[1];
  checkSessionDataItem(
    numberDataItem,
    "fakemodule",
    "testCategory",
    ContextDescriptorType.All,
    12
  );

  info("Store a boolean value in session data");
  sessionData.updateSessionData([
    {
      method: "add",
      moduleName: "fakemodule",
      category: "testCategory",
      contextDescriptor: contextDescriptorAll,
      values: [true],
    },
  ]);
  snapshot = (await getSessionDataFromContent()).get(sessionId);
  is(snapshot.length, 3);

  const boolDataItem = snapshot[2];
  checkSessionDataItem(
    boolDataItem,
    "fakemodule",
    "testCategory",
    ContextDescriptorType.All,
    true
  );

  info("Remove one value");
  sessionData.updateSessionData([
    {
      method: "remove",
      moduleName: "fakemodule",
      category: "testCategory",
      contextDescriptor: contextDescriptorAll,
      values: [12],
    },
  ]);
  snapshot = (await getSessionDataFromContent()).get(sessionId);
  is(snapshot.length, 2);
  checkSessionDataItem(
    snapshot[0],
    "fakemodule",
    "testCategory",
    ContextDescriptorType.All,
    "value-1"
  );
  checkSessionDataItem(
    snapshot[1],
    "fakemodule",
    "testCategory",
    ContextDescriptorType.All,
    true
  );

  info("Remove all values");
  sessionData.updateSessionData([
    {
      method: "remove",
      moduleName: "fakemodule",
      category: "testCategory",
      contextDescriptor: contextDescriptorAll,
      values: ["value-1", true],
    },
  ]);
  snapshot = (await getSessionDataFromContent()).get(sessionId);
  is(snapshot.length, 0, "Session data is now empty");

  info("Add another value before destroy");
  sessionData.updateSessionData([
    {
      method: "add",
      moduleName: "fakemodule",
      category: "testCategory",
      contextDescriptor: contextDescriptorAll,
      values: ["value-2"],
    },
  ]);
  snapshot = (await getSessionDataFromContent()).get(sessionId);
  is(snapshot.length, 1);
  checkSessionDataItem(
    snapshot[0],
    "fakemodule",
    "testCategory",
    ContextDescriptorType.All,
    "value-2"
  );

  sessionData.destroy();
  sessionDataSnapshot = await getSessionDataFromContent();
  is(sessionDataSnapshot.size, 0, "session data should be empty again");
});

add_task(async function test_sessionDataRootOnlyModule() {
  const sessionId = "sessionData-test-rootOnly";

  const rootMessageHandler = createRootMessageHandler(sessionId);
  ok(rootMessageHandler, "Valid ROOT MessageHandler created");

  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    "https://example.com/document-builder.sjs?html=tab"
  );

  const windowGlobalCreated = rootMessageHandler.once(
    "window-global-handler-created"
  );

  info("Test that adding SessionData items works the root module");
  // Updating the session data on the root message handler should not cause
  // failures for other message handlers if the module only exists for root.
  await rootMessageHandler.addSessionDataItem({
    moduleName: "rootOnly",
    category: "session_data_root_only",
    contextDescriptor: {
      type: ContextDescriptorType.All,
    },
    values: [true],
  });

  await windowGlobalCreated;
  ok(true, "Window global has been initialized");

  let sessionDataReceivedByRoot = await rootMessageHandler.handleCommand({
    moduleName: "rootOnly",
    commandName: "getSessionDataReceived",
    destination: {
      type: RootMessageHandler.type,
    },
  });

  is(sessionDataReceivedByRoot.length, 1);
  is(sessionDataReceivedByRoot[0].category, "session_data_root_only");
  is(sessionDataReceivedByRoot[0].added.length, 1);
  is(sessionDataReceivedByRoot[0].added[0], true);
  is(
    sessionDataReceivedByRoot[0].contextDescriptor.type,
    ContextDescriptorType.All
  );

  info("Now test that removing items also works on the root module");
  await rootMessageHandler.removeSessionDataItem({
    moduleName: "rootOnly",
    category: "session_data_root_only",
    contextDescriptor: {
      type: ContextDescriptorType.All,
    },
    values: [true],
  });

  sessionDataReceivedByRoot = await rootMessageHandler.handleCommand({
    moduleName: "rootOnly",
    commandName: "getSessionDataReceived",
    destination: {
      type: RootMessageHandler.type,
    },
  });

  is(sessionDataReceivedByRoot.length, 2);
  is(sessionDataReceivedByRoot[1].category, "session_data_root_only");
  is(sessionDataReceivedByRoot[1].removed.length, 1);
  is(sessionDataReceivedByRoot[1].removed[0], true);
  is(
    sessionDataReceivedByRoot[1].contextDescriptor.type,
    ContextDescriptorType.All
  );

  rootMessageHandler.destroy();
});

function checkSessionDataItem(item, moduleName, category, contextType, value) {
  is(item.moduleName, moduleName, "Data item has the expected module name");
  is(item.category, category, "Data item has the expected category");
  is(
    item.contextDescriptor.type,
    contextType,
    "Data item has the expected context type"
  );
  is(item.value, value, "Data item has the expected value");
}

function getSessionDataFromContent() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const { readSessionData } = ChromeUtils.importESModule(
      "chrome://remote/content/shared/messagehandler/sessiondata/SessionDataReader.sys.mjs"
    );
    return readSessionData();
  });
}
