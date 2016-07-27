"use strict";

var {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
const {
  promiseDocumentLoaded,
  promiseObserved,
} = ExtensionUtils;

const XUL_URL = "data:application/vnd.mozilla.xul+xml;charset=utf-8," + encodeURI(
  `<?xml version="1.0"?>
  <window id="documentElement"/>`);

// WeakMap[Extension -> BackgroundPage]
var backgroundPagesMap = new WeakMap();

// Responsible for the background_page section of the manifest.
function BackgroundPage(options, extension) {
  this.extension = extension;
  this.page = options.page || null;
  this.isGenerated = !!options.scripts;
  this.contentWindow = null;
  this.windowlessBrowser = null;
  this.webNav = null;
  this.context = null;
}

BackgroundPage.prototype = {
  build: Task.async(function* () {
    let windowlessBrowser = Services.appShell.createWindowlessBrowser(true);
    this.windowlessBrowser = windowlessBrowser;

    let url;
    if (this.page) {
      url = this.extension.baseURI.resolve(this.page);
    } else if (this.isGenerated) {
      url = this.extension.baseURI.resolve("_generated_background_page.html");
    }

    if (!this.extension.isExtensionURL(url)) {
      this.extension.manifestError("Background page must be a file within the extension");
      url = this.extension.baseURI.resolve("_blank.html");
    }

    let system = Services.scriptSecurityManager.getSystemPrincipal();

    // The windowless browser is a thin wrapper around a docShell that keeps
    // its related resources alive. It implements nsIWebNavigation and
    // forwards its methods to the underlying docShell, but cannot act as a
    // docShell itself. Calling `getInterface(nsIDocShell)` gives us the
    // underlying docShell, and `QueryInterface(nsIWebNavigation)` gives us
    // access to the webNav methods that are already available on the
    // windowless browser, but contrary to appearances, they are not the same
    // object.
    let chromeShell = windowlessBrowser.QueryInterface(Ci.nsIInterfaceRequestor)
                                       .getInterface(Ci.nsIDocShell)
                                       .QueryInterface(Ci.nsIWebNavigation);

    chromeShell.createAboutBlankContentViewer(system);
    chromeShell.loadURI(XUL_URL, 0, null, null, null);


    yield promiseObserved("chrome-document-global-created",
                          win => win.document == chromeShell.document);

    let chromeDoc = yield promiseDocumentLoaded(chromeShell.document);

    let browser = chromeDoc.createElement("browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("disableglobalhistory", "true");
    browser.setAttribute("webextension-view-type", "background");
    browser.setAttribute("src", url);
    chromeDoc.documentElement.appendChild(browser);


    yield new Promise(resolve => {
      browser.addEventListener("load", function onLoad(event) {
        if (event.target === browser.contentDocument) {
          browser.removeEventListener("load", onLoad, true);
          resolve();
        }
      }, true);
    });

    this.webNav = browser.docShell.QueryInterface(Ci.nsIWebNavigation);

    let window = this.webNav.document.defaultView;
    this.contentWindow = window;


    // Set the add-on's main debugger global, for use in the debugger
    // console.
    if (this.extension.addonData.instanceID) {
      AddonManager.getAddonByInstanceID(this.extension.addonData.instanceID)
                  .then(addon => addon.setDebugGlobal(window));
    }

    // TODO(robwu): This implementation of onStartup is wrong, see
    // https://bugzil.la/1247435#c1
    if (this.extension.onStartup) {
      this.extension.onStartup();
    }
  }),

  shutdown() {
    if (this.extension.addonData.instanceID) {
      AddonManager.getAddonByInstanceID(this.extension.addonData.instanceID)
                  .then(addon => addon.setDebugGlobal(null));
    }

    // Navigate away from the background page to invalidate any
    // setTimeouts or other callbacks.
    if (this.webNav) {
      this.webNav.loadURI("about:blank", 0, null, null, null);
      this.webNav = null;
    }

    this.windowlessBrowser.loadURI("about:blank", 0, null, null, null);
    this.windowlessBrowser.close();
    this.windowlessBrowser = null;
  },
};

/* eslint-disable mozilla/balanced-listeners */
extensions.on("manifest_background", (type, directive, extension, manifest) => {
  let bgPage = new BackgroundPage(manifest.background, extension);
  backgroundPagesMap.set(extension, bgPage);
  return bgPage.build();
});

extensions.on("shutdown", (type, extension) => {
  if (backgroundPagesMap.has(extension)) {
    backgroundPagesMap.get(extension).shutdown();
    backgroundPagesMap.delete(extension);
  }
});
/* eslint-enable mozilla/balanced-listeners */

extensions.registerSchemaAPI("extension", (extension, context) => {
  return {
    extension: {
      getBackgroundPage: function() {
        return backgroundPagesMap.get(extension).contentWindow;
      },
    },

    runtime: {
      getBackgroundPage() {
        return context.cloneScope.Promise.resolve(backgroundPagesMap.get(extension).contentWindow);
      },
    },
  };
});
