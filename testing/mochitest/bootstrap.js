/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

/////// Android ///////

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
    Services.obs.addObserver(this, "chrome-document-global-created");
  },

  overlays: new DefaultMap(() => new Set()),

  async observe(window, topic, data) {
    if (topic === "chrome-document-global-created") {
      await new Promise(resolve =>
        window.addEventListener("DOMContentLoaded", resolve, {once: true}));

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

function androidStartup(data, reason) {
  windowTracker.init();

  for (let line of readSync(data.resourceURI.resolve("chrome.manifest")).split("\n")) {
    let [directive, ...args] = line.trim().split(/\s+/);
    if (directive === "overlay") {
      let [url, overlay] = args;
      windowTracker.overlays.get(url).add(overlay);
    }
  }
}

/////// Desktop ///////

var WindowListener = {
  // browser-test.js is only loaded into the first window. Setup that
  // needs to happen in all navigator:browser windows should go here.
  setupWindow: function(win) {
    win.nativeConsole = win.console;
    ChromeUtils.defineModuleGetter(win, "console",
      "resource://gre/modules/Console.jsm");
  },

  tearDownWindow: function(win) {
    if (win.nativeConsole) {
      win.console = win.nativeConsole;
      win.nativeConsole = undefined;
    }
  },

  onOpenWindow: function (win) {
    win = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);

    win.addEventListener("load", function() {
      if (win.document.documentElement.getAttribute("windowtype") == "navigator:browser") {
        WindowListener.setupWindow(win);
      }
    }, {once: true});
  }
}

function loadMochitest(e) {
  let flavor = e.detail[0];
  let url = e.detail[1];

  let win = Services.wm.getMostRecentWindow("navigator:browser");
  win.removeEventListener('mochitest-load', loadMochitest);

  // for mochitest-plain, navigating to the url is all we need
  win.loadURI(url);
  if (flavor == "mochitest") {
    return;
  }

  WindowListener.setupWindow(win);
  Services.wm.addListener(WindowListener);

  let overlay = "chrome://mochikit/content/browser-test-overlay.xul";
  win.document.loadOverlay(overlay, null);
}

function startup(data, reason) {
  if (AppConstants.platform == "android") {
    androidStartup(data, reason);
  } else {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    // wait for event fired from start_desktop.js containing the
    // suite and url to load
    win.addEventListener('mochitest-load', loadMochitest);
  }
}

function shutdown(data, reason) {
  if (AppConstants.platform != "android") {
    let windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      let win = windows.getNext().QueryInterface(Ci.nsIDOMWindow);
      WindowListener.tearDownWindow(win);
    }

    Services.wm.removeListener(WindowListener);
  }
}

function install(data, reason) {}
function uninstall(data, reason) {}

