/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

class Delegate {
  actor(window) {
    return window.windowGlobalChild.getActor("AppTestDelegate");
  }

  clickPageAction(window, extension) {
    return this.actor(window).sendQuery("clickPageAction", {
      extensionId: extension.id,
    });
  }

  clickBrowserAction(window, extension) {
    return this.actor(window).sendQuery("clickBrowserAction", {
      extensionId: extension.id,
    });
  }

  closeBrowserAction(window, extension) {
    return this.actor(window).sendQuery("closeBrowserAction", {
      extensionId: extension.id,
    });
  }

  closePageAction(window, extension) {
    return this.actor(window).sendQuery("closePageAction", {
      extensionId: extension.id,
    });
  }

  awaitExtensionPanel(window, extension) {
    return this.actor(window).sendQuery("awaitExtensionPanel", {
      extensionId: extension.id,
    });
  }

  async openNewForegroundTab(window, url, waitForLoad = true) {
    const tabId = await this.actor(window).sendQuery("openNewForegroundTab", {
      url,
      waitForLoad,
    });
    return { id: tabId };
  }

  removeTab(window, tab) {
    return this.actor(window).sendQuery("removeTab", { tabId: tab.id });
  }
}

export var AppTestDelegate = new Delegate();
