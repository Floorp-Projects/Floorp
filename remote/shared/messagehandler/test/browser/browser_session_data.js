/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MessageHandlerRegistry } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/MessageHandlerRegistry.jsm"
);
const { RootMessageHandler } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.jsm"
);
const { SessionData } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/sessiondata/SessionData.jsm"
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

  const rootMessageHandler = rootMessageHandlerRegistry.getOrCreateMessageHandler(
    sessionId
  );
  ok(rootMessageHandler, "Valid ROOT MessageHandler created");

  const sessionData = rootMessageHandler.sessionData;
  ok(
    sessionData instanceof SessionData,
    "ROOT MessageHandler has a valid sessionData"
  );

  let sessionDataSnapshot = await getSessionDataFromContent();
  is(sessionDataSnapshot.size, 0, "session data is empty");

  info("Store a string value in session data");
  sessionData.addSessionData(
    "fakemodule",
    "testCategory",
    contextDescriptorAll,
    ["value-1"]
  );

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
    CONTEXT_DESCRIPTOR_TYPES.ALL,
    "value-1"
  );

  info("Store a number value in session data");
  sessionData.addSessionData(
    "fakemodule",
    "testCategory",
    contextDescriptorAll,
    [12]
  );
  snapshot = (await getSessionDataFromContent()).get(sessionId);
  is(snapshot.length, 2);

  const numberDataItem = snapshot[1];
  checkSessionDataItem(
    numberDataItem,
    "fakemodule",
    "testCategory",
    CONTEXT_DESCRIPTOR_TYPES.ALL,
    12
  );

  info("Store a boolean value in session data");
  sessionData.addSessionData(
    "fakemodule",
    "testCategory",
    contextDescriptorAll,
    [true]
  );
  snapshot = (await getSessionDataFromContent()).get(sessionId);
  is(snapshot.length, 3);

  const boolDataItem = snapshot[2];
  checkSessionDataItem(
    boolDataItem,
    "fakemodule",
    "testCategory",
    CONTEXT_DESCRIPTOR_TYPES.ALL,
    true
  );

  info("Remove one value");
  sessionData.removeSessionData(
    "fakemodule",
    "testCategory",
    contextDescriptorAll,
    [12]
  );
  snapshot = (await getSessionDataFromContent()).get(sessionId);
  is(snapshot.length, 2);
  checkSessionDataItem(
    snapshot[0],
    "fakemodule",
    "testCategory",
    CONTEXT_DESCRIPTOR_TYPES.ALL,
    "value-1"
  );
  checkSessionDataItem(
    snapshot[1],
    "fakemodule",
    "testCategory",
    CONTEXT_DESCRIPTOR_TYPES.ALL,
    true
  );

  info("Remove all values");
  sessionData.removeSessionData(
    "fakemodule",
    "testCategory",
    contextDescriptorAll,
    ["value-1", true]
  );
  snapshot = (await getSessionDataFromContent()).get(sessionId);
  is(snapshot.length, 0, "Session data is now empty");

  info("Add another value before destroy");
  sessionData.addSessionData(
    "fakemodule",
    "testCategory",
    contextDescriptorAll,
    ["value-2"]
  );
  snapshot = (await getSessionDataFromContent()).get(sessionId);
  is(snapshot.length, 1);
  checkSessionDataItem(
    snapshot[0],
    "fakemodule",
    "testCategory",
    CONTEXT_DESCRIPTOR_TYPES.ALL,
    "value-2"
  );

  sessionData.destroy();
  sessionDataSnapshot = await getSessionDataFromContent();
  is(sessionDataSnapshot.size, 0, "session data should be empty again");
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
    const { readSessionData } = ChromeUtils.import(
      "chrome://remote/content/shared/messagehandler/sessiondata/SessionDataReader.jsm"
    );
    return readSessionData();
  });
}
