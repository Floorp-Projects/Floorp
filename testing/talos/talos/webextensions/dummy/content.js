"use strict";

/* eslint-env webextensions */

browser.runtime.sendMessage({
  msg: "Hello from content script",
  url: location.href,
});

browser.runtime.onMessage.addListener(msg => {
  return Promise.resolve({ code: "10-4", msg });
});
