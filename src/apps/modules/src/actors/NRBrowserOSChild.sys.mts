// deno-lint-ignore-file
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRBrowserOSChild extends JSWindowActorChild {
  actorCreated() {
    try {
      const window = this.contentWindow;
      if (!window) return;

      // Allow dev server and chrome/about privileged pages
      if (
        window.location.port === "8999" ||
        window.location.href.startsWith("chrome://") ||
        window.location.href.startsWith("about:")
      ) {
        // Create an object on window to group APIs
        const api = Cu.createObjectIn(window, { defineAs: "BrowserOS" });

        const exportAsync = (
          name: string,
          // eslint-disable-next-line @typescript-eslint/no-explicit-any
          fn: (...args: any[]) => any,
        ) => {
          // Wrap chrome-side Promise into content global Promise to avoid Xray issues
          Cu.exportFunction(
            (...argsWrapped: unknown[]) => {
              return new window.Promise(
                (
                  resolve: (value: unknown) => void,
                  reject: (reason?: unknown) => void,
                ) => {
                  try {
                    const result = fn(...argsWrapped);
                    Promise.resolve(result).then(
                      resolve,
                      (err: unknown) => {
                        try {
                          // Avoid cross-compartment wrapper as rejection reason
                          if (err && typeof err === "object") {
                            // Prefer message if available; fallback to toString
                            // deno-lint-ignore no-explicit-any
                            const msg = (err as any).message ?? String(err);
                            reject(String(msg));
                          } else {
                            reject(
                              typeof err === "undefined"
                                ? "Unknown error"
                                : String(err),
                            );
                          }
                        } catch (_) {
                          reject("Unknown error");
                        }
                      },
                    );
                  } catch (e) {
                    try {
                      // Normalize thrown error to primitive
                      // deno-lint-ignore no-explicit-any
                      const msg = (e as any)?.message ?? String(e);
                      reject(String(msg));
                    } catch (_) {
                      reject("Unknown error");
                    }
                  }
                },
              );
            },
            api,
            { defineAs: name },
          );
        };

        // ---- Browser Info ----
        exportAsync(
          "getAllContextData",
          (params?: { historyLimit?: number; downloadLimit?: number }) => {
            return this.sendQuery("BrowserOS:GetAllContextData", params ?? {});
          },
        );

        // ---- WebScraper (HiddenFrame) ----
        exportAsync("wsCreate", () => this.sendQuery("BrowserOS:WSCreate", {}));
        exportAsync(
          "wsDestroy",
          (instanceId: string) =>
            this.sendQuery("BrowserOS:WSDestroy", { instanceId }),
        );
        exportAsync(
          "wsNavigate",
          (instanceId: string, url: string) =>
            this.sendQuery("BrowserOS:WSNavigate", { instanceId, url }),
        );
        exportAsync(
          "wsGetURI",
          (instanceId: string) =>
            this.sendQuery("BrowserOS:WSGetURI", { instanceId }),
        );
        exportAsync(
          "wsGetHTML",
          (instanceId: string) =>
            this.sendQuery("BrowserOS:WSGetHTML", { instanceId }),
        );
        exportAsync(
          "wsGetElement",
          (instanceId: string, selector: string) =>
            this.sendQuery("BrowserOS:WSGetElement", { instanceId, selector }),
        );
        exportAsync(
          "wsGetElementText",
          (instanceId: string, selector: string) =>
            this.sendQuery("BrowserOS:WSGetElementText", {
              instanceId,
              selector,
            }),
        );
        exportAsync(
          "wsGetValue",
          (instanceId: string, selector: string) =>
            this.sendQuery("BrowserOS:WSGetValue", {
              instanceId,
              selector,
            }),
        );
        exportAsync(
          "wsClickElement",
          (instanceId: string, selector: string) =>
            this.sendQuery("BrowserOS:WSClickElement", {
              instanceId,
              selector,
            }),
        );
        exportAsync(
          "wsWaitForElement",
          (instanceId: string, selector: string, timeout?: number) =>
            this.sendQuery("BrowserOS:WSWaitForElement", {
              instanceId,
              selector,
              timeout,
            }),
        );
        exportAsync(
          "wsExecuteScript",
          (instanceId: string, script: string) =>
            this.sendQuery("BrowserOS:WSExecuteScript", { instanceId, script }),
        );
        exportAsync(
          "wsTakeScreenshot",
          (instanceId: string) =>
            this.sendQuery("BrowserOS:WSTakeScreenshot", { instanceId }),
        );
        exportAsync(
          "wsTakeElementScreenshot",
          (instanceId: string, selector: string) =>
            this.sendQuery("BrowserOS:WSTakeElementScreenshot", {
              instanceId,
              selector,
            }),
        );
        exportAsync(
          "wsTakeFullPageScreenshot",
          (instanceId: string) =>
            this.sendQuery("BrowserOS:WSTakeFullPageScreenshot", {
              instanceId,
            }),
        );
        exportAsync("wsTakeRegionScreenshot", (
          instanceId: string,
          rect?: { x?: number; y?: number; width?: number; height?: number },
        ) =>
          this.sendQuery("BrowserOS:WSTakeRegionScreenshot", {
            instanceId,
            rect,
          }));
        exportAsync(
          "wsFillForm",
          (instanceId: string, formData: Record<string, string>) =>
            this.sendQuery("BrowserOS:WSFillForm", { instanceId, formData }),
        );
        exportAsync(
          "wsSubmit",
          (instanceId: string, selector: string) =>
            this.sendQuery("BrowserOS:WSSubmit", { instanceId, selector }),
        );
        exportAsync(
          "wsWait",
          (ms: number) => this.sendQuery("BrowserOS:WSWait", { ms }),
        );

        // ---- Tab Manager (Visible tabs) ----
        exportAsync(
          "tmCreateInstance",
          (url: string, options?: { inBackground?: boolean }) =>
            this.sendQuery("BrowserOS:TMCreateInstance", { url, options }),
        );
        exportAsync(
          "tmAttachToTab",
          (browserId: string) =>
            this.sendQuery("BrowserOS:TMAttachToTab", { browserId }),
        );
        exportAsync(
          "tmListTabs",
          () => this.sendQuery("BrowserOS:TMListTabs", {}),
        );
        exportAsync(
          "tmGetInstanceInfo",
          (instanceId: string) =>
            this.sendQuery("BrowserOS:TMGetInstanceInfo", { instanceId }),
        );
        exportAsync(
          "tmDestroyInstance",
          (instanceId: string) =>
            this.sendQuery("BrowserOS:TMDestroyInstance", { instanceId }),
        );
        exportAsync(
          "tmNavigate",
          (instanceId: string, url: string) =>
            this.sendQuery("BrowserOS:TMNavigate", { instanceId, url }),
        );
        exportAsync(
          "tmGetURI",
          (instanceId: string) =>
            this.sendQuery("BrowserOS:TMGetURI", { instanceId }),
        );
        exportAsync(
          "tmGetHTML",
          (instanceId: string) =>
            this.sendQuery("BrowserOS:TMGetHTML", { instanceId }),
        );
        exportAsync(
          "tmGetElement",
          (instanceId: string, selector: string) =>
            this.sendQuery("BrowserOS:TMGetElement", { instanceId, selector }),
        );
        exportAsync(
          "tmGetElementText",
          (instanceId: string, selector: string) =>
            this.sendQuery("BrowserOS:TMGetElementText", {
              instanceId,
              selector,
            }),
        );
        exportAsync(
          "tmGetValue",
          (instanceId: string, selector: string) =>
            this.sendQuery("BrowserOS:TMGetValue", {
              instanceId,
              selector,
            }),
        );
        exportAsync(
          "tmClickElement",
          (instanceId: string, selector: string) =>
            this.sendQuery("BrowserOS:TMClickElement", {
              instanceId,
              selector,
            }),
        );
        exportAsync(
          "tmWaitForElement",
          (instanceId: string, selector: string, timeout?: number) =>
            this.sendQuery("BrowserOS:TMWaitForElement", {
              instanceId,
              selector,
              timeout,
            }),
        );
        exportAsync(
          "tmExecuteScript",
          (instanceId: string, script: string) =>
            this.sendQuery("BrowserOS:TMExecuteScript", { instanceId, script }),
        );
        exportAsync(
          "tmTakeScreenshot",
          (instanceId: string) =>
            this.sendQuery("BrowserOS:TMTakeScreenshot", { instanceId }),
        );
        exportAsync(
          "tmTakeElementScreenshot",
          (instanceId: string, selector: string) =>
            this.sendQuery("BrowserOS:TMTakeElementScreenshot", {
              instanceId,
              selector,
            }),
        );
        exportAsync(
          "tmTakeFullPageScreenshot",
          (instanceId: string) =>
            this.sendQuery("BrowserOS:TMTakeFullPageScreenshot", {
              instanceId,
            }),
        );
        exportAsync("tmTakeRegionScreenshot", (
          instanceId: string,
          rect?: { x?: number; y?: number; width?: number; height?: number },
        ) =>
          this.sendQuery("BrowserOS:TMTakeRegionScreenshot", {
            instanceId,
            rect,
          }));
        exportAsync(
          "tmFillForm",
          (instanceId: string, formData: Record<string, string>) =>
            this.sendQuery("BrowserOS:TMFillForm", { instanceId, formData }),
        );
        exportAsync(
          "tmSubmit",
          (instanceId: string, selector: string) =>
            this.sendQuery("BrowserOS:TMSubmit", { instanceId, selector }),
        );
        exportAsync(
          "tmWait",
          (ms: number) => this.sendQuery("BrowserOS:TMWaIt", { ms }),
        );
      }
    } catch (e) {
      console.error("NRBrowserOSChild actorCreated error", e);
    }
  }
}
