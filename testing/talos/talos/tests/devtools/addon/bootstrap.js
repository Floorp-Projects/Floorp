"use strict";

// PLEASE NOTE:
//
// The canonical version of this file lives in testing/talos/talos, and
// is duplicated in a number of test add-ons in directories below it.
// Please do not update one withput updating all.

// Reads the chrome.manifest from a legacy non-restartless extension and loads
// its overlays into the appropriate top-level windows.

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

Cu.importGlobalProperties(["TextDecoder"]);

class DefaultMap extends Map {
  constructor(defaultConstructor = undefined, init = undefined) {
    super(init);
    if (defaultConstructor) {
      this.defaultConstructor = defaultConstructor;
    }
  }

  get(key) {
    let value = super.get(key);
    if (value === undefined && !this.has(key)) {
      value = this.defaultConstructor(key);
      this.set(key, value);
    }
    return value;
  }
}

const windowTracker = {
  init() {
    Services.ww.registerNotification(this);
  },

  overlays: new DefaultMap(() => new Set()),

  async observe(window, topic, data) {
    if (topic === "domwindowopened") {
      await new Promise(resolve =>
        window.addEventListener("DOMWindowCreated", resolve, {once: true}));

      let {document} = window;
      let {documentURI} = document;

      if (this.overlays.has(documentURI)) {
        for (let overlay of this.overlays.get(documentURI)) {
          document.loadOverlay(overlay, null);
        }
      }
    }
  },
};

function readSync(uri) {
  let channel = NetUtil.newChannel({uri, loadUsingSystemPrincipal: true});
  let buffer = NetUtil.readInputStream(channel.open2());
  return new TextDecoder().decode(buffer);
}

function startup(data, reason) {
  windowTracker.init();

  for (let line of readSync(data.resourceURI.resolve("chrome.manifest")).split("\n")) {
    let [directive, ...args] = line.trim().split(/\s+/);
    if (directive === "overlay") {
      let [url, overlay] = args;
      windowTracker.overlays.get(url).add(overlay);
    }
  }
}

function shutdown(data, reason) {}
function install(data, reason) {}
function uninstall(data, reason) {}
