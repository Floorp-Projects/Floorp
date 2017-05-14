"use strict";

Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");

Cu.import("resource://gre/modules/ExtensionParent.jsm");
var {
  HiddenExtensionPage,
  promiseExtensionViewLoaded,
} = ExtensionParent;

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

  async build() {
    await this.createBrowserElement();

    extensions.emit("extension-browser-inserted", this.browser);

    this.browser.loadURI(this.url);

    let context = await promiseExtensionViewLoaded(this.browser);

    if (this.browser.docShell) {
      this.webNav = this.browser.docShell.QueryInterface(Ci.nsIWebNavigation);
      let window = this.webNav.document.defaultView;

      // Set the add-on's main debugger global, for use in the debugger
      // console.
      if (this.extension.addonData.instanceID) {
        AddonManager.getAddonByInstanceID(this.extension.addonData.instanceID)
                    .then(addon => addon && addon.setDebugGlobal(window));
      }
    }

    if (context) {
      // Wait until all event listeners registered by the script so far
      // to be handled.
      await Promise.all(context.listenerPromises);
      context.listenerPromises = null;
    }

    this.extension.emit("startup");
  }

  shutdown() {
    if (this.extension.addonData.instanceID) {
      AddonManager.getAddonByInstanceID(this.extension.addonData.instanceID)
                  .then(addon => addon && addon.setDebugGlobal(null));
    }

    super.shutdown();
  }
}

this.backgroundPage = class extends ExtensionAPI {
  onManifestEntry(entryName) {
    let {manifest} = this.extension;

    this.bgPage = new BackgroundPage(this.extension, manifest.background);

    return this.bgPage.build();
  }

  onShutdown() {
    this.bgPage.shutdown();
  }
};
