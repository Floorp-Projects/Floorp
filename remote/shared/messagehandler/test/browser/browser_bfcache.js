/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RootMessageHandler } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs"
);

const TEST_PREF = "remote.messagehandler.test.pref";

// Check that pages in bfcache no longer have message handlers attached to them,
// and that they will not emit unexpected events.
add_task(async function test_bfcache_broadcast() {
  const tab = await addTab("https://example.com/document-builder.sjs?html=tab");
  const rootMessageHandler = createRootMessageHandler("session-id-bfcache");

  try {
    const browsingContext = tab.linkedBrowser.browsingContext;
    const contextDescriptor = {
      type: ContextDescriptorType.TopBrowsingContext,
      id: browsingContext.browserId,
    };

    // Whenever a "preference-changed" event from the eventonprefchange module
    // will be received on the root MessageHandler, increment a counter.
    let preferenceChangeEventCount = 0;
    const onEvent = (evtName, wrappedEvt) => {
      if (wrappedEvt.name === "preference-changed") {
        preferenceChangeEventCount++;
      }
    };
    rootMessageHandler.on("message-handler-event", onEvent);

    // Initialize the preference, no eventonprefchange module should be created
    // yet so preferenceChangeEventCount is not expected to be updated.
    Services.prefs.setIntPref(TEST_PREF, 0);
    await TestUtils.waitForCondition(() => preferenceChangeEventCount >= 0);
    is(preferenceChangeEventCount, 0);

    // Broadcast a "ping" command to force the creation of the eventonprefchange
    // module
    let values = await sendPingCommand(rootMessageHandler, contextDescriptor);
    is(values.length, 1, "Broadcast returned a single value");

    Services.prefs.setIntPref(TEST_PREF, 1);
    await TestUtils.waitForCondition(() => preferenceChangeEventCount >= 1);
    is(preferenceChangeEventCount, 1);

    info("Navigate to another page");
    await loadURL(
      tab.linkedBrowser,
      "https://example.com/document-builder.sjs?html=othertab"
    );

    values = await sendPingCommand(rootMessageHandler, contextDescriptor);
    is(values.length, 1, "Broadcast returned a single value after navigation");

    info("Update the preference and check we only receive 1 event");
    Services.prefs.setIntPref(TEST_PREF, 2);
    await TestUtils.waitForCondition(() => preferenceChangeEventCount >= 2);
    is(preferenceChangeEventCount, 2);

    info("Navigate to another origin");
    await loadURL(
      tab.linkedBrowser,
      "https://example.org/document-builder.sjs?html=otherorigin"
    );

    values = await sendPingCommand(rootMessageHandler, contextDescriptor);
    is(
      values.length,
      1,
      "Broadcast returned a single value after cross origin navigation"
    );

    info("Update the preference and check again that we only receive 1 event");
    Services.prefs.setIntPref(TEST_PREF, 3);
    await TestUtils.waitForCondition(() => preferenceChangeEventCount >= 3);
    is(preferenceChangeEventCount, 3);
  } finally {
    rootMessageHandler.destroy();
    gBrowser.removeTab(tab);
    Services.prefs.clearUserPref(TEST_PREF);
  }
});

function sendPingCommand(rootMessageHandler, contextDescriptor) {
  return rootMessageHandler.handleCommand({
    moduleName: "eventonprefchange",
    commandName: "ping",
    params: {},
    destination: {
      contextDescriptor,
      type: WindowGlobalMessageHandler.type,
    },
  });
}
