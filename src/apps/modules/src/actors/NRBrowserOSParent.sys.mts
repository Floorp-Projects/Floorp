/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { BrowserInfo } = ChromeUtils.importESModule(
  "resource://noraneko/modules/os-apis/browser-info/BrowserInfo.sys.mjs",
);
const { WebScraper } = ChromeUtils.importESModule(
  "resource://noraneko/modules/os-apis/web/WebScraperServices.sys.mjs",
);
const { TabManagerServices } = ChromeUtils.importESModule(
  "resource://noraneko/modules/os-apis/web/TabManagerServices.sys.mjs",
);

export class NRBrowserOSParent extends JSWindowActorParent {
  receiveMessage(message: ReceiveMessageArgument) {
    try {
      const { name, data } = message;
      const toStringValue = (val: unknown): string => {
        try {
          if (val === undefined || val === null) return "null";
          if (typeof val === "string") return val;
          return JSON.stringify(val);
        } catch (_e) {
          try {
            return String(val);
          } catch (_e2) {
            return "null";
          }
        }
      };
      switch (name) {
        // ---- Browser Info ----
        case "BrowserOS:GetAllContextData": {
          return BrowserInfo
            .getAllContextData(
              data?.historyLimit ?? 10,
              data?.downloadLimit ?? 10,
            )
            .then((result: unknown) => JSON.stringify(result));
        }

        // ---- WebScraper ----
        case "BrowserOS:WSCreate":
          return WebScraper.createInstance();
        case "BrowserOS:WSDestroy":
          return WebScraper.destroyInstance(data.instanceId);
        case "BrowserOS:WSNavigate":
          return WebScraper.navigate(data.instanceId, data.url);
        case "BrowserOS:WSGetURI":
          return WebScraper.getURI(data.instanceId);
        case "BrowserOS:WSGetHTML":
          return WebScraper.getHTML(data.instanceId);
        case "BrowserOS:WSGetElement":
          return WebScraper.getElement(data.instanceId, data.selector);
        case "BrowserOS:WSGetElementText":
          return WebScraper.getElementText(data.instanceId, data.selector);
        case "BrowserOS:WSGetValue":
          return WebScraper.getValue(data.instanceId, data.selector);
        case "BrowserOS:WSClickElement":
          return WebScraper.clickElement(data.instanceId, data.selector);
        case "BrowserOS:WSWaitForElement":
          return WebScraper.waitForElement(
            data.instanceId,
            data.selector,
            data.timeout,
          );
        case "BrowserOS:WSExecuteScript":
          return WebScraper
            .executeScript(data.instanceId, data.script)
            .then(() => null);
        case "BrowserOS:WSTakeScreenshot":
          return WebScraper.takeScreenshot(data.instanceId);
        case "BrowserOS:WSTakeElementScreenshot":
          return WebScraper.takeElementScreenshot(
            data.instanceId,
            data.selector,
          );
        case "BrowserOS:WSTakeFullPageScreenshot":
          return WebScraper.takeFullPageScreenshot(data.instanceId);
        case "BrowserOS:WSTakeRegionScreenshot":
          return WebScraper.takeRegionScreenshot(data.instanceId, data.rect);
        case "BrowserOS:WSFillForm":
          return WebScraper.fillForm(data.instanceId, data.formData);
        case "BrowserOS:WSSubmit":
          return WebScraper.submit(data.instanceId, data.selector);
        case "BrowserOS:WSWait":
          return WebScraper.wait(data.ms);

        // ---- Tab Manager ----
        case "BrowserOS:TMCreateInstance":
          return TabManagerServices.createInstance(data.url, data.options);
        case "BrowserOS:TMAttachToTab":
          return TabManagerServices.attachToTab(data.browserId);
        case "BrowserOS:TMListTabs": {
          return TabManagerServices
            .listTabs()
            .then((res: unknown) => JSON.stringify(res));
        }
        case "BrowserOS:TMGetInstanceInfo": {
          return TabManagerServices
            .getInstanceInfo(data.instanceId)
            .then((res: unknown) => (res == null ? null : JSON.stringify(res)));
        }
        case "BrowserOS:TMDestroyInstance":
          return TabManagerServices.destroyInstance(data.instanceId);
        case "BrowserOS:TMNavigate":
          return TabManagerServices.navigate(data.instanceId, data.url);
        case "BrowserOS:TMGetURI":
          return TabManagerServices.getURI(data.instanceId);
        case "BrowserOS:TMGetHTML":
          return TabManagerServices.getHTML(data.instanceId);
        case "BrowserOS:TMGetElement":
          return TabManagerServices.getElement(data.instanceId, data.selector);
        case "BrowserOS:TMGetElementText":
          return TabManagerServices.getElementText(
            data.instanceId,
            data.selector,
          );
        case "BrowserOS:TMGetValue":
          return TabManagerServices.getValue(data.instanceId, data.selector);
        case "BrowserOS:TMClickElement":
          return TabManagerServices.clickElement(
            data.instanceId,
            data.selector,
          );
        case "BrowserOS:TMWaitForElement":
          return TabManagerServices.waitForElement(
            data.instanceId,
            data.selector,
            data.timeout,
          );
        case "BrowserOS:TMExecuteScript":
          return TabManagerServices
            .executeScript(data.instanceId, data.script)
            .then(() => null);
        case "BrowserOS:TMTakeScreenshot":
          return TabManagerServices.takeScreenshot(data.instanceId);
        case "BrowserOS:TMTakeElementScreenshot":
          return TabManagerServices.takeElementScreenshot(
            data.instanceId,
            data.selector,
          );
        case "BrowserOS:TMTakeFullPageScreenshot":
          return TabManagerServices.takeFullPageScreenshot(data.instanceId);
        case "BrowserOS:TMTakeRegionScreenshot":
          return TabManagerServices.takeRegionScreenshot(
            data.instanceId,
            data.rect,
          );
        case "BrowserOS:TMFillForm":
          return TabManagerServices.fillForm(data.instanceId, data.formData);
        case "BrowserOS:TMSubmit":
          return TabManagerServices.submit(data.instanceId, data.selector);
        case "BrowserOS:TMWaIt":
          // NOTE: the child uses TMWaIt (capital I) for differentiation; keep as-is
          return TabManagerServices.wait(data.ms);

        default:
          console.warn("NRBrowserOSParent: unknown message", name, data);
          return null;
      }
    } catch (e) {
      console.error("NRBrowserOSParent receiveMessage error", e);
      return null;
    }
  }
}
