/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { NavigationManager } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/NavigationManager.sys.mjs"
);
const { TabManager } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/TabManager.sys.mjs"
);

add_task(async function testDocumentOpenWriteClose() {
  const events = [];
  const onEvent = (name, data) => events.push({ name, data });

  const navigationManager = new NavigationManager();
  navigationManager.on("location-changed", onEvent);
  navigationManager.on("navigation-started", onEvent);
  navigationManager.on("navigation-stopped", onEvent);

  const url = "https://example.com/document-builder.sjs?html=test";

  const tab = addTab(gBrowser, url);
  const browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);

  navigationManager.startMonitoring();
  is(events.length, 0, "No event recorded");

  info("Replace the document");
  await SpecialPowers.spawn(browser, [], async () => {
    // Note: we need to use eval here to have reduced permissions and avoid
    // security errors.
    content.eval(`
      document.open();
      document.write("<h1 class='replaced'>Replaced</h1>");
      document.close();
    `);

    await ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector(".replaced")
    );
  });

  // See Bug 1844517.
  // document.open/write/close is identical to same-url + same-hash navigations.
  todo_is(events.length, 0, "No event recorded after replacing the document");

  info("Reload the page, which should trigger a navigation");
  await loadURL(browser, url);

  // See Bug 1844517.
  // document.open/write/close is identical to same-url + same-hash navigations.
  todo_is(events.length, 2, "Recorded navigation events");

  navigationManager.off("location-changed", onEvent);
  navigationManager.off("navigation-started", onEvent);
  navigationManager.off("navigation-stopped", onEvent);
  navigationManager.stopMonitoring();
});
