"use strict";

Cu.import("resource://gre/modules/ExtensionManagement.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function createWindowWithAddonId(addonId) {
  let baseURI = Services.io.newURI("about:blank", null, null);
  let originAttributes = {addonId};
  let principal = Services.scriptSecurityManager
                          .createCodebasePrincipal(baseURI, originAttributes);
  let chromeNav = Services.appShell.createWindowlessBrowser(true);
  let interfaceRequestor = chromeNav.QueryInterface(Ci.nsIInterfaceRequestor);
  let docShell = interfaceRequestor.getInterface(Ci.nsIDocShell);
  docShell.createAboutBlankContentViewer(principal);

  return {chromeNav, window: docShell.contentViewer.DOMDocument.defaultView};
}

add_task(function* test_eventpages() {
  const {getAPILevelForWindow, getAddonIdForWindow} = ExtensionManagement;
  const {NO_PRIVILEGES, FULL_PRIVILEGES} = ExtensionManagement.API_LEVELS;
  const FAKE_ADDON_ID = "fakeAddonId";
  const OTHER_ADDON_ID = "otherFakeAddonId";
  const EMPTY_ADDON_ID = "";

  let fakeAddonId = createWindowWithAddonId(FAKE_ADDON_ID);
  equal(getAddonIdForWindow(fakeAddonId.window), FAKE_ADDON_ID,
        "the window has the expected addonId");

  let apiLevel = getAPILevelForWindow(fakeAddonId.window, FAKE_ADDON_ID);
  equal(apiLevel, FULL_PRIVILEGES,
        "apiLevel for the window with the right addonId should be FULL_PRIVILEGES");

  apiLevel = getAPILevelForWindow(fakeAddonId.window, OTHER_ADDON_ID);
  equal(apiLevel, NO_PRIVILEGES,
        "apiLevel for the window with a different addonId should be NO_PRIVILEGES");

  fakeAddonId.chromeNav.close();

  // NOTE: check that window with an empty addon Id (which are window that are
  // not Extensions pages) always get no WebExtensions APIs.
  let emptyAddonId = createWindowWithAddonId(EMPTY_ADDON_ID);
  equal(getAddonIdForWindow(emptyAddonId.window), EMPTY_ADDON_ID,
        "the window has the expected addonId");

  apiLevel = getAPILevelForWindow(emptyAddonId.window, EMPTY_ADDON_ID);
  equal(apiLevel, NO_PRIVILEGES,
        "apiLevel for empty addonId should be NO_PRIVILEGES");

  apiLevel = getAPILevelForWindow(emptyAddonId.window, OTHER_ADDON_ID);
  equal(apiLevel, NO_PRIVILEGES,
        "apiLevel for an 'empty addonId' window should be always NO_PRIVILEGES");

  emptyAddonId.chromeNav.close();
});
