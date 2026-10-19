// SPDX-License-Identifier: MPL-2.0
chrome.runtime.onMessage.addListener((message, sender, respond) => {
  if (message.target === "background") {
    respond({
      value: message.value,
      senderId: sender.id,
      receiverId: chrome.runtime.id,
    });
  } else if (message.target === "background-info") {
    respond({
      hasDocument: typeof document !== "undefined",
      importScripts: typeof importScripts,
    });
  }
});
chrome.runtime.onConnect.addListener((port) => {
  if (port.name === "comparison-background") {
    port.onMessage.addListener((message) =>
      port.postMessage({
        value: message.value,
        senderId: port.sender.id,
        receiverId: chrome.runtime.id,
      })
    );
  }
});
