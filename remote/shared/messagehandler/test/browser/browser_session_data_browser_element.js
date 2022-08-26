/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE = "https://example.com/document-builder.sjs?html=tab";

/**
 * Check that message handlers are not created for parent process browser
 * elements, even if they have the type="content" attribute (eg used for the
 * DevTools toolbox), as well as for webextension contexts.
 */
add_task(async function test_session_data_broadcast() {
  // Prepare:
  // - one content tab
  // - one browser type content
  // - one browser type chrome
  // - one sidebar webextension
  // We only expect session data to be applied to the content tab
  const tab1 = BrowserTestUtils.addTab(gBrowser, TEST_PAGE);
  const contentBrowser1 = tab1.linkedBrowser;
  await BrowserTestUtils.browserLoaded(contentBrowser1);
  const parentBrowser1 = createParentBrowserElement(tab1, "content");
  const parentBrowser2 = createParentBrowserElement(tab1, "chrome");
  const {
    extension: extension1,
    sidebarBrowser: extSidebarBrowser1,
  } = await installSidebarExtension();

  const root = createRootMessageHandler("session-id-event");

  // When the windowglobal command.jsm module applies the session data
  // browser_session_data_browser_element, it will emit an event.
  // Collect the events to detect which MessageHandlers have been started.
  info("Watch events emitted when session data is applied");
  const sessionDataEvents = [];
  const onRootEvent = function(evtName, wrappedEvt) {
    if (wrappedEvt.name === "received-session-data") {
      sessionDataEvents.push(wrappedEvt.data.contextId);
    }
  };
  root.on("message-handler-event", onRootEvent);

  info("Add a new session data item, expect one return value");
  await root.addSessionData({
    moduleName: "command",
    category: "browser_session_data_browser_element",
    contextDescriptor: {
      type: ContextDescriptorType.All,
    },
    values: [true],
  });

  function hasSessionData(browsingContext) {
    return sessionDataEvents.includes(browsingContext.id);
  }

  info(
    "Check that only the content tab window global received the session data"
  );
  is(hasSessionData(contentBrowser1.browsingContext), true);
  is(hasSessionData(parentBrowser1.browsingContext), false);
  is(hasSessionData(parentBrowser2.browsingContext), false);
  is(hasSessionData(extSidebarBrowser1.browsingContext), false);

  const tab2 = BrowserTestUtils.addTab(gBrowser, TEST_PAGE);
  const contentBrowser2 = tab2.linkedBrowser;
  await BrowserTestUtils.browserLoaded(contentBrowser2);
  const parentBrowser3 = createParentBrowserElement(contentBrowser2, "content");
  const parentBrowser4 = createParentBrowserElement(contentBrowser2, "chrome");

  const {
    extension: extension2,
    sidebarBrowser: extSidebarBrowser2,
  } = await installSidebarExtension();

  info("Wait until the session data was applied to the new tab");
  await TestUtils.waitForCondition(() =>
    sessionDataEvents.includes(contentBrowser2.browsingContext.id)
  );

  info("Check that parent browser elements did not apply the session data");
  is(hasSessionData(parentBrowser3.browsingContext), false);
  is(hasSessionData(parentBrowser4.browsingContext), false);

  info(
    "Check that extension did not apply the session data, " +
      extSidebarBrowser2.browsingContext.id
  );
  is(hasSessionData(extSidebarBrowser2.browsingContext), false);

  root.destroy();

  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);
  await extension1.unload();
  await extension2.unload();
});
