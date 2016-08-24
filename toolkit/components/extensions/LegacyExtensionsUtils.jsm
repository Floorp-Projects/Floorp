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

this.LegacyExtensionsUtils = {};
