/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { isBrowsingContextCompatible } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/transports/BrowsingContextUtils.sys.mjs"
);
const TEST_COM_PAGE = "https://example.com/document-builder.sjs?html=com";
const TEST_NET_PAGE = "https://example.net/document-builder.sjs?html=net";

// Test helpers from BrowsingContextUtils in various processes.
add_task(async function () {
  const tab1 = BrowserTestUtils.addTab(gBrowser, TEST_COM_PAGE);
  const contentBrowser1 = tab1.linkedBrowser;
  await BrowserTestUtils.browserLoaded(contentBrowser1);
  const browserId1 = contentBrowser1.browsingContext.browserId;

  const tab2 = BrowserTestUtils.addTab(gBrowser, TEST_NET_PAGE);
  const contentBrowser2 = tab2.linkedBrowser;
  await BrowserTestUtils.browserLoaded(contentBrowser2);
  const browserId2 = contentBrowser2.browsingContext.browserId;

  const { extension, sidebarBrowser } = await installSidebarExtension();

  const tab3 = BrowserTestUtils.addTab(
    gBrowser,
    `moz-extension://${extension.uuid}/tab.html`
  );
  const { bcId } = await extension.awaitMessage("tab-loaded");
  const tabExtensionBrowser = BrowsingContext.get(bcId).top.embedderElement;

  const parentBrowser1 = createParentBrowserElement(tab1, "content");
  const parentBrowser2 = createParentBrowserElement(tab1, "chrome");

  info("Check browsing context compatibility for content browser 1");
  await checkBrowsingContextCompatible(contentBrowser1, undefined, true);
  await checkBrowsingContextCompatible(contentBrowser1, browserId1, true);
  await checkBrowsingContextCompatible(contentBrowser1, browserId2, false);

  info("Check browsing context compatibility for content browser 2");
  await checkBrowsingContextCompatible(contentBrowser2, undefined, true);
  await checkBrowsingContextCompatible(contentBrowser2, browserId1, false);
  await checkBrowsingContextCompatible(contentBrowser2, browserId2, true);

  info("Check browsing context compatibility for parent browser 1");
  await checkBrowsingContextCompatible(parentBrowser1, undefined, false);
  await checkBrowsingContextCompatible(parentBrowser1, browserId1, false);
  await checkBrowsingContextCompatible(parentBrowser1, browserId2, false);

  info("Check browsing context compatibility for parent browser 2");
  await checkBrowsingContextCompatible(parentBrowser2, undefined, false);
  await checkBrowsingContextCompatible(parentBrowser2, browserId1, false);
  await checkBrowsingContextCompatible(parentBrowser2, browserId2, false);

  info("Check browsing context compatibility for extension");
  await checkBrowsingContextCompatible(sidebarBrowser, undefined, false);
  await checkBrowsingContextCompatible(sidebarBrowser, browserId1, false);
  await checkBrowsingContextCompatible(sidebarBrowser, browserId2, false);

  info("Check browsing context compatibility for extension viewed in a tab");
  await checkBrowsingContextCompatible(tabExtensionBrowser, undefined, false);
  await checkBrowsingContextCompatible(tabExtensionBrowser, browserId1, false);
  await checkBrowsingContextCompatible(tabExtensionBrowser, browserId2, false);

  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);
  gBrowser.removeTab(tab3);
  await extension.unload();
});

async function checkBrowsingContextCompatible(browser, browserId, expected) {
  const options = { browserId };
  info("Check browsing context compatibility from the parent process");
  is(isBrowsingContextCompatible(browser.browsingContext, options), expected);

  info(
    "Check browsing context compatibility from the browsing context's process"
  );
  await SpecialPowers.spawn(
    browser,
    [browserId, expected],
    (_browserId, _expected) => {
      const BrowsingContextUtils = ChromeUtils.importESModule(
        "chrome://remote/content/shared/messagehandler/transports/BrowsingContextUtils.sys.mjs"
      );
      is(
        BrowsingContextUtils.isBrowsingContextCompatible(
          content.browsingContext,
          {
            browserId: _browserId,
          }
        ),
        _expected
      );
    }
  );
}
