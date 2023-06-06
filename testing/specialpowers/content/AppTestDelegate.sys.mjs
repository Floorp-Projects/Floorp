/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file offers the test helpers that are directly exposed to mochitests.
 * Their implementations are in app-specific "AppUiTestDelegate.sys.mjs" files.
 *
 * For documentation, see AppTestDelegateParent.sys.mjs.
 * For documentation on the methods of AppUiTestDelegate, see below.
 */

class AppTestDelegateImplementation {
  actor(window) {
    return window.windowGlobalChild.getActor("AppTestDelegate");
  }

  /**
   * Click on the pageAction button, to open its popup, or to trigger
   * pageAction.onClicked if there is no popup.
   */
  clickPageAction(window, extension) {
    return this.actor(window).sendQuery("clickPageAction", {
      extensionId: extension.id,
    });
  }

  /**
   * Click on the browserAction button, to open its popup, or to trigger
   * browserAction.onClicked if there is no popup.
   */
  clickBrowserAction(window, extension) {
    return this.actor(window).sendQuery("clickBrowserAction", {
      extensionId: extension.id,
    });
  }

  /**
   * Close the browserAction popup, if any.
   * Do not use this for pageAction popups, use closePageAction instead.
   */
  closeBrowserAction(window, extension) {
    return this.actor(window).sendQuery("closeBrowserAction", {
      extensionId: extension.id,
    });
  }

  /**
   * Close the pageAction popup, if any.
   * Do not use this for browserAction popups, use closeBrowserAction instead.
   */
  closePageAction(window, extension) {
    return this.actor(window).sendQuery("closePageAction", {
      extensionId: extension.id,
    });
  }

  /**
   * Wait for the pageAction or browserAction/action popup panel to open.
   * This must be called BEFORE any attempt to open the popup.
   */
  awaitExtensionPanel(window, extension) {
    return this.actor(window).sendQuery("awaitExtensionPanel", {
      extensionId: extension.id,
    });
  }

  /**
   * Open a tab with the given url in the given window.
   * Returns an opaque object that can be passed to AppTestDelegate.removeTab.
   */
  async openNewForegroundTab(window, url, waitForLoad = true) {
    const tabId = await this.actor(window).sendQuery("openNewForegroundTab", {
      url,
      waitForLoad,
    });
    // Note: this id is only meaningful to AppTestDelegate and independent of
    // any other concept of "tab id".
    return { id: tabId };
  }

  /**
   * Close a tab opened by AppTestDelegate.openNewForegroundTab.
   */
  removeTab(window, tab) {
    return this.actor(window).sendQuery("removeTab", { tabId: tab.id });
  }
}

export var AppTestDelegate = new AppTestDelegateImplementation();
