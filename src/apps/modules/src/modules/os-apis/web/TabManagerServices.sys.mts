/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * TabManagerService - A service for managing and interacting with browser tabs.
 */

const { E10SUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/E10SUtils.sys.mjs",
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

// Global sets to prevent garbage collection of active components
const TAB_MANAGER_ACTOR_SETS: Set<XULBrowserElement> = new Set();
const PROGRESS_LISTENERS = new Set();

/**
 * A utility function to get the most recent browser window.
 * @returns The browser window.
 */
function getBrowserWindow() {
  const window = Services.wm.getMostRecentWindow("navigator:browser");
  if (!window) {
    throw new Error("Could not get browser window.");
  }
  return window;
}

/**
 * TabManager class that manages browser tabs for automation and interaction.
 */
class TabManager {
  // Map to store tab instances with their unique IDs
  private _browserInstances: Map<
    string,
    { tab: object; browser: XULBrowserElement }
  > = new Map();

  constructor() {}

  // --- Private Helper Methods ---

  /**
   * Retrieves an instance by its ID, throwing an error if not found.
   */
  private _getInstance(instanceId: string) {
    const instance = this._browserInstances.get(instanceId);
    if (!instance) {
      throw new Error(`Instance not found for ID: ${instanceId}`);
    }
    return instance;
  }

  /**
   * Waits for a tab to finish loading a specific URL.
   */
  private _waitForLoad(browser: XULBrowserElement, url: string): Promise<void> {
    return new Promise((resolve) => {
      const uri = Services.io.newURI(url);
      const progressListener = {
        onLocationChange(
          progress: any,
          _request: any,
          location: any,
          flags: any,
        ) {
          if (
            !progress.isTopLevel ||
            flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT ||
            (location.spec === "about:blank" && uri.spec !== "about:blank")
          ) {
            return;
          }

          PROGRESS_LISTENERS.delete(progressListener);
          browser.webProgress.removeProgressListener(progressListener);
          resolve();
        },
        QueryInterface: ChromeUtils.generateQI([
          "nsIWebProgressListener",
          "nsISupportsWeakReference",
        ]),
      };
      PROGRESS_LISTENERS.add(progressListener);
      browser.webProgress.addProgressListener(
        progressListener,
        Ci.nsIWebProgress.NOTIFY_LOCATION,
      );
    });
  }

  /**
   * Sends a query to the NRWebScraper content actor and returns the result.
   */
  private async _queryActor<T>(
    instanceId: string,
    query: string,
    data?: object,
  ): Promise<T | null> {
    try {
      const { browser } = this._getInstance(instanceId);
      const actor = browser.browsingContext?.currentWindowGlobal?.getActor(
        "NRWebScraper",
      );

      if (!actor) {
        console.warn(`NRWebScraper actor not found for instance ${instanceId}`);
        return null;
      }
      return await actor.sendQuery(query, data);
    } catch (e) {
      console.error(`Error in actor query '${query}':`, e);
      return null;
    }
  }

  // --- Public API Methods ---

  public async createInstance(
    url: string,
    options?: { inBackground?: boolean },
  ): Promise<string> {
    const win = getBrowserWindow() as Window;
    const gBrowser = win.gBrowser;

    const tab = await gBrowser.addTab(url, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      skipAnimation: true,
      inBackground: options?.inBackground ?? false,
    });

    const browser = tab.linkedBrowser;
    await this._waitForLoad(browser, url);

    const instanceId = crypto.randomUUID();
    this._browserInstances.set(instanceId, { tab, browser });
    TAB_MANAGER_ACTOR_SETS.add(browser);

    return instanceId;
  }

  public async attachToTab(browserId: string): Promise<string | null> {
    const win = getBrowserWindow() as Window;
    const gBrowser = win.gBrowser;

    const targetTab = gBrowser.tabs.find(
      (tab: any) => tab.linkedBrowser.browserId.toString() === browserId,
    );

    if (!targetTab) {
      return null;
    }

    const browser = targetTab.linkedBrowser;
    const instanceId = crypto.randomUUID();
    this._browserInstances.set(instanceId, { tab: targetTab, browser });
    TAB_MANAGER_ACTOR_SETS.add(browser);

    return instanceId;
  }

  public async listTabs(): Promise<any[]> {
    const win = getBrowserWindow() as Window;
    const gBrowser = win.gBrowser;

    return gBrowser.tabs.map((tab: any) => ({
      browserId: tab.linkedBrowser.browserId.toString(),
      title: tab.label,
      url: tab.linkedBrowser.currentURI.spec,
      selected: tab.selected,
      pinned: tab.pinned,
    }));
  }

  public async getInstanceInfo(instanceId: string): Promise<any | null> {
    const { tab, browser } = this._getInstance(instanceId) as {
      tab: any;
      browser: any;
    };
    const win = getBrowserWindow() as Window;
    const gBrowser = win.gBrowser;

    const [html, screenshot] = await Promise.all([
      this.getHTML(instanceId),
      this.takeScreenshot(instanceId),
    ]);

    return {
      browserId: browser.browserId.toString(),
      url: browser.currentURI.spec,
      title: browser.contentTitle,
      favIconUrl: gBrowser.getIcon(tab),
      isActive: tab.selected,
      isPinned: tab.pinned,
      isLoading: browser.webProgress.isLoadingDocument,
      html,
      screenshot,
    };
  }

  public destroyInstance(instanceId: string): void {
    const { tab, browser } = this._getInstance(instanceId);
    const win = getBrowserWindow() as Window;
    win.gBrowser.removeTab(tab);
    this._browserInstances.delete(instanceId);
    TAB_MANAGER_ACTOR_SETS.delete(browser);
  }

  public navigate(instanceId: string, url: string): Promise<void> {
    const { browser } = this._getInstance(instanceId);
    const principal = Services.scriptSecurityManager.getSystemPrincipal();

    const oa = E10SUtils.predictOriginAttributes({ browser });
    const loadURIOptions = {
      triggeringPrincipal: principal,
      remoteType: E10SUtils.getRemoteTypeForURI(
        url,
        true,
        false,
        E10SUtils.DEFAULT_REMOTE_TYPE,
        null,
        oa,
      ),
    };

    browser.loadURI(Services.io.newURI(url), loadURIOptions);
    return this._waitForLoad(browser, url);
  }

  public getURI(instanceId: string): Promise<string> {
    const { browser } = this._getInstance(instanceId);
    return Promise.resolve(browser.browsingContext.currentURI.spec);
  }

  public getHTML(instanceId: string): Promise<string | null> {
    return this._queryActor<string>(instanceId, "WebScraper:GetHTML");
  }

  public getElement(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    return this._queryActor<string>(instanceId, "WebScraper:GetElement", {
      selector,
    });
  }

  public getElementText(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    return this._queryActor<string>(instanceId, "WebScraper:GetElementText", {
      selector,
    });
  }

  public clickElement(
    instanceId: string,
    selector: string,
  ): Promise<boolean | null> {
    return this._queryActor<boolean>(instanceId, "WebScraper:ClickElement", {
      selector,
    });
  }

  public waitForElement(
    instanceId: string,
    selector: string,
    timeout = 5000,
  ): Promise<boolean | null> {
    return this._queryActor<boolean>(instanceId, "WebScraper:WaitForElement", {
      selector,
      timeout,
    });
  }

  public executeScript(
    instanceId: string,
    script: string,
  ): Promise<unknown | null> {
    return this._queryActor(instanceId, "WebScraper:ExecuteScript", { script });
  }

  public takeScreenshot(instanceId: string): Promise<string | null> {
    return this._queryActor<string>(instanceId, "WebScraper:TakeScreenshot");
  }

  public takeElementScreenshot(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    return this._queryActor<string>(
      instanceId,
      "WebScraper:TakeElementScreenshot",
      { selector },
    );
  }

  public takeFullPageScreenshot(instanceId: string): Promise<string | null> {
    return this._queryActor<string>(
      instanceId,
      "WebScraper:TakeFullPageScreenshot",
    );
  }

  public takeRegionScreenshot(
    instanceId: string,
    rect?: object,
  ): Promise<string | null> {
    return this._queryActor<string>(
      instanceId,
      "WebScraper:TakeRegionScreenshot",
      { rect },
    );
  }

  public fillForm(
    instanceId: string,
    formData: object,
  ): Promise<boolean | null> {
    return this._queryActor<boolean>(instanceId, "WebScraper:FillForm", {
      formData,
    });
  }

  public wait(ms: number): Promise<void> {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }
}

// Export a singleton instance of the TabManager service
export const TabManagerServices = new TabManager();
