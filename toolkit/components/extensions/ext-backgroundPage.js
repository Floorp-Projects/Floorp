"use strict";

Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStopwatch",
                                  "resource://gre/modules/TelemetryStopwatch.jsm");

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
    TelemetryStopwatch.start("WEBEXT_BACKGROUND_PAGE_LOAD_MS", this);
    await this.createBrowserElement();
    this.extension._backgroundPageFrameLoader = this.browser.frameLoader;

    extensions.emit("extension-browser-inserted", this.browser);

    this.browser.loadURI(this.url);

    let context = await promiseExtensionViewLoaded(this.browser);
    TelemetryStopwatch.finish("WEBEXT_BACKGROUND_PAGE_LOAD_MS", this);

    if (context) {
      // Wait until all event listeners registered by the script so far
      // to be handled.
      await Promise.all(context.listenerPromises);
      context.listenerPromises = null;
    }

    this.extension.emit("startup");
  }

  shutdown() {
    this.extension._backgroundPageFrameLoader = null;
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
