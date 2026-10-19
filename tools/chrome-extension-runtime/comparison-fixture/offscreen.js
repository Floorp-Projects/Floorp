// SPDX-License-Identifier: MPL-2.0
chrome.runtime.onMessage.addListener((message, sender, respond) => {
  if (message.target === "offscreen") {
    respond({
      value: message.value,
      senderId: sender.id,
      receiverId: chrome.runtime.id,
      storage: typeof chrome.storage,
      tabs: typeof chrome.tabs,
      getBackgroundPage: typeof chrome.runtime.getBackgroundPage,
    });
  }
});
