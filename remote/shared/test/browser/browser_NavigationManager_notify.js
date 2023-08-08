/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { NavigationManager, notifyNavigationStarted, notifyNavigationStopped } =
  ChromeUtils.importESModule(
    "chrome://remote/content/shared/NavigationManager.sys.mjs"
  );
const { TabManager } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/TabManager.sys.mjs"
);

const FIRST_URL = "https://example.com/document-builder.sjs?html=first";
const SECOND_URL = "https://example.com/document-builder.sjs?html=second";

add_task(async function test_notifyNavigationStartedStopped() {
  const tab = addTab(gBrowser, FIRST_URL);
  const browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser, false, FIRST_URL);

  const events = [];
  const onEvent = (name, data) => events.push({ name, data });

  const navigationManager = new NavigationManager();
  navigationManager.on("navigation-started", onEvent);
  navigationManager.on("navigation-stopped", onEvent);

  navigationManager.startMonitoring();

  const navigableId = TabManager.getIdForBrowser(browser);

  info("Programmatically start a navigation");
  const startedNavigation = notifyNavigationStarted({
    contextDetails: {
      context: browser.browsingContext,
    },
    url: SECOND_URL,
  });

  const navigation = navigationManager.getNavigationForBrowsingContext(
    browser.browsingContext
  );
  assertNavigation(navigation, SECOND_URL);

  is(
    startedNavigation,
    navigation,
    "notifyNavigationStarted returned the expected navigation"
  );
  is(events.length, 1, "Only one event recorded");

  info("Attempt to start a navigation while another one is in progress");
  const alreadyStartedNavigation = notifyNavigationStarted({
    contextDetails: {
      context: browser.browsingContext,
    },
    url: SECOND_URL,
  });
  is(
    alreadyStartedNavigation,
    navigation,
    "notifyNavigationStarted returned the ongoing navigation"
  );
  is(events.length, 1, "Still only one event recorded");

  info("Programmatically stop the navigation");
  const stoppedNavigation = notifyNavigationStopped({
    contextDetails: {
      context: browser.browsingContext,
    },
    url: SECOND_URL,
  });
  is(
    stoppedNavigation,
    navigation,
    "notifyNavigationStopped returned the expected navigation"
  );

  is(events.length, 2, "Two events recorded");
  assertNavigationEvents(
    events,
    SECOND_URL,
    navigation.navigationId,
    navigableId
  );

  info("Attempt to stop an already stopped navigation");
  const alreadyStoppedNavigation = notifyNavigationStopped({
    contextDetails: {
      context: browser.browsingContext,
    },
    url: SECOND_URL,
  });
  is(
    alreadyStoppedNavigation,
    navigation,
    "notifyNavigationStopped returned the already stopped navigation"
  );
  is(events.length, 2, "Still only two events recorded");

  navigationManager.off("navigation-started", onEvent);
  navigationManager.off("navigation-stopped", onEvent);
  navigationManager.stopMonitoring();
});

add_task(async function test_notifyNavigationWithContextDetails() {
  const tab = addTab(gBrowser, FIRST_URL);
  const browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser, false, FIRST_URL);

  const events = [];
  const onEvent = (name, data) => events.push({ name, data });

  const navigationManager = new NavigationManager();
  navigationManager.on("navigation-started", onEvent);
  navigationManager.on("navigation-stopped", onEvent);

  navigationManager.startMonitoring();

  const navigableId = TabManager.getIdForBrowser(browser);

  info("Programmatically start a navigation using browsing context details");
  const startedNavigation = notifyNavigationStarted({
    contextDetails: {
      browsingContextId: browser.browsingContext.id,
      browserId: browser.browsingContext.browserId,
      isTopBrowsingContext: browser.browsingContext.parent === null,
    },
    url: SECOND_URL,
  });

  const navigation = navigationManager.getNavigationForBrowsingContext(
    browser.browsingContext
  );
  assertNavigation(navigation, SECOND_URL);

  is(
    startedNavigation,
    navigation,
    "notifyNavigationStarted returned the expected navigation"
  );
  is(events.length, 1, "Only one event recorded");

  info("Programmatically stop the navigation using browsing context details");
  const stoppedNavigation = notifyNavigationStopped({
    contextDetails: {
      browsingContextId: browser.browsingContext.id,
      browserId: browser.browsingContext.browserId,
      isTopBrowsingContext: browser.browsingContext.parent === null,
    },
    url: SECOND_URL,
  });
  is(
    stoppedNavigation,
    navigation,
    "notifyNavigationStopped returned the expected navigation"
  );

  is(events.length, 2, "Two events recorded");
  assertNavigationEvents(
    events,
    SECOND_URL,
    navigation.navigationId,
    navigableId
  );

  navigationManager.off("navigation-started", onEvent);
  navigationManager.off("navigation-stopped", onEvent);
  navigationManager.stopMonitoring();
});
