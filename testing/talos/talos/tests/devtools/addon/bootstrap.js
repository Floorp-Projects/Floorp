"use strict";

/* globals initializeBrowser */

// PLEASE NOTE:
//
// The canonical version of this file lives in testing/talos/talos, and
// is duplicated in a number of test add-ons in directories below it.
// Please do not update one withput updating all.

// Reads the chrome.manifest from a legacy non-restartless extension and loads
// its overlays into the appropriate top-level windows.

ChromeUtils.import("resource://gre/modules/Services.jsm");

const windowTracker = {
  init() {
    Services.ww.registerNotification(this);
  },

  async observe(window, topic, data) {
    if (topic === "domwindowopened") {
      await new Promise(resolve =>
        window.addEventListener("DOMWindowCreated", resolve, {once: true}));

      let {document} = window;
      let {documentURI} = document;

      if (documentURI !== "chrome://browser/content/browser.xul") {
        return;
      }
      initializeBrowser(window);
    }
  },
};

function startup(data, reason) {
  Services.scriptloader.loadSubScript(data.resourceURI.resolve("content/initialize_browser.js"));
  windowTracker.init();
}

function shutdown(data, reason) {}
function install(data, reason) {}
function uninstall(data, reason) {}
