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
    const windowlessBrowser = await FRAME.get();

    const doc = windowlessBrowser.document;
    const browser = doc.createXULElement("browser");
    browser.setAttribute("remote", "true");
    browser.setAttribute("type", "content");
    browser.setAttribute("maychangeremoteness", "true");

    // Set default size for browser element
    browser.style.width = "1366px";
    browser.style.minWidth = "1366px";
    browser.style.height = "768px";
    browser.style.minHeight = "768px";

    // Add browser element to document to properly initialize webNavigation
    doc.documentElement.appendChild(browser);

    this._instanceId = crypto.randomUUID();
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

  public getCurrentURI(instanceId: string): Promise<string | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    return new Promise((resolve) => {
      if (browser.browsingContext.currentURI.spec != "about:blank") {
        console.log("currentURI", browser);
        resolve(browser.browsingContext.currentURI.spec);
      } else {
        resolve(null);
      }
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
    const testUrl = "https://example.com";
    await WebScraper.navigate(instanceId, testUrl);
    console.log("✅ Navigation completed to:", testUrl);

    // Test 3: Get current URI
    console.log("3. Getting current URI...");
    const currentUri = await WebScraper.getCurrentURI(instanceId);
    console.log("✅ Current URI:", currentUri);

    // Test 4: Navigate to another URL
    console.log("4. Navigating to another URL...");
    const secondUrl = "https://httpbin.org/get";
    await WebScraper.navigate(instanceId, secondUrl);
    console.log("✅ Navigation completed to:", secondUrl);

    // Test 5: Get updated URI
    console.log("5. Getting updated URI...");
    const updatedUri = await WebScraper.getCurrentURI(instanceId);
    console.log("✅ Updated URI:", updatedUri);

    // Test 6: Destroy instance
    console.log("6. Destroying instance...");
    WebScraper.destroyInstance(instanceId);
    console.log("✅ Instance destroyed");

    console.log("=== WebScraper Test Completed Successfully ===");
  } catch (error) {
    console.error("❌ WebScraper Test Failed:", error);
    console.error("Error details:", error.message);
    console.error("Stack trace:", error.stack);
  }
}

/**
 * Quick test function for basic functionality
 *
 * Usage in Browser Console:
 * 1. Copy and paste this function
 * 2. Call: quickTest()
 */
export async function quickTest() {
  console.log("=== Quick WebScraper Test ===");

  try {
    const instanceId = await WebScraper.createInstance();
    console.log("Instance created:", instanceId);

    await WebScraper.navigate(instanceId, "https://example.com");
    console.log("Navigation successful");

    const uri = await WebScraper.getCurrentURI(instanceId);
    console.log("Current URI:", uri);

    WebScraper.destroyInstance(instanceId);
    console.log("Instance destroyed");

    console.log("✅ Quick test passed!");
  } catch (error) {
    console.error("❌ Quick test failed:", error.message);
  }
}

/**
 * Stress test function to test multiple instances
 *
 * Usage in Browser Console:
 * 1. Copy and paste this function
 * 2. Call: stressTest(5) // Creates 5 instances
 */
export async function stressTest(instanceCount = 3) {
  console.log(`=== WebScraper Stress Test (${instanceCount} instances) ===`);

  const instances = [];

  try {
    // Create multiple instances
    for (let i = 0; i < instanceCount; i++) {
      const instanceId = await WebScraper.createInstance();
      instances.push(instanceId);
      console.log(`Instance ${i + 1} created:`, instanceId);
    }

    // Navigate all instances to different URLs
    const testUrls = [
      "https://example.com",
      "https://httpbin.org/get",
      "https://jsonplaceholder.typicode.com/posts/1",
      "https://httpbin.org/headers",
      "https://httpbin.org/ip",
    ];

    for (let i = 0; i < instances.length; i++) {
      const url = testUrls[i % testUrls.length];
      await WebScraper.navigate(instances[i], url);
      console.log(`Instance ${i + 1} navigated to:`, url);
    }

    // Get URIs from all instances
    for (let i = 0; i < instances.length; i++) {
      const uri = await WebScraper.getCurrentURI(instances[i]);
      console.log(`Instance ${i + 1} current URI:`, uri);
    }

    // Destroy all instances
    for (let i = 0; i < instances.length; i++) {
      WebScraper.destroyInstance(instances[i]);
      console.log(`Instance ${i + 1} destroyed`);
    }

    console.log("✅ Stress test completed successfully!");
  } catch (error) {
    console.error("❌ Stress test failed:", error.message);

    // Clean up any remaining instances
    for (const instanceId of instances) {
      try {
        WebScraper.destroyInstance(instanceId);
      } catch (cleanupError) {
        console.error(
          "Cleanup error for instance",
          instanceId,
          ":",
          cleanupError.message,
        );
      }
    }
  }
}
