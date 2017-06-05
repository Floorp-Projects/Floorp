"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "JSONFile",
                                  "resource://gre/modules/JSONFile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

this.EXPORTED_SYMBOLS = ["ExtensionPermissions"];

const FILE_NAME = "extension-preferences.json";

let prefs;
let _initPromise;
function lazyInit() {
  if (!_initPromise) {
    prefs = new JSONFile({path: OS.Path.join(OS.Constants.Path.profileDir, FILE_NAME)});

    _initPromise = prefs.load();
  }
  return _initPromise;
}

function emptyPermissions() {
  return {permissions: [], origins: []};
}

this.ExtensionPermissions = {
  async get(extension) {
    await lazyInit();

    let perms = emptyPermissions();
    if (prefs.data[extension.id]) {
      Object.assign(perms, prefs.data[extension.id]);
    }
    return perms;
  },

  // Add new permissions for the given extension.  `permissions` is
  // in the format that is passed to browser.permissions.request().
  async add(extension, perms) {
    await lazyInit();

    if (!prefs.data[extension.id]) {
      prefs.data[extension.id] = emptyPermissions();
    }
    let {permissions, origins} = prefs.data[extension.id];

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
      prefs.saveSoon();
      extension.emit("add-permissions", added);
    }
  },

  // Revoke permissions from the given extension.  `permissions` is
  // in the format that is passed to browser.permissions.remove().
  async remove(extension, perms) {
    await lazyInit();

    if (!prefs.data[extension.id]) {
      return;
    }
    let {permissions, origins} = prefs.data[extension.id];

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
      prefs.saveSoon();
      extension.emit("remove-permissions", removed);
    }
  },

  async removeAll(extension) {
    await lazyInit();
    delete prefs.data[extension.id];
    prefs.saveSoon();
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
