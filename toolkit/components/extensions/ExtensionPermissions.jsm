/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  ExtensionParent: "resource://gre/modules/ExtensionParent.jsm",
  JSONFile: "resource://gre/modules/JSONFile.jsm",
  OS: "resource://gre/modules/osfile.jsm",
});

XPCOMUtils.defineLazyGetter(this, "StartupCache", () => ExtensionParent.StartupCache);

this.EXPORTED_SYMBOLS = ["ExtensionPermissions"];

const FILE_NAME = "extension-preferences.json";

let prefs;
let _initPromise;

async function _lazyInit() {
  let path = OS.Path.join(OS.Constants.Path.profileDir, FILE_NAME);

  prefs = new JSONFile({path});
  prefs.data = {};

  try {
    let {buffer} = await OS.File.read(path);
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
  return {permissions: [], origins: []};
}

this.ExtensionPermissions = {
  async _saveSoon(extension) {
    await lazyInit();

    prefs.data[extension.id] = await this._getCached(extension);
    return prefs.saveSoon();
  },

  async _get(extension) {
    await lazyInit();

    let perms = prefs.data[extension.id];
    if (!perms) {
      perms = emptyPermissions();
      prefs.data[extension.id] = perms;
    }

    return perms;
  },

  async _getCached(extension) {
    return StartupCache.permissions.get(extension.id,
                                        () => this._get(extension));
  },

  get(extension) {
    return this._getCached(extension);
  },

  // Add new permissions for the given extension.  `permissions` is
  // in the format that is passed to browser.permissions.request().
  async add(extension, perms) {
    let {permissions, origins} = await this._getCached(extension);

    let added = emptyPermissions();

    for (let perm of perms.permissions) {
      if (!permissions.includes(perm)) {
        added.permissions.push(perm);
        permissions.push(perm);
      }
    }

    for (let origin of perms.origins) {
      origin = new MatchPattern(origin, {ignorePath: true}).pattern;
      if (!origins.includes(origin)) {
        added.origins.push(origin);
        origins.push(origin);
      }
    }

    if (added.permissions.length > 0 || added.origins.length > 0) {
      this._saveSoon(extension);
      extension.emit("add-permissions", added);
    }
  },

  // Revoke permissions from the given extension.  `permissions` is
  // in the format that is passed to browser.permissions.remove().
  async remove(extension, perms) {
    let {permissions, origins} = await this._getCached(extension);

    let removed = emptyPermissions();

    for (let perm of perms.permissions) {
      let i = permissions.indexOf(perm);
      if (i >= 0) {
        removed.permissions.push(perm);
        permissions.splice(i, 1);
      }
    }

    for (let origin of perms.origins) {
      origin = new MatchPattern(origin, {ignorePath: true}).pattern;

      let i = origins.indexOf(origin);
      if (i >= 0) {
        removed.origins.push(origin);
        origins.splice(i, 1);
      }
    }

    if (removed.permissions.length > 0 || removed.origins.length > 0) {
      this._saveSoon(extension);
      extension.emit("remove-permissions", removed);
    }
  },

  async removeAll(extension) {
    let perms = await this._getCached(extension);

    if (perms.permissions.length || perms.origins.length) {
      Object.assign(perms, emptyPermissions());
      prefs.saveSoon();
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
