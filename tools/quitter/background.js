"use strict";

/* eslint-env webextensions */

browser.runtime.onMessage.addListener(msg => {
  if (msg === "quit") {
    browser.quitter.quit();
  }
});
