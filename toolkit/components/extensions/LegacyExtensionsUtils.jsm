/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["LegacyExtensionsUtils"];

/* exported LegacyExtensionsUtils, LegacyExtensionContext */

/**
 * This file exports helpers for Legacy Extensions that want to embed a webextensions
 * and exchange messages with the embedded WebExtension.
 */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "Extension",
                               "resource://gre/modules/Extension.jsm");
ChromeUtils.defineModuleGetter(this, "ExtensionChild",
                               "resource://gre/modules/ExtensionChild.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");

ChromeUtils.import("resource://gre/modules/ExtensionCommon.jsm");

var {
  BaseContext,
} = ExtensionCommon;

/**
 * Instances created from this class provide to a legacy extension
 * a simple API to exchange messages with a webextension.
 */
var LegacyExtensionContext = class extends BaseContext {
  /**
   * Create a new LegacyExtensionContext given a target Extension instance.
   *
   * @param {Extension} targetExtension
   *   The webextension instance associated with this context. This will be the
   *   instance of the newly created embedded webextension when this class is
   *   used through the EmbeddedWebExtensionsUtils.
   */
  constructor(targetExtension) {
    super("legacy_extension", targetExtension);

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

    let sender = {id: targetExtension.id};
    let filter = {extensionId: targetExtension.id};
    // Legacy addons live in the main process. Messages from other addons are
    // Messages from WebExtensions are sent to the main process and forwarded via
    // the parent process manager to the legacy extension.
    this.messenger = new ExtensionChild.Messenger(this, [Services.cpmm], sender, filter);

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
   * @param {string} containerAddonParams.version
   *   The add-on version.
   * @param {nsIURI} containerAddonParams.resourceURI
   *   The nsIURI of the Legacy Extension container add-on.
   */
  constructor({id, resourceURI, version}) {
    this.addonId = id;
    this.resourceURI = resourceURI;
    this.version = version;

    // Setup status flag.
    this.started = false;
  }

  /**
   * Start the embedded webextension.
   *
   * @param {number} reason
   *   The add-on startup bootstrap reason received from the XPIProvider.
   * @param {object} [addonData]
   *   Additional data to pass to the Extension constructor.
   *
   * @returns {Promise<LegacyContextAPI>} A promise which resolve to the API exposed to the
   *   legacy context.
   */
  startup(reason, addonData = {}) {
    if (this.started) {
      return Promise.reject(new Error("This embedded extension has already been started"));
    }

    // Setup the startup promise.
    this.startupPromise = new Promise((resolve, reject) => {
      let embeddedExtensionURI = Services.io.newURI("webextension/", null, this.resourceURI);

      let {builtIn, signedState, temporarilyInstalled} = addonData;

      // This is the instance of the WebExtension embedded in the hybrid add-on.
      this.extension = new Extension({
        builtIn,
        signedState,
        temporarilyInstalled,
        id: this.addonId,
        resourceURI: embeddedExtensionURI,
        version: this.version,
      });

      this.extension.isEmbedded = true;

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
        this.context = new LegacyExtensionContext(this.extension);

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
      this.extension.startup(reason).catch((err) => {
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
   * @param {number} reason
   *   The add-on shutdown bootstrap reason received from the XPIProvider.
   *
   * @returns {Promise<void>} a promise that is resolved when the shutdown has been done
   */
  async shutdown(reason) {
    EmbeddedExtensionManager.untrackEmbeddedExtension(this);

    if (this.extension && !this.extension.hasShutdown) {
      let {extension} = this;
      this.extension = null;

      await extension.shutdown(reason);
    }
    return undefined;
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

  getEmbeddedExtensionFor({id, resourceURI, version}) {
    let embeddedExtension = this.embeddedExtensionsByAddonId.get(id);

    if (!embeddedExtension) {
      embeddedExtension = new EmbeddedExtension({id, resourceURI, version});
      // Keep track of the embedded extension instance.
      this.embeddedExtensionsByAddonId.set(id, embeddedExtension);
    }

    return embeddedExtension;
  },
};

var LegacyExtensionsUtils = {
  getEmbeddedExtensionFor: (addon) => {
    return EmbeddedExtensionManager.getEmbeddedExtensionFor(addon);
  },
};
