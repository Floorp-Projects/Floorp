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
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ExtensionParent: "resource://gre/modules/ExtensionParent.jsm",
  JSONFile: "resource://gre/modules/JSONFile.jsm",
});

XPCOMUtils.defineLazyGetter(
  lazy,
  "StartupCache",
  () => lazy.ExtensionParent.StartupCache
);

ChromeUtils.defineModuleGetter(
  lazy,
  "KeyValueService",
  "resource://gre/modules/kvstore.jsm"
);

ChromeUtils.defineModuleGetter(
  lazy,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);

XPCOMUtils.defineLazyGetter(
  lazy,
  "Management",
  () => lazy.ExtensionParent.apiManager
);

var EXPORTED_SYMBOLS = ["ExtensionPermissions", "OriginControls"];

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
    let path = PathUtils.join(
      Services.dirsvc.get("ProfD", Ci.nsIFile).path,
      FILE_NAME
    );

    prefs = new lazy.JSONFile({ path });
    prefs.data = {};

    try {
      prefs.data = await IOUtils.readJSON(path);
    } catch (e) {
      if (!(DOMException.isInstance(e) && e.name == "NotFoundError")) {
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
    const storePath = lazy.FileUtils.getDir("ProfD", ["extension-store"]).path;
    // Make sure the folder exists
    await IOUtils.makeDirectory(storePath, { ignoreExisting: true });
    this._store = await lazy.KeyValueService.getOrCreate(
      storePath,
      "permissions"
    );
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
    let oldStore = PathUtils.join(
      Services.dirsvc.get("ProfD", Ci.nsIFile).path,
      FILE_NAME
    );
    try {
      await this.migrateFrom(oldStore);
      migrationWasSuccessful = true;
    } catch (e) {
      if (!(DOMException.isInstance(e) && e.name == "NotFoundError")) {
        Cu.reportError(e);
      }
    }

    await this._store.put(VERSION_KEY, VERSION_VALUE);

    if (migrationWasSuccessful) {
      IOUtils.remove(oldStore);
    }
  }

  async migrateFrom(oldStore) {
    // Some other migration job might have started and not completed, let's
    // start from scratch
    await this._store.clear();

    let json = await IOUtils.readJSON(oldStore);
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
    return lazy.StartupCache.permissions.set(extensionId, perms);
  },

  async _get(extensionId) {
    return store.get(extensionId);
  },

  async _getCached(extensionId) {
    return lazy.StartupCache.permissions.get(extensionId, () =>
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

  _fixupAllUrlsPerms(perms) {
    // Unfortunately, we treat <all_urls> as an API permission as well.
    // If it is added to either, ensure it is added to both.
    if (perms.origins.includes("<all_urls>")) {
      perms.permissions.push("<all_urls>");
    } else if (perms.permissions.includes("<all_urls>")) {
      perms.origins.push("<all_urls>");
    }
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

    this._fixupAllUrlsPerms(perms);

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
      lazy.Management.emit("change-permissions", { extensionId, added });
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

    this._fixupAllUrlsPerms(perms);

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
      lazy.Management.emit("change-permissions", { extensionId, removed });
      if (emitter) {
        emitter.emit("remove-permissions", removed);
      }
    }
  },

  async removeAll(extensionId) {
    lazy.StartupCache.permissions.delete(extensionId);

    let removed = store.get(extensionId);
    await store.delete(extensionId);
    lazy.Management.emit("change-permissions", {
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

  // Convenience listener members for all permission changes.
  addListener(listener) {
    lazy.Management.on("change-permissions", listener);
  },

  removeListener(listener) {
    lazy.Management.off("change-permissions", listener);
  },
};

var OriginControls = {
  /**
   * Get origin controls state for a given extension on a given host.
   * @param {WebExtensionPolicy} policy
   * @param {nsIURI} uri
   * @returns {object} Extension origin controls for this host include:
   *  @param {boolean} noAccess     no options, can never access host.
   *  @param {boolean} whenClicked  option to access host when clicked.
   *  @param {boolean} alwaysOn     option to always access this host.
   *  @param {boolean} allDomains   option to access to all domains.
   *  @param {boolean} hasAccess    extension currently has access to host.
   */
  getState(policy, uri) {
    let allDomains = new MatchPattern("*://*/*");
    let activeTab = policy.permissions.includes("activeTab");
    let couldRequest = policy.extension.optionalOrigins.matches(uri);
    let hasAccess = policy.canAccessURI(uri);

    if (
      !allDomains.matches(uri) ||
      WebExtensionPolicy.isRestrictedURI(uri) ||
      (!couldRequest && !hasAccess && !activeTab)
    ) {
      return { noAccess: true };
    }
    if (!couldRequest && !hasAccess && activeTab) {
      return { whenClicked: true };
    }
    if (policy.allowedOrigins.subsumes(allDomains)) {
      return { allDomains: true, hasAccess };
    }
    return {
      whenClicked: true,
      alwaysOn: true,
      hasAccess,
    };
  },

  // Grant extension host permission to always run on this host.
  setAlwaysOn(policy, uri) {
    if (!policy.active) {
      return;
    }
    let perms = { permissions: [], origins: ["*://" + uri.host] };
    ExtensionPermissions.add(policy.id, perms, policy.extension);
  },

  // Revoke permission, extension should run only when clicked on this host.
  setWhenClicked(policy, uri) {
    if (!policy.active) {
      return;
    }
    let perms = { permissions: [], origins: ["*://" + uri.host] };
    ExtensionPermissions.remove(policy.id, perms, policy.extension);
  },
};
