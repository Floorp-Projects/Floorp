/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { NavigationManager } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/NavigationManager.sys.mjs"
);
const { TabManager } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/TabManager.sys.mjs"
);

const FIRST_URL = "https://example.com/document-builder.sjs?html=first";
const SECOND_URL = "https://example.com/document-builder.sjs?html=second";
const THIRD_URL = "https://example.com/document-builder.sjs?html=third";

const FIRST_COOP_URL =
  "https://example.com/document-builder.sjs?headers=Cross-Origin-Opener-Policy:same-origin&html=first_coop";
const SECOND_COOP_URL =
  "https://example.net/document-builder.sjs?headers=Cross-Origin-Opener-Policy:same-origin&html=second_coop";

add_task(async function test_simpleNavigation() {
  const events = [];
  const onEvent = (name, data) => events.push({ name, data });

  const navigationManager = new NavigationManager();
  navigationManager.on("navigation-started", onEvent);
  navigationManager.on("navigation-stopped", onEvent);

  const tab = addTab(gBrowser, FIRST_URL);
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

  await loadURL(browser, SECOND_URL);

  const firstNavigation = navigationManager.getNavigationForBrowsingContext(
    browser.browsingContext
  );
  assertNavigation(firstNavigation, SECOND_URL);

  is(events.length, 2, "Two events recorded");
  assertNavigationEvents(
    events,
    SECOND_URL,
    firstNavigation.navigationId,
    navigableId
  );

  await loadURL(browser, THIRD_URL);

  const secondNavigation = navigationManager.getNavigationForBrowsingContext(
    browser.browsingContext
  );
  assertNavigation(secondNavigation, THIRD_URL);
  assertUniqueNavigationIds(firstNavigation, secondNavigation);

  is(events.length, 4, "Two new events recorded");
  assertNavigationEvents(
    events,
    THIRD_URL,
    secondNavigation.navigationId,
    navigableId
  );

  navigationManager.stopMonitoring();

  // Navigate again to the first URL
  await loadURL(browser, FIRST_URL);
  is(events.length, 4, "No new event recorded");
  is(
    navigationManager.getNavigationForBrowsingContext(browser.browsingContext),
    null,
    "No navigation recorded"
  );

  navigationManager.off("navigation-started", onEvent);
  navigationManager.off("navigation-stopped", onEvent);
});

add_task(async function test_loadTwoTabsSimultaneously() {
  const events = [];
  const onEvent = (name, data) => events.push({ name, data });

  const navigationManager = new NavigationManager();
  navigationManager.on("navigation-started", onEvent);
  navigationManager.on("navigation-stopped", onEvent);

  navigationManager.startMonitoring();

  info("Add two tabs simultaneously");
  const tab1 = addTab(gBrowser, FIRST_URL);
  const browser1 = tab1.linkedBrowser;
  const navigableId1 = TabManager.getIdForBrowser(browser1);
  const onLoad1 = BrowserTestUtils.browserLoaded(browser1, false, FIRST_URL);

  const tab2 = addTab(gBrowser, SECOND_URL);
  const browser2 = tab2.linkedBrowser;
  const navigableId2 = TabManager.getIdForBrowser(browser2);
  const onLoad2 = BrowserTestUtils.browserLoaded(browser2, false, SECOND_URL);

  info("Wait for the tabs to load");
  await Promise.all([onLoad1, onLoad2]);

  is(events.length, 4, "Recorded 4 navigation events");

  info("Check navigation monitored for tab1");
  const nav1 = navigationManager.getNavigationForBrowsingContext(
    browser1.browsingContext
  );
  assertNavigation(nav1, FIRST_URL);
  assertNavigationEvents(events, FIRST_URL, nav1.navigationId, navigableId1);

  info("Check navigation monitored for tab2");
  const nav2 = navigationManager.getNavigationForBrowsingContext(
    browser2.browsingContext
  );
  assertNavigation(nav2, SECOND_URL);
  assertNavigationEvents(events, SECOND_URL, nav2.navigationId, navigableId2);
  assertUniqueNavigationIds(nav1, nav2);

  info("Reload the two tabs simultaneously");
  await Promise.all([
    BrowserTestUtils.reloadTab(tab1),
    BrowserTestUtils.reloadTab(tab2),
  ]);

  is(events.length, 8, "Recorded 8 navigation events");

  info("Check the second navigation for tab1");
  const nav3 = navigationManager.getNavigationForBrowsingContext(
    browser1.browsingContext
  );
  assertNavigation(nav3, FIRST_URL);
  assertNavigationEvents(events, FIRST_URL, nav3.navigationId, navigableId1);

  info("Check the second navigation monitored for tab2");
  const nav4 = navigationManager.getNavigationForBrowsingContext(
    browser2.browsingContext
  );
  assertNavigation(nav4, SECOND_URL);
  assertNavigationEvents(events, SECOND_URL, nav4.navigationId, navigableId2);
  assertUniqueNavigationIds(nav1, nav2, nav3, nav4);

  navigationManager.off("navigation-started", onEvent);
  navigationManager.off("navigation-stopped", onEvent);
  navigationManager.stopMonitoring();
});

add_task(async function test_loadPageWithIframes() {
  const events = [];
  const onEvent = (name, data) => events.push({ name, data });

  const navigationManager = new NavigationManager();
  navigationManager.on("navigation-started", onEvent);
  navigationManager.on("navigation-stopped", onEvent);

  navigationManager.startMonitoring();

  info("Add a tab with iframes");
  const testUrl = createTestPageWithFrames();
  const tab = addTab(gBrowser, testUrl);
  const browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser, false, testUrl);

  is(events.length, 8, "Recorded 8 navigation events");
  const contexts = browser.browsingContext.getAllBrowsingContextsInSubtree();

  const navigations = [];
  for (const context of contexts) {
    const navigation =
      navigationManager.getNavigationForBrowsingContext(context);
    const navigable = TabManager.getIdForBrowsingContext(context);

    const url = context.currentWindowGlobal.documentURI.spec;
    assertNavigation(navigation, url);
    assertNavigationEvents(events, url, navigation.navigationId, navigable);
    navigations.push(navigation);
  }
  assertUniqueNavigationIds(...navigations);

  await BrowserTestUtils.reloadTab(tab);

  is(events.length, 16, "Recorded 8 additional navigation events");
  const newContexts = browser.browsingContext.getAllBrowsingContextsInSubtree();

  for (const context of newContexts) {
    const navigation =
      navigationManager.getNavigationForBrowsingContext(context);
    const navigable = TabManager.getIdForBrowsingContext(context);

    const url = context.currentWindowGlobal.documentURI.spec;
    assertNavigation(navigation, url);
    assertNavigationEvents(events, url, navigation.navigationId, navigable);
    navigations.push(navigation);
  }
  assertUniqueNavigationIds(...navigations);

  navigationManager.off("navigation-started", onEvent);
  navigationManager.off("navigation-stopped", onEvent);
  navigationManager.stopMonitoring();
});

add_task(async function test_loadPageWithCoop() {
  const tab = addTab(gBrowser, FIRST_COOP_URL);
  const browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser, false, FIRST_COOP_URL);

  const events = [];
  const onEvent = (name, data) => events.push({ name, data });

  const navigationManager = new NavigationManager();
  navigationManager.on("navigation-started", onEvent);
  navigationManager.on("navigation-stopped", onEvent);

  navigationManager.startMonitoring();

  const navigableId = TabManager.getIdForBrowser(browser);
  await loadURL(browser, SECOND_COOP_URL);

  const coopNavigation = navigationManager.getNavigationForBrowsingContext(
    browser.browsingContext
  );
  assertNavigation(coopNavigation, SECOND_COOP_URL);

  is(events.length, 2, "Two events recorded");
  assertNavigationEvents(
    events,
    SECOND_COOP_URL,
    coopNavigation.navigationId,
    navigableId
  );

  navigationManager.off("navigation-started", onEvent);
  navigationManager.off("navigation-stopped", onEvent);
  navigationManager.stopMonitoring();
});

add_task(async function test_sameDocumentNavigation() {
  const events = [];
  const onEvent = (name, data) => events.push({ name, data });

  const navigationManager = new NavigationManager();
  navigationManager.on("navigation-started", onEvent);
  navigationManager.on("location-changed", onEvent);
  navigationManager.on("navigation-stopped", onEvent);

  const url = "https://example.com/document-builder.sjs?html=test";
  const tab = addTab(gBrowser, url);
  const browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);

  navigationManager.startMonitoring();
  const navigableId = TabManager.getIdForBrowser(browser);

  is(events.length, 0, "No event recorded");

  info("Perform a same-document navigation");
  let onLocationChanged = navigationManager.once("location-changed");
  BrowserTestUtils.startLoadingURIString(browser, url + "#hash");
  await onLocationChanged;

  const hashNavigation = navigationManager.getNavigationForBrowsingContext(
    browser.browsingContext
  );
  is(events.length, 1, "Recorded 1 navigation event");
  assertNavigationEvents(
    events,
    url + "#hash",
    hashNavigation.navigationId,
    navigableId,
    true
  );

  // Navigate from `url + "#hash"` to `url`, this will trigger a regular
  // navigation and we can use `loadURL` to properly wait for the navigation to
  // complete.
  info("Perform a regular navigation");
  await loadURL(browser, url);

  const regularNavigation = navigationManager.getNavigationForBrowsingContext(
    browser.browsingContext
  );
  is(events.length, 3, "Recorded 2 additional navigation events");
  assertNavigationEvents(
    events,
    url,
    regularNavigation.navigationId,
    navigableId
  );

  info("Perform another same-document navigation");
  onLocationChanged = navigationManager.once("location-changed");
  BrowserTestUtils.startLoadingURIString(browser, url + "#foo");
  await onLocationChanged;

  const otherHashNavigation = navigationManager.getNavigationForBrowsingContext(
    browser.browsingContext
  );

  is(events.length, 4, "Recorded 1 additional navigation event");

  info("Perform a same-hash navigation");
  onLocationChanged = navigationManager.once("location-changed");
  BrowserTestUtils.startLoadingURIString(browser, url + "#foo");
  await onLocationChanged;

  const sameHashNavigation = navigationManager.getNavigationForBrowsingContext(
    browser.browsingContext
  );

  is(events.length, 5, "Recorded 1 additional navigation event");
  assertNavigationEvents(
    events,
    url + "#foo",
    sameHashNavigation.navigationId,
    navigableId,
    true
  );

  assertUniqueNavigationIds([
    hashNavigation,
    regularNavigation,
    otherHashNavigation,
    sameHashNavigation,
  ]);

  navigationManager.off("navigation-started", onEvent);
  navigationManager.off("location-changed", onEvent);
  navigationManager.off("navigation-stopped", onEvent);

  navigationManager.stopMonitoring();
});

add_task(async function test_startNavigationAndCloseTab() {
  const events = [];
  const onEvent = (name, data) => events.push({ name, data });

  const navigationManager = new NavigationManager();
  navigationManager.on("navigation-started", onEvent);
  navigationManager.on("navigation-stopped", onEvent);

  const tab = addTab(gBrowser, FIRST_URL);
  const browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);

  navigationManager.startMonitoring();
  loadURL(browser, SECOND_URL);
  gBrowser.removeTab(tab);

  // On top of the assertions below, the test also validates that there is no
  // unhandled promise rejection related to handling the navigation-started event
  // for the destroyed browsing context.
  is(events.length, 0, "No event was received");
  is(
    navigationManager.getNavigationForBrowsingContext(browser.browsingContext),
    null,
    "No navigation was recorded for the destroyed tab"
  );
  navigationManager.stopMonitoring();

  navigationManager.off("navigation-started", onEvent);
  navigationManager.off("navigation-stopped", onEvent);
});
