/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
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

ChromeUtils.defineModuleGetter(
  this,
  "KeyValueService",
  "resource://gre/modules/kvstore.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);

XPCOMUtils.defineLazyGetter(
  this,
  "Management",
  () => ExtensionParent.apiManager
);

var EXPORTED_SYMBOLS = ["ExtensionPermissions"];

// This is the old preference file pre-migration to rkv
const FILE_NAME = "extension-preferences.json";

function emptyPermissions() {
  return { permissions: [], origins: [] };
}

const DEFAULT_VALUE = JSON.stringify(emptyPermissions());

const VERSION_KEY = "_version";
const VERSION_VALUE = 1;

const KEY_PREFIX = "id-";

// Bug 1646182: remove once we fully migrate to rkv
let prefs;

// Bug 1646182: remove once we fully migrate to rkv
class LegacyPermissionStore {
  async lazyInit() {
    if (!this._initPromise) {
      this._initPromise = this._init();
    }
    return this._initPromise;
  }

  async _init() {
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

  async has(extensionId) {
    await this.lazyInit();
    return !!prefs.data[extensionId];
  }

  async get(extensionId) {
    await this.lazyInit();

    let perms = prefs.data[extensionId];
    if (!perms) {
      perms = emptyPermissions();
    }

    return perms;
  }

  async put(extensionId, permissions) {
    await this.lazyInit();
    prefs.data[extensionId] = permissions;
    prefs.saveSoon();
  }

  async delete(extensionId) {
    await this.lazyInit();
    if (prefs.data[extensionId]) {
      delete prefs.data[extensionId];
      prefs.saveSoon();
    }
  }

  async uninitForTest() {
    if (!this._initPromise) {
      return;
    }

    await this._initPromise;
    await prefs.finalize();
    prefs = null;
    this._initPromise = null;
  }

  async resetVersionForTest() {
    throw new Error("Not supported");
  }
}

class PermissionStore {
  async _init() {
    const storePath = FileUtils.getDir("ProfD", ["extension-store"]).path;
    // Make sure the folder exists
    await OS.File.makeDir(storePath, { ignoreExisting: true });
    this._store = await KeyValueService.getOrCreate(storePath, "permissions");
    if (!(await this._store.has(VERSION_KEY))) {
      await this.maybeMigrateData();
    }
  }

  lazyInit() {
    if (!this._initPromise) {
      this._initPromise = this._init();
    }
    return this._initPromise;
  }

  validateMigratedData(json) {
    let data = {};
    for (let [extensionId, permissions] of Object.entries(json)) {
      // If both arrays are empty there's no need to include the value since
      // it's the default
      if (
        "permissions" in permissions &&
        "origins" in permissions &&
        (permissions.permissions.length || permissions.origins.length)
      ) {
        data[extensionId] = permissions;
      }
    }
    return data;
  }

  async maybeMigrateData() {
    let migrationWasSuccessful = false;
    let oldStore = OS.Path.join(OS.Constants.Path.profileDir, FILE_NAME);
    try {
      await this.migrateFrom(oldStore);
      migrationWasSuccessful = true;
    } catch (e) {
      if (!e.becauseNoSuchFile) {
        Cu.reportError(e);
      }
    }

    await this._store.put(VERSION_KEY, VERSION_VALUE);

    if (migrationWasSuccessful) {
      OS.File.remove(oldStore);
    }
  }

  async migrateFrom(oldStore) {
    // Some other migration job might have started and not completed, let's
    // start from scratch
    await this._store.clear();

    let { buffer } = await OS.File.read(oldStore);
    let json = JSON.parse(new TextDecoder().decode(buffer));
    let data = this.validateMigratedData(json);

    if (data) {
      let entries = Object.entries(data).map(([extensionId, permissions]) => [
        this.makeKey(extensionId),
        JSON.stringify(permissions),
      ]);
      if (entries.length) {
        await this._store.writeMany(entries);
      }
    }
  }

  makeKey(extensionId) {
    // We do this so that the extensionId field cannot clash with internal
    // fields like `_version`
    return KEY_PREFIX + extensionId;
  }

  async has(extensionId) {
    await this.lazyInit();
    return this._store.has(this.makeKey(extensionId));
  }

  async get(extensionId) {
    await this.lazyInit();
    return this._store
      .get(this.makeKey(extensionId), DEFAULT_VALUE)
      .then(JSON.parse);
  }

  async put(extensionId, permissions) {
    await this.lazyInit();
    return this._store.put(
      this.makeKey(extensionId),
      JSON.stringify(permissions)
    );
  }

  async delete(extensionId) {
    await this.lazyInit();
    return this._store.delete(this.makeKey(extensionId));
  }

  async resetVersionForTest() {
    await this.lazyInit();
    return this._store.delete(VERSION_KEY);
  }

  async uninitForTest() {
    // Nothing special to do to unitialize, let's just
    // make sure we're not in the middle of initialization
    return this._initPromise;
  }
}

// Bug 1646182: turn on rkv on all channels
function createStore(useRkv = AppConstants.NIGHTLY_BUILD) {
  if (useRkv) {
    return new PermissionStore();
  }
  return new LegacyPermissionStore();
}

let store = createStore();

var ExtensionPermissions = {
  async _update(extensionId, perms) {
    await store.put(extensionId, perms);
    return StartupCache.permissions.set(extensionId, perms);
  },

  async _get(extensionId) {
    return store.get(extensionId);
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
    StartupCache.permissions.delete(extensionId);

    let removed = store.get(extensionId);
    await store.delete(extensionId);
    Management.emit("change-permissions", {
      extensionId,
      removed: await removed,
    });
  },

  // This is meant for tests only
  async _has(extensionId) {
    return store.has(extensionId);
  },

  // This is meant for tests only
  async _resetVersion() {
    await store.resetVersionForTest();
  },

  // This is meant for tests only
  _useLegacyStorageBackend: false,

  // This is meant for tests only
  async _uninit() {
    await store.uninitForTest();
    store = createStore(!this._useLegacyStorageBackend);
  },
};
