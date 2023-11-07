/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { computeSha1HashAsString } from "resource://gre/modules/addons/crypto-utils.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.sys.mjs",
  ExtensionParent: "resource://gre/modules/ExtensionParent.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
  JSONFile: "resource://gre/modules/JSONFile.sys.mjs",
  KeyValueService: "resource://gre/modules/kvstore.sys.mjs",
});

ChromeUtils.defineLazyGetter(
  lazy,
  "StartupCache",
  () => lazy.ExtensionParent.StartupCache
);

ChromeUtils.defineLazyGetter(
  lazy,
  "Management",
  () => lazy.ExtensionParent.apiManager
);

function emptyPermissions() {
  return { permissions: [], origins: [] };
}

const DEFAULT_VALUE = JSON.stringify(emptyPermissions());

const KEY_PREFIX = "id-";

// This is the old preference file pre-migration to rkv.
const OLD_JSON_FILENAME = "extension-preferences.json";
// This is the old path to the rkv store dir (which used to be shared with ExtensionScriptingStore).
const OLD_RKV_DIRNAME = "extension-store";
// This is the new path to the rkv store dir.
const RKV_DIRNAME = "extension-store-permissions";

const VERSION_KEY = "_version";

const VERSION_VALUE = 1;

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
      OLD_JSON_FILENAME
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
  _shouldMigrateFromOldKVStorePath = AppConstants.NIGHTLY_BUILD;

  async _init() {
    const storePath = lazy.FileUtils.getDir("ProfD", [RKV_DIRNAME]).path;
    // Make sure the folder exists
    await IOUtils.makeDirectory(storePath, { ignoreExisting: true });
    this._store = await lazy.KeyValueService.getOrCreate(
      storePath,
      "permissions"
    );
    if (!(await this._store.has(VERSION_KEY))) {
      // If _shouldMigrateFromOldKVStorePath is true (default only on Nightly channel
      // where the rkv store has been enabled by default for a while), we need to check
      // if we would need to import data from the old kvstore path (ProfD/extensions-store)
      // first, and fallback to try to import from the JSONFile if there was no data in
      // the old kvstore path.
      // NOTE: _shouldMigrateFromOldKVStorePath is also explicitly set to true in unit tests
      // that are meant to explicitly cover this path also when running on on non-Nightly channels.
      if (this._shouldMigrateFromOldKVStorePath) {
        // Try to import data from the old kvstore path (ProfD/extensions-store).
        await this.maybeImportFromOldKVStorePath();
        if (!(await this._store.has(VERSION_KEY))) {
          // There was no data in the old kvstore path, migrate any data
          // available from the LegacyPermissionStore JSONFile if any.
          await this.maybeMigrateDataFromOldJSONFile();
        }
      } else {
        // On non-Nightly channels, where LegacyPermissionStore was still the
        // only backend ever enabled, try to import permissions data from the
        // legacy JSONFile, if any data is available there.
        await this.maybeMigrateDataFromOldJSONFile();
      }
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

  async maybeMigrateDataFromOldJSONFile() {
    let migrationWasSuccessful = false;
    let oldStore = PathUtils.join(
      Services.dirsvc.get("ProfD", Ci.nsIFile).path,
      OLD_JSON_FILENAME
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

  async maybeImportFromOldKVStorePath() {
    try {
      const oldStorePath = lazy.FileUtils.getDir("ProfD", [
        OLD_RKV_DIRNAME,
      ]).path;
      if (!(await IOUtils.exists(oldStorePath))) {
        return;
      }
      const oldStore = await lazy.KeyValueService.getOrCreate(
        oldStorePath,
        "permissions"
      );
      const enumerator = await oldStore.enumerate();
      const kvpairs = [];
      while (enumerator.hasMoreElements()) {
        const { key, value } = enumerator.getNext();
        kvpairs.push([key, value]);
      }

      // NOTE: we don't add a VERSION_KEY entry explicitly here because
      // if the database was not empty the VERSION_KEY is already set to
      // 1 and will be copied into the new file as part of the pairs
      // written below (along with the entries for the actual extensions
      // permissions).
      if (kvpairs.length) {
        await this._store.writeMany(kvpairs);
      }

      // NOTE: the old rkv store path used to be shared with the
      // ExtensionScriptingStore, and so we are not removing the old
      // rkv store dir here (that is going to be left to a separate
      // migration we will be adding to ExtensionScriptingStore).
    } catch (err) {
      Cu.reportError(err);
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

export var ExtensionPermissions = {
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
   * @param {object} perms Object with permissions and origins array.
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
   * @param {object} perms Object with permissions and origins array.
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
  async _uninit({ recreateStore = true } = {}) {
    await store?.uninitForTest();
    store = null;
    if (recreateStore) {
      store = createStore(!this._useLegacyStorageBackend);
    }
  },

  // This is meant for tests only
  _getStore() {
    return store;
  },

  // Convenience listener members for all permission changes.
  addListener(listener) {
    lazy.Management.on("change-permissions", listener);
  },

  removeListener(listener) {
    lazy.Management.off("change-permissions", listener);
  },
};

export var OriginControls = {
  allDomains: new MatchPattern("*://*/*"),

  /**
   * @typedef {object} OriginControlState
   * @param {boolean} noAccess        no options, can never access host.
   * @param {boolean} whenClicked     option to access host when clicked.
   * @param {boolean} alwaysOn        option to always access this host.
   * @param {boolean} allDomains      option to access to all domains.
   * @param {boolean} hasAccess       extension currently has access to host.
   * @param {boolean} temporaryAccess extension has temporary access to the tab.
   */

  /**
   * Get origin controls state for a given extension on a given tab.
   *
   * @param {WebExtensionPolicy} policy
   * @param {NativeTab} nativeTab
   * @returns {OriginControlState} Extension origin controls for this host include:
   */
  getState(policy, nativeTab) {
    // Note: don't use the nativeTab directly because it's different on mobile.
    let tab = policy?.extension?.tabManager.getWrapper(nativeTab);
    let temporaryAccess = tab?.hasActiveTabPermission;
    let uri = tab?.browser.currentURI;

    if (!uri) {
      return { noAccess: true };
    }

    // activeTab and the resulting whenClicked state is only applicable for MV2
    // extensions with a browser action and MV3 extensions (with or without).
    let activeTab =
      policy.permissions.includes("activeTab") &&
      (policy.manifestVersion >= 3 || policy.extension?.hasBrowserActionUI);

    let couldRequest = policy.extension.optionalOrigins.matches(uri);
    let hasAccess = policy.canAccessURI(uri);

    // If any of (MV2) content script patterns match the URI.
    let csPatternMatches = false;
    let quarantinedFrom = policy.quarantinedFromURI(uri);

    if (policy.manifestVersion < 3 && !hasAccess) {
      csPatternMatches = policy.contentScripts.some(cs =>
        cs.matches.patterns.some(p => p.matches(uri))
      );
      // MV2 access through content scripts is implicit.
      hasAccess = csPatternMatches && !quarantinedFrom;
    }

    // If extension is quarantined from this host, but could otherwise have
    // access (via activeTab, optional, allowedOrigins or content scripts).
    let quarantined =
      quarantinedFrom &&
      (activeTab ||
        couldRequest ||
        csPatternMatches ||
        policy.allowedOrigins.matches(uri));

    if (
      quarantined ||
      !this.allDomains.matches(uri) ||
      WebExtensionPolicy.isRestrictedURI(uri) ||
      (!couldRequest && !hasAccess && !activeTab)
    ) {
      return { noAccess: true, quarantined };
    }

    if (!couldRequest && !hasAccess && activeTab) {
      return { whenClicked: true, temporaryAccess };
    }
    if (policy.allowedOrigins.subsumes(this.allDomains)) {
      return { allDomains: true, hasAccess };
    }

    return {
      whenClicked: true,
      alwaysOn: true,
      temporaryAccess,
      hasAccess,
    };
  },

  /**
   * Whether to show the attention indicator for extension on current tab. We
   * usually show attention when:
   *
   * - some permissions are needed (in MV3+)
   * - the extension is not allowed on the domain (quarantined)
   *
   * @param {WebExtensionPolicy} policy an extension's policy
   * @param {Window} window The window for which we can get the attention state
   * @returns {{attention: boolean, quarantined: boolean}}
   */
  getAttentionState(policy, window) {
    if (policy?.manifestVersion >= 3) {
      const state = this.getState(policy, window.gBrowser.selectedTab);
      // quarantined is always false when the feature is disabled.
      const quarantined = !!state.quarantined;
      const attention =
        quarantined ||
        (!!state.whenClicked && !state.hasAccess && !state.temporaryAccess);

      return { attention, quarantined };
    }

    // No need to check whether the Quarantined Domains feature is enabled
    // here, it's already done in `getState()`.
    const state = this.getState(policy, window.gBrowser.selectedTab);
    const attention = !!state.quarantined;
    // If it needs attention, it's because of the quarantined domains.
    return { attention, quarantined: attention };
  },

  // Grant extension host permission to always run on this host.
  setAlwaysOn(policy, uri) {
    if (!policy.active) {
      return;
    }
    let perms = { permissions: [], origins: ["*://" + uri.host] };
    return ExtensionPermissions.add(policy.id, perms, policy.extension);
  },

  // Revoke permission, extension should run only when clicked on this host.
  setWhenClicked(policy, uri) {
    if (!policy.active) {
      return;
    }
    let perms = { permissions: [], origins: ["*://" + uri.host] };
    return ExtensionPermissions.remove(policy.id, perms, policy.extension);
  },

  /**
   * @typedef {object} FluentIdInfo
   * @param {string} default      the message ID corresponding to the state
   *                              that should be displayed by default.
   * @param {string | null} onHover an optional message ID to be shown when
   *                              users hover interactive elements (e.g. a
   *                              button).
   */

  /**
   * Get origin controls messages (fluent IDs) to be shown to users for a given
   * extension on a given host. The messages might be different for extensions
   * with a browser action (that might or might not open a popup).
   *
   * @param {object} params
   * @param {WebExtensionPolicy} params.policy an extension's policy
   * @param {NativeTab} params.tab             the current tab
   * @param {boolean} params.isAction          this should be true for
   *                                           extensions with a browser
   *                                           action, false otherwise.
   * @param {boolean} params.hasPopup          this should be true when the
   *                                           browser action opens a popup,
   *                                           false otherwise.
   *
   * @returns {FluentIdInfo?} An object with origin controls message IDs or
   *                        `null` when there is no message for the state.
   */
  getStateMessageIDs({ policy, tab, isAction = false, hasPopup = false }) {
    const state = this.getState(policy, tab);

    const onHoverForAction = hasPopup
      ? "origin-controls-state-runnable-hover-open"
      : "origin-controls-state-runnable-hover-run";

    if (state.noAccess) {
      return {
        default: state.quarantined
          ? "origin-controls-state-quarantined"
          : "origin-controls-state-no-access",
        onHover: isAction ? onHoverForAction : null,
      };
    }

    if (state.allDomains || (state.alwaysOn && state.hasAccess)) {
      return {
        default: "origin-controls-state-always-on",
        onHover: isAction ? onHoverForAction : null,
      };
    }

    if (state.whenClicked) {
      return {
        default: state.temporaryAccess
          ? "origin-controls-state-temporary-access"
          : "origin-controls-state-when-clicked",
        onHover: "origin-controls-state-hover-run-visit-only",
      };
    }

    return null;
  },
};

export var QuarantinedDomains = {
  getUserAllowedAddonIdPrefName(addonId) {
    return `${this.PREF_ADDONS_BRANCH_NAME}${addonId}`;
  },
  isUserAllowedAddonId(addonId) {
    return Services.prefs.getBoolPref(
      this.getUserAllowedAddonIdPrefName(addonId),
      false
    );
  },
  setUserAllowedAddonIdPref(addonId, userAllowed) {
    Services.prefs.setBoolPref(
      this.getUserAllowedAddonIdPrefName(addonId),
      userAllowed
    );
  },
  clearUserPref(addonId) {
    Services.prefs.clearUserPref(this.getUserAllowedAddonIdPrefName(addonId));
  },

  // Implementation internals.

  PREF_ADDONS_BRANCH_NAME: `extensions.quarantineIgnoredByUser.`,
  PREF_DOMAINSLIST_NAME: `extensions.quarantinedDomains.list`,
  _initialized: false,
  _init() {
    if (this._initialized) {
      return;
    }

    const onUserAllowedPrefChanged = this._onUserAllowedPrefChanged.bind(this);
    Services.prefs.addObserver(
      this.PREF_ADDONS_BRANCH_NAME,
      onUserAllowedPrefChanged
    );

    const onUpdatedDomainsListTelemetry =
      this._onUpdatedDomainsListTelemetry.bind(this);
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "currentDomainsList",
      this.PREF_DOMAINSLIST_NAME,
      "",
      onUpdatedDomainsListTelemetry,
      value => this._transformDomainsListPrefValue(value || "")
    );
    // Collect it at least once per session (and update it when the pref value changes).
    onUpdatedDomainsListTelemetry();

    const onAMRemoteSettingsSetPref =
      this._onAMRemoteSettingsSetPref.bind(this);
    Services.obs.addObserver(
      onAMRemoteSettingsSetPref,
      "am-remote-settings-setpref"
    );

    this._initialized = true;
  },
  async _onAMRemoteSettingsSetPref(subject, _topic) {
    const { prefName, prefValue } = subject?.wrappedJSObject ?? {};
    if (prefName !== this.PREF_DOMAINSLIST_NAME) {
      return;
    }
    Glean.extensionsQuarantinedDomains.remotehash.set(
      computeSha1HashAsString(prefValue || "")
    );
  },
  async _onUserAllowedPrefChanged(_subject, _topic, prefName) {
    let addonId = prefName.slice(this.PREF_ADDONS_BRANCH_NAME.length);
    // Sanity check.
    if (!addonId || prefName !== this.getUserAllowedAddonIdPrefName(addonId)) {
      return;
    }

    // Notify listeners, e.g. to update details in TelemetryEnvironment.
    const addon = await lazy.AddonManager.getAddonByID(addonId);
    // Do not call onPropertyChanged listeners if the addon cannot be found
    // anymore (e.g. it has been uninstalled).
    if (addon) {
      lazy.AddonManagerPrivate.callAddonListeners("onPropertyChanged", addon, [
        "quarantineIgnoredByUser",
      ]);
    }
  },
  _onUpdatedDomainsListTelemetry(_subject, _topic, _prefName) {
    Glean.extensionsQuarantinedDomains.listsize.set(
      this.currentDomainsList.set.size
    );
    Glean.extensionsQuarantinedDomains.listhash.set(
      this.currentDomainsList.hash
    );
  },
  _transformDomainsListPrefValue(value) {
    try {
      return {
        // NOTE: using a sha1 hash to make sure the resulting string will fit into the
        // unified telemetry scalar string the glean metrics is mirrored to (which is
        // limited to 50 characters).
        hash: computeSha1HashAsString(value || ""),
        set: new Set(
          value
            .split(",")
            .map(v => v.trim())
            .filter(v => v.length)
        ),
      };
    } catch (err) {
      return { hash: "unexpected-error", set: new Set() };
    }
  },
};
QuarantinedDomains._init();

// Constants exported for testing purpose.
export {
  OLD_JSON_FILENAME,
  OLD_RKV_DIRNAME,
  RKV_DIRNAME,
  VERSION_KEY,
  VERSION_VALUE,
};
