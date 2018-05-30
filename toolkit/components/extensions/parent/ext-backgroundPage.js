"use strict";

ChromeUtils.defineModuleGetter(this, "TelemetryStopwatch",
                               "resource://gre/modules/TelemetryStopwatch.jsm");

ChromeUtils.import("resource://gre/modules/ExtensionParent.jsm");
var {
  HiddenExtensionPage,
  promiseExtensionViewLoaded,
} = ExtensionParent;

XPCOMUtils.defineLazyPreferenceGetter(this, "DELAYED_STARTUP",
                                      "extensions.webextensions.background-delayed-startup");

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
  }

  async build() {
    TelemetryStopwatch.start("WEBEXT_BACKGROUND_PAGE_LOAD_MS", this);
    await this.createBrowserElement();
    this.extension._backgroundPageFrameLoader = this.browser.frameLoader;

    extensions.emit("extension-browser-inserted", this.browser);

    this.browser.loadURI(this.url, {triggeringPrincipal: this.extension.principal});

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
    let {extension} = this;
    let {manifest} = extension;

    this.bgPage = new BackgroundPage(extension, manifest.background);
    if (extension.startupReason !== "APP_STARTUP" || !DELAYED_STARTUP) {
      return this.bgPage.build();
    }

    EventManager.primeListeners(extension);

    extension.once("start-background-page", async () => {
      await this.bgPage.build();
      EventManager.clearPrimedListeners(extension);
    });

    // There are two ways to start the background page:
    // 1. If a primed event fires, then start the background page as
    //    soon as we have painted a browser window.  Note that we have
    //    to touch browserPaintedPromise here to initialize the listener
    //    or else we can miss it if the event occurs after the first
    //    window is painted but before #2
    // 2. After all windows have been restored.
    extension.once("background-page-event", async () => {
      await ExtensionParent.browserPaintedPromise;
      extension.emit("start-background-page");
    });

    ExtensionParent.browserStartupPromise.then(() => {
      extension.emit("start-background-page");
    });
  }

  onShutdown() {
    this.bgPage.shutdown();
  }
};
