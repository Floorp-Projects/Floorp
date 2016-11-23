"use strict";

var {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
const {
  promiseDocumentLoaded,
  promiseEvent,
  promiseObserved,
} = ExtensionUtils;

const XUL_URL = "data:application/vnd.mozilla.xul+xml;charset=utf-8," + encodeURI(
  `<?xml version="1.0"?>
  <window id="documentElement"/>`);

// WeakMap[Extension -> BackgroundPage]
var backgroundPagesMap = new WeakMap();

// Responsible for the background_page section of the manifest.
class BackgroundPageBase {
  constructor(options, extension) {
    this.extension = extension;
    this.page = options.page || null;
    this.isGenerated = !!options.scripts;
    this.webNav = null;
  }

  build() {
    return Task.spawn(function* () {
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

      let chromeDoc = yield this.getParentDocument();

      let browser = chromeDoc.createElement("browser");
      browser.setAttribute("type", "content");
      browser.setAttribute("disableglobalhistory", "true");
      browser.setAttribute("webextension-view-type", "background");

      let awaitFrameLoader;
      if (this.extension.remote) {
        browser.setAttribute("remote", "true");
        awaitFrameLoader = promiseEvent(browser, "XULFrameLoaderCreated");
      }

      chromeDoc.documentElement.appendChild(browser);
      yield awaitFrameLoader;

      this.browser = browser;

      extensions.emit("extension-browser-inserted", browser);

      browser.loadURI(url);

      yield new Promise(resolve => {
        browser.messageManager.addMessageListener("Extension:ExtensionViewLoaded", function onLoad() {
          browser.messageManager.removeMessageListener("Extension:ExtensionViewLoaded", onLoad);
          resolve();
        });
      });

      if (browser.docShell) {
        this.webNav = browser.docShell.QueryInterface(Ci.nsIWebNavigation);
        let window = this.webNav.document.defaultView;


        // Set the add-on's main debugger global, for use in the debugger
        // console.
        if (this.extension.addonData.instanceID) {
          AddonManager.getAddonByInstanceID(this.extension.addonData.instanceID)
                      .then(addon => addon.setDebugGlobal(window));
        }
      }

      this.extension.emit("startup");
    }.bind(this));
  }

  initParentWindow(chromeShell) {
    if (PrivateBrowsingUtils.permanentPrivateBrowsing) {
      let attrs = chromeShell.getOriginAttributes();
      attrs.privateBrowsingId = 1;
      chromeShell.setOriginAttributes(attrs);
    }

    let system = Services.scriptSecurityManager.getSystemPrincipal();
    chromeShell.createAboutBlankContentViewer(system);
    chromeShell.useGlobalHistory = false;
    chromeShell.loadURI(XUL_URL, 0, null, null, null);

    return promiseObserved("chrome-document-global-created",
                           win => win.document == chromeShell.document);
  }

  shutdown() {
    if (this.extension.addonData.instanceID) {
      AddonManager.getAddonByInstanceID(this.extension.addonData.instanceID)
                  .then(addon => addon.setDebugGlobal(null));
    }

    if (this.browser) {
      this.browser.remove();
      this.browser = null;
    }

    // Navigate away from the background page to invalidate any
    // setTimeouts or other callbacks.
    if (this.webNav) {
      this.webNav.loadURI("about:blank", 0, null, null, null);
      this.webNav = null;
    }
  }
}

/**
 * A background page loaded into a windowless browser, with no on-screen
 * representation or graphical display abilities.
 *
 * This currently does not support remote browsers, and therefore cannot
 * be used with out-of-process extensions.
 */
class WindowlessBackgroundPage extends BackgroundPageBase {
  constructor(options, extension) {
    super(options, extension);
    this.windowlessBrowser = null;
  }

  getParentDocument() {
    return Task.spawn(function* () {
      let windowlessBrowser = Services.appShell.createWindowlessBrowser(true);
      this.windowlessBrowser = windowlessBrowser;

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

      yield this.initParentWindow(chromeShell);

      return promiseDocumentLoaded(windowlessBrowser.document);
    }.bind(this));
  }

  shutdown() {
    super.shutdown();

    this.windowlessBrowser.loadURI("about:blank", 0, null, null, null);
    this.windowlessBrowser.close();
    this.windowlessBrowser = null;
  }
}

/**
 * A background page loaded into a visible dialog window. Only to be
 * used for debugging, and in temporary, test-only use for
 * out-of-process extensions.
 */
class WindowedBackgroundPage extends BackgroundPageBase {
  constructor(options, extension) {
    super(options, extension);
    this.parentWindow = null;
  }

  getParentDocument() {
    return Task.spawn(function* () {
      let window = Services.ww.openWindow(null, "about:blank", "_blank",
                                          "chrome,alwaysLowered,dialog", null);

      this.parentWindow = window;

      let chromeShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIDocShell)
                              .QueryInterface(Ci.nsIWebNavigation);

      yield this.initParentWindow(chromeShell);

      window.minimize();

      return promiseDocumentLoaded(window.document);
    }.bind(this));
  }

  shutdown() {
    super.shutdown();

    if (this.parentWindow) {
      this.parentWindow.close();
      this.parentWindow = null;
    }
  }
}

/* eslint-disable mozilla/balanced-listeners */
extensions.on("manifest_background", (type, directive, extension, manifest) => {
  let bgPage;
  if (extension.remote) {
    bgPage = new WindowedBackgroundPage(manifest.background, extension);
  } else {
    bgPage = new WindowlessBackgroundPage(manifest.background, extension);
  }

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
