/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { utils: Cu, interfaces: Ci, classes: Cc } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var WindowListener = {
  // browser-test.js is only loaded into the first window. Setup that
  // needs to happen in all navigator:browser windows should go here.
  setupWindow: function(win) {
    win.nativeConsole = win.console;
    XPCOMUtils.defineLazyModuleGetter(win, "console",
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

    win.addEventListener("load", function listener() {
      win.removeEventListener("load", listener, false);
      if (win.document.documentElement.getAttribute("windowtype") == "navigator:browser") {
        WindowListener.setupWindow(win);
      }
    }, false);
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

  let overlay;
  if (flavor == "jetpack-addon") {
    overlay = "chrome://mochikit/content/jetpack-addon-overlay.xul";
  } else if (flavor == "jetpack-package") {
    overlay = "chrome://mochikit/content/jetpack-package-overlay.xul";
  } else {
    overlay = "chrome://mochikit/content/browser-test-overlay.xul";
  }

  win.document.loadOverlay(overlay, null);
}

function startup(data, reason) {
  let win = Services.wm.getMostRecentWindow("navigator:browser");
  // wait for event fired from start_desktop.js containing the
  // suite and url to load
  win.addEventListener('mochitest-load', loadMochitest);
}

function shutdown(data, reason) {
  let windows = Services.wm.getEnumerator("navigator:browser");
  while (windows.hasMoreElements()) {
    let win = windows.getNext().QueryInterface(Ci.nsIDOMWindow);
    WindowListener.tearDownWindow(win);
  }

  Services.wm.removeListener(WindowListener);
}

function install(data, reason) {}
function uninstall(data, reason) {}
