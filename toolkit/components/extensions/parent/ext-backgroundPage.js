/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ExtensionParent } = ChromeUtils.import(
  "resource://gre/modules/ExtensionParent.jsm"
);
var {
  HiddenExtensionPage,
  promiseExtensionViewLoaded,
  watchExtensionWorkerContextLoaded,
} = ExtensionParent;

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionTelemetry",
  "resource://gre/modules/ExtensionTelemetry.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "DELAYED_STARTUP",
  "extensions.webextensions.background-delayed-startup"
);

XPCOMUtils.defineLazyGetter(this, "serviceWorkerManager", () => {
  return Cc["@mozilla.org/serviceworkers/manager;1"].getService(
    Ci.nsIServiceWorkerManager
  );
});

// Responsible for the background_page section of the manifest.
class BackgroundPage extends HiddenExtensionPage {
  constructor(extension, options) {
    super(extension, "background");

    this.page = options.page || null;
    this.isGenerated = !!options.scripts;

    if (this.page) {
      this.url = this.extension.baseURI.resolve(this.page);
    } else if (this.isGenerated) {
      this.url = this.extension.baseURI.resolve(
        "_generated_background_page.html"
      );
    }
  }

  async build() {
    const { extension } = this;

    ExtensionTelemetry.backgroundPageLoad.stopwatchStart(extension, this);

    let context;
    try {
      await this.createBrowserElement();
      if (!this.browser) {
        throw new Error(
          "Extension shut down before the background page was created"
        );
      }
      extension._backgroundPageFrameLoader = this.browser.frameLoader;

      extensions.emit("extension-browser-inserted", this.browser);

      let contextPromise = promiseExtensionViewLoaded(this.browser);
      this.browser.loadURI(this.url, {
        triggeringPrincipal: extension.principal,
      });

      context = await contextPromise;
    } catch (e) {
      // Extension was down before the background page has loaded.
      Cu.reportError(e);
      ExtensionTelemetry.backgroundPageLoad.stopwatchCancel(extension, this);
      if (extension.persistentListeners) {
        EventManager.clearPrimedListeners(this.extension, false);
      }
      extension.emit("background-script-aborted");
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

    extension.emit("background-script-started");
  }

  shutdown() {
    this.extension._backgroundPageFrameLoader = null;
    super.shutdown();
  }
}

// Responsible for the background.service_worker section of the manifest.
class BackgroundWorker {
  constructor(extension, options) {
    this.extension = extension;
    this.workerScript = options.service_worker;

    if (!this.workerScript) {
      throw new Error("Missing mandatory background.service_worker property");
    }
  }

  get registrationInfo() {
    const { principal } = this.extension;
    return serviceWorkerManager.getRegistrationForAddonPrincipal(principal);
  }

  getWorkerInfo(descriptorId) {
    return this.registrationInfo?.getWorkerByID(descriptorId);
  }

  validateWorkerInfoForContext(context) {
    const { extension } = this;
    if (!this.getWorkerInfo(context.workerDescriptorId)) {
      throw new Error(
        `ServiceWorkerInfo not found for ${extension.policy.debugName} contextId ${context.contextId}`
      );
    }
  }

  async build() {
    const { extension } = this;

    let context;
    try {
      const contextPromise = new Promise(resolve => {
        let unwatch = watchExtensionWorkerContextLoaded(
          { extension, viewType: "background_worker" },
          context => {
            unwatch();
            this.validateWorkerInfoForContext(context);
            resolve(context);
          }
        );
      });

      // TODO(Bug 17228327): follow up to spawn the active worker for a previously installed
      // background service worker.
      await serviceWorkerManager.registerForAddonPrincipal(
        this.extension.principal
      );

      context = await contextPromise;

      await this.waitForActiveWorker();
    } catch (e) {
      // Extension may be shutting down before the background worker has registered or
      // loaded.
      Cu.reportError(e);

      if (extension.persistentListeners) {
        EventManager.clearPrimedListeners(this.extension, false);
      }

      extension.emit("background-script-aborted");
      return;
    }

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

    extension.emit("background-script-started");
  }

  shutdown(isAppShutdown) {
    // All service worker registrations related to the extensions will be unregistered
    // - when the extension is shutting down if the application is not also shutting down
    //   shutdown (in which case a previously registered service worker is expected to stay
    //   active across browser restarts).
    // - when the extension has been uninstalled
    if (!isAppShutdown) {
      this.registrationInfo?.forceShutdown();
    }
  }

  waitForActiveWorker() {
    const { extension, registrationInfo } = this;
    return new Promise((resolve, reject) => {
      const resolveOnActive = () => {
        if (
          registrationInfo.activeWorker?.state ===
          Ci.nsIServiceWorkerInfo.STATE_ACTIVATED
        ) {
          resolve();
          return true;
        }
        return false;
      };

      const rejectOnUnregistered = () => {
        if (registrationInfo.unregistered) {
          reject(
            new Error(
              `Background service worker unregistered for "${extension.policy.debugName}"`
            )
          );
          return true;
        }
        return false;
      };

      if (resolveOnActive() || rejectOnUnregistered()) {
        return;
      }

      const listener = {
        onChange() {
          if (resolveOnActive() || rejectOnUnregistered()) {
            registrationInfo.removeListener(listener);
          }
        },
      };
      registrationInfo.addListener(listener);
    });
  }
}

this.backgroundPage = class extends ExtensionAPI {
  async build() {
    if (this.bgInstance) {
      return;
    }

    let { extension } = this;
    let { manifest } = extension;

    let BackgroundClass = manifest.background.service_worker
      ? BackgroundWorker
      : BackgroundPage;

    this.bgInstance = new BackgroundClass(extension, manifest.background);
    return this.bgInstance.build();
  }

  async primeBackground(isInStartup = true) {
    let { extension } = this;

    if (this.bgInstance) {
      Cu.reportError(`background script exists before priming ${extension.id}`);
    }
    this.bgInstance = null;

    // When in PPB background pages all run in a private context.  This check
    // simply avoids an extraneous error in the console since the BaseContext
    // will throw.
    if (
      PrivateBrowsingUtils.permanentPrivateBrowsing &&
      !extension.privateBrowsingAllowed
    ) {
      return;
    }

    // Used by runtime messaging to wait for background page listeners.
    let bgStartupPromise = new Promise(resolve => {
      let done = () => {
        extension.off("background-script-started", done);
        extension.off("background-script-aborted", done);
        extension.off("shutdown", done);
        resolve();
      };
      extension.on("background-script-started", done);
      extension.on("background-script-aborted", done);
      extension.on("shutdown", done);
    });

    extension.wakeupBackground = () => {
      extension.emit("background-script-event");
      extension.wakeupBackground = () => bgStartupPromise;
      return bgStartupPromise;
    };

    extension.terminateBackground = async () => {
      await bgStartupPromise;
      this.onShutdown(false);
      EventManager.clearPrimedListeners(this.extension, false);
      // Setup background startup listeners for next primed event.
      return this.primeBackground(false);
    };

    extension.once("terminate-background-script", async () => {
      if (!this.extension) {
        // Extension was already shut down.
        return;
      }
      this.extension.terminateBackground();
    });

    // Persistent backgrounds are started immediately except during APP_STARTUP.
    // Non-persistent backgrounds must be started immediately for new install or enable
    // to initialize the addon and create the persisted listeners.
    if (
      isInStartup &&
      (!DELAYED_STARTUP ||
        (extension.persistentBackground &&
          extension.startupReason !== "APP_STARTUP") ||
        ["ADDON_INSTALL", "ADDON_ENABLE"].includes(extension.startupReason))
    ) {
      return this.build();
    }

    EventManager.primeListeners(extension, isInStartup);

    extension.once("start-background-script", async () => {
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
    // 2. After all windows have been restored on startup (see onManifestEntry).
    extension.once("background-script-event", async () => {
      await ExtensionParent.browserPaintedPromise;
      extension.emit("start-background-script");
    });
  }

  onShutdown(isAppShutdown) {
    if (this.bgInstance) {
      this.bgInstance.shutdown(isAppShutdown);
      this.bgInstance = null;
      this.extension.emit("shutdown-background-script");
    } else {
      EventManager.clearPrimedListeners(this.extension, false);
    }
  }

  async onManifestEntry(entryName) {
    let { extension } = this;

    await this.primeBackground();

    ExtensionParent.browserStartupPromise.then(() => {
      // If the background has been created earlier than session restore,
      // we do not want to continue with creating it here.
      if (this.bgInstance) {
        return;
      }

      // If there are no listeners for the extension that were persisted, we need to
      // start the event page so they can be registered.
      if (
        extension.persistentBackground ||
        !extension.persistentListeners?.size
      ) {
        extension.emit("start-background-script");
      } else {
        // During startup we only prime startup blocking listeners.  At
        // this stage we need to prime all listeners for event pages.
        EventManager.clearPrimedListeners(extension, false);
        // Allow re-priming by deleting existing listeners.
        extension.persistentListeners = null;
        EventManager.primeListeners(extension, false);
      }
    });
  }
};
