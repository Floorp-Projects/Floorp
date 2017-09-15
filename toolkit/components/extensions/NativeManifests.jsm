/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["NativeManifests"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  Schemas: "resource://gre/modules/Schemas.jsm",
  Services: "resource://gre/modules/Services.jsm",
  WindowsRegistry: "resource://gre/modules/WindowsRegistry.jsm",
});

const HOST_MANIFEST_SCHEMA = "chrome://extensions/content/schemas/native_host_manifest.json";
const VALID_APPLICATION = /^\w+(\.\w+)*$/;

const REGPATH = "Software\\Mozilla\\NativeMessagingHosts";

this.NativeManifests = {
  _initializePromise: null,
  _lookup: null,

  init() {
    if (!this._initializePromise) {
      let platform = AppConstants.platform;
      if (platform == "win") {
        this._lookup = this._winLookup;
      } else if (platform == "macosx" || platform == "linux") {
        let dirs = [
          Services.dirsvc.get("XREUserNativeManifests", Ci.nsIFile).path,
          Services.dirsvc.get("XRESysNativeManifests", Ci.nsIFile).path,
        ];
        this._lookup = (application, context) => this._tryPaths(application, dirs, context);
      } else {
        throw new Error(`Native messaging is not supported on ${AppConstants.platform}`);
      }
      this._initializePromise = Schemas.load(HOST_MANIFEST_SCHEMA);
    }
    return this._initializePromise;
  },

  _winLookup(application, context) {
    const REGISTRY = Ci.nsIWindowsRegKey;
    let regPath = `${REGPATH}\\${application}`;
    let path = WindowsRegistry.readRegKey(REGISTRY.ROOT_KEY_CURRENT_USER,
                                          regPath, "", REGISTRY.WOW64_64);
    if (!path) {
      path = WindowsRegistry.readRegKey(Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                                        regPath, "", REGISTRY.WOW64_64);
    }
    if (!path) {
      return null;
    }
    return this._tryPath(path, application, context)
      .then(manifest => manifest ? {path, manifest} : null);
  },

  _tryPath(path, application, context) {
    return Promise.resolve()
      .then(() => OS.File.read(path, {encoding: "utf-8"}))
      .then(data => {
        let manifest;
        try {
          manifest = JSON.parse(data);
        } catch (ex) {
          let msg = `Error parsing native host manifest ${path}: ${ex.message}`;
          Cu.reportError(msg);
          return null;
        }

        let normalized = Schemas.normalize(manifest, "manifest.NativeHostManifest", context);
        if (normalized.error) {
          Cu.reportError(normalized.error);
          return null;
        }
        manifest = normalized.value;
        if (manifest.name != application) {
          let msg = `Native host manifest ${path} has name property ${manifest.name} (expected ${application})`;
          Cu.reportError(msg);
          return null;
        }
        return normalized.value;
      }).catch(ex => {
        if (ex instanceof OS.File.Error && ex.becauseNoSuchFile) {
          return null;
        }
        throw ex;
      });
  },

  async _tryPaths(application, dirs, context) {
    for (let dir of dirs) {
      let path = OS.Path.join(dir, `${application}.json`);
      let manifest = await this._tryPath(path, application, context);
      if (manifest) {
        return {path, manifest};
      }
    }
    return null;
  },

  /**
   * Search for a valid native host manifest for the given application name.
   * The directories searched and rules for manifest validation are all
   * detailed in the native messaging documentation.
   *
   * @param {string} application The name of the applciation to search for.
   * @param {object} context A context object as expected by Schemas.normalize.
   * @returns {object} The contents of the validated manifest, or null if
   *                   no valid manifest can be found for this application.
   */
  lookupApplication(application, context) {
    if (!VALID_APPLICATION.test(application)) {
      throw new Error(`Invalid application "${application}"`);
    }
    return this.init().then(() => this._lookup(application, context));
  },
};
