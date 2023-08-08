/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { NavigationManager } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/NavigationManager.sys.mjs"
);
const { TabManager } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/TabManager.sys.mjs"
);

const TEST_URL = "https://example.com/document-builder.sjs?html=test1";
const TEST_URL_CLOSED_PORT = "http://127.0.0.1:36325/";
const TEST_URL_WRONG_URI = "https://www.wronguri.wronguri/";

add_task(async function testClosedPort() {
  const events = [];
  const onEvent = (name, data) => events.push({ name, data });

  const navigationManager = new NavigationManager();
  navigationManager.on("navigation-started", onEvent);
  navigationManager.on("navigation-stopped", onEvent);

  const tab = addTab(gBrowser, TEST_URL);
  const browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);

  const navigableId = TabManager.getIdForBrowser(browser);

  navigationManager.startMonitoring();
  is(
    navigationManager.getNavigationForBrowsingContext(browser.browsingContext),
    null,
    "No navigation recorded yet"
  );
  is(events.length, 0, "No event recorded");

  await loadURL(browser, TEST_URL_CLOSED_PORT, { maybeErrorPage: true });

  const firstNavigation = navigationManager.getNavigationForBrowsingContext(
    browser.browsingContext
  );
  assertNavigation(firstNavigation, TEST_URL_CLOSED_PORT);

  is(events.length, 2, "Two events recorded");
  assertNavigationEvents(
    events,
    TEST_URL_CLOSED_PORT,
    firstNavigation.navigationId,
    navigableId
  );

  navigationManager.off("navigation-started", onEvent);
  navigationManager.off("navigation-stopped", onEvent);
  navigationManager.stopMonitoring();
});

add_task(async function testWrongURI() {
  const events = [];
  const onEvent = (name, data) => events.push({ name, data });

  const navigationManager = new NavigationManager();
  navigationManager.on("navigation-started", onEvent);
  navigationManager.on("navigation-stopped", onEvent);

  const tab = addTab(gBrowser, TEST_URL);
  const browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);

  const navigableId = TabManager.getIdForBrowser(browser);

  navigationManager.startMonitoring();

  is(
    navigationManager.getNavigationForBrowsingContext(browser.browsingContext),
    null,
    "No navigation recorded yet"
  );
  is(events.length, 0, "No event recorded");

  await loadURL(browser, TEST_URL_WRONG_URI, { maybeErrorPage: true });

  const firstNavigation = navigationManager.getNavigationForBrowsingContext(
    browser.browsingContext
  );
  assertNavigation(firstNavigation, TEST_URL_WRONG_URI);

  is(events.length, 2, "Two events recorded");
  assertNavigationEvents(
    events,
    TEST_URL_WRONG_URI,
    firstNavigation.navigationId,
    navigableId
  );

  navigationManager.off("navigation-started", onEvent);
  navigationManager.off("navigation-stopped", onEvent);
  navigationManager.stopMonitoring();
});
