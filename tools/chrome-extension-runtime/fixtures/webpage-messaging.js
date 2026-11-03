/* SPDX-License-Identifier: MPL-2.0 */
"use strict";
(() => {
  /** @type {import("../webpage-test-types.ts").WebPagePort|undefined} */
  let _heldPort;
  /** @type {import("../webpage-test-types.ts").WebPageFixtureWindow} */
  const page =
    /** @type {typeof globalThis & import("../webpage-test-types.ts").WebPageFixtureWindow} */ (globalThis);
  const document = page.document;
  if (!document) throw new Error("Ordinary web fixture has no document");
  // Retain the real Port without manually disconnecting on navigation. Native
  // document teardown remains responsible for the tested lifetime revocation.
  Object.defineProperty(page, "__floorpWebPageFixtureHeldPort", {
    get: () => _heldPort,
    configurable: true,
  });
  /** @returns {import("../webpage-test-types.ts").WebPageRuntime} */
  const nativeRuntime = () => {
    if (!page.chrome) throw new ReferenceError("chrome is not defined");
    return /** @type {import("../webpage-test-types.ts").WebPageRuntime} */ (page
      .chrome.runtime);
  };
  page.addEventListener("message", async (event) => {
    const data =
      /** @type {import("../webpage-test-types.ts").WebPageFixtureRequest} */ (event
        .data);
    if (!data?.floorpWebPageFixture) return;
    try {
      let result;
      if (data.operation === "inspect") {
        result = {
          runtime: typeof page.chrome?.runtime,
          extensionId: page.chrome?.runtime?.id,
          native: typeof page.chrome?.runtime?.sendNativeMessage,
          listener: typeof page.chrome?.runtime?.onMessage,
          storage: typeof page.chrome?.storage,
        };
      } else if (data.operation === "message") {
        result = await nativeRuntime().sendMessage(data.target, {
          value: "webpage",
        });
      } else if (data.operation === "fetch") {
        const response = await fetch(data.target);
        result = { url: response.url, body: await response.text() };
      } else if (data.operation === "callback") {
        result = await new Promise((resolve, reject) => {
          const runtime = nativeRuntime();
          runtime.sendMessage(
            data.target,
            { value: "webpage" },
            (value) => {
              if (runtime.lastError) {
                reject(new Error(runtime.lastError.message));
              } else resolve(value);
            },
          );
        });
      } else if (data.operation === "port" || data.operation === "holdPort") {
        const port = nativeRuntime().connect(data.target, {
          name: "webpage-fixture",
        });
        result = await new Promise((resolve, reject) => {
          port.onMessage.addListener((value) => {
            if (data.operation === "holdPort") _heldPort = port;
            else port.disconnect();
            resolve(value);
          });
          port.onDisconnect.addListener(() =>
            reject(new Error(port.error?.message || "Disconnected"))
          );
          port.postMessage({ value: "webpage" });
        });
      }
      document.title = JSON.stringify({ requestId: data.requestId, result });
    } catch (error) {
      document.title = JSON.stringify({
        requestId: data.requestId,
        error: String(error),
      });
    }
  });
  document.title = JSON.stringify({ ready: true });
  page.addEventListener("pageshow", (event) => {
    if (event.persisted) document.title = JSON.stringify({ ready: true });
  });
})();
