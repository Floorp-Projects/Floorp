/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionAPI", "ExtensionAPIs"];

/* exported ExtensionAPIs */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/ExtensionManagement.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ConsoleAPI",
                                  "resource://gre/modules/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "EventEmitter",
                                  "resource://gre/modules/EventEmitter.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

const global = this;

class ExtensionAPI {
  constructor(extension) {
    this.extension = extension;

    extension.once("shutdown", () => {
      if (this.onShutdown) {
        this.onShutdown(extension.shutdownReason);
      }
      this.extension = null;
    });
  }

  destroy() {
  }

  onManifestEntry(entry) {
  }

  getAPI(context) {
    throw new Error("Not Implemented");
  }
}

var ExtensionAPIs = {
  apis: ExtensionManagement.APIs.apis,

  load(apiName) {
    let api = this.apis.get(apiName);

    if (api.loadPromise) {
      return api.loadPromise;
    }

    let {script, schema} = api;

    let addonId = `${apiName}@experiments.addons.mozilla.org`;
    api.sandbox = Cu.Sandbox(global, {
      wantXrays: false,
      sandboxName: script,
      addonId,
      metadata: {addonID: addonId},
    });

    api.sandbox.ExtensionAPI = ExtensionAPI;

    // Create a console getter which lazily provide a ConsoleAPI instance.
    XPCOMUtils.defineLazyGetter(api.sandbox, "console", () => {
      return new ConsoleAPI({prefix: addonId});
    });

    Services.scriptloader.loadSubScript(script, api.sandbox, "UTF-8");

    api.loadPromise = Schemas.load(schema).then(() => {
      let API = Cu.evalInSandbox("API", api.sandbox);
      API.prototype.namespace = apiName;
      return API;
    });

    return api.loadPromise;
  },

  unload(apiName) {
    let api = this.apis.get(apiName);

    let {schema} = api;

    Schemas.unload(schema);
    Cu.nukeSandbox(api.sandbox);

    api.sandbox = null;
    api.loadPromise = null;
  },
};
