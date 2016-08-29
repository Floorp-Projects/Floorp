/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["LegacyExtensionsUtils"];

/* exported LegacyExtensionsUtils, LegacyExtensionContext */

/**
 * This file exports helpers for Legacy Extensions that want to embed a webextensions
 * and exchange messages with the embedded WebExtension.
 */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// Lazy imports.
XPCOMUtils.defineLazyModuleGetter(this, "Extension",
                                  "resource://gre/modules/Extension.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionContext",
                                  "resource://gre/modules/Extension.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

/**
 * Instances created from this class provide to a legacy extension
 * a simple API to exchange messages with a webextension.
 */
var LegacyExtensionContext = class extends ExtensionContext {
  /**
   * Create a new LegacyExtensionContext given a target Extension instance and an optional
   * url (which can be used to recognize the messages of container context).
   *
   * @param {Extension} targetExtension
   *   The webextension instance associated with this context. This will be the
   *   instance of the newly created embedded webextension when this class is
   *   used through the EmbeddedWebExtensionsUtils.
   * @param {Object} [optionalParams]
   *   An object with the following properties:
   * @param {string}  [optionalParams.url]
   *   An URL to mark the messages sent from this context
   *   (e.g. EmbeddedWebExtension sets it to the base url of the container addon).
   */
  constructor(targetExtension, optionalParams = {}) {
    let {url} = optionalParams;

    super(targetExtension, {
      contentWindow: null,
      uri: NetUtil.newURI(url || "about:blank"),
      type: "legacy_extension",
    });

    // Legacy Extensions (xul overlays, bootstrap restartless and Addon SDK)
    // runs with a systemPrincipal.
    let addonPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
    Object.defineProperty(
      this, "principal",
      {value: addonPrincipal, enumerable: true, configurable: true}
    );

    let cloneScope = Cu.Sandbox(this.principal, {});
    Cu.setSandboxMetadata(cloneScope, {addonId: targetExtension.id});
    Object.defineProperty(
      this, "cloneScope",
      {value: cloneScope, enumerable: true, configurable: true, writable: true}
    );

    this.api = {
      browser: {
        runtime: {
          onConnect: this.messenger.onConnect("runtime.onConnect"),
          onMessage: this.messenger.onMessage("runtime.onMessage"),
        },
      },
    };
  }

  /**
   * This method is called when the extension shuts down or is unloaded,
   * and it nukes the cloneScope sandbox, if any.
   */
  unload() {
    if (this.unloaded) {
      throw new Error("Error trying to unload LegacyExtensionContext twice.");
    }
    super.unload();
    Cu.nukeSandbox(this.cloneScope);
    this.cloneScope = null;
  }

  /**
   * The LegacyExtensionContext is not a visible context.
   */
  get externallyVisible() {
    return false;
  }
};

var EmbeddedExtensionManager;

/**
 * Instances of this class are used internally by the exported EmbeddedWebExtensionsUtils
 * to manage the embedded webextension instance and the related LegacyExtensionContext
 * instance used to exchange messages with it.
 */
class EmbeddedExtension {
  /**
   * Create a new EmbeddedExtension given the add-on id and the base resource URI of the
   * container add-on (the webextension resources will be loaded from the "webextension/"
   * subdir of the base resource URI for the legacy extension add-on).
   *
   * @param {Object} containerAddonParams
   *   An object with the following properties:
   * @param {string} containerAddonParams.id
   *   The Add-on id of the Legacy Extension which will contain the embedded webextension.
   * @param {nsIURI} containerAddonParams.resourceURI
   *   The nsIURI of the Legacy Extension container add-on.
   */
  constructor({id, resourceURI}) {
    this.addonId = id;
    this.resourceURI = resourceURI;

    // Setup status flag.
    this.started = false;
  }

  /**
   * Start the embedded webextension.
   *
   * @returns {Promise<LegacyContextAPI>} A promise which resolve to the API exposed to the
   *   legacy context.
   */
  startup() {
    if (this.started) {
      return Promise.reject(new Error("This embedded extension has already been started"));
    }

    // Setup the startup promise.
    this.startupPromise = new Promise((resolve, reject) => {
      let embeddedExtensionURI = Services.io.newURI("webextension/", null, this.resourceURI);

      // This is the instance of the WebExtension embedded in the hybrid add-on.
      this.extension = new Extension({
        id: this.addonId,
        resourceURI: embeddedExtensionURI,
      });

      // This callback is register to the "startup" event, emitted by the Extension instance
      // after the extension manifest.json has been loaded without any errors, but before
      // starting any of the defined contexts (which give the legacy part a chance to subscribe
      // runtime.onMessage/onConnect listener before the background page has been loaded).
      const onBeforeStarted = () => {
        this.extension.off("startup", onBeforeStarted);

        // Resolve the startup promise and reset the startupError.
        this.started = true;
        this.startupPromise = null;

        // Create the legacy extension context, the legacy container addon
        // needs to use it before the embedded webextension startup,
        // because it is supposed to be used during the legacy container startup
        // to subscribe its message listeners (which are supposed to be able to
        // receive any message that the embedded part can try to send to it
        // during its startup).
        this.context = new LegacyExtensionContext(this.extension, {
          url: this.resourceURI.resolve("/"),
        });

        // Destroy the LegacyExtensionContext cloneScope when
        // the embedded webextensions is unloaded.
        this.extension.callOnClose({
          close: () => {
            this.context.unload();
          },
        });

        // resolve startupPromise to execute any pending shutdown that has been
        // chained to it.
        resolve(this.context.api);
      };

      this.extension.on("startup", onBeforeStarted);

      // Run ambedded extension startup and catch any error during embedded extension
      // startup.
      this.extension.startup().catch((err) => {
        this.started = false;
        this.startupPromise = null;
        this.extension.off("startup", onBeforeStarted);

        reject(err);
      });
    });

    return this.startupPromise;
  }

  /**
   * Shuts down the embedded webextension.
   *
   * @returns {Promise<void>} a promise that is resolved when the shutdown has been done
   */
  shutdown() {
    EmbeddedExtensionManager.untrackEmbeddedExtension(this);

    // If there is a pending startup,  wait to be completed and then shutdown.
    if (this.startupPromise) {
      return this.startupPromise.then(() => {
        this.extension.shutdown();
      });
    }

    // Run shutdown now if the embedded webextension has been correctly started
    if (this.extension && this.started && !this.extension.hasShutdown) {
      this.extension.shutdown();
    }

    return Promise.resolve();
  }
}

// Keep track on the created EmbeddedExtension instances and destroy
// them when their container addon is going to be disabled or uninstalled.
EmbeddedExtensionManager = {
  // Map of the existent EmbeddedExtensions instances by addon id.
  embeddedExtensionsByAddonId: new Map(),

  untrackEmbeddedExtension(embeddedExtensionInstance) {
    // Remove this instance from the tracked embedded extensions
    let id = embeddedExtensionInstance.addonId;
    if (this.embeddedExtensionsByAddonId.get(id) == embeddedExtensionInstance) {
      this.embeddedExtensionsByAddonId.delete(id);
    }
  },

  getEmbeddedExtensionFor({id, resourceURI}) {
    let embeddedExtension = this.embeddedExtensionsByAddonId.get(id);

    if (!embeddedExtension) {
      embeddedExtension = new EmbeddedExtension({id, resourceURI});
      // Keep track of the embedded extension instance.
      this.embeddedExtensionsByAddonId.set(id, embeddedExtension);
    }

    return embeddedExtension;
  },
};

this.LegacyExtensionsUtils = {
  getEmbeddedExtensionFor: (addon) => {
    return EmbeddedExtensionManager.getEmbeddedExtensionFor(addon);
  },
};
