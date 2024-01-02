/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals AppConstants, ExtensionAPI, Services */

function loadChromeScripts(win) {
  Services.scriptloader.loadSubScript(
    "chrome://mochikit/content/chrome-harness.js",
    win
  );
  Services.scriptloader.loadSubScript(
    "chrome://mochikit/content/mochitest-e10s-utils.js",
    win
  );
  Services.scriptloader.loadSubScript(
    "chrome://mochikit/content/browser-test.js",
    win
  );
}

// ///// Android ///////

const windowTracker = {
  init() {
    Services.obs.addObserver(this, "chrome-document-global-created");
  },

  async observe(window, topic, data) {
    if (topic === "chrome-document-global-created") {
      await new Promise(resolve =>
        window.addEventListener("DOMContentLoaded", resolve, { once: true })
      );

      let { document } = window;
      let { documentURI } = document;

      if (documentURI !== AppConstants.BROWSER_CHROME_URL) {
        return;
      }
      loadChromeScripts(window);
    }
  },
};

function androidStartup() {
  // Only browser chrome tests need help starting.
  let testRoot = Services.prefs.getStringPref("mochitest.testRoot", "");
  if (testRoot.endsWith("/chrome")) {
    // The initial window is created from browser startup, which races
    // against extension initialization.  If it has already been created,
    // inject the test scripts now, otherwise wait for the browser window
    // to show up.
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    if (win) {
      loadChromeScripts(win);
      return;
    }

    windowTracker.init();
  }

  Services.fog.initializeFOG();
}

// ///// Desktop ///////

// Special case for Thunderbird windows.
const IS_THUNDERBIRD =
  Services.appinfo.ID == "{3550f703-e582-4d05-9a08-453d09bdfdc6}";
const WINDOW_TYPE = IS_THUNDERBIRD ? "mail:3pane" : "navigator:browser";

var WindowListener = {
  // browser-test.js is only loaded into the first window. Setup that
  // needs to happen in all navigator:browser windows should go here.
  setupWindow(win) {
    win.nativeConsole = win.console;
    let { ConsoleAPI } = ChromeUtils.importESModule(
      "resource://gre/modules/Console.sys.mjs"
    );
    win.console = new ConsoleAPI();
  },

  tearDownWindow(win) {
    if (win.nativeConsole) {
      win.console = win.nativeConsole;
      win.nativeConsole = undefined;
    }
  },

  onOpenWindow(xulWin) {
    let win = xulWin.docShell.domWindow;

    win.addEventListener(
      "load",
      function () {
        if (
          win.document.documentElement.getAttribute("windowtype") == WINDOW_TYPE
        ) {
          WindowListener.setupWindow(win);
        }
      },
      { once: true }
    );
  },
};

function loadMochitest(e) {
  let flavor = e.detail[0];
  let url = e.detail[1];

  let win = Services.wm.getMostRecentWindow(WINDOW_TYPE);
  win.removeEventListener("mochitest-load", loadMochitest);

  // for mochitest-plain, navigating to the url is all we need
  if (!IS_THUNDERBIRD) {
    win.openLinkIn(url, "current", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  }
  if (flavor == "mochitest") {
    return;
  }

  WindowListener.setupWindow(win);
  Services.wm.addListener(WindowListener);

  loadChromeScripts(win);
}

this.mochikit = class extends ExtensionAPI {
  onStartup() {
    let aomStartup = Cc[
      "@mozilla.org/addons/addon-manager-startup;1"
    ].getService(Ci.amIAddonManagerStartup);
    const manifestURI = Services.io.newURI(
      "manifest.json",
      null,
      this.extension.rootURI
    );
    const targetURL = this.extension.rootURI.resolve("content/");
    this.chromeHandle = aomStartup.registerChrome(manifestURI, [
      ["content", "mochikit", targetURL],
    ]);

    if (AppConstants.platform == "android") {
      androidStartup();
    } else {
      let win = Services.wm.getMostRecentWindow(WINDOW_TYPE);
      // wait for event fired from start_desktop.js containing the
      // suite and url to load
      win.addEventListener("mochitest-load", loadMochitest);
    }
  }

  onShutdown() {
    if (AppConstants.platform != "android") {
      for (let win of Services.wm.getEnumerator(WINDOW_TYPE)) {
        WindowListener.tearDownWindow(win);
      }

      Services.wm.removeListener(WindowListener);
    }

    this.chromeHandle.destruct();
    this.chromeHandle = null;
  }
};
