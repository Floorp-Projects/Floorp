"use strict";

var {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");

// WeakMap[Extension -> BackgroundPage]
var backgroundPagesMap = new WeakMap();

// Responsible for the background_page section of the manifest.
function BackgroundPage(options, extension) {
  this.extension = extension;
  this.scripts = options.scripts || [];
  this.page = options.page || null;
  this.contentWindow = null;
  this.chromeWebNav = null;
  this.webNav = null;
  this.context = null;
}

BackgroundPage.prototype = {
  build() {
    let chromeWebNav = Services.appShell.createWindowlessBrowser(true);
    this.chromeWebNav = chromeWebNav;

    let url;
    if (this.page) {
      url = this.extension.baseURI.resolve(this.page);
    } else {
      // TODO: Chrome uses "_generated_background_page.html" for this.
      url = this.extension.baseURI.resolve("_blank.html");
    }

    if (!this.extension.isExtensionURL(url)) {
      this.extension.manifestError("Background page must be a file within the extension");
      url = this.extension.baseURI.resolve("_blank.html");
    }

    let uri = Services.io.newURI(url, null, null);

    let system = Services.scriptSecurityManager.getSystemPrincipal();
    let interfaceRequestor = chromeWebNav.QueryInterface(Ci.nsIInterfaceRequestor);
    let chromeShell = interfaceRequestor.getInterface(Ci.nsIDocShell);
    chromeShell.createAboutBlankContentViewer(system);

    let chromeDoc = chromeWebNav.document;
    const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    let browser = chromeDoc.createElementNS(XUL_NS, "browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("disableglobalhistory", "true");
    chromeDoc.body.appendChild(browser);

    let frameLoader = browser.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader;
    let docShell = frameLoader.docShell;

    this.context = new ExtensionContext(this.extension, {type: "background", docShell, uri});
    GlobalManager.injectInDocShell(docShell, this.extension, this.context);

    let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    this.webNav = webNav;

    webNav.loadURI(url, 0, null, null, null);

    let window = webNav.document.defaultView;
    this.contentWindow = window;
    this.context.contentWindow = window;

    if (this.extension.addonData.instanceID) {
      AddonManager.getAddonByInstanceID(this.extension.addonData.instanceID)
                  .then(addon => addon.setDebugGlobal(window));
    }

    // TODO: Right now we run onStartup after the background page
    // finishes. See if this is what Chrome does.
    let loadListener = event => {
      // Override the `alert()` method inside background windows;
      // we alias it to console.log().
      // See: https://bugzilla.mozilla.org/show_bug.cgi?id=1203394
      let alertDisplayedWarning = false;
      let alertOverwrite = text => {
        if (!alertDisplayedWarning) {
          let consoleWindow = Services.wm.getMostRecentWindow("devtools:webconsole");
          if (!consoleWindow) {
            let {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
            require("devtools/client/framework/devtools-browser");
            let hudservice = require("devtools/client/webconsole/hudservice");
            hudservice.toggleBrowserConsole().catch(Cu.reportError);
          } else {
            // the Browser Console was already open
            consoleWindow.focus();
          }

          this.context.contentWindow.console.warn("alert() is not supported in background windows; please use console.log instead.");

          alertDisplayedWarning = true;
        }

        window.console.log(text);
      };
      Components.utils.exportFunction(alertOverwrite, window, {
        defineAs: "alert",
      });
      if (event.target != window.document) {
        return;
      }
      event.currentTarget.removeEventListener("load", loadListener, true);

      if (this.scripts) {
        let doc = window.document;
        for (let script of this.scripts) {
          let tag = doc.createElement("script");
          tag.setAttribute("src", script);
          tag.async = false;
          doc.body.appendChild(tag);
        }
      }

      if (this.extension.onStartup) {
        this.extension.onStartup();
      }
    };
    browser.addEventListener("load", loadListener, true);
  },

  shutdown() {
    // Navigate away from the background page to invalidate any
    // setTimeouts or other callbacks.
    this.webNav.loadURI("about:blank", 0, null, null, null);
    this.webNav = null;

    this.chromeWebNav.loadURI("about:blank", 0, null, null, null);
    this.chromeWebNav.close();
    this.chromeWebNav = null;

    if (this.extension.addonData.instanceID) {
      AddonManager.getAddonByInstanceID(this.extension.addonData.instanceID)
                  .then(addon => addon.setDebugGlobal(null));
    }
  },
};

/* eslint-disable mozilla/balanced-listeners */
extensions.on("manifest_background", (type, directive, extension, manifest) => {
  let bgPage = new BackgroundPage(manifest.background, extension);
  bgPage.build();
  backgroundPagesMap.set(extension, bgPage);
});

extensions.on("shutdown", (type, extension) => {
  if (backgroundPagesMap.has(extension)) {
    backgroundPagesMap.get(extension).shutdown();
    backgroundPagesMap.delete(extension);
  }
});
/* eslint-enable mozilla/balanced-listeners */

extensions.registerSchemaAPI("extension", null, (extension, context) => {
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
