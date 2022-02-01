/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE = "https://example.com/document-builder.sjs?html=tab";

/**
 * Check that message handlers are not created for parent process browser
 * elements, even if they have the type="content" attribute (eg used for the
 * DevTools toolbox).
 */
add_task(async function test_session_data_broadcast() {
  const tab1 = BrowserTestUtils.addTab(gBrowser, TEST_PAGE);
  const contentBrowser1 = tab1.linkedBrowser;
  await BrowserTestUtils.browserLoaded(contentBrowser1);
  const parentBrowser1 = createParentBrowserElement(tab1, "content");
  const parentBrowser2 = createParentBrowserElement(tab1, "chrome");

  const root = createRootMessageHandler("session-id-event");

  info("Add a new session data item, expect one return value");
  await root.addSessionData({
    moduleName: "command",
    category: "browser_session_data_browser_element",
    contextDescriptor: {
      type: CONTEXT_DESCRIPTOR_TYPES.ALL,
    },
    values: [true],
  });

  info(
    "Check that only the content tab window global received the session data"
  );
  is(await hasSessionDataFlag(contentBrowser1), true);
  is(await hasSessionDataFlag(parentBrowser1), undefined);
  is(await hasSessionDataFlag(parentBrowser2), undefined);

  const tab2 = BrowserTestUtils.addTab(gBrowser, TEST_PAGE);
  const contentBrowser2 = tab2.linkedBrowser;
  await BrowserTestUtils.browserLoaded(contentBrowser2);
  const parentBrowser3 = createParentBrowserElement(contentBrowser2, "content");
  const parentBrowser4 = createParentBrowserElement(contentBrowser2, "chrome");

  info("Wait until the session data was applied to the new tab");
  await SpecialPowers.spawn(contentBrowser2, [], async () => {
    await ContentTaskUtils.waitForCondition(() => content.hasSessionDataFlag);
  });

  info("Check that parent browser elements did not apply the session data");
  is(await hasSessionDataFlag(parentBrowser3), undefined);
  is(await hasSessionDataFlag(parentBrowser4), undefined);

  root.destroy();

  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);
});

function hasSessionDataFlag(browser) {
  return SpecialPowers.spawn(browser, [], () => {
    return content.hasSessionDataFlag;
  });
}

function createParentBrowserElement(tab, type) {
  const parentBrowser = gBrowser.ownerDocument.createXULElement("browser");
  parentBrowser.setAttribute("type", type);
  const container = gBrowser.getBrowserContainer(tab.linkedBrowser);
  container.appendChild(parentBrowser);

  return parentBrowser;
}
