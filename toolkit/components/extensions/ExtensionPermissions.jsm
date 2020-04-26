/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ExtensionParent: "resource://gre/modules/ExtensionParent.jsm",
  JSONFile: "resource://gre/modules/JSONFile.jsm",
  OS: "resource://gre/modules/osfile.jsm",
});

XPCOMUtils.defineLazyGetter(
  this,
  "StartupCache",
  () => ExtensionParent.StartupCache
);

XPCOMUtils.defineLazyGetter(
  this,
  "Management",
  () => ExtensionParent.apiManager
);

var EXPORTED_SYMBOLS = ["ExtensionPermissions"];

const FILE_NAME = "extension-preferences.json";

let prefs;
let _initPromise;

async function _lazyInit() {
  let path = OS.Path.join(OS.Constants.Path.profileDir, FILE_NAME);

  prefs = new JSONFile({ path });
  prefs.data = {};

  try {
    let { buffer } = await OS.File.read(path);
    prefs.data = JSON.parse(new TextDecoder().decode(buffer));
  } catch (e) {
    if (!e.becauseNoSuchFile) {
      Cu.reportError(e);
    }
  }
}

function lazyInit() {
  if (!_initPromise) {
    _initPromise = _lazyInit();
  }
  return _initPromise;
}

function emptyPermissions() {
  return { permissions: [], origins: [] };
}

var ExtensionPermissions = {
  _update(extensionId, perms) {
    prefs.data[extensionId] = perms;
    prefs.saveSoon();
    return StartupCache.permissions.set(extensionId, perms);
  },

  async _get(extensionId) {
    await lazyInit();

    let perms = prefs.data[extensionId];
    if (!perms) {
      perms = emptyPermissions();
    }

    return perms;
  },

  async _getCached(extensionId) {
    return StartupCache.permissions.get(extensionId, () =>
      this._get(extensionId)
    );
  },

  /**
   * Retrieves the optional permissions for the given extension.
   * The information may be retrieved from the StartupCache, and otherwise fall
   * back to data from the disk (and cache the result in the StartupCache).
   *
   * @param {string} extensionId The extensionId
   * @returns {object} An object with "permissions" and "origins" array.
   *   The object may be a direct reference to the storage or cache, so its
   *   value should immediately be used and not be modified by callers.
   */
  get(extensionId) {
    return this._getCached(extensionId);
  },

  /**
   * Add new permissions for the given extension.  `permissions` is
   * in the format that is passed to browser.permissions.request().
   *
   * @param {string} extensionId The extension id
   * @param {Object} perms Object with permissions and origins array.
   * @param {EventEmitter} emitter optional object implementing emitter interfaces
   */
  async add(extensionId, perms, emitter) {
    let { permissions, origins } = await this._get(extensionId);

    let added = emptyPermissions();

    for (let perm of perms.permissions) {
      if (!permissions.includes(perm)) {
        added.permissions.push(perm);
        permissions.push(perm);
      }
    }

    for (let origin of perms.origins) {
      origin = new MatchPattern(origin, { ignorePath: true }).pattern;
      if (!origins.includes(origin)) {
        added.origins.push(origin);
        origins.push(origin);
      }
    }

    if (added.permissions.length || added.origins.length) {
      await this._update(extensionId, { permissions, origins });
      Management.emit("change-permissions", { extensionId, added });
      if (emitter) {
        emitter.emit("add-permissions", added);
      }
    }
  },

  /**
   * Revoke permissions from the given extension.  `permissions` is
   * in the format that is passed to browser.permissions.request().
   *
   * @param {string} extensionId The extension id
   * @param {Object} perms Object with permissions and origins array.
   * @param {EventEmitter} emitter optional object implementing emitter interfaces
   */
  async remove(extensionId, perms, emitter) {
    let { permissions, origins } = await this._get(extensionId);

    let removed = emptyPermissions();

    for (let perm of perms.permissions) {
      let i = permissions.indexOf(perm);
      if (i >= 0) {
        removed.permissions.push(perm);
        permissions.splice(i, 1);
      }
    }

    for (let origin of perms.origins) {
      origin = new MatchPattern(origin, { ignorePath: true }).pattern;

      let i = origins.indexOf(origin);
      if (i >= 0) {
        removed.origins.push(origin);
        origins.splice(i, 1);
      }
    }

    if (removed.permissions.length || removed.origins.length) {
      await this._update(extensionId, { permissions, origins });
      Management.emit("change-permissions", { extensionId, removed });
      if (emitter) {
        emitter.emit("remove-permissions", removed);
      }
    }
  },

  async removeAll(extensionId) {
    await lazyInit();
    StartupCache.permissions.delete(extensionId);
    if (prefs.data[extensionId]) {
      let removed = prefs.data[extensionId];
      delete prefs.data[extensionId];
      prefs.saveSoon();
      Management.emit("change-permissions", {
        extensionId,
        removed,
      });
    }
  },

  // This is meant for tests only
  async _uninit() {
    if (!_initPromise) {
      return;
    }

    await _initPromise;
    await prefs.finalize();
    prefs = null;
    _initPromise = null;
  },
};
