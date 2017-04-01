"use strict";

var {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");

Cu.import("resource://gre/modules/ExtensionParent.jsm");
const {
  HiddenExtensionPage,
  promiseExtensionViewLoaded,
} = ExtensionParent;

// WeakMap[Extension -> BackgroundPage]
var backgroundPagesMap = new WeakMap();

// Responsible for the background_page section of the manifest.
class BackgroundPage extends HiddenExtensionPage {
  constructor(extension, options) {
    super(extension, "background");

    this.page = options.page || null;
    this.isGenerated = !!options.scripts;
    this.webNav = null;

    if (this.page) {
      this.url = this.extension.baseURI.resolve(this.page);
    } else if (this.isGenerated) {
      this.url = this.extension.baseURI.resolve("_generated_background_page.html");
    }

    if (!this.extension.isExtensionURL(this.url)) {
      this.extension.manifestError("Background page must be a file within the extension");
      this.url = this.extension.baseURI.resolve("_blank.html");
    }
  }

  build() {
    return Task.spawn(function* () {
      yield this.createBrowserElement();

      extensions.emit("extension-browser-inserted", this.browser);

      this.browser.loadURI(this.url);

      yield promiseExtensionViewLoaded(this.browser);

      if (this.browser.docShell) {
        this.webNav = this.browser.docShell.QueryInterface(Ci.nsIWebNavigation);
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

  shutdown() {
    if (this.extension.addonData.instanceID) {
      AddonManager.getAddonByInstanceID(this.extension.addonData.instanceID)
                  .then(addon => addon.setDebugGlobal(null));
    }

    super.shutdown();
  }
}

/* eslint-disable mozilla/balanced-listeners */
extensions.on("manifest_background", (type, directive, extension, manifest) => {
  let bgPage = new BackgroundPage(extension, manifest.background);

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
