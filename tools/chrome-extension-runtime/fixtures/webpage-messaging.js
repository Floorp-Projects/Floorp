/* SPDX-License-Identifier: MPL-2.0 */
"use strict";
let heldPort;
window.addEventListener("message", async event => {
  const data = event.data;
  if (!data?.floorpWebPageFixture) return;
  try {
    let result;
    if (data.operation === "inspect") {
      result = { runtime: typeof window.chrome?.runtime,
        extensionId: window.chrome?.runtime?.id,
        native: typeof window.chrome?.runtime?.sendNativeMessage,
        listener: typeof window.chrome?.runtime?.onMessage,
        storage: typeof window.chrome?.storage };
    } else if (data.operation === "message") {
      result = await chrome.runtime.sendMessage(data.target, { value: "webpage" });
    } else if (data.operation === "fetch") {
      const response = await fetch(data.target);
      result = { url: response.url, body: await response.text() };
    } else if (data.operation === "callback") {
      result = await new Promise((resolve, reject) => {
        chrome.runtime.sendMessage(data.target, { value: "webpage" }, value => {
          if (chrome.runtime.lastError) reject(new Error(chrome.runtime.lastError.message));
          else resolve(value);
        });
      });
    } else if (data.operation === "port" || data.operation === "holdPort") {
      const port = chrome.runtime.connect(data.target, { name: "webpage-fixture" });
      result = await new Promise((resolve, reject) => {
        port.onMessage.addListener(value => {
          if (data.operation === "holdPort") heldPort = port;
          else port.disconnect();
          resolve(value);
        });
        port.onDisconnect.addListener(() => reject(new Error(port.error?.message || "Disconnected")));
        port.postMessage({ value: "webpage" });
      });
    }
    document.title = JSON.stringify({ requestId: data.requestId, result });
  } catch (error) {
    document.title = JSON.stringify({ requestId: data.requestId, error: String(error) });
  }
});
document.title = JSON.stringify({ ready: true });
window.addEventListener("pageshow", event => {
  if (event.persisted) document.title = JSON.stringify({ ready: true });
});
