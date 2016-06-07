/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["HostManifestManager"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://devtools/shared/event-emitter.js");

XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

const HOST_MANIFEST_SCHEMA = "chrome://extensions/content/schemas/native_host_manifest.json";
const VALID_APPLICATION = /^\w+(\.\w+)*$/;

this.HostManifestManager = {
  _initializePromise: null,
  _lookup: null,

  init() {
    if (!this._initializePromise) {
      let platform = AppConstants.platform;
      if (platform == "win") {
        throw new Error("Windows not yet implemented (bug 1270359)");
      } else if (platform == "macosx" || platform == "linux") {
        let dirs = [
          Services.dirsvc.get("XREUserNativeMessaging", Ci.nsIFile).path,
          Services.dirsvc.get("XRESysNativeMessaging", Ci.nsIFile).path,
        ];
        this._lookup = (application, context) => this._tryPaths(application, dirs, context);
      } else {
        throw new Error(`Native messaging is not supported on ${AppConstants.platform}`);
      }
      this._initializePromise = Schemas.load(HOST_MANIFEST_SCHEMA);
    }
    return this._initializePromise;
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

  _tryPaths: Task.async(function* (application, dirs, context) {
    for (let dir of dirs) {
      let path = OS.Path.join(dir, `${application}.json`);
      let manifest = yield this._tryPath(path, application, context);
      if (manifest) {
        return {path, manifest};
      }
    }
    return null;
  }),

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
      throw new context.cloneScope.Error(`Invalid application "${application}"`);
    }
    return this.init().then(() => this._lookup(application, context));
  },
};
