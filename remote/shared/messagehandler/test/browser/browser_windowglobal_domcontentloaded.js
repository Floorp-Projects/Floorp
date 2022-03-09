/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const NET_TEST_URL = "https://example.com/document-builder.sjs?html=net";
const ORG_TEST_URL = "https://example.org/document-builder.sjs?html=org";
const COM_TEST_URL = "https://example.com/document-builder.sjs?html=com";
const SLOW_IMG = encodeURIComponent(
  `<img src="https://example.org/document-builder.sjs?delay=100000&html=image">`
);
const SLOW_RESOURCE_URL =
  "https://example.org/document-builder.sjs?html=" + SLOW_IMG;

/**
 * Check that the message handler framework emits DOMContentLoaded events
 * for all windows.
 */
add_task(async function test_windowGlobalDOMContentLoaded() {
  const rootMessageHandler = createRootMessageHandler(
    "session-id-event_with_frames"
  );

  info("Add a new session data item to get window global handlers created");
  await rootMessageHandler.addSessionData({
    moduleName: "command",
    category: "browser_session_data_browser_element",
    contextDescriptor: {
      type: ContextDescriptorType.All,
    },
    values: [true],
  });

  const loadEvents = [];
  const onEvent = (evtName, wrappedEvt) => {
    if (wrappedEvt.name === "window-global-dom-content-loaded") {
      info(`Received event for context ${wrappedEvt.data.contextId}`);
      loadEvents.push(wrappedEvt.data);
    }
  };
  rootMessageHandler.on("message-handler-event", onEvent);

  info("Navigate the initial tab to the test URL");
  const browser = gBrowser.selectedBrowser;
  await loadURL(browser, ORG_TEST_URL);
  await TestUtils.waitForCondition(() => loadEvents.length >= 1);
  is(loadEvents.length, 1, "The expected number of events was received");
  checkEvent(loadEvents[0], browser.browsingContext.id, ORG_TEST_URL);

  info("Navigate the tab to another URL");
  await loadURL(browser, COM_TEST_URL);
  await TestUtils.waitForCondition(() => loadEvents.length >= 2);
  is(loadEvents.length, 2, "The expected number of events was received");
  checkEvent(loadEvents[1], browser.browsingContext.id, COM_TEST_URL);

  info("Create an iframe in the current page");
  await SpecialPowers.spawn(browser, [NET_TEST_URL], async function(url) {
    const iframe = content.document.createElement("iframe");
    iframe.src = url;
    content.document.body.appendChild(iframe);
  });
  await TestUtils.waitForCondition(() => loadEvents.length >= 3);
  is(loadEvents.length, 3, "The expected number of events was received");
  const frameContextId = browser.browsingContext.children[0].id;
  checkEvent(loadEvents[2], frameContextId, NET_TEST_URL);

  info("Reload the current page without the dynamically created iframe");
  gBrowser.reloadTab(gBrowser.selectedTab);
  await TestUtils.waitForCondition(() => loadEvents.length >= 4);
  is(loadEvents.length, 4, "The expected number of events was received");
  checkEvent(loadEvents[3], browser.browsingContext.id, COM_TEST_URL);

  info("Navigate to a page with a slow loading resource (no await here)");
  loadURL(browser, SLOW_RESOURCE_URL);
  await TestUtils.waitForCondition(() => loadEvents.length >= 5);
  is(loadEvents.length, 5, "The expected number of events was received");
  checkEvent(loadEvents[4], browser.browsingContext.id, SLOW_RESOURCE_URL);
  const readyState = await SpecialPowers.spawn(
    browser,
    [],
    () => content.document.readyState
  );
  is(
    readyState,
    "interactive",
    "The document of the slow loading page is still interactive"
  );

  info("Navigate to about:blank");
  await loadURL(browser, "about:blank");
  await TestUtils.waitForCondition(() => loadEvents.length >= 6);
  is(loadEvents.length, 6, "The expected number of events was received");
  checkEvent(loadEvents[5], browser.browsingContext.id, "about:blank");

  info("Open a new tab on a test URL");
  const tab = BrowserTestUtils.addTab(gBrowser, NET_TEST_URL);
  await TestUtils.waitForCondition(() => loadEvents.length >= 7);
  is(loadEvents.length, 7, "The expected number of events was received");
  checkEvent(loadEvents[6], tab.linkedBrowser.browsingContext.id, NET_TEST_URL);

  // Note that we are not testing back/forward, as those will lead only to
  // pagehide/pageshow events in case of bfcache navigations.

  rootMessageHandler.off("message-handler-event", onEvent);
  rootMessageHandler.destroy();
  gBrowser.removeTab(tab);
});

function checkEvent(event, expectedContextId, expectedURI) {
  is(
    event.contextId,
    expectedContextId,
    "DOMContentLoaded event was received for the correct browsing context"
  );

  is(
    event.documentURI,
    expectedURI,
    "DOMContentLoaded event was received with the expected document URI"
  );

  is(
    event.readyState,
    "interactive",
    "DOMContentLoaded event was received with the expected readyState"
  );
}
