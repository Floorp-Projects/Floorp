"use strict";

add_task(async function webnav_test_eventpage() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.eventPages.enabled", true]],
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webNavigation", "*://mochi.test/*"],
      background: { persistent: false },
    },
    background() {
      const EVENTS = [
        "onTabReplaced",
        "onBeforeNavigate",
        "onCommitted",
        "onDOMContentLoaded",
        "onCompleted",
        "onErrorOccurred",
        "onReferenceFragmentUpdated",
        "onHistoryStateUpdated",
      ];

      for (let event of EVENTS) {
        browser.webNavigation[event].addListener(() => {});
      }
      browser.test.sendMessage("ready");
    },
  });

  // onTabReplaced is never persisted, it is an empty event handler.
  const EVENTS = [
    "onBeforeNavigate",
    "onCommitted",
    "onDOMContentLoaded",
    "onCompleted",
    "onErrorOccurred",
    "onReferenceFragmentUpdated",
    "onHistoryStateUpdated",
  ];

  await extension.startup();
  await extension.awaitMessage("ready");
  for (let event of EVENTS) {
    assertPersistentListeners(extension, "webNavigation", event, {
      primed: false,
    });
  }

  await extension.terminateBackground();
  for (let event of EVENTS) {
    assertPersistentListeners(extension, "webNavigation", event, {
      primed: true,
    });
  }

  // wake up the background, we don't really care which event does it,
  // we're just verifying the state after.
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  await extension.awaitMessage("ready");
  for (let event of EVENTS) {
    assertPersistentListeners(extension, "webNavigation", event, {
      primed: false,
    });
  }

  await BrowserTestUtils.closeWindow(newWin);

  await extension.unload();
  await SpecialPowers.popPrefEnv();
});
