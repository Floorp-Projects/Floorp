/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

class WebScraperService {
  // The map stores nsIWebBrowser objects returned by createWindowlessBrowser.
  private _browserInstances: Map<string, XULBrowserElement> = new Map();

  constructor() {
    // The service is a singleton.
  }

  public createInstance(): string {
    // Create a content browser (isChrome = false) that can load web pages.
    const browser = Services.appShell.createWindowlessBrowser(
      true,
      Ci.nsIWebBrowserChrome.CHROME_REMOTE_WINDOW,
    ) as unknown as XULBrowserElement;

    const instanceId = `scraper-instance-${Date.now()}-${
      Math.random().toString(36).substring(2, 10)
    }`;
    this._browserInstances.set(instanceId, browser);

    return instanceId;
  }

  public async navigate(instanceId: string, url: string): Promise<void> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Instance not found: ${instanceId}`);
    }
    // loadURI is a method on nsIWebBrowser
    const uri = Services.io.newURI(url);

    console.log("uri", uri, browser);

    const loadURIOptions = {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    };

    browser.loadURI(uri, loadURIOptions);
    // Note: In a real implementation, you'd want to listen for load events.
  }

  public getHTML(instanceId: string): string | null {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      return null;
    }

    const actor = browser.browsingContext.currentWindowGlobal.getActor(
      "NRWebScraper",
    );

    console.log("actor", actor);

    if (!actor) {
      return null;
    }
    return actor.sendQuery("WebScraper:GetHTML");
  }

  public destroyInstance(instanceId: string): void {
    const browser = this._browserInstances.get(instanceId);
    if (browser) {
      return;
      browser.close();
      this._browserInstances.delete(instanceId);
    }
  }

  public getInstance(instanceId: string): XULBrowserElement | undefined {
    return this._browserInstances.get(instanceId);
  }
}

export const WebScraper = new WebScraperService();
