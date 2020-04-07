/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = [
  "Dictionary",
  "Extension",
  "ExtensionData",
  "Langpack",
  "Management",
  "ExtensionAddonObserver",
];

/* exported Extension, ExtensionData */

/*
 * This file is the main entry point for extensions. When an extension
 * loads, its bootstrap.js file creates a Extension instance
 * and calls .startup() on it. It calls .shutdown() when the extension
 * unloads. Extension manages any extension-specific state in
 * the chrome process.
 *
 * TODO(rpl): we are current restricting the extensions to a single process
 * (set as the current default value of the "dom.ipc.processCount.extension"
 * preference), if we switch to use more than one extension process, we have to
 * be sure that all the browser's frameLoader are associated to the same process,
 * e.g. by using the `sameProcessAsFrameLoader` property.
 * (http://searchfox.org/mozilla-central/source/dom/interfaces/base/nsIBrowser.idl)
 *
 * At that point we are going to keep track of the existing browsers associated to
 * a webextension to ensure that they are all running in the same process (and we
 * are also going to do the same with the browser element provided to the
 * addon debugging Remote Debugging actor, e.g. because the addon has been
 * reloaded by the user, we have to  ensure that the new extension pages are going
 * to run in the same process of the existing addon debugging browser element).
 */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.jsm",
  AddonSettings: "resource://gre/modules/addons/AddonSettings.jsm",
  AMTelemetry: "resource://gre/modules/AddonManager.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  ExtensionPermissions: "resource://gre/modules/ExtensionPermissions.jsm",
  ExtensionPreferencesManager:
    "resource://gre/modules/ExtensionPreferencesManager.jsm",
  ExtensionProcessScript: "resource://gre/modules/ExtensionProcessScript.jsm",
  ExtensionStorage: "resource://gre/modules/ExtensionStorage.jsm",
  ExtensionStorageIDB: "resource://gre/modules/ExtensionStorageIDB.jsm",
  ExtensionTelemetry: "resource://gre/modules/ExtensionTelemetry.jsm",
  FileSource: "resource://gre/modules/L10nRegistry.jsm",
  L10nRegistry: "resource://gre/modules/L10nRegistry.jsm",
  LightweightThemeManager: "resource://gre/modules/LightweightThemeManager.jsm",
  Log: "resource://gre/modules/Log.jsm",
  MessageChannel: "resource://gre/modules/MessageChannel.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  PluralForm: "resource://gre/modules/PluralForm.jsm",
  Schemas: "resource://gre/modules/Schemas.jsm",
  XPIProvider: "resource://gre/modules/addons/XPIProvider.jsm",
});

// This is used for manipulating jar entry paths, which always use Unix
// separators.
XPCOMUtils.defineLazyGetter(this, "OSPath", () => {
  let obj = {};
  ChromeUtils.import("resource://gre/modules/osfile/ospath_unix.jsm", obj);
  return obj;
});

XPCOMUtils.defineLazyGetter(this, "resourceProtocol", () =>
  Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler)
);

const { ExtensionCommon } = ChromeUtils.import(
  "resource://gre/modules/ExtensionCommon.jsm"
);
const { ExtensionParent } = ChromeUtils.import(
  "resource://gre/modules/ExtensionParent.jsm"
);
const { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetters(this, {
  aomStartup: [
    "@mozilla.org/addons/addon-manager-startup;1",
    "amIAddonManagerStartup",
  ],
  spellCheck: ["@mozilla.org/spellchecker/engine;1", "mozISpellCheckingEngine"],
  uuidGen: ["@mozilla.org/uuid-generator;1", "nsIUUIDGenerator"],
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "processCount",
  "dom.ipc.processCount.extension"
);

// Temporary pref to be turned on when ready.
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "allowPrivateBrowsingByDefault",
  "extensions.allowPrivateBrowsingByDefault",
  true
);

var {
  GlobalManager,
  ParentAPIManager,
  StartupCache,
  apiManager: Management,
} = ExtensionParent;

const { getUniqueId, promiseTimeout } = ExtensionUtils;

const { EventEmitter } = ExtensionCommon;

XPCOMUtils.defineLazyGetter(this, "console", ExtensionCommon.getConsole);

XPCOMUtils.defineLazyGetter(
  this,
  "LocaleData",
  () => ExtensionCommon.LocaleData
);

const { sharedData } = Services.ppmm;

const PRIVATE_ALLOWED_PERMISSION = "internal:privateBrowsingAllowed";

// The userContextID reserved for the extension storage (its purpose is ensuring that the IndexedDB
// storage used by the browser.storage.local API is not directly accessible from the extension code,
// it is defined and reserved as "userContextIdInternal.webextStorageLocal" in ContextualIdentityService.jsm).
const WEBEXT_STORAGE_USER_CONTEXT_ID = -1 >>> 0;

// The maximum time to wait for extension child shutdown blockers to complete.
const CHILD_SHUTDOWN_TIMEOUT_MS = 8000;

// Permissions that are only available to privileged extensions.
const PRIVILEGED_PERMS = new Set([
  "activityLog",
  "mozillaAddons",
  "geckoViewAddons",
  "telemetry",
  "urlbar",
  "normandyAddonStudy",
  "networkStatus",
]);

/**
 * Classify an individual permission from a webextension manifest
 * as a host/origin permission, an api permission, or a regular permission.
 *
 * @param {string} perm  The permission string to classify
 * @param {boolean} restrictSchemes
 * @param {boolean} isPrivileged whether or not the webextension is privileged
 *
 * @returns {object}
 *          An object with exactly one of the following properties:
 *          "origin" to indicate this is a host/origin permission.
 *          "api" to indicate this is an api permission
 *                (as used for webextensions experiments).
 *          "permission" to indicate this is a regular permission.
 *          "invalid" to indicate that the given permission cannot be used.
 */
function classifyPermission(perm, restrictSchemes, isPrivileged) {
  let match = /^(\w+)(?:\.(\w+)(?:\.\w+)*)?$/.exec(perm);
  if (!match) {
    try {
      let { pattern } = new MatchPattern(perm, {
        restrictSchemes,
        ignorePath: true,
      });
      return { origin: pattern };
    } catch (e) {
      return { invalid: perm };
    }
  } else if (match[1] == "experiments" && match[2]) {
    return { api: match[2] };
  } else if (!isPrivileged && PRIVILEGED_PERMS.has(match[1])) {
    return { invalid: perm };
  }
  return { permission: perm };
}

const LOGGER_ID_BASE = "addons.webextension.";
const UUID_MAP_PREF = "extensions.webextensions.uuids";
const LEAVE_STORAGE_PREF = "extensions.webextensions.keepStorageOnUninstall";
const LEAVE_UUID_PREF = "extensions.webextensions.keepUuidOnUninstall";

const COMMENT_REGEXP = new RegExp(
  String.raw`
    ^
    (
      (?:
        [^"\n] |
        " (?:[^"\\\n] | \\.)* "
      )*?
    )

    //.*
  `.replace(/\s+/g, ""),
  "gm"
);

// All moz-extension URIs use a machine-specific UUID rather than the
// extension's own ID in the host component. This makes it more
// difficult for web pages to detect whether a user has a given add-on
// installed (by trying to load a moz-extension URI referring to a
// web_accessible_resource from the extension). UUIDMap.get()
// returns the UUID for a given add-on ID.
var UUIDMap = {
  _read() {
    let pref = Services.prefs.getStringPref(UUID_MAP_PREF, "{}");
    try {
      return JSON.parse(pref);
    } catch (e) {
      Cu.reportError(`Error parsing ${UUID_MAP_PREF}.`);
      return {};
    }
  },

  _write(map) {
    Services.prefs.setStringPref(UUID_MAP_PREF, JSON.stringify(map));
  },

  get(id, create = true) {
    let map = this._read();

    if (id in map) {
      return map[id];
    }

    let uuid = null;
    if (create) {
      uuid = uuidGen.generateUUID().number;
      uuid = uuid.slice(1, -1); // Strip { and } off the UUID.

      map[id] = uuid;
      this._write(map);
    }
    return uuid;
  },

  remove(id) {
    let map = this._read();
    delete map[id];
    this._write(map);
  },
};

/**
 * Observer AddonManager events and translate them into extension events,
 * as well as handle any last cleanup after uninstalling an extension.
 */
var ExtensionAddonObserver = {
  initialized: false,

  init() {
    if (!this.initialized) {
      AddonManager.addAddonListener(this);
      this.initialized = true;
    }
  },

  // AddonTestUtils will call this as necessary.
  uninit() {
    if (this.initialized) {
      AddonManager.removeAddonListener(this);
      this.initialized = false;
    }
  },

  onEnabling(addon) {
    if (addon.type !== "extension") {
      return;
    }
    Management._callHandlers([addon.id], "enabling", "onEnabling");
  },

  onDisabled(addon) {
    if (addon.type !== "extension") {
      return;
    }
    if (Services.appinfo.inSafeMode) {
      // Ensure ExtensionPreferencesManager updates its data and
      // modules can run any disable logic they need to.  We only
      // handle safeMode here because there is a bunch of additional
      // logic that happens in Extension.shutdown when running in
      // normal mode.
      Management._callHandlers([addon.id], "disable", "onDisable");
    }
  },

  onUninstalling(addon) {
    let extension = GlobalManager.extensionMap.get(addon.id);
    if (extension) {
      // Let any other interested listeners respond
      // (e.g., display the uninstall URL)
      Management.emit("uninstalling", extension);
    }
  },

  onUninstalled(addon) {
    let uuid = UUIDMap.get(addon.id, false);
    if (!uuid) {
      return;
    }

    if (!Services.prefs.getBoolPref(LEAVE_STORAGE_PREF, false)) {
      // Clear browser.storage.local backends.
      AsyncShutdown.profileChangeTeardown.addBlocker(
        `Clear Extension Storage ${addon.id} (File Backend)`,
        ExtensionStorage.clear(addon.id, { shouldNotifyListeners: false })
      );

      // Clear any IndexedDB storage created by the extension
      // If LSNG is enabled, this also clears localStorage.
      let baseURI = Services.io.newURI(`moz-extension://${uuid}/`);
      let principal = Services.scriptSecurityManager.createContentPrincipal(
        baseURI,
        {}
      );
      Services.qms.clearStoragesForPrincipal(principal);

      // Clear any storage.local data stored in the IDBBackend.
      let storagePrincipal = Services.scriptSecurityManager.createContentPrincipal(
        baseURI,
        {
          userContextId: WEBEXT_STORAGE_USER_CONTEXT_ID,
        }
      );
      Services.qms.clearStoragesForPrincipal(storagePrincipal);

      ExtensionStorageIDB.clearMigratedExtensionPref(addon.id);

      // If LSNG is not enabled, we need to clear localStorage explicitly using
      // the old API.
      if (!Services.lsm.nextGenLocalStorageEnabled) {
        // Clear localStorage created by the extension
        let storage = Services.domStorageManager.getStorage(
          null,
          principal,
          principal
        );
        if (storage) {
          storage.clear();
        }
      }

      // Remove any permissions related to the unlimitedStorage permission
      // if we are also removing all the data stored by the extension.
      Services.perms.removeFromPrincipal(
        principal,
        "WebExtensions-unlimitedStorage"
      );
      Services.perms.removeFromPrincipal(principal, "indexedDB");
      Services.perms.removeFromPrincipal(principal, "persistent-storage");
    }

    ExtensionPermissions.removeAll(addon.id);

    if (!Services.prefs.getBoolPref(LEAVE_UUID_PREF, false)) {
      // Clear the entry in the UUID map
      UUIDMap.remove(addon.id);
    }
  },
};

ExtensionAddonObserver.init();

const manifestTypes = new Map([
  ["theme", "manifest.ThemeManifest"],
  ["langpack", "manifest.WebExtensionLangpackManifest"],
  ["dictionary", "manifest.WebExtensionDictionaryManifest"],
  ["extension", "manifest.WebExtensionManifest"],
]);

/**
 * Represents the data contained in an extension, contained either
 * in a directory or a zip file, which may or may not be installed.
 * This class implements the functionality of the Extension class,
 * primarily related to manifest parsing and localization, which is
 * useful prior to extension installation or initialization.
 *
 * No functionality of this class is guaranteed to work before
 * `loadManifest` has been called, and completed.
 */
class ExtensionData {
  constructor(rootURI) {
    this.rootURI = rootURI;
    this.resourceURL = rootURI.spec;

    this.manifest = null;
    this.type = null;
    this.id = null;
    this.uuid = null;
    this.localeData = null;
    this.fluentL10n = null;
    this._promiseLocales = null;

    this.apiNames = new Set();
    this.dependencies = new Set();
    this.permissions = new Set();

    this.startupData = null;

    this.errors = [];
    this.warnings = [];
  }

  get builtinMessages() {
    return null;
  }

  get logger() {
    let id = this.id || "<unknown>";
    return Log.repository.getLogger(LOGGER_ID_BASE + id);
  }

  /**
   * Report an error about the extension's manifest file.
   * @param {string} message The error message
   */
  manifestError(message) {
    this.packagingError(`Reading manifest: ${message}`);
  }

  /**
   * Report a warning about the extension's manifest file.
   * @param {string} message The warning message
   */
  manifestWarning(message) {
    this.packagingWarning(`Reading manifest: ${message}`);
  }

  // Report an error about the extension's general packaging.
  packagingError(message) {
    this.errors.push(message);
    this.logError(message);
  }

  packagingWarning(message) {
    this.warnings.push(message);
    this.logWarning(message);
  }

  logWarning(message) {
    this._logMessage(message, "warn");
  }

  logError(message) {
    this._logMessage(message, "error");
  }

  _logMessage(message, severity) {
    this.logger[severity](`Loading extension '${this.id}': ${message}`);
  }

  /**
   * Returns the moz-extension: URL for the given path within this
   * extension.
   *
   * Must not be called unless either the `id` or `uuid` property has
   * already been set.
   *
   * @param {string} path The path portion of the URL.
   * @returns {string}
   */
  getURL(path = "") {
    if (!(this.id || this.uuid)) {
      throw new Error(
        "getURL may not be called before an `id` or `uuid` has been set"
      );
    }
    if (!this.uuid) {
      this.uuid = UUIDMap.get(this.id);
    }
    return `moz-extension://${this.uuid}/${path}`;
  }

  async readDirectory(path) {
    if (this.rootURI instanceof Ci.nsIFileURL) {
      let uri = Services.io.newURI("./" + path, null, this.rootURI);
      let fullPath = uri.QueryInterface(Ci.nsIFileURL).file.path;

      let iter = new OS.File.DirectoryIterator(fullPath);
      let results = [];

      try {
        await iter.forEach(entry => {
          results.push(entry);
        });
      } catch (e) {
        // Always return a list, even if the directory does not exist (or is
        // not a directory) for symmetry with the ZipReader behavior.
        if (!e.becauseNoSuchFile) {
          Cu.reportError(e);
        }
      }
      iter.close();

      return results;
    }

    let uri = this.rootURI.QueryInterface(Ci.nsIJARURI);

    // Append the sub-directory path to the base JAR URI and normalize the
    // result.
    let entry = `${uri.JAREntry}/${path}/`
      .replace(/\/\/+/g, "/")
      .replace(/^\//, "");
    uri = Services.io.newURI(`jar:${uri.JARFile.spec}!/${entry}`);

    let results = [];
    for (let name of aomStartup.enumerateJARSubtree(uri)) {
      if (!name.startsWith(entry)) {
        throw new Error("Unexpected ZipReader entry");
      }

      // The enumerator returns the full path of all entries.
      // Trim off the leading path, and filter out entries from
      // subdirectories.
      name = name.slice(entry.length);
      if (name && !/\/./.test(name)) {
        results.push({
          name: name.replace("/", ""),
          isDir: name.endsWith("/"),
        });
      }
    }

    return results;
  }

  readJSON(path) {
    return new Promise((resolve, reject) => {
      let uri = this.rootURI.resolve(`./${path}`);

      NetUtil.asyncFetch(
        { uri, loadUsingSystemPrincipal: true },
        (inputStream, status) => {
          if (!Components.isSuccessCode(status)) {
            // Convert status code to a string
            let e = Components.Exception("", status);
            reject(new Error(`Error while loading '${uri}' (${e.name})`));
            return;
          }
          try {
            let text = NetUtil.readInputStreamToString(
              inputStream,
              inputStream.available(),
              { charset: "utf-8" }
            );

            text = text.replace(COMMENT_REGEXP, "$1");

            resolve(JSON.parse(text));
          } catch (e) {
            reject(e);
          }
        }
      );
    });
  }

  get restrictSchemes() {
    // ExtensionData can't check the signature (as it is not yet passed to its constructor
    // as it is for the Extension class, where this getter is overridden to check both the
    // signature and the permissions).
    return !this.hasPermission("mozillaAddons");
  }

  /**
   * Given an array of host and permissions, generate a structured permissions object
   * that contains seperate host origins and permissions arrays.
   *
   * @param {Array} permissionsArray
   * @returns {Object} permissions object
   */
  permissionsObject(permissionsArray) {
    let permissions = new Set();
    let origins = new Set();
    let { restrictSchemes, isPrivileged } = this;
    for (let perm of permissionsArray || []) {
      let type = classifyPermission(perm, restrictSchemes, isPrivileged);
      if (type.origin) {
        origins.add(perm);
      } else if (type.permission) {
        permissions.add(perm);
      }
    }
    return {
      permissions,
      origins,
    };
  }

  /**
   * Returns an object representing any capabilities that the extension
   * has access to based on fixed properties in the manifest.  The result
   * includes the contents of the "permissions" property as well as other
   * capabilities that are derived from manifest fields that users should
   * be informed of (e.g., origins where content scripts are injected).
   */
  get manifestPermissions() {
    if (this.type !== "extension") {
      return null;
    }

    let { permissions, origins } = this.permissionsObject(
      this.manifest.permissions
    );

    if (this.manifest.devtools_page) {
      permissions.add("devtools");
    }

    for (let entry of this.manifest.content_scripts || []) {
      for (let origin of entry.matches) {
        origins.add(origin);
      }
    }

    return {
      permissions: Array.from(permissions),
      origins: Array.from(origins),
    };
  }

  get manifestOptionalPermissions() {
    let { permissions, origins } = this.permissionsObject(
      this.manifest.optional_permissions
    );
    return {
      permissions: Array.from(permissions),
      origins: Array.from(origins),
    };
  }

  /**
   * Returns an object representing all capabilities this extension has
   * access to, including fixed ones from the manifest as well as dynamically
   * granted permissions.
   */
  get activePermissions() {
    if (this.type !== "extension") {
      return null;
    }

    let result = {
      origins: this.whiteListedHosts.patterns
        .map(matcher => matcher.pattern)
        // moz-extension://id/* is always added to whiteListedHosts, but it
        // is not a valid host permission in the API. So, remove it.
        .filter(pattern => !pattern.startsWith("moz-extension:")),
      apis: [...this.apiNames],
    };

    const EXP_PATTERN = /^experiments\.\w+/;
    result.permissions = [...this.permissions].filter(
      p => !result.origins.includes(p) && !EXP_PATTERN.test(p)
    );
    return result;
  }

  // Compute the difference between two sets of permissions, suitable
  // for presenting to the user.
  static comparePermissions(oldPermissions, newPermissions) {
    let oldMatcher = new MatchPatternSet(oldPermissions.origins, {
      restrictSchemes: false,
    });
    return {
      // formatPermissionStrings ignores any scheme, so only look at the domain.
      origins: newPermissions.origins.filter(
        perm =>
          !oldMatcher.subsumesDomain(
            new MatchPattern(perm, { restrictSchemes: false })
          )
      ),
      permissions: newPermissions.permissions.filter(
        perm => !oldPermissions.permissions.includes(perm)
      ),
    };
  }

  // Return those permissions in oldPermissions that also exist in newPermissions.
  static intersectPermissions(oldPermissions, newPermissions) {
    let matcher = new MatchPatternSet(newPermissions.origins, {
      restrictSchemes: false,
    });

    return {
      origins: oldPermissions.origins.filter(perm =>
        matcher.subsumesDomain(
          new MatchPattern(perm, { restrictSchemes: false })
        )
      ),
      permissions: oldPermissions.permissions.filter(perm =>
        newPermissions.permissions.includes(perm)
      ),
    };
  }

  /**
   * When updating the addon, find and migrate permissions that have moved from required
   * to optional.  This also handles any updates required for permission removal.
   *
   * @param {string} id The id of the addon being updated
   * @param {Object} oldPermissions
   * @param {Object} oldOptionalPermissions
   * @param {Object} newPermissions
   * @param {Object} newOptionalPermissions
   */
  static async migratePermissions(
    id,
    oldPermissions,
    oldOptionalPermissions,
    newPermissions,
    newOptionalPermissions
  ) {
    let migrated = ExtensionData.intersectPermissions(
      oldPermissions,
      newOptionalPermissions
    );
    // If a permission is optional in this version and was mandatory in the previous
    // version, it was already accepted by the user at install time so add it to the
    // list of granted optional permissions now.
    await ExtensionPermissions.add(id, migrated);

    // Now we need to update ExtensionPreferencesManager, removing any settings
    // for old permissions that no longer exist.
    let permSet = new Set(
      newPermissions.permissions.concat(newOptionalPermissions.permissions)
    );
    let oldPerms = oldPermissions.permissions.concat(
      oldOptionalPermissions.permissions
    );

    let removed = oldPerms.filter(x => !permSet.has(x));
    if (removed.length) {
      await Management.asyncLoadSettingsModules();
      for (let name of removed) {
        await ExtensionPreferencesManager.removeSettingsForPermission(id, name);
      }
    }

    // Remove any optional permissions that have been removed from the manifest.
    await ExtensionPermissions.remove(id, {
      permissions: removed,
      origins: [],
    });
  }

  canUseExperiment(manifest) {
    return this.experimentsAllowed && manifest.experiment_apis;
  }

  /**
   * Load a locale and return a localized manifest.  The extension must
   * be initialized, and manifest parsed prior to calling.
   *
   * @param {string} locale to load, if necessary.
   * @returns {object} normalized manifest.
   */
  async getLocalizedManifest(locale) {
    if (!this.type || !this.localeData) {
      throw new Error("The extension has not been initialized.");
    }
    // Upon update or reinstall, the Extension.manifest may be read from
    // StartupCache.manifest, however rawManifest is *not*.  We need the
    // raw manifest in order to get a localized manifest.
    if (!this.rawManifest) {
      this.rawManifest = await this.readJSON("manifest.json");
    }

    if (!this.localeData.has(locale)) {
      // Locales are not avialable until some additional
      // initialization is done.  We could just call initAllLocales,
      // but that is heavy handed, especially when we likely only
      // need one out of 20.
      let locales = await this.promiseLocales();
      if (locales.get(locale)) {
        await this.initLocale(locale);
      }
      if (!this.localeData.has(locale)) {
        throw new Error(`The extension does not contain the locale ${locale}`);
      }
    }
    let normalized = await this._getNormalizedManifest(locale);
    if (normalized.error) {
      throw new Error(normalized.error);
    }
    return normalized.value;
  }

  async _getNormalizedManifest(locale) {
    let manifestType = manifestTypes.get(this.type);

    let context = {
      url: this.baseURI && this.baseURI.spec,
      principal: this.principal,
      logError: error => {
        this.manifestWarning(error);
      },
      preprocessors: {},
    };

    if (this.fluentL10n || this.localeData) {
      context.preprocessors.localize = (value, context) =>
        this.localize(value, locale);
    }

    return Schemas.normalize(this.rawManifest, manifestType, context);
  }

  // eslint-disable-next-line complexity
  async parseManifest() {
    let [manifest] = await Promise.all([
      this.readJSON("manifest.json"),
      Management.lazyInit(),
    ]);

    this.manifest = manifest;
    this.rawManifest = manifest;

    if (
      allowPrivateBrowsingByDefault &&
      "incognito" in manifest &&
      manifest.incognito == "not_allowed"
    ) {
      throw new Error(
        `manifest.incognito set to "not_allowed" is currently unvailable for use.`
      );
    }

    if (manifest && manifest.default_locale) {
      await this.initLocale();
    }

    // When parsing the manifest from an ExtensionData instance, we don't
    // have isPrivileged, so ignore fluent localization in that pass.
    // This means that fluent cannot be used to localize manifest properties
    // read from the add-on manager (e.g., author, homepage, etc.)
    if (manifest && manifest.l10n_resources && "isPrivileged" in this) {
      if (this.isPrivileged) {
        this.fluentL10n = new Localization(manifest.l10n_resources, true);
      } else {
        // Warn but don't make this fatal.
        Cu.reportError("Ignoring l10n_resources in unprivileged extension");
      }
    }

    if (this.manifest.theme) {
      this.type = "theme";
    } else if (this.manifest.langpack_id) {
      this.type = "langpack";
    } else if (this.manifest.dictionaries) {
      this.type = "dictionary";
    } else {
      this.type = "extension";
    }

    let normalized = await this._getNormalizedManifest();
    if (normalized.error) {
      this.manifestError(normalized.error);
      return null;
    }

    manifest = normalized.value;

    let id;
    try {
      if (manifest.applications.gecko.id) {
        id = manifest.applications.gecko.id;
      }
    } catch (e) {
      // Errors are handled by the type checks above.
    }

    if (!this.id) {
      this.id = id;
    }

    let apiNames = new Set();
    let dependencies = new Set();
    let originPermissions = new Set();
    let permissions = new Set();
    let webAccessibleResources = [];

    let schemaPromises = new Map();

    let result = {
      apiNames,
      dependencies,
      id,
      manifest,
      modules: null,
      originPermissions,
      permissions,
      schemaURLs: null,
      type: this.type,
      webAccessibleResources,
    };

    if (this.type === "extension") {
      let { isPrivileged } = this;
      let restrictSchemes = !(
        isPrivileged && manifest.permissions.includes("mozillaAddons")
      );

      for (let perm of manifest.permissions) {
        if (perm === "geckoProfiler" && !isPrivileged) {
          const acceptedExtensions = Services.prefs.getStringPref(
            "extensions.geckoProfiler.acceptedExtensionIds",
            ""
          );
          if (!acceptedExtensions.split(",").includes(id)) {
            this.manifestError(
              "Only whitelisted extensions are allowed to access the geckoProfiler."
            );
            continue;
          }
        }

        let type = classifyPermission(perm, restrictSchemes, isPrivileged);
        if (type.origin) {
          perm = type.origin;
          originPermissions.add(perm);
        } else if (type.api) {
          apiNames.add(type.api);
        } else if (type.invalid) {
          this.manifestWarning(`Invalid extension permission: ${perm}`);
          continue;
        }

        permissions.add(perm);
      }

      if (this.id) {
        // An extension always gets permission to its own url.
        let matcher = new MatchPattern(this.getURL(), { ignorePath: true });
        originPermissions.add(matcher.pattern);

        // Apply optional permissions
        let perms = await ExtensionPermissions.get(this.id);
        for (let perm of perms.permissions) {
          permissions.add(perm);
        }
        for (let origin of perms.origins) {
          originPermissions.add(origin);
        }
      }

      for (let api of apiNames) {
        dependencies.add(`${api}@experiments.addons.mozilla.org`);
      }

      let moduleData = data => ({
        url: this.rootURI.resolve(data.script),
        events: data.events,
        paths: data.paths,
        scopes: data.scopes,
      });

      let computeModuleInit = (scope, modules) => {
        let manager = new ExtensionCommon.SchemaAPIManager(scope);
        return manager.initModuleJSON([modules]);
      };

      result.contentScripts = [];
      for (let options of manifest.content_scripts || []) {
        result.contentScripts.push({
          allFrames: options.all_frames,
          matchAboutBlank: options.match_about_blank,
          frameID: options.frame_id,
          runAt: options.run_at,

          matches: options.matches,
          excludeMatches: options.exclude_matches || [],
          includeGlobs: options.include_globs,
          excludeGlobs: options.exclude_globs,

          jsPaths: options.js || [],
          cssPaths: options.css || [],
        });
      }

      if (this.canUseExperiment(manifest)) {
        let parentModules = {};
        let childModules = {};

        for (let [name, data] of Object.entries(manifest.experiment_apis)) {
          let schema = this.getURL(data.schema);

          if (!schemaPromises.has(schema)) {
            schemaPromises.set(
              schema,
              this.readJSON(data.schema).then(json =>
                Schemas.processSchema(json)
              )
            );
          }

          if (data.parent) {
            parentModules[name] = moduleData(data.parent);
          }

          if (data.child) {
            childModules[name] = moduleData(data.child);
          }
        }

        result.modules = {
          child: computeModuleInit("addon_child", childModules),
          parent: computeModuleInit("addon_parent", parentModules),
        };
      }

      // Normalize all patterns to contain a single leading /
      if (manifest.web_accessible_resources) {
        webAccessibleResources.push(
          ...manifest.web_accessible_resources.map(path =>
            path.replace(/^\/*/, "/")
          )
        );
      }
    } else if (this.type == "langpack") {
      // Langpack startup is performance critical, so we want to compute as much
      // as possible here to make startup not trigger async DB reads.
      // We'll store the four items below in the startupData.

      // 1. Compute the chrome resources to be registered for this langpack.
      const platform = AppConstants.platform;
      const chromeEntries = [];
      for (const [language, entry] of Object.entries(manifest.languages)) {
        for (const [alias, path] of Object.entries(
          entry.chrome_resources || {}
        )) {
          if (typeof path === "string") {
            chromeEntries.push(["locale", alias, language, path]);
          } else if (platform in path) {
            // If the path is not a string, it's an object with path per
            // platform where the keys are taken from AppConstants.platform
            chromeEntries.push(["locale", alias, language, path[platform]]);
          }
        }
      }

      // 2. Compute langpack ID.
      const productCodeName = AppConstants.MOZ_BUILD_APP.replace("/", "-");

      // The result path looks like this:
      //   Firefox - `langpack-pl-browser`
      //   Fennec - `langpack-pl-mobile-android`
      const langpackId = `langpack-${manifest.langpack_id}-${productCodeName}`;

      // 3. Compute L10nRegistry sources for this langpack.
      const l10nRegistrySources = {};

      // Check if there's a root directory `/localization` in the langpack.
      // If there is one, add it with the name `toolkit` as a FileSource.
      const entries = await this.readDirectory("localization");
      if (entries.length) {
        l10nRegistrySources.toolkit = "";
      }

      // Add any additional sources listed in the manifest
      if (manifest.sources) {
        for (const [sourceName, { base_path }] of Object.entries(
          manifest.sources
        )) {
          l10nRegistrySources[sourceName] = base_path;
        }
      }

      // 4. Save the list of languages handled by this langpack.
      const languages = Object.keys(manifest.languages);

      this.startupData = {
        chromeEntries,
        langpackId,
        l10nRegistrySources,
        languages,
      };
    } else if (this.type == "dictionary") {
      let dictionaries = {};
      for (let [lang, path] of Object.entries(manifest.dictionaries)) {
        path = path.replace(/^\/+/, "");

        let dir = OSPath.dirname(path);
        if (dir === ".") {
          dir = "";
        }
        let leafName = OSPath.basename(path);
        let affixPath = leafName.slice(0, -3) + "aff";

        let entries = Array.from(
          await this.readDirectory(dir),
          entry => entry.name
        );
        if (!entries.includes(leafName)) {
          this.manifestError(
            `Invalid dictionary path specified for '${lang}': ${path}`
          );
        }
        if (!entries.includes(affixPath)) {
          this.manifestError(
            `Invalid dictionary path specified for '${lang}': Missing affix file: ${path}`
          );
        }

        dictionaries[lang] = path;
      }

      this.startupData = { dictionaries };
    }

    if (schemaPromises.size) {
      let schemas = new Map();
      for (let [url, promise] of schemaPromises) {
        schemas.set(url, await promise);
      }
      result.schemaURLs = schemas;
    }

    return result;
  }

  // Reads the extension's |manifest.json| file, and stores its
  // parsed contents in |this.manifest|.
  async loadManifest() {
    let [manifestData] = await Promise.all([
      this.parseManifest(),
      Management.lazyInit(),
    ]);

    if (!manifestData) {
      return;
    }

    // Do not override the add-on id that has been already assigned.
    if (!this.id) {
      this.id = manifestData.id;
    }

    this.manifest = manifestData.manifest;
    this.apiNames = manifestData.apiNames;
    this.contentScripts = manifestData.contentScripts;
    this.dependencies = manifestData.dependencies;
    this.permissions = manifestData.permissions;
    this.schemaURLs = manifestData.schemaURLs;
    this.type = manifestData.type;

    this.modules = manifestData.modules;

    this.apiManager = this.getAPIManager();
    await this.apiManager.lazyInit();

    this.webAccessibleResources = manifestData.webAccessibleResources.map(
      res => new MatchGlob(res)
    );
    this.whiteListedHosts = new MatchPatternSet(
      manifestData.originPermissions,
      {
        restrictSchemes: this.restrictSchemes,
      }
    );

    return this.manifest;
  }

  hasPermission(perm, includeOptional = false) {
    // If the permission is a "manifest property" permission, we check if the extension
    // does have the required property in its manifest.
    let manifest_ = "manifest:";
    if (perm.startsWith(manifest_)) {
      // Handle nested "manifest property" permission (e.g. as in "manifest:property.nested").
      let value = this.manifest;
      for (let prop of perm.substr(manifest_.length).split(".")) {
        if (!value) {
          break;
        }
        value = value[prop];
      }

      return value != null;
    }

    if (this.permissions.has(perm)) {
      return true;
    }

    if (includeOptional && this.manifest.optional_permissions.includes(perm)) {
      return true;
    }

    return false;
  }

  getAPIManager() {
    let apiManagers = [Management];

    for (let id of this.dependencies) {
      let policy = WebExtensionPolicy.getByID(id);
      if (policy) {
        if (policy.extension.experimentAPIManager) {
          apiManagers.push(policy.extension.experimentAPIManager);
        } else if (AppConstants.DEBUG) {
          Cu.reportError(`Cannot find experimental API exported from ${id}`);
        }
      }
    }

    if (this.modules) {
      this.experimentAPIManager = new ExtensionCommon.LazyAPIManager(
        "main",
        this.modules.parent,
        this.schemaURLs
      );

      apiManagers.push(this.experimentAPIManager);
    }

    if (apiManagers.length == 1) {
      return apiManagers[0];
    }

    return new ExtensionCommon.MultiAPIManager("main", apiManagers.reverse());
  }

  localizeMessage(...args) {
    return this.localeData.localizeMessage(...args);
  }

  localize(str, locale) {
    // If the extension declares fluent resources in the manifest, try
    // first to localize with fluent.  Also use the original webextension
    // method (_locales/xx.json) so extensions can migrate bit by bit.
    // Note also that fluent keys typically use hyphense, so hyphens are
    // allowed in the __MSG_foo__ keys used by fluent, though they are
    // not allowed in the keys used for json translations.
    if (this.fluentL10n) {
      str = str.replace(/__MSG_([-A-Za-z0-9@_]+?)__/g, (matched, message) => {
        let translation = this.fluentL10n.formatValueSync(message);
        return translation !== undefined ? translation : matched;
      });
    }
    if (this.localeData) {
      str = this.localeData.localize(str, locale);
    }
    return str;
  }

  // If a "default_locale" is specified in that manifest, returns it
  // as a Gecko-compatible locale string. Otherwise, returns null.
  get defaultLocale() {
    if (this.manifest.default_locale != null) {
      return this.normalizeLocaleCode(this.manifest.default_locale);
    }

    return null;
  }

  // Normalizes a Chrome-compatible locale code to the appropriate
  // Gecko-compatible variant. Currently, this means simply
  // replacing underscores with hyphens.
  normalizeLocaleCode(locale) {
    return locale.replace(/_/g, "-");
  }

  // Reads the locale file for the given Gecko-compatible locale code, and
  // stores its parsed contents in |this.localeMessages.get(locale)|.
  async readLocaleFile(locale) {
    let locales = await this.promiseLocales();
    let dir = locales.get(locale) || locale;
    let file = `_locales/${dir}/messages.json`;

    try {
      let messages = await this.readJSON(file);
      return this.localeData.addLocale(locale, messages, this);
    } catch (e) {
      this.packagingError(`Loading locale file ${file}: ${e}`);
      return new Map();
    }
  }

  async _promiseLocaleMap() {
    let locales = new Map();

    let entries = await this.readDirectory("_locales");
    for (let file of entries) {
      if (file.isDir) {
        let locale = this.normalizeLocaleCode(file.name);
        locales.set(locale, file.name);
      }
    }

    return locales;
  }

  _setupLocaleData(locales) {
    if (this.localeData) {
      return this.localeData.locales;
    }

    this.localeData = new LocaleData({
      defaultLocale: this.defaultLocale,
      locales,
      builtinMessages: this.builtinMessages,
    });

    return locales;
  }

  // Reads the list of locales available in the extension, and returns a
  // Promise which resolves to a Map upon completion.
  // Each map key is a Gecko-compatible locale code, and each value is the
  // "_locales" subdirectory containing that locale:
  //
  // Map(gecko-locale-code -> locale-directory-name)
  promiseLocales() {
    if (!this._promiseLocales) {
      this._promiseLocales = (async () => {
        let locales = this._promiseLocaleMap();
        return this._setupLocaleData(locales);
      })();
    }

    return this._promiseLocales;
  }

  // Reads the locale messages for all locales, and returns a promise which
  // resolves to a Map of locale messages upon completion. Each key in the map
  // is a Gecko-compatible locale code, and each value is a locale data object
  // as returned by |readLocaleFile|.
  async initAllLocales() {
    let locales = await this.promiseLocales();

    await Promise.all(
      Array.from(locales.keys(), locale => this.readLocaleFile(locale))
    );

    let defaultLocale = this.defaultLocale;
    if (defaultLocale) {
      if (!locales.has(defaultLocale)) {
        this.manifestError(
          'Value for "default_locale" property must correspond to ' +
            'a directory in "_locales/". Not found: ' +
            JSON.stringify(`_locales/${this.manifest.default_locale}/`)
        );
      }
    } else if (locales.size) {
      this.manifestError(
        'The "default_locale" property is required when a ' +
          '"_locales/" directory is present.'
      );
    }

    return this.localeData.messages;
  }

  // Reads the locale file for the given Gecko-compatible locale code, or the
  // default locale if no locale code is given, and sets it as the currently
  // selected locale on success.
  //
  // Pre-loads the default locale for fallback message processing, regardless
  // of the locale specified.
  //
  // If no locales are unavailable, resolves to |null|.
  async initLocale(locale = this.defaultLocale) {
    if (locale == null) {
      return null;
    }

    let promises = [this.readLocaleFile(locale)];

    let { defaultLocale } = this;
    if (locale != defaultLocale && !this.localeData.has(defaultLocale)) {
      promises.push(this.readLocaleFile(defaultLocale));
    }

    let results = await Promise.all(promises);

    this.localeData.selectedLocale = locale;
    return results[0];
  }

  /**
   * Formats all the strings for a permissions dialog/notification.
   *
   * @param {object} info Information about the permissions being requested.
   *
   * @param {array<string>} info.permissions.origins
   *                        Origin permissions requested.
   * @param {array<string>} info.permissions.permissions
   *                        Regular (non-origin) permissions requested.
   * @param {boolean} info.unsigned
   *                  True if the prompt is for installing an unsigned addon.
   * @param {string} info.type
   *                 The type of prompt being shown.  May be one of "update",
   *                 "sideload", "optional", or omitted for a regular
   *                 install prompt.
   * @param {string} info.appName
   *                 The localized name of the application, to be substituted
   *                 in computed strings as needed.
   * @param {nsIStringBundle} bundle
   *                          The string bundle to use for l10n.
   * @param {object} options
   * @param {boolean} options.collapseOrigins
   *                  Wether to limit the number of displayed host permissions.
   *                  Default is false.
   *
   * @returns {object} An object with properties containing localized strings
   *                   for various elements of a permission dialog. The "header"
   *                   property on this object is the notification header text
   *                   and it has the string "<>" as a placeholder for the
   *                   addon name.
   */
  static formatPermissionStrings(
    info,
    bundle,
    { collapseOrigins = false } = {}
  ) {
    let result = {};

    let perms = info.permissions || { origins: [], permissions: [] };

    // First classify our host permissions
    let allUrls = false,
      wildcards = new Set(),
      sites = new Set();
    for (let permission of perms.origins) {
      if (permission == "<all_urls>") {
        allUrls = true;
        break;
      }

      // Privileged extensions may request access to "about:"-URLs, such as
      // about:reader.
      let match = /^[a-z*]+:\/\/([^/]*)\/|^about:/.exec(permission);
      if (!match) {
        throw new Error(`Unparseable host permission ${permission}`);
      }
      // Note: the scheme is ignored in the permission warnings. If this ever
      // changes, update the comparePermissions method as needed.
      if (!match[1] || match[1] == "*") {
        allUrls = true;
      } else if (match[1].startsWith("*.")) {
        wildcards.add(match[1].slice(2));
      } else {
        sites.add(match[1]);
      }
    }

    // Format the host permissions.  If we have a wildcard for all urls,
    // a single string will suffice.  Otherwise, show domain wildcards
    // first, then individual host permissions.
    result.msgs = [];
    if (allUrls) {
      result.msgs.push(
        bundle.GetStringFromName("webextPerms.hostDescription.allUrls")
      );
    } else {
      // Formats a list of host permissions.  If we have 4 or fewer, display
      // them all, otherwise display the first 3 followed by an item that
      // says "...plus N others"
      let format = (list, itemKey, moreKey) => {
        function formatItems(items) {
          result.msgs.push(
            ...items.map(item => bundle.formatStringFromName(itemKey, [item]))
          );
        }
        if (list.length < 5 || !collapseOrigins) {
          formatItems(list);
        } else {
          formatItems(list.slice(0, 3));

          let remaining = list.length - 3;
          result.msgs.push(
            PluralForm.get(
              remaining,
              bundle.GetStringFromName(moreKey)
            ).replace("#1", remaining)
          );
        }
      };

      format(
        Array.from(wildcards),
        "webextPerms.hostDescription.wildcard",
        "webextPerms.hostDescription.tooManyWildcards"
      );
      format(
        Array.from(sites),
        "webextPerms.hostDescription.oneSite",
        "webextPerms.hostDescription.tooManySites"
      );
    }

    let permissionKey = perm => `webextPerms.description.${perm}`;

    // Next, show the native messaging permission if it is present.
    const NATIVE_MSG_PERM = "nativeMessaging";
    if (perms.permissions.includes(NATIVE_MSG_PERM)) {
      result.msgs.push(
        bundle.formatStringFromName(permissionKey(NATIVE_MSG_PERM), [
          info.appName,
        ])
      );
    }

    // Finally, show remaining permissions, in the same order as AMO.
    // The permissions are sorted alphabetically by the permission
    // string to match AMO.
    let permissionsCopy = perms.permissions.slice(0);
    for (let permission of permissionsCopy.sort()) {
      // Handled above
      if (permission == "nativeMessaging") {
        continue;
      }
      try {
        result.msgs.push(bundle.GetStringFromName(permissionKey(permission)));
      } catch (err) {
        // We deliberately do not include all permissions in the prompt.
        // So if we don't find one then just skip it.
      }
    }

    const haveAccessKeys = AppConstants.platform !== "android";

    result.header = bundle.formatStringFromName("webextPerms.header", ["<>"]);
    result.text = info.unsigned
      ? bundle.GetStringFromName("webextPerms.unsignedWarning")
      : "";
    result.listIntro = bundle.GetStringFromName("webextPerms.listIntro");

    result.acceptText = bundle.GetStringFromName("webextPerms.add.label");
    result.cancelText = bundle.GetStringFromName("webextPerms.cancel.label");
    if (haveAccessKeys) {
      result.acceptKey = bundle.GetStringFromName("webextPerms.add.accessKey");
      result.cancelKey = bundle.GetStringFromName(
        "webextPerms.cancel.accessKey"
      );
    }

    if (info.type == "sideload") {
      result.header = bundle.formatStringFromName(
        "webextPerms.sideloadHeader",
        ["<>"]
      );
      let key = !result.msgs.length
        ? "webextPerms.sideloadTextNoPerms"
        : "webextPerms.sideloadText2";
      result.text = bundle.GetStringFromName(key);
      result.acceptText = bundle.GetStringFromName(
        "webextPerms.sideloadEnable.label"
      );
      result.cancelText = bundle.GetStringFromName(
        "webextPerms.sideloadCancel.label"
      );
      if (haveAccessKeys) {
        result.acceptKey = bundle.GetStringFromName(
          "webextPerms.sideloadEnable.accessKey"
        );
        result.cancelKey = bundle.GetStringFromName(
          "webextPerms.sideloadCancel.accessKey"
        );
      }
    } else if (info.type == "update") {
      result.header = bundle.formatStringFromName("webextPerms.updateText", [
        "<>",
      ]);
      result.text = "";
      result.acceptText = bundle.GetStringFromName(
        "webextPerms.updateAccept.label"
      );
      if (haveAccessKeys) {
        result.acceptKey = bundle.GetStringFromName(
          "webextPerms.updateAccept.accessKey"
        );
      }
    } else if (info.type == "optional") {
      result.header = bundle.formatStringFromName(
        "webextPerms.optionalPermsHeader",
        ["<>"]
      );
      result.text = "";
      result.listIntro = bundle.GetStringFromName(
        "webextPerms.optionalPermsListIntro"
      );
      result.acceptText = bundle.GetStringFromName(
        "webextPerms.optionalPermsAllow.label"
      );
      result.cancelText = bundle.GetStringFromName(
        "webextPerms.optionalPermsDeny.label"
      );
      if (haveAccessKeys) {
        result.acceptKey = bundle.GetStringFromName(
          "webextPerms.optionalPermsAllow.accessKey"
        );
        result.cancelKey = bundle.GetStringFromName(
          "webextPerms.optionalPermsDeny.accessKey"
        );
      }
    }

    return result;
  }
}

const PROXIED_EVENTS = new Set([
  "test-harness-message",
  "add-permissions",
  "remove-permissions",
]);

class BootstrapScope {
  install(data, reason) {}
  uninstall(data, reason) {
    AsyncShutdown.profileChangeTeardown.addBlocker(
      `Uninstalling add-on: ${data.id}`,
      Management.emit("uninstall", { id: data.id }).then(() => {
        Management.emit("uninstall-complete", { id: data.id });
      })
    );
  }

  fetchState() {
    if (this.extension) {
      return { state: this.extension.state };
    }
    return null;
  }

  async update(data, reason) {
    // Retain any previously granted permissions that may have migrated into the optional list.
    await ExtensionData.migratePermissions(
      data.id,
      data.oldPermissions,
      data.oldOptionalPermissions,
      data.userPermissions,
      data.optionalPermissions
    );

    return Management.emit("update", {
      id: data.id,
      resourceURI: data.resourceURI,
    });
  }

  startup(data, reason) {
    // eslint-disable-next-line no-use-before-define
    this.extension = new Extension(
      data,
      this.BOOTSTRAP_REASON_TO_STRING_MAP[reason]
    );
    return this.extension.startup();
  }

  async shutdown(data, reason) {
    let result = await this.extension.shutdown(
      this.BOOTSTRAP_REASON_TO_STRING_MAP[reason]
    );
    this.extension = null;
    return result;
  }
}

XPCOMUtils.defineLazyGetter(
  BootstrapScope.prototype,
  "BOOTSTRAP_REASON_TO_STRING_MAP",
  () => {
    const { BOOTSTRAP_REASONS } = AddonManagerPrivate;

    return Object.freeze({
      [BOOTSTRAP_REASONS.APP_STARTUP]: "APP_STARTUP",
      [BOOTSTRAP_REASONS.APP_SHUTDOWN]: "APP_SHUTDOWN",
      [BOOTSTRAP_REASONS.ADDON_ENABLE]: "ADDON_ENABLE",
      [BOOTSTRAP_REASONS.ADDON_DISABLE]: "ADDON_DISABLE",
      [BOOTSTRAP_REASONS.ADDON_INSTALL]: "ADDON_INSTALL",
      [BOOTSTRAP_REASONS.ADDON_UNINSTALL]: "ADDON_UNINSTALL",
      [BOOTSTRAP_REASONS.ADDON_UPGRADE]: "ADDON_UPGRADE",
      [BOOTSTRAP_REASONS.ADDON_DOWNGRADE]: "ADDON_DOWNGRADE",
    });
  }
);

class DictionaryBootstrapScope extends BootstrapScope {
  install(data, reason) {}
  uninstall(data, reason) {}

  startup(data, reason) {
    // eslint-disable-next-line no-use-before-define
    this.dictionary = new Dictionary(data);
    return this.dictionary.startup(this.BOOTSTRAP_REASON_TO_STRING_MAP[reason]);
  }

  shutdown(data, reason) {
    this.dictionary.shutdown(this.BOOTSTRAP_REASON_TO_STRING_MAP[reason]);
    this.dictionary = null;
  }
}

class LangpackBootstrapScope {
  install(data, reason) {}
  uninstall(data, reason) {}

  startup(data, reason) {
    // eslint-disable-next-line no-use-before-define
    this.langpack = new Langpack(data);
    return this.langpack.startup();
  }

  shutdown(data, reason) {
    this.langpack.shutdown();
    this.langpack = null;
  }
}

let activeExtensionIDs = new Set();

let pendingExtensions = new Map();

/**
 * This class is the main representation of an active WebExtension
 * in the main process.
 * @extends ExtensionData
 */
class Extension extends ExtensionData {
  constructor(addonData, startupReason) {
    super(addonData.resourceURI);

    this.startupStates = new Set();
    this.state = "Not started";

    this.sharedDataKeys = new Set();

    this.uuid = UUIDMap.get(addonData.id);
    this.instanceId = getUniqueId();

    this.MESSAGE_EMIT_EVENT = `Extension:EmitEvent:${this.instanceId}`;
    Services.ppmm.addMessageListener(this.MESSAGE_EMIT_EVENT, this);

    if (addonData.cleanupFile) {
      Services.obs.addObserver(this, "xpcom-shutdown");
      this.cleanupFile = addonData.cleanupFile || null;
      delete addonData.cleanupFile;
    }

    if (addonData.TEST_NO_ADDON_MANAGER) {
      this.dontSaveStartupData = true;
    }

    this.addonData = addonData;
    this.startupData = addonData.startupData || {};
    this.startupReason = startupReason;

    if (["ADDON_UPGRADE", "ADDON_DOWNGRADE"].includes(startupReason)) {
      StartupCache.clearAddonData(addonData.id);
    }

    this.remote = !WebExtensionPolicy.isExtensionProcess;
    this.remoteType = this.remote ? E10SUtils.EXTENSION_REMOTE_TYPE : null;

    if (this.remote && processCount !== 1) {
      throw new Error(
        "Out-of-process WebExtensions are not supported with multiple child processes"
      );
    }

    // This is filled in the first time an extension child is created.
    this.parentMessageManager = null;

    this.id = addonData.id;
    this.version = addonData.version;
    this.baseURL = this.getURL("");
    this.baseURI = Services.io.newURI(this.baseURL).QueryInterface(Ci.nsIURL);
    this.principal = this.createPrincipal();

    this.views = new Set();
    this._backgroundPageFrameLoader = null;

    this.onStartup = null;

    this.hasShutdown = false;
    this.onShutdown = new Set();

    this.uninstallURL = null;

    this.whiteListedHosts = null;
    this._optionalOrigins = null;
    this.webAccessibleResources = null;

    this.registeredContentScripts = new Map();

    this.emitter = new EventEmitter();

    if (this.startupData.lwtData && this.startupReason == "APP_STARTUP") {
      LightweightThemeManager.fallbackThemeData = this.startupData.lwtData;
    }

    /* eslint-disable mozilla/balanced-listeners */
    this.on("add-permissions", (ignoreEvent, permissions) => {
      for (let perm of permissions.permissions) {
        this.permissions.add(perm);
      }

      if (permissions.origins.length) {
        let patterns = this.whiteListedHosts.patterns.map(host => host.pattern);

        this.whiteListedHosts = new MatchPatternSet(
          new Set([...patterns, ...permissions.origins]),
          {
            restrictSchemes: this.restrictSchemes,
            ignorePath: true,
          }
        );
      }

      this.policy.permissions = Array.from(this.permissions);
      this.policy.allowedOrigins = this.whiteListedHosts;

      this.cachePermissions();
      this.updatePermissions();
    });

    this.on("remove-permissions", (ignoreEvent, permissions) => {
      for (let perm of permissions.permissions) {
        this.permissions.delete(perm);
      }

      let origins = permissions.origins.map(
        origin => new MatchPattern(origin, { ignorePath: true }).pattern
      );

      this.whiteListedHosts = new MatchPatternSet(
        this.whiteListedHosts.patterns.filter(
          host => !origins.includes(host.pattern)
        )
      );

      this.policy.permissions = Array.from(this.permissions);
      this.policy.allowedOrigins = this.whiteListedHosts;

      this.cachePermissions();
      this.updatePermissions();
    });
    /* eslint-enable mozilla/balanced-listeners */
  }

  set state(startupState) {
    this.startupStates.clear();
    this.startupStates.add(startupState);
  }

  get state() {
    return `${Array.from(this.startupStates).join(", ")}`;
  }

  async addStartupStatePromise(name, fn) {
    this.startupStates.add(name);
    try {
      await fn();
    } finally {
      this.startupStates.delete(name);
    }
  }

  get restrictSchemes() {
    return !(this.isPrivileged && this.hasPermission("mozillaAddons"));
  }

  // Some helpful properties added elsewhere:
  /**
   * An object used to map between extension-visible tab ids and
   * native Tab object
   * @property {TabManager} tabManager
   */

  static getBootstrapScope() {
    return new BootstrapScope();
  }

  get groupFrameLoader() {
    let frameLoader = this._backgroundPageFrameLoader;
    for (let view of this.views) {
      if (view.viewType === "background" && view.xulBrowser) {
        return view.xulBrowser.frameLoader;
      }
      if (!frameLoader && view.xulBrowser) {
        frameLoader = view.xulBrowser.frameLoader;
      }
    }
    return frameLoader || ExtensionParent.DebugUtils.getFrameLoader(this.id);
  }

  on(hook, f) {
    return this.emitter.on(hook, f);
  }

  off(hook, f) {
    return this.emitter.off(hook, f);
  }

  once(hook, f) {
    return this.emitter.once(hook, f);
  }

  emit(event, ...args) {
    if (PROXIED_EVENTS.has(event)) {
      Services.ppmm.broadcastAsyncMessage(this.MESSAGE_EMIT_EVENT, {
        event,
        args,
      });
    }

    return this.emitter.emit(event, ...args);
  }

  receiveMessage({ name, data }) {
    if (name === this.MESSAGE_EMIT_EVENT) {
      this.emitter.emit(data.event, ...data.args);
    }
  }

  testMessage(...args) {
    this.emit("test-harness-message", ...args);
  }

  createPrincipal(uri = this.baseURI, originAttributes = {}) {
    return Services.scriptSecurityManager.createContentPrincipal(
      uri,
      originAttributes
    );
  }

  // Checks that the given URL is a child of our baseURI.
  isExtensionURL(url) {
    let uri = Services.io.newURI(url);

    let common = this.baseURI.getCommonBaseSpec(uri);
    return common == this.baseURL;
  }

  checkLoadURL(url, options = {}) {
    // As an optimization, f the URL starts with the extension's base URL,
    // don't do any further checks. It's always allowed to load it.
    if (url.startsWith(this.baseURL)) {
      return true;
    }

    return ExtensionCommon.checkLoadURL(url, this.principal, options);
  }

  async promiseLocales(locale) {
    let locales = await StartupCache.locales.get(
      [this.id, "@@all_locales"],
      () => this._promiseLocaleMap()
    );

    return this._setupLocaleData(locales);
  }

  readLocaleFile(locale) {
    return StartupCache.locales
      .get([this.id, this.version, locale], () => super.readLocaleFile(locale))
      .then(result => {
        this.localeData.messages.set(locale, result);
      });
  }

  get manifestCacheKey() {
    return [this.id, this.version, Services.locale.appLocaleAsBCP47];
  }

  get isPrivileged() {
    return (
      this.addonData.signedState === AddonManager.SIGNEDSTATE_PRIVILEGED ||
      this.addonData.signedState === AddonManager.SIGNEDSTATE_SYSTEM ||
      this.addonData.builtIn ||
      (AddonSettings.EXPERIMENTS_ENABLED && this.addonData.temporarilyInstalled)
    );
  }

  get experimentsAllowed() {
    return AddonSettings.EXPERIMENTS_ENABLED || this.isPrivileged;
  }

  saveStartupData() {
    if (this.dontSaveStartupData) {
      return;
    }
    XPIProvider.setStartupData(this.id, this.startupData);
  }

  parseManifest() {
    return StartupCache.manifests.get(this.manifestCacheKey, () =>
      super.parseManifest()
    );
  }

  async cachePermissions() {
    let manifestData = await this.parseManifest();

    manifestData.originPermissions = this.whiteListedHosts.patterns.map(
      pat => pat.pattern
    );
    manifestData.permissions = this.permissions;
    return StartupCache.manifests.set(this.manifestCacheKey, manifestData);
  }

  async loadManifest() {
    let manifest = await super.loadManifest();

    if (this.errors.length) {
      return Promise.reject({ errors: this.errors });
    }

    return manifest;
  }

  get extensionPageCSP() {
    const { content_security_policy } = this.manifest;
    if (
      content_security_policy &&
      typeof content_security_policy === "object"
    ) {
      return content_security_policy.extension_pages;
    }
    return content_security_policy;
  }

  get contentScriptCSP() {
    let { content_security_policy } = this.manifest;
    if (
      content_security_policy &&
      typeof content_security_policy === "object"
    ) {
      return (
        content_security_policy.content_scripts ||
        content_security_policy.isolated_world
      );
    }
  }

  get backgroundScripts() {
    return this.manifest.background && this.manifest.background.scripts;
  }

  get optionalPermissions() {
    return this.manifest.optional_permissions;
  }

  get privateBrowsingAllowed() {
    return this.policy.privateBrowsingAllowed;
  }

  canAccessWindow(window) {
    return this.policy.canAccessWindow(window);
  }

  // Representation of the extension to send to content
  // processes. This should include anything the content process might
  // need.
  serialize() {
    return {
      id: this.id,
      uuid: this.uuid,
      name: this.name,
      extensionPageCSP: this.extensionPageCSP,
      contentScriptCSP: this.contentScriptCSP,
      instanceId: this.instanceId,
      resourceURL: this.resourceURL,
      contentScripts: this.contentScripts,
      webAccessibleResources: this.webAccessibleResources.map(res => res.glob),
      whiteListedHosts: this.whiteListedHosts.patterns.map(pat => pat.pattern),
      permissions: this.permissions,
      optionalPermissions: this.optionalPermissions,
      isPrivileged: this.isPrivileged,
    };
  }

  // Extended serialized data which is only needed in the extensions process,
  // and is never deserialized in web content processes.
  serializeExtended() {
    return {
      backgroundScripts: this.backgroundScripts,
      childModules: this.modules && this.modules.child,
      dependencies: this.dependencies,
      schemaURLs: this.schemaURLs,
    };
  }

  broadcast(msg, data) {
    return new Promise(resolve => {
      let { ppmm } = Services;
      let children = new Set();
      for (let i = 0; i < ppmm.childCount; i++) {
        children.add(ppmm.getChildAt(i));
      }

      let maybeResolve;
      function listener(data) {
        children.delete(data.target);
        maybeResolve();
      }
      function observer(subject, topic, data) {
        children.delete(subject);
        maybeResolve();
      }

      maybeResolve = () => {
        if (children.size === 0) {
          ppmm.removeMessageListener(msg + "Complete", listener);
          Services.obs.removeObserver(observer, "message-manager-close");
          Services.obs.removeObserver(observer, "message-manager-disconnect");
          resolve();
        }
      };
      ppmm.addMessageListener(msg + "Complete", listener, true);
      Services.obs.addObserver(observer, "message-manager-close");
      Services.obs.addObserver(observer, "message-manager-disconnect");

      ppmm.broadcastAsyncMessage(msg, data);
    });
  }

  setSharedData(key, value) {
    key = `extension/${this.id}/${key}`;
    this.sharedDataKeys.add(key);

    sharedData.set(key, value);
  }

  getSharedData(key, value) {
    key = `extension/${this.id}/${key}`;
    return sharedData.get(key);
  }

  initSharedData() {
    this.setSharedData("", this.serialize());
    this.setSharedData("extendedData", this.serializeExtended());
    this.setSharedData("locales", this.localeData.serialize());
    this.setSharedData("manifest", this.manifest);
    this.updateContentScripts();
  }

  updateContentScripts() {
    this.setSharedData("contentScripts", this.registeredContentScripts);
  }

  runManifest(manifest) {
    let promises = [];
    let addPromise = (name, fn) => {
      promises.push(this.addStartupStatePromise(name, fn));
    };

    for (let directive in manifest) {
      if (manifest[directive] !== null) {
        addPromise(`asyncEmitManifestEntry("${directive}")`, () =>
          Management.asyncEmitManifestEntry(this, directive)
        );
      }
    }

    activeExtensionIDs.add(this.id);
    sharedData.set("extensions/activeIDs", activeExtensionIDs);

    pendingExtensions.delete(this.id);
    sharedData.set("extensions/pending", pendingExtensions);

    Services.ppmm.sharedData.flush();
    this.broadcast("Extension:Startup", this.id);

    return Promise.all(promises);
  }

  /**
   * Call the close() method on the given object when this extension
   * is shut down.  This can happen during browser shutdown, or when
   * an extension is manually disabled or uninstalled.
   *
   * @param {object} obj
   *        An object on which to call the close() method when this
   *        extension is shut down.
   */
  callOnClose(obj) {
    this.onShutdown.add(obj);
  }

  forgetOnClose(obj) {
    this.onShutdown.delete(obj);
  }

  get builtinMessages() {
    return new Map([["@@extension_id", this.uuid]]);
  }

  // Reads the locale file for the given Gecko-compatible locale code, or if
  // no locale is given, the available locale closest to the UI locale.
  // Sets the currently selected locale on success.
  async initLocale(locale = undefined) {
    if (locale === undefined) {
      let locales = await this.promiseLocales();

      let matches = Services.locale.negotiateLanguages(
        Services.locale.appLocalesAsBCP47,
        Array.from(locales.keys()),
        this.defaultLocale
      );

      locale = matches[0];
    }

    return super.initLocale(locale);
  }

  /**
   * Update site permissions as necessary.
   *
   * @param {string|undefined} reason
   *        If provided, this is a BOOTSTRAP_REASON string.  If reason is undefined,
   *        addon permissions are being added or removed that may effect the site permissions.
   */
  updatePermissions(reason) {
    const { principal } = this;

    const testPermission = perm =>
      Services.perms.testPermissionFromPrincipal(principal, perm);

    const addUnlimitedStoragePermissions = () => {
      // Set the indexedDB permission and a custom "WebExtensions-unlimitedStorage" to
      // remember that the permission hasn't been selected manually by the user.
      Services.perms.addFromPrincipal(
        principal,
        "WebExtensions-unlimitedStorage",
        Services.perms.ALLOW_ACTION
      );
      Services.perms.addFromPrincipal(
        principal,
        "indexedDB",
        Services.perms.ALLOW_ACTION
      );
      Services.perms.addFromPrincipal(
        principal,
        "persistent-storage",
        Services.perms.ALLOW_ACTION
      );
    };

    // Only update storage permissions when the extension changes in
    // some way.
    if (reason !== "APP_STARTUP" && reason !== "APP_SHUTDOWN") {
      if (this.hasPermission("unlimitedStorage")) {
        addUnlimitedStoragePermissions();
      } else {
        // Remove the indexedDB permission if it has been enabled using the
        // unlimitedStorage WebExtensions permissions.
        Services.perms.removeFromPrincipal(
          principal,
          "WebExtensions-unlimitedStorage"
        );
        Services.perms.removeFromPrincipal(principal, "indexedDB");
        Services.perms.removeFromPrincipal(principal, "persistent-storage");
      }
    } else if (
      reason === "APP_STARTUP" &&
      this.hasPermission("unlimitedStorage") &&
      (testPermission("indexedDB") !== Services.perms.ALLOW_ACTION ||
        testPermission("persistent-storage") !== Services.perms.ALLOW_ACTION)
    ) {
      // If the extension does have the unlimitedStorage permission, but the
      // expected site permissions are missing during the app startup, then
      // add them back (See Bug 1454192).
      addUnlimitedStoragePermissions();
    }

    // Never change geolocation permissions at shutdown, since it uses a
    // session-only permission.
    if (reason !== "APP_SHUTDOWN") {
      if (this.hasPermission("geolocation")) {
        if (testPermission("geo") === Services.perms.UNKNOWN_ACTION) {
          Services.perms.addFromPrincipal(
            principal,
            "geo",
            Services.perms.ALLOW_ACTION,
            Services.perms.EXPIRE_SESSION
          );
        }
      } else if (
        reason !== "APP_STARTUP" &&
        testPermission("geo") === Services.perms.ALLOW_ACTION
      ) {
        Services.perms.removeFromPrincipal(principal, "geo");
      }
    }
  }

  static async migratePrivateBrowsing(addonData) {
    if (addonData.incognito !== "not_allowed") {
      ExtensionPermissions.add(addonData.id, {
        permissions: [PRIVATE_ALLOWED_PERMISSION],
        origins: [],
      });
      await StartupCache.clearAddonData(addonData.id);

      // Record a telemetry event for the extension automatically allowed on private browsing as
      // part of the Firefox upgrade.
      AMTelemetry.recordActionEvent({
        extra: { addonId: addonData.id },
        object: "appUpgrade",
        action: "privateBrowsingAllowed",
        value: "on",
      });
    }
  }

  async startup() {
    this.state = "Startup";

    let resolveReadyPromise;
    let readyPromise = new Promise(resolve => {
      resolveReadyPromise = resolve;
    });

    // Create a temporary policy object for the devtools and add-on
    // manager callers that depend on it being available early.
    this.policy = new WebExtensionPolicy({
      id: this.id,
      mozExtensionHostname: this.uuid,
      baseURL: this.resourceURL,
      isPrivileged: this.isPrivileged,
      allowedOrigins: new MatchPatternSet([]),
      localizeCallback() {},
      readyPromise,
    });

    this.policy.extension = this;
    if (!WebExtensionPolicy.getByID(this.id)) {
      this.policy.active = true;
    }

    pendingExtensions.set(this.id, {
      mozExtensionHostname: this.uuid,
      baseURL: this.resourceURL,
      isPrivileged: this.isPrivileged,
    });
    sharedData.set("extensions/pending", pendingExtensions);

    ExtensionTelemetry.extensionStartup.stopwatchStart(this);
    try {
      this.state = "Startup: Loading manifest";
      await this.loadManifest();
      this.state = "Startup: Loaded manifest";

      if (!this.hasShutdown) {
        this.state = "Startup: Init locale";
        await this.initLocale();
        this.state = "Startup: Initted locale";
      }

      if (this.errors.length) {
        return Promise.reject({ errors: this.errors });
      }

      if (this.hasShutdown) {
        return;
      }

      // We automatically add permissions to system/built-in extensions.
      // Extensions expliticy stating not_allowed will never get permission.
      if (!allowPrivateBrowsingByDefault) {
        let isAllowed = this.permissions.has(PRIVATE_ALLOWED_PERMISSION);
        if (this.manifest.incognito === "not_allowed") {
          // If an extension previously had permission, but upgrades/downgrades to
          // a version that specifies "not_allowed" in manifest, remove the
          // permission.
          if (isAllowed) {
            ExtensionPermissions.remove(this.id, {
              permissions: [PRIVATE_ALLOWED_PERMISSION],
              origins: [],
            });
            this.permissions.delete(PRIVATE_ALLOWED_PERMISSION);
          }
        } else if (
          !isAllowed &&
          this.isPrivileged &&
          !this.addonData.temporarilyInstalled
        ) {
          // Add to EP so it is preserved after ADDON_INSTALL.  We don't wait on the add here
          // since we are pushing the value into this.permissions.  EP will eventually save.
          ExtensionPermissions.add(this.id, {
            permissions: [PRIVATE_ALLOWED_PERMISSION],
            origins: [],
          });
          this.permissions.add(PRIVATE_ALLOWED_PERMISSION);
        }
      }

      GlobalManager.init(this);

      this.initSharedData();

      this.policy.active = false;
      this.policy = ExtensionProcessScript.initExtension(this);
      this.policy.extension = this;

      this.updatePermissions(this.startupReason);

      // Select the storage.local backend if it is already known,
      // and start the data migration if needed.
      if (this.hasPermission("storage")) {
        if (!ExtensionStorageIDB.isBackendEnabled) {
          this.setSharedData("storageIDBBackend", false);
        } else if (ExtensionStorageIDB.isMigratedExtension(this)) {
          this.setSharedData("storageIDBBackend", true);
          this.setSharedData(
            "storageIDBPrincipal",
            ExtensionStorageIDB.getStoragePrincipal(this)
          );
        } else if (
          this.startupReason === "ADDON_INSTALL" &&
          !Services.prefs.getBoolPref(LEAVE_STORAGE_PREF, false)
        ) {
          // If the extension has been just installed, set it as migrated,
          // because there will not be any data to migrate.
          ExtensionStorageIDB.setMigratedExtensionPref(this, true);
          this.setSharedData("storageIDBBackend", true);
          this.setSharedData(
            "storageIDBPrincipal",
            ExtensionStorageIDB.getStoragePrincipal(this)
          );
        }
      }

      resolveReadyPromise(this.policy);

      // The "startup" Management event sent on the extension instance itself
      // is emitted just before the Management "startup" event,
      // and it is used to run code that needs to be executed before
      // any of the "startup" listeners.
      this.emit("startup", this);

      this.startupStates.clear();
      await Promise.all([
        this.addStartupStatePromise("Startup: Emit startup", () =>
          Management.emit("startup", this)
        ),
        this.addStartupStatePromise("Startup: Run manifest", () =>
          this.runManifest(this.manifest)
        ),
      ]);
      this.state = "Startup: Ran manifest";

      Management.emit("ready", this);
      this.emit("ready");

      this.state = "Startup: Complete";
    } catch (errors) {
      this.state = `Startup: Error: ${errors}`;

      for (let e of [].concat(errors)) {
        dump(
          `Extension error: ${e.message || e} ${e.filename || e.fileName}:${
            e.lineNumber
          } :: ${e.stack || new Error().stack}\n`
        );
        Cu.reportError(e);
      }

      if (this.policy) {
        this.policy.active = false;
      }

      this.cleanupGeneratedFile();

      throw errors;
    } finally {
      ExtensionTelemetry.extensionStartup.stopwatchFinish(this);
    }
  }

  cleanupGeneratedFile() {
    if (!this.cleanupFile) {
      return;
    }

    let file = this.cleanupFile;
    this.cleanupFile = null;

    Services.obs.removeObserver(this, "xpcom-shutdown");

    return this.broadcast("Extension:FlushJarCache", { path: file.path })
      .then(() => {
        // We can't delete this file until everyone using it has
        // closed it (because Windows is dumb). So we wait for all the
        // child processes (including the parent) to flush their JAR
        // caches. These caches may keep the file open.
        file.remove(false);
      })
      .catch(Cu.reportError);
  }

  async shutdown(reason) {
    this.state = "Shutdown";

    this.hasShutdown = true;

    if (!this.policy) {
      return;
    }

    if (
      this.hasPermission("storage") &&
      ExtensionStorageIDB.selectedBackendPromises.has(this)
    ) {
      this.state = "Shutdown: Storage";

      // Wait the data migration to complete.
      try {
        await ExtensionStorageIDB.selectedBackendPromises.get(this);
      } catch (err) {
        Cu.reportError(
          `Error while waiting for extension data migration on shutdown: ${this.policy.debugName} - ${err.message}::${err.stack}`
        );
      }
      this.state = "Shutdown: Storage complete";
    }

    if (this.rootURI instanceof Ci.nsIJARURI) {
      this.state = "Shutdown: Flush jar cache";
      let file = this.rootURI.JARFile.QueryInterface(Ci.nsIFileURL).file;
      Services.ppmm.broadcastAsyncMessage("Extension:FlushJarCache", {
        path: file.path,
      });
      this.state = "Shutdown: Flushed jar cache";
    }

    const isAppShutdown = reason === "APP_SHUTDOWN";
    if (this.cleanupFile || !isAppShutdown) {
      StartupCache.clearAddonData(this.id);
    }

    activeExtensionIDs.delete(this.id);
    sharedData.set("extensions/activeIDs", activeExtensionIDs);

    for (let key of this.sharedDataKeys) {
      sharedData.delete(key);
    }

    Services.ppmm.removeMessageListener(this.MESSAGE_EMIT_EVENT, this);

    this.updatePermissions(reason);

    if (!this.manifest) {
      this.state = "Shutdown: Complete: No manifest";
      this.policy.active = false;

      return this.cleanupGeneratedFile();
    }

    GlobalManager.uninit(this);

    for (let obj of this.onShutdown) {
      obj.close();
    }

    ParentAPIManager.shutdownExtension(this.id, reason);

    Management.emit("shutdown", this);
    this.emit("shutdown", isAppShutdown);

    const TIMED_OUT = Symbol();

    this.state = "Shutdown: Emit shutdown";
    let result = await Promise.race([
      this.broadcast("Extension:Shutdown", { id: this.id }),
      promiseTimeout(CHILD_SHUTDOWN_TIMEOUT_MS).then(() => TIMED_OUT),
    ]);
    this.state = `Shutdown: Emitted shutdown: ${result === TIMED_OUT}`;
    if (result === TIMED_OUT) {
      Cu.reportError(
        `Timeout while waiting for extension child to shutdown: ${this.policy.debugName}`
      );
    }

    MessageChannel.abortResponses({ extensionId: this.id });

    this.policy.active = false;

    this.state = `Shutdown: Complete (${this.cleanupFile})`;
    return this.cleanupGeneratedFile();
  }

  observe(subject, topic, data) {
    if (topic === "xpcom-shutdown") {
      this.cleanupGeneratedFile();
    }
  }

  get name() {
    return this.manifest.name;
  }

  get optionalOrigins() {
    if (this._optionalOrigins == null) {
      let { restrictSchemes, isPrivileged } = this;
      let origins = this.manifest.optional_permissions.filter(
        perm => classifyPermission(perm, restrictSchemes, isPrivileged).origin
      );
      this._optionalOrigins = new MatchPatternSet(origins, {
        restrictSchemes,
        ignorePath: true,
      });
    }
    return this._optionalOrigins;
  }
}

class Dictionary extends ExtensionData {
  constructor(addonData, startupReason) {
    super(addonData.resourceURI);
    this.id = addonData.id;
    this.startupData = addonData.startupData;
  }

  static getBootstrapScope() {
    return new DictionaryBootstrapScope();
  }

  async startup(reason) {
    this.dictionaries = {};
    for (let [lang, path] of Object.entries(this.startupData.dictionaries)) {
      let uri = Services.io.newURI(
        path.slice(0, -4) + ".aff",
        null,
        this.rootURI
      );
      this.dictionaries[lang] = uri;

      spellCheck.addDictionary(lang, uri);
    }

    Management.emit("ready", this);
  }

  async shutdown(reason) {
    if (reason !== "APP_SHUTDOWN") {
      XPIProvider.unregisterDictionaries(this.dictionaries);
    }
  }
}

class Langpack extends ExtensionData {
  constructor(addonData, startupReason) {
    super(addonData.resourceURI);
    this.startupData = addonData.startupData;
    this.manifestCacheKey = [addonData.id, addonData.version];
  }

  static getBootstrapScope() {
    return new LangpackBootstrapScope();
  }

  async promiseLocales(locale) {
    let locales = await StartupCache.locales.get(
      [this.id, "@@all_locales"],
      () => this._promiseLocaleMap()
    );

    return this._setupLocaleData(locales);
  }

  parseManifest() {
    return StartupCache.manifests.get(this.manifestCacheKey, () =>
      super.parseManifest()
    );
  }

  async startup(reason) {
    this.chromeRegistryHandle = null;
    if (this.startupData.chromeEntries.length) {
      const manifestURI = Services.io.newURI(
        "manifest.json",
        null,
        this.rootURI
      );
      this.chromeRegistryHandle = aomStartup.registerChrome(
        manifestURI,
        this.startupData.chromeEntries
      );
    }

    const langpackId = this.startupData.langpackId;
    const l10nRegistrySources = this.startupData.l10nRegistrySources;

    resourceProtocol.setSubstitution(langpackId, this.rootURI);

    for (const [sourceName, basePath] of Object.entries(l10nRegistrySources)) {
      L10nRegistry.registerSource(
        new FileSource(
          `${sourceName}-${langpackId}`,
          this.startupData.languages,
          `resource://${langpackId}/${basePath}localization/{locale}/`
        )
      );
    }

    Services.obs.notifyObservers(
      { wrappedJSObject: { langpack: this } },
      "webextension-langpack-startup"
    );
  }

  async shutdown(reason) {
    if (reason === "APP_SHUTDOWN") {
      // If we're shutting down, let's not bother updating the state of each
      // system.
      return;
    }
    for (const sourceName of Object.keys(
      this.startupData.l10nRegistrySources
    )) {
      L10nRegistry.removeSource(`${sourceName}-${this.startupData.langpackId}`);
    }
    if (this.chromeRegistryHandle) {
      this.chromeRegistryHandle.destruct();
      this.chromeRegistryHandle = null;
    }

    resourceProtocol.setSubstitution(this.startupData.langpackId, null);
  }
}
