/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * TabManagerService - A service for managing and interacting with browser tabs.
 *
 * This module provides functionality to:
 * - Create new browser tabs
 * - Attach to existing tabs
 * - Navigate tabs to URLs and wait for page loads
 * - Interact with tab content (get HTML, click elements, etc.)
 * - Manage multiple tabs with unique IDs
 * - Clean up resources when tabs are closed
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
 *
 * This class provides a high-level interface for creating, managing, and
 * interacting with regular browser tabs.
 */
class TabManager {
  // Map to store tab instances with their unique IDs
  private _browserInstances: Map<
    string,
    { tab: object; browser: XULBrowserElement }
  > = new Map();

  constructor() {}

  /**
   * Creates a new browser tab and navigates to the specified URL.
   *
   * @param url - The URL to open in the new tab.
   * @param options - Options for creating the new tab.
   * @returns Promise<string> - A unique identifier for the created tab instance.
   */
  public async createInstance(
    url: string,
    options?: { inBackground?: boolean },
  ): Promise<string> {
    const browserWin = getBrowserWindow() as Window;
    const gBrowser = browserWin.gBrowser;

    const tab = await gBrowser.addTab(url, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      skipAnimation: true,
      inBackground: options?.inBackground ?? false,
    });

    const browser = tab.linkedBrowser;

    // Wait for the tab to finish loading
    await new Promise((resolve) => {
      const uri = Services.io.newURI(url);
      const progressListener = {
        onLocationChange(progress, _request, location, flags) {
          // Ignore inner-frame events
          if (!progress.isTopLevel) {
            return;
          }
          // Ignore events that don't change the document
          if (flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) {
            return;
          }
          // Ignore the initial about:blank, unless about:blank is requested
          if (location.spec == "about:blank" && uri.spec != "about:blank") {
            return;
          }

          PROGRESS_LISTENERS.delete(progressListener);
          browser.webProgress.removeProgressListener(progressListener);
          resolve(undefined);
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

    const instanceId = crypto.randomUUID();
    this._browserInstances.set(instanceId, { tab, browser });
    TAB_MANAGER_ACTOR_SETS.add(browser);

    return instanceId;
  }

  /**
   * Attaches to an existing tab using its browserId.
   *
   * @param browserId - The browserId of the tab to attach to.
   * @returns Promise<string | null> - A unique identifier for the tab instance, or null if not found.
   */
  public async attachToTab(browserId: string): Promise<string | null> {
    const browserWin = getBrowserWindow() as Window;
    const gBrowser = browserWin.gBrowser;

    let targetTab = null;
    for (const tab of gBrowser.tabs) {
      if (tab.linkedBrowser.browserId.toString() === browserId) {
        targetTab = tab;
        break;
      }
    }

    if (!targetTab) {
      return null;
    }

    const browser = targetTab.linkedBrowser;
    const instanceId = crypto.randomUUID();
    this._browserInstances.set(instanceId, { tab: targetTab, browser });
    TAB_MANAGER_ACTOR_SETS.add(browser);

    return instanceId;
  }

  /**
   * Lists all open tabs with their session IDs, titles, and URLs.
   *
   * @returns Promise<Array<{browserId: string, title: string, url: string}>>
   */
  public async listTabs(): Promise<
    { browserId: string; title: string; url: string }[]
  > {
    const browserWin = getBrowserWindow() as Window;
    const gBrowser = browserWin.gBrowser;

    const tabsInfo = [];
    for (const tab of gBrowser.tabs) {
      tabsInfo.push({
        browserId: tab.linkedBrowser.browserId.toString(),
        title: tab.label,
        url: tab.linkedBrowser.currentURI.spec,
      });
    }
    return tabsInfo;
  }

  /**
   * Destroys a tab instance and cleans up associated resources.
   *
   * @param instanceId - The unique identifier of the tab instance to destroy.
   */
  public destroyInstance(instanceId: string): void {
    const instance = this._browserInstances.get(instanceId);
    if (instance) {
      const win = getBrowserWindow() as Window;
      win.gBrowser.removeTab(instance.tab);
      this._browserInstances.delete(instanceId);
      TAB_MANAGER_ACTOR_SETS.delete(instance.browser);
    }
  }

  /**
   * Navigates a browser instance to a specified URL and waits for the page to load
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param url - The URL to navigate to
   * @returns Promise<void> - Resolves when the page has finished loading
   * @throws Error - If the browser instance is not found
   */
  public navigate(instanceId: string, url: string): Promise<void> {
    const instance = this._browserInstances.get(instanceId);
    if (!instance) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }
    const { browser } = instance;

    const principal = Services.scriptSecurityManager.getSystemPrincipal();
    return new Promise((resolve) => {
      const oa = E10SUtils.predictOriginAttributes({
        browser,
      });
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

      const uri = Services.io.newURI(url);

      browser.loadURI(uri, loadURIOptions);
      const { webProgress } = browser;

      const progressListener = {
        onLocationChange(progress, _request, location, flags) {
          // Ignore inner-frame events
          if (!progress.isTopLevel) {
            return;
          }
          // Ignore events that don't change the document
          if (flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) {
            return;
          }
          // Ignore the initial about:blank, unless about:blank is requested
          if (location.spec == "about:blank" && uri.spec != "about:blank") {
            return;
          }

          PROGRESS_LISTENERS.delete(progressListener);
          webProgress.removeProgressListener(progressListener);
          resolve();
        },
        QueryInterface: ChromeUtils.generateQI([
          "nsIWebProgressListener",
          "nsISupportsWeakReference",
        ]),
      };
      PROGRESS_LISTENERS.add(progressListener);
      webProgress.addProgressListener(
        progressListener,
        Ci.nsIWebProgress.NOTIFY_LOCATION,
      );
    });
  }

  public getURI(instanceId: string): Promise<string | null> {
    const instance = this._browserInstances.get(instanceId);
    if (!instance) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }
    const { browser } = instance;

    return Promise.resolve(browser.browsingContext.currentURI.spec);
  }

  /**
   * Retrieves the HTML content from a browser instance using the NRWebScraper actor
   *
   * @param instanceId - The unique identifier of the browser instance
   * @returns Promise<string | null> - The HTML content as a string, or null if unavailable
   * @throws Error - If the browser instance is not found
   */
  public async getHTML(instanceId: string): Promise<string | null> {
    const instance = this._browserInstances.get(instanceId);
    if (!instance) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }
    const { browser } = instance;

    try {
      const actor = browser.browsingContext?.currentWindowGlobal?.getActor(
        "NRWebScraper",
      );

      if (!actor) {
        return null;
      }

      return await actor.sendQuery("WebScraper:GetHTML");
    } catch (e) {
      console.error("Error getting HTML:", e);
      return null;
    }
  }

  /**
   * Retrieves a specific element from a browser instance using the NRWebScraper actor
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target element
   * @returns Promise<string | null> - The element's HTML content, or null if not found
   * @throws Error - If the browser instance is not found
   */
  public async getElement(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    const instance = this._browserInstances.get(instanceId);
    if (!instance) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }
    const { browser } = instance;

    const actor = browser.browsingContext?.currentWindowGlobal?.getActor(
      "NRWebScraper",
    );

    if (!actor) {
      return null;
    }

    return await actor.sendQuery("WebScraper:GetElement", { selector });
  }

  /**
   * Retrieves the text content of an element from a browser instance
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target element
   * @returns Promise<string | null> - The element's text content, or null if not found
   * @throws Error - If the browser instance is not found
   */
  public async getElementText(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    const instance = this._browserInstances.get(instanceId);
    if (!instance) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }
    const { browser } = instance;

    const actor = browser.browsingContext?.currentWindowGlobal?.getActor(
      "NRWebScraper",
    );

    if (!actor) {
      return null;
    }

    return await actor.sendQuery("WebScraper:GetElementText", { selector });
  }

  /**
   * Clicks on an element in a browser instance
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target element
   * @returns Promise<boolean> - True if click was successful, false otherwise
   * @throws Error - If the browser instance is not found
   */
  public async clickElement(
    instanceId: string,
    selector: string,
  ): Promise<boolean> {
    const instance = this._browserInstances.get(instanceId);
    if (!instance) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }
    const { browser } = instance;

    const actor = browser.browsingContext?.currentWindowGlobal?.getActor(
      "NRWebScraper",
    );

    if (!actor) {
      return false;
    }

    return await actor.sendQuery("WebScraper:ClickElement", { selector });
  }

  /**
   * Waits for an element to appear in a browser instance
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target element
   * @param timeout - Maximum time to wait in milliseconds (default: 5000)
   * @returns Promise<boolean> - True if element was found, false if timeout reached
   * @throws Error - If the browser instance is not found
   */
  public async waitForElement(
    instanceId: string,
    selector: string,
    timeout = 5000,
  ): Promise<boolean> {
    const instance = this._browserInstances.get(instanceId);
    if (!instance) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }
    const { browser } = instance;

    const actor = browser.browsingContext?.currentWindowGlobal?.getActor(
      "NRWebScraper",
    );

    if (!actor) {
      return false;
    }

    return await actor.sendQuery("WebScraper:WaitForElement", {
      selector,
      timeout,
    });
  }

  /**
   * Executes JavaScript code in a browser instance
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param script - JavaScript code to execute
   * @returns Promise<any> - The result of the script execution, or null if error
   * @throws Error - If the browser instance is not found
   */
  public async executeScript(
    instanceId: string,
    script: string,
  ): Promise<unknown> {
    const instance = this._browserInstances.get(instanceId);
    if (!instance) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }
    const { browser } = instance;

    const actor = browser.browsingContext?.currentWindowGlobal?.getActor(
      "NRWebScraper",
    );

    if (!actor) {
      return null;
    }

    return await actor.sendQuery("WebScraper:ExecuteScript", { script });
  }

  /**
   * Takes a screenshot of the current viewport
   *
   * @param instanceId - The unique identifier of the browser instance
   * @returns Promise<string | null> - Base64 encoded PNG image, or null on error
   * @throws Error - If the browser instance is not found
   */
  public async takeScreenshot(instanceId: string): Promise<string | null> {
    const instance = this._browserInstances.get(instanceId);
    if (!instance) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }
    const { browser } = instance;

    const actor = browser.browsingContext?.currentWindowGlobal?.getActor(
      "NRWebScraper",
    );

    if (!actor) {
      return null;
    }

    return await actor.sendQuery("WebScraper:TakeScreenshot");
  }

  /**
   * Takes a screenshot of a specific element
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target element
   * @returns Promise<string | null> - Base64 encoded PNG image, or null on error
   * @throws Error - If the browser instance is not found
   */
  public async takeElementScreenshot(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    const instance = this._browserInstances.get(instanceId);
    if (!instance) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }
    const { browser } = instance;

    const actor = browser.browsingContext?.currentWindowGlobal?.getActor(
      "NRWebScraper",
    );

    if (!actor) {
      return null;
    }

    return await actor.sendQuery("WebScraper:TakeElementScreenshot", {
      selector,
    });
  }

  /**
   * Takes a screenshot of the full page
   *
   * @param instanceId - The unique identifier of the browser instance
   * @returns Promise<string | null> - Base64 encoded PNG image, or null on error
   * @throws Error - If the browser instance is not found
   */
  public async takeFullPageScreenshot(
    instanceId: string,
  ): Promise<string | null> {
    const instance = this._browserInstances.get(instanceId);
    if (!instance) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }
    const { browser } = instance;

    const actor = browser.browsingContext?.currentWindowGlobal?.getActor(
      "NRWebScraper",
    );

    if (!actor) {
      return null;
    }

    return await actor.sendQuery("WebScraper:TakeFullPageScreenshot");
  }

  /**
   * Takes a screenshot of a specific region of the page.
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param rect The rectangular area to capture { x, y, width, height }.
   * @returns Promise<string | null> - Base64 encoded PNG image, or null on error
   * @throws Error - If the browser instance is not found
   */
  public async takeRegionScreenshot(
    instanceId: string,
    rect?: { x?: number; y?: number; width?: number; height?: number },
  ): Promise<string | null> {
    const instance = this._browserInstances.get(instanceId);
    if (!instance) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }
    const { browser } = instance;

    const actor = browser.browsingContext?.currentWindowGlobal?.getActor(
      "NRWebScraper",
    );

    if (!actor) {
      return null;
    }

    return await actor.sendQuery("WebScraper:TakeRegionScreenshot", {
      rect,
    });
  }

  /**
   * Fills multiple form fields based on a selector-value map.
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param formData A map where keys are CSS selectors for input fields
   * and values are the corresponding values to set.
   * @returns Promise<boolean> - True if all fields were filled successfully, false otherwise.
   * @throws Error - If the browser instance is not found
   */
  public async fillForm(
    instanceId: string,
    formData: { [selector: string]: string },
  ): Promise<boolean> {
    const instance = this._browserInstances.get(instanceId);
    if (!instance) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }
    const { browser } = instance;

    const actor = browser.browsingContext?.currentWindowGlobal?.getActor(
      "NRWebScraper",
    );

    if (!actor) {
      return false;
    }

    return await actor.sendQuery("WebScraper:FillForm", {
      formData,
    });
  }

  /**
   * Waits for the specified number of milliseconds.
   *
   * @param ms - The number of milliseconds to wait
   * @returns Promise<void> - Resolves after the specified delay
   */
  public wait(ms: number): Promise<void> {
    return new Promise((resolve) => {
      setTimeout(resolve, ms);
    });
  }
}

// Export a singleton instance of the TabManager service
export const TabManagerServices = new TabManager();
