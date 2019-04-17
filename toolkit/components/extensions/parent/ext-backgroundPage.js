"use strict";

var {ExtensionParent} = ChromeUtils.import("resource://gre/modules/ExtensionParent.jsm");
var {
  HiddenExtensionPage,
  promiseExtensionViewLoaded,
} = ExtensionParent;


ChromeUtils.defineModuleGetter(this, "ExtensionTelemetry",
                               "resource://gre/modules/ExtensionTelemetry.jsm");
ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
                               "resource://gre/modules/PrivateBrowsingUtils.jsm");

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
    const {extension} = this;

    ExtensionTelemetry.backgroundPageLoad.stopwatchStart(extension, this);

    let context;
    try {
      await this.createBrowserElement();
      if (!this.browser) {
        throw new Error("Extension shut down before the background page was created");
      }
      extension._backgroundPageFrameLoader = this.browser.frameLoader;

      extensions.emit("extension-browser-inserted", this.browser);

      let contextPromise = promiseExtensionViewLoaded(this.browser);
      this.browser.loadURI(this.url, {triggeringPrincipal: extension.principal});

      context = await contextPromise;
    } catch (e) {
      // Extension was down before the background page has loaded.
      Cu.reportError(e);
      ExtensionTelemetry.backgroundPageLoad.stopwatchCancel(extension, this);
      extension.emit("background-page-aborted");
      return;
    }

    ExtensionTelemetry.backgroundPageLoad.stopwatchFinish(extension, this);

    if (context) {
      // Wait until all event listeners registered by the script so far
      // to be handled.
      await Promise.all(context.listenerPromises);
      context.listenerPromises = null;
    }

    if (extension.persistentListeners) {
      // |this.extension| may be null if the extension was shut down.
      // In that case, we still want to clear the primed listeners,
      // but not update the persistent listeners in the startupData.
      EventManager.clearPrimedListeners(extension, !!this.extension);
    }

    extension.emit("startup");
  }

  shutdown() {
    this.extension._backgroundPageFrameLoader = null;
    super.shutdown();
  }
}

this.backgroundPage = class extends ExtensionAPI {
  build() {
    if (this.bgPage) {
      return;
    }

    let {extension} = this;
    let {manifest} = extension;

    this.bgPage = new BackgroundPage(extension, manifest.background);
    return this.bgPage.build();
  }

  onManifestEntry(entryName) {
    let {extension} = this;

    this.bgPage = null;

    // When in PPB background pages all run in a private context.  This check
    // simply avoids an extraneous error in the console since the BaseContext
    // will throw.
    if (PrivateBrowsingUtils.permanentPrivateBrowsing && !extension.privateBrowsingAllowed) {
      return;
    }

    if (extension.startupReason !== "APP_STARTUP" || !DELAYED_STARTUP) {
      return this.build();
    }

    EventManager.primeListeners(extension);

    extension.once("start-background-page", async () => {
      if (!this.extension) {
        // Extension was shut down. Don't build the background page.
        // Primed listeners have been cleared in onShutdown.
        return;
      }
      await this.build();
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
    if (this.bgPage) {
      this.bgPage.shutdown();
    } else {
      EventManager.clearPrimedListeners(this.extension, false);
    }
  }
};
