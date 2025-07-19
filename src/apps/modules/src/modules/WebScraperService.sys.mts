/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * WebScraperService - A service for creating and managing headless browser instances
 *
 * This module provides functionality to:
 * - Create isolated browser instances using HiddenFrame
 * - Navigate to URLs and wait for page loads
 * - Manage multiple browser instances with unique IDs
 * - Clean up resources when instances are no longer needed
 */

const { E10SUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/E10SUtils.sys.mjs",
);
const { HiddenFrame } = ChromeUtils.importESModule(
  "resource://gre/modules/HiddenFrame.sys.mjs",
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

// Global sets to prevent garbage collection of active components
const SCRAPER_ACTOR_SETS: Set<XULBrowserElement> = new Set();
const PROGRESS_LISTENERS = new Set();
const FRAME = new HiddenFrame();

/**
 * WebScraper class that manages headless browser instances for web scraping
 *
 * This class provides a high-level interface for creating, managing, and
 * navigating browser instances without visible UI elements.
 */
class webScraper {
  // Map to store browser instances with their unique IDs
  private _browserInstances: Map<string, XULBrowserElement> = new Map();
  private _instanceId: string;
  private _windowlessBrowser: nsIWindowlessBrowser;

  constructor() {
    this._initializeWindowlessBrowser();
  }

  private async _initializeWindowlessBrowser(): Promise<void> {
    this._windowlessBrowser = await FRAME.get();
  }

  /**
   * Creates a new headless browser instance
   *
   * This method:
   * - Creates a HiddenFrame to host the browser
   * - Initializes a browser element with remote content capabilities
   * - Sets up proper sizing and attributes
   * - Returns a unique instance ID for future operations
   *
   * @returns Promise<string> - A unique identifier for the created browser instance
   */
  public async createInstance(): Promise<string> {
    const doc = this._windowlessBrowser.document;
    const browser = doc.createXULElement("browser");
    browser.setAttribute("remote", "true");
    browser.setAttribute("type", "content");
    browser.setAttribute("disableglobalhistory", "true");

    // Set default size for browser element
    browser.style.width = "1366px";
    browser.style.minWidth = "1366px";
    browser.style.height = "768px";
    browser.style.minHeight = "768px";

    // Add browser element to document to properly initialize webNavigation
    doc.documentElement.appendChild(browser);
    browser.browsingContext.allowJavascript = true;

    this._instanceId = await crypto.randomUUID();
    this._browserInstances.set(this._instanceId, browser);
    SCRAPER_ACTOR_SETS.add(browser);
    return this._instanceId;
  }

  /**
   * Destroys a browser instance and cleans up associated resources
   *
   * This method:
   * - Closes the browser instance
   * - Removes it from internal tracking maps
   * - Cleans up global references to prevent memory leaks
   *
   * @param instanceId - The unique identifier of the browser instance to destroy
   */
  public destroyInstance(instanceId: string): void {
    const browser = this._browserInstances.get(instanceId);
    if (browser) {
      browser.remove();
    }

    this._browserInstances.delete(instanceId);
    SCRAPER_ACTOR_SETS.delete(browser);
  }

  /**
   * Navigates a browser instance to a specified URL and waits for the page to load
   *
   * This method:
   * - Validates the browser instance exists
   * - Sets up proper security principals and origin attributes
   * - Loads the URL with appropriate remote type configuration
   * - Monitors page load progress and resolves when navigation is complete
   * - Handles various edge cases like inner-frame events and same-document changes
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param url - The URL to navigate to
   * @returns Promise<void> - Resolves when the page has finished loading
   * @throws Error - If the browser instance is not found
   */
  public navigate(instanceId: string, url: string): Promise<void> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

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

      /**
       * Progress listener to monitor page load completion
       *
       * This listener:
       * - Filters out non-top-level navigation events
       * - Ignores same-document location changes
       * - Handles about:blank page initialization
       * - Cleans up listeners when navigation is complete
       */
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    return new Promise((resolve) => {
      if (browser.browsingContext.currentURI.spec != "about:blank") {
        resolve(browser.browsingContext.currentURI.spec);
      } else {
        resolve(null);
      }
    });
  }

  /**
   * Retrieves the HTML content from a browser instance using the NRWebScraper actor
   *
   * This method:
   * - Validates the browser instance exists
   * - Gets the NRWebScraper actor from the current window global
   * - Sends a query to extract HTML content from the content process
   * - Handles cases where the actor is not available or errors occur
   *
   * The actor communication allows safe access to content process DOM
   * without violating cross-process security boundaries.
   *
   * @param instanceId - The unique identifier of the browser instance
   * @returns Promise<string | null> - The HTML content as a string, or null if unavailable
   * @throws Error - If the browser instance is not found
   */
  public async getHTML(instanceId: string): Promise<string | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

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
   * This method:
   * - Validates the browser instance exists
   * - Gets the NRWebScraper actor from the current window global
   * - Sends a query with a CSS selector to find and return a specific element
   * - Returns null if the actor is not available
   *
   * The actor communication enables safe DOM element access across
   * process boundaries while maintaining security isolation.
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

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
   * This method:
   * - Validates the browser instance exists
   * - Gets the NRWebScraper actor from the current window global
   * - Sends a query to extract text content from a specific element
   * - Returns null if the actor is not available or element not found
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

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
   * This method:
   * - Validates the browser instance exists
   * - Gets the NRWebScraper actor from the current window global
   * - Sends a query to click on a specific element
   * - Returns boolean indicating success or failure
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

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
   * This method:
   * - Validates the browser instance exists
   * - Gets the NRWebScraper actor from the current window global
   * - Sends a query to wait for a specific element to appear
   * - Returns boolean indicating if element was found within timeout
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

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
   * This method:
   * - Validates the browser instance exists
   * - Gets the NRWebScraper actor from the current window global
   * - Sends a query to execute JavaScript in the content context
   * - Returns the result of the script execution
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

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
   * This method captures the visible area of the page and returns it
   * as a base64 encoded PNG image. It includes error handling.
   *
   * @param instanceId - The unique identifier of the browser instance
   * @returns Promise<string | null> - Base64 encoded PNG image, or null on error
   * @throws Error - If the browser instance is not found
   */
  public async takeScreenshot(instanceId: string): Promise<string | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

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
   * This method captures a specific element on the page and returns it
   * as a base64 encoded PNG image. It includes error handling.
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

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
   * This method captures the entire scrollable area of the page and returns it
   * as a base64 encoded PNG image. It includes error handling.
   *
   * @param instanceId - The unique identifier of the browser instance
   * @returns Promise<string | null> - Base64 encoded PNG image, or null on error
   * @throws Error - If the browser instance is not found
   */
  public async takeFullPageScreenshot(
    instanceId: string,
  ): Promise<string | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

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
   * If properties are omitted, they default to the maximum possible size.
   *
   * This method captures a specific area of the page defined by a rectangle.
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

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

// Export a singleton instance of the WebScraper service
export const WebScraper = new webScraper();

/**
 * Test function for WebScraperService that can be executed in Browser Console
 *
 * Usage in Browser Console:
 * 1. Copy and paste this entire function
 * 2. Call: testWebScraper()
 *
 * This test will:
 * - Create a browser instance
 * - Navigate to a test URL
 * - Get the current URI
 * - Get HTML content
 * - Navigate to another URL
 * - Take viewport and full page screenshots
 * - Destroy the instance
 * - Log all results to console
 */
export async function testWebScraper() {
  console.log("=== WebScraper Test Started ===");

  try {
    // Test 1: Create instance
    console.log("1. Creating browser instance...");
    const instanceId = await WebScraper.createInstance();
    console.log("✅ Instance created with ID:", instanceId);

    // Test 2: Navigate to a test URL
    console.log("2. Navigating to test URL...");
    const testUrl = "https://x.com/takesako";
    await WebScraper.navigate(instanceId, testUrl);
    await WebScraper.wait(5000);
    console.log("✅ Navigation completed to:", testUrl);

    // Test 3: Get current URI
    console.log("3. Getting current URI...");
    const currentUri = await WebScraper.getURI(instanceId);
    console.log("✅ Current URI:", currentUri);

    // Test 4: Get HTML content
    console.log("4. Getting HTML content...");
    const htmlContent = await WebScraper.getHTML(instanceId);
    if (htmlContent) {
      console.log("✅ HTML preview:", htmlContent.substring(0, 200) + "...");
    }

    // Test 5: Get element text
    console.log("5. Getting element text...");
    const titleText = await WebScraper.getElementText(instanceId, "title");
    console.log("✅ Title text:", titleText);

    // Test 6: Get specific HTML element
    console.log("6. Getting specific HTML element...");
    const titleElement = await WebScraper.getElement(instanceId, "title");
    console.log("✅ Title element HTML:", titleElement);

    // Test 7: Get navigation element
    console.log("7. Getting navigation element...");
    const navElement = await WebScraper.getElement(instanceId, "nav");
    if (navElement) {
      console.log(
        "✅ Navigation element preview:",
        navElement.substring(0, 200) + "...",
      );
    } else {
      console.log("✅ Navigation element: Not found");
    }

    // Test 8: Wait for element and click
    console.log("8. Testing element interaction...");
    const elementFound = await WebScraper.waitForElement(
      instanceId,
      "body",
      3000,
    );
    console.log("✅ Element found:", elementFound);

    // Test 9: Execute JavaScript
    console.log("9. Executing JavaScript...");
    const pageTitle = await WebScraper.executeScript(
      instanceId,
      "return document.title;",
    );
    console.log("✅ Page title via JavaScript:", pageTitle);

    // Test 10: Take viewport screenshot
    console.log("10. Taking viewport screenshot...");
    const viewportScreenshot = await WebScraper.takeScreenshot(instanceId);
    if (viewportScreenshot) {
      console.log(
        "✅ Viewport screenshot captured:",
        viewportScreenshot,
      );
    } else {
      console.log("❌ Viewport screenshot failed");
    }

    // Test 11: Take element screenshot
    console.log("11. Taking element screenshot...");
    const elementScreenshot = await WebScraper.takeElementScreenshot(
      instanceId,
      "nav",
    );
    if (elementScreenshot) {
      console.log(
        "✅ Element screenshot captured:",
        elementScreenshot,
      );
    } else {
      console.log("❌ Element screenshot failed");
    }

    // Test 12: Take full page screenshot
    console.log("12. Taking full page screenshot...");
    const fullPageScreenshot = await WebScraper.takeFullPageScreenshot(
      instanceId,
    );
    if (fullPageScreenshot) {
      console.log(
        "✅ Full page screenshot captured:",
        fullPageScreenshot,
      );
    } else {
      console.log("❌ Full page screenshot failed");
    }

    // Test 13: Take region screenshot
    console.log(
      "13. Taking region screenshot (from 100,100 with size 300x200)...",
    );
    const regionScreenshot = await WebScraper.takeRegionScreenshot(instanceId, {
      x: 100,
      y: 100,
      width: 300,
      height: 200,
    });
    if (regionScreenshot) {
      console.log(
        "✅ Region screenshot captured:",
        regionScreenshot,
      );
    } else {
      console.log("❌ Region screenshot failed");
    }

    // Test 14: Take region screenshot with partial arguments (should be from top-left)
    console.log(
      "14. Taking region screenshot with partial arguments (height: 400px)...",
    );
    const partialRegionScreenshot = await WebScraper.takeRegionScreenshot(
      instanceId,
      { height: 2000 },
    );
    if (partialRegionScreenshot) {
      console.log(
        "✅ Partial region screenshot captured:",
        partialRegionScreenshot,
      );
    } else {
      console.log("❌ Partial region screenshot failed");
    }

    // Test 15: Destroy instance
    console.log("15. Destroying instance...");
    WebScraper.destroyInstance(instanceId);
    console.log("✅ Instance destroyed");

    console.log("=== WebScraper Test Completed Successfully ===");
  } catch (error) {
    console.error("❌ WebScraper Test Failed:", error);
    console.error("Error details:", error.message);
    console.error("Stack trace:", error.stack);
  }
}
