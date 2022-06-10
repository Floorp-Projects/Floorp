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
  "SitePermission",
  "ExtensionAddonObserver",
  "PRIVILEGED_PERMS",
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
 * e.g. by enabling the `maychangeremoteness` attribute, and/or setting
 * `initialBrowsingContextGroupId` attribute to the correct value.
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
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.jsm",
  AddonSettings: "resource://gre/modules/addons/AddonSettings.jsm",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  ExtensionPermissions: "resource://gre/modules/ExtensionPermissions.jsm",
  ExtensionPreferencesManager:
    "resource://gre/modules/ExtensionPreferencesManager.jsm",
  ExtensionProcessScript: "resource://gre/modules/ExtensionProcessScript.jsm",
  ExtensionStorage: "resource://gre/modules/ExtensionStorage.jsm",
  ExtensionStorageIDB: "resource://gre/modules/ExtensionStorageIDB.jsm",
  ExtensionTelemetry: "resource://gre/modules/ExtensionTelemetry.jsm",
  LightweightThemeManager: "resource://gre/modules/LightweightThemeManager.jsm",
  Log: "resource://gre/modules/Log.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  PluralForm: "resource://gre/modules/PluralForm.jsm",
  Schemas: "resource://gre/modules/Schemas.jsm",
  ServiceWorkerCleanUp: "resource://gre/modules/ServiceWorkerCleanUp.jsm",

  // These are used for manipulating jar entry paths, which always use Unix
  // separators.
  basename: "resource://gre/modules/osfile/ospath_unix.jsm",
  dirname: "resource://gre/modules/osfile/ospath_unix.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "resourceProtocol", () =>
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

XPCOMUtils.defineLazyServiceGetters(lazy, {
  aomStartup: [
    "@mozilla.org/addons/addon-manager-startup;1",
    "amIAddonManagerStartup",
  ],
  spellCheck: ["@mozilla.org/spellchecker/engine;1", "mozISpellCheckingEngine"],
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "processCount",
  "dom.ipc.processCount.extension"
);

// Temporary pref to be turned on when ready.
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "userContextIsolation",
  "extensions.userContextIsolation.enabled",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "userContextIsolationDefaultRestricted",
  "extensions.userContextIsolation.defaults.restricted",
  "[]"
);

// This pref modifies behavior for MV2.  MV3 is enabled regardless.
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "eventPagesEnabled",
  "extensions.eventPages.enabled"
);

var {
  GlobalManager,
  ParentAPIManager,
  StartupCache,
  apiManager: Management,
} = ExtensionParent;

const { getUniqueId, promiseTimeout } = ExtensionUtils;

const { EventEmitter, updateAllowedOrigins } = ExtensionCommon;

XPCOMUtils.defineLazyGetter(lazy, "console", ExtensionCommon.getConsole);

XPCOMUtils.defineLazyGetter(
  lazy,
  "LocaleData",
  () => ExtensionCommon.LocaleData
);

XPCOMUtils.defineLazyGetter(lazy, "NO_PROMPT_PERMISSIONS", async () => {
  // Wait until all extension API schemas have been loaded and parsed.
  await Management.lazyInit();
  return new Set(
    lazy.Schemas.getPermissionNames([
      "PermissionNoPrompt",
      "OptionalPermissionNoPrompt",
      "PermissionPrivileged",
    ])
  );
});

XPCOMUtils.defineLazyGetter(lazy, "SCHEMA_SITE_PERMISSIONS", async () => {
  // Wait until all extension API schemas have been loaded and parsed.
  await Management.lazyInit();
  return lazy.Schemas.getPermissionNames(["SitePermission"]);
});

const { sharedData } = Services.ppmm;

const PRIVATE_ALLOWED_PERMISSION = "internal:privateBrowsingAllowed";
const SVG_CONTEXT_PROPERTIES_PERMISSION =
  "internal:svgContextPropertiesAllowed";

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
  "networkStatus",
  "normandyAddonStudy",
  "telemetry",
  "urlbar",
]);

const PRIVILEGED_PERMS_ANDROID_ONLY = new Set([
  "geckoViewAddons",
  "nativeMessagingFromContent",
  "nativeMessaging",
]);

const PRIVILEGED_PERMS_DESKTOP_ONLY = new Set(["normandyAddonStudy", "urlbar"]);

if (AppConstants.platform == "android") {
  for (const perm of PRIVILEGED_PERMS_ANDROID_ONLY) {
    PRIVILEGED_PERMS.add(perm);
  }
}

if (
  AppConstants.MOZ_APP_NAME != "firefox" ||
  AppConstants.platform == "android"
) {
  for (const perm of PRIVILEGED_PERMS_DESKTOP_ONLY) {
    PRIVILEGED_PERMS.delete(perm);
  }
}

// Message included in warnings and errors related to privileged permissions and
// privileged manifest properties. Provides a link to the firefox-source-docs.mozilla.org
// section related to developing and sign Privileged Add-ons.
const PRIVILEGED_ADDONS_DEVDOCS_MESSAGE =
  "See https://mzl.la/3NS9KJd for more details about how to develop a privileged add-on.";

const INSTALL_AND_UPDATE_STARTUP_REASONS = new Set([
  "ADDON_INSTALL",
  "ADDON_UPGRADE",
  "ADDON_DOWNGRADE",
]);

// Returns true if the extension is owned by Mozilla (is either privileged,
// using one of the @mozilla.com/@mozilla.org protected addon id suffixes).
//
// This method throws if the extension's startupReason is not one of the expected
// ones (either ADDON_INSTALL, ADDON_UPGRADE or ADDON_DOWNGRADE).
//
// NOTE: This methos is internally referring to "addonData.recommendationState" to
// identify a Mozilla line extension. That property is part of the addonData only when
// the extension is installed or updated, and so we enforce the expected
// startup reason values to prevent it from silently returning different results
// if called with an unexpected startupReason.
function isMozillaExtension(extension) {
  const { addonData, id, isPrivileged, startupReason } = extension;

  if (!INSTALL_AND_UPDATE_STARTUP_REASONS.has(startupReason)) {
    throw new Error(
      `isMozillaExtension called with unexpected startupReason: ${startupReason}`
    );
  }

  if (isPrivileged) {
    return true;
  }

  if (id.endsWith("@mozilla.com") || id.endsWith("@mozilla.org")) {
    return true;
  }

  // This check is a subset of what is being checked in AddonWrapper's
  // recommendationStates (states expire dates for line extensions are
  // not consideredcimportant in determining that the extension is
  // provided by mozilla, and so they are omitted here on purpose).
  const isMozillaLineExtension = addonData.recommendationState?.states?.includes(
    "line"
  );
  const isSigned =
    addonData.signedState > lazy.AddonManager.SIGNEDSTATE_MISSING;

  return isSigned && isMozillaLineExtension;
}

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
    return { invalid: perm, privileged: true };
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
      uuid = Services.uuid.generateUUID().number;
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

function clearCacheForExtensionPrincipal(principal, clearAll = false) {
  if (!principal.schemeIs("moz-extension")) {
    return Promise.reject(new Error("Unexpected non extension principal"));
  }

  // TODO(Bug 1750053): replace the two specific flags with a "clear all caches one"
  // (along with covering the other kind of cached data with tests).
  const clearDataFlags = clearAll
    ? Ci.nsIClearDataService.CLEAR_ALL_CACHES
    : Ci.nsIClearDataService.CLEAR_IMAGE_CACHE |
      Ci.nsIClearDataService.CLEAR_CSS_CACHE;

  return new Promise(resolve =>
    Services.clearData.deleteDataFromPrincipal(
      principal,
      false,
      clearDataFlags,
      () => resolve()
    )
  );
}

/**
 * Observer AddonManager events and translate them into extension events,
 * as well as handle any last cleanup after uninstalling an extension.
 */
var ExtensionAddonObserver = {
  initialized: false,

  init() {
    if (!this.initialized) {
      lazy.AddonManager.addAddonListener(this);
      this.initialized = true;
    }
  },

  // AddonTestUtils will call this as necessary.
  uninit() {
    if (this.initialized) {
      lazy.AddonManager.removeAddonListener(this);
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
    // Cleanup anything that is used by non-extension addon types
    // since only extensions have uuid's.
    lazy.ExtensionPermissions.removeAll(addon.id);

    let uuid = UUIDMap.get(addon.id, false);
    if (!uuid) {
      return;
    }

    let baseURI = Services.io.newURI(`moz-extension://${uuid}/`);
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      baseURI,
      {}
    );

    // Clear all cached resources (e.g. CSS and images);
    lazy.AsyncShutdown.profileChangeTeardown.addBlocker(
      `Clear cache for ${addon.id}`,
      clearCacheForExtensionPrincipal(principal, /* clearAll */ true)
    );

    // Clear all the registered service workers for the extension
    // principal (the one that may have been registered through the
    // manifest.json file and the ones that may have been registered
    // from an extension page through the service worker API).
    //
    // Any stored data would be cleared below (if the pref
    // "extensions.webextensions.keepStorageOnUninstall has not been
    // explicitly set to true, which is usually only done in
    // tests and by some extensions developers for testing purpose).
    //
    // TODO: ServiceWorkerCleanUp may go away once Bug 1183245
    // is fixed, and so this may actually go away, replaced by
    // marking the registration as disabled or to be removed on
    // shutdown (where we do know if the extension is shutting
    // down because is being uninstalled) and then cleared from
    // the persisted serviceworker registration on the next
    // startup.
    lazy.AsyncShutdown.profileChangeTeardown.addBlocker(
      `Clear ServiceWorkers for ${addon.id}`,
      lazy.ServiceWorkerCleanUp.removeFromPrincipal(principal)
    );

    if (!Services.prefs.getBoolPref(LEAVE_STORAGE_PREF, false)) {
      // Clear browser.storage.local backends.
      lazy.AsyncShutdown.profileChangeTeardown.addBlocker(
        `Clear Extension Storage ${addon.id} (File Backend)`,
        lazy.ExtensionStorage.clear(addon.id, { shouldNotifyListeners: false })
      );

      // Clear any IndexedDB and Cache API storage created by the extension.
      // If LSNG is enabled, this also clears localStorage.
      Services.qms.clearStoragesForPrincipal(principal);

      // Clear any storage.local data stored in the IDBBackend.
      let storagePrincipal = Services.scriptSecurityManager.createContentPrincipal(
        baseURI,
        {
          userContextId: WEBEXT_STORAGE_USER_CONTEXT_ID,
        }
      );
      Services.qms.clearStoragesForPrincipal(storagePrincipal);

      lazy.ExtensionStorageIDB.clearMigratedExtensionPref(addon.id);

      // If LSNG is not enabled, we need to clear localStorage explicitly using
      // the old API.
      if (!Services.domStorageManager.nextGenLocalStorageEnabled) {
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

    if (!Services.prefs.getBoolPref(LEAVE_UUID_PREF, false)) {
      // Clear the entry in the UUID map
      UUIDMap.remove(addon.id);
    }
  },
};

ExtensionAddonObserver.init();

const manifestTypes = new Map([
  ["theme", "manifest.ThemeManifest"],
  ["sitepermission", "manifest.WebExtensionSitePermissionsManifest"],
  ["locale", "manifest.WebExtensionLangpackManifest"],
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
  constructor(rootURI, isPrivileged = false) {
    this.rootURI = rootURI;
    this.resourceURL = rootURI.spec;
    this.isPrivileged = isPrivileged;

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
    this.eventPagesEnabled = lazy.eventPagesEnabled;
  }

  /**
   * A factory function that allows the construction of ExtensionData, with
   * the isPrivileged flag computed asynchronously.
   *
   * @param {nsIURI} rootURI
   *  The URI pointing to the extension root.
   * @param {function(type, id)} checkPrivileged
   *  An (async) function that takes the addon type and addon ID and returns
   *  whether the given add-on is privileged.
   * @param {boolean} temporarilyInstalled
   *  whether the given add-on is installed as temporary.
   * @returns {ExtensionData}
   */
  static async constructAsync({
    rootURI,
    checkPrivileged,
    temporarilyInstalled,
  }) {
    let extension = new ExtensionData(rootURI);
    // checkPrivileged depends on the extension type and id.
    await extension.initializeAddonTypeAndID();
    let { type, id } = extension;
    extension.isPrivileged = await checkPrivileged(type, id);
    extension.temporarilyInstalled = temporarilyInstalled;
    return extension;
  }

  static getIsPrivileged({ signedState, builtIn, temporarilyInstalled }) {
    return (
      signedState === lazy.AddonManager.SIGNEDSTATE_PRIVILEGED ||
      signedState === lazy.AddonManager.SIGNEDSTATE_SYSTEM ||
      builtIn ||
      (lazy.AddonSettings.EXPERIMENTS_ENABLED && temporarilyInstalled)
    );
  }

  get builtinMessages() {
    return null;
  }

  get logger() {
    let id = this.id || "<unknown>";
    return lazy.Log.repository.getLogger(LOGGER_ID_BASE + id);
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

  ensureNoErrors() {
    if (this.errors.length) {
      // startup() repeatedly checks whether there are errors after parsing the
      // extension/manifest before proceeding with starting up.
      throw new Error(this.errors.join("\n"));
    }
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

  /**
   * Discovers the file names within a directory or JAR file.
   *
   * @param {Ci.nsIFileURL|Ci.nsIJARURI} path
   *   The path to the directory or jar file to look at.
   * @param {boolean} [directoriesOnly]
   *   If true, this will return only the directories present within the directory.
   * @returns {string[]}
   *   An array of names of files/directories (only the name, not the path).
   */
  async _readDirectory(path, directoriesOnly = false) {
    if (this.rootURI instanceof Ci.nsIFileURL) {
      let uri = Services.io.newURI("./" + path, null, this.rootURI);
      let fullPath = uri.QueryInterface(Ci.nsIFileURL).file.path;

      let results = [];
      try {
        let children = await IOUtils.getChildren(fullPath);
        for (let child of children) {
          if (
            !directoriesOnly ||
            (await IOUtils.stat(child)).type == "directory"
          ) {
            results.push(PathUtils.filename(child));
          }
        }
      } catch (ex) {
        // Fall-through, return what we have.
      }
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
    for (let name of lazy.aomStartup.enumerateJARSubtree(uri)) {
      if (!name.startsWith(entry)) {
        throw new Error("Unexpected ZipReader entry");
      }

      // The enumerator returns the full path of all entries.
      // Trim off the leading path, and filter out entries from
      // subdirectories.
      name = name.slice(entry.length);
      if (
        name &&
        !/\/./.test(name) &&
        (!directoriesOnly || name.endsWith("/"))
      ) {
        results.push(name.replace("/", ""));
      }
    }

    return results;
  }

  readJSON(path) {
    return new Promise((resolve, reject) => {
      let uri = this.rootURI.resolve(`./${path}`);

      lazy.NetUtil.asyncFetch(
        { uri, loadUsingSystemPrincipal: true },
        (inputStream, status) => {
          if (!Components.isSuccessCode(status)) {
            // Convert status code to a string
            let e = Components.Exception("", status);
            reject(new Error(`Error while loading '${uri}' (${e.name})`));
            return;
          }
          try {
            let text = lazy.NetUtil.readInputStreamToString(
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
    return !(this.isPrivileged && this.hasPermission("mozillaAddons"));
  }

  /**
   * Given an array of host and permissions, generate a structured permissions object
   * that contains seperate host origins and permissions arrays.
   *
   * @param {Array} permissionsArray
   * @param {Array} [hostPermissions]
   * @returns {Object} permissions object
   */
  permissionsObject(permissionsArray = [], hostPermissions = []) {
    let permissions = new Set();
    let origins = new Set();
    let { restrictSchemes, isPrivileged } = this;

    for (let perm of permissionsArray.concat(hostPermissions)) {
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

    let { permissions } = this.permissionsObject(this.manifest.permissions);

    if (
      this.manifest.devtools_page &&
      !this.manifest.optional_permissions.includes("devtools")
    ) {
      permissions.add("devtools");
    }

    return {
      permissions: Array.from(permissions),
      origins: this.originControls ? [] : this.getManifestOrigins(),
    };
  }

  /**
   * @returns {string[]} all origins that are referenced in manifest via
   * permissions, host_permissions, or content_scripts keys.
   */
  getManifestOrigins() {
    if (this.type !== "extension") {
      return null;
    }

    let { origins } = this.permissionsObject(
      this.manifest.permissions,
      this.manifest.host_permissions
    );

    for (let entry of this.manifest.content_scripts || []) {
      for (let origin of entry.matches) {
        origins.add(origin);
      }
    }

    return Array.from(origins);
  }

  /**
   * Returns optional permissions from the manifest, including host permissions
   * if originControls is true.
   */
  get manifestOptionalPermissions() {
    if (this.type !== "extension") {
      return null;
    }

    let { permissions, origins } = this.permissionsObject(
      this.manifest.optional_permissions
    );
    if (this.originControls) {
      for (let origin of this.getManifestOrigins()) {
        origins.add(origin);
      }
    }

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
      origins: this.allowedOrigins.patterns
        .map(matcher => matcher.pattern)
        // moz-extension://id/* is always added to allowedOrigins, but it
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

  // Returns whether the front end should prompt for this permission
  static async shouldPromptFor(permission) {
    return !(await lazy.NO_PROMPT_PERMISSIONS).has(permission);
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
    await lazy.ExtensionPermissions.add(id, migrated);

    // Now we need to update ExtensionPreferencesManager, removing any settings
    // for old permissions that no longer exist.
    let permSet = new Set(
      newPermissions.permissions.concat(newOptionalPermissions.permissions)
    );
    let oldPerms = oldPermissions.permissions.concat(
      oldOptionalPermissions.permissions
    );

    let removed = oldPerms.filter(x => !permSet.has(x));
    // Force the removal here to ensure the settings are removed prior
    // to startup.  This will remove both required or optional permissions,
    // whereas the call from within ExtensionPermissions would only result
    // in a removal for optional permissions that were removed.
    await lazy.ExtensionPreferencesManager.removeSettingsForPermissions(
      id,
      removed
    );

    // Remove any optional permissions that have been removed from the manifest.
    await lazy.ExtensionPermissions.remove(id, {
      permissions: removed,
      origins: [],
    });
  }

  canUseAPIExperiment() {
    return (
      this.type == "extension" &&
      (this.isPrivileged ||
        // TODO(Bug 1771341): Allowing the "experiment_apis" property when only
        // AddonSettings.EXPERIMENTS_ENABLED is true is currently needed to allow,
        // while running under automation, the test harness extensions (like mochikit
        // and specialpowers) to use that privileged manifest property.
        lazy.AddonSettings.EXPERIMENTS_ENABLED)
    );
  }

  canUseThemeExperiment() {
    return (
      ["extension", "theme"].includes(this.type) &&
      (this.isPrivileged ||
        // "theme_experiment" MDN docs are currently explicitly mentioning this is expected
        // to be allowed also for non-signed extensions installed non-temporarily on builds
        // where the signature checks can be disabled).
        //
        // NOTE: be careful to don't regress "theme_experiment" (see Bug 1773076) while changing
        // AddonSettings.EXPERIMENTS_ENABLED (e.g. as part of fixing Bug 1771341).
        lazy.AddonSettings.EXPERIMENTS_ENABLED)
    );
  }

  get manifestVersion() {
    return this.manifest.manifest_version;
  }

  get persistentBackground() {
    let { manifest } = this;
    if (
      !manifest.background ||
      manifest.background.service_worker ||
      this.manifestVersion > 2
    ) {
      return false;
    }
    // V2 addons can only use event pages if the pref is also flipped and
    // persistent is explicilty set to false.
    return !this.eventPagesEnabled || manifest.background.persistent;
  }

  /**
   * backgroundState can be starting, running, suspending or stopped.
   * It is undefined if the extension has no background page.
   * See ext-backgroundPage.js for more details.
   *
   * @param {string} state starting, running, suspending or stopped
   */
  set backgroundState(state) {
    this._backgroundState = state;
  }

  get backgroundState() {
    return this._backgroundState;
  }

  async getExtensionVersionWithoutValidation() {
    return (await this.readJSON("manifest.json")).version;
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
      manifestVersion: this.manifestVersion,
    };

    if (this.fluentL10n || this.localeData) {
      context.preprocessors.localize = (value, context) =>
        this.localize(value, locale);
    }

    return lazy.Schemas.normalize(this.rawManifest, manifestType, context);
  }

  async initializeAddonTypeAndID() {
    if (this.type) {
      // Already initialized.
      return;
    }
    this.rawManifest = await this.readJSON("manifest.json");
    let manifest = this.rawManifest;

    if (manifest.theme) {
      this.type = "theme";
    } else if (manifest.langpack_id) {
      this.type = "locale";
    } else if (manifest.dictionaries) {
      this.type = "dictionary";
    } else if (manifest.site_permissions) {
      this.type = "sitepermission";
    } else {
      this.type = "extension";
    }

    if (!this.id) {
      let bss =
        manifest.browser_specific_settings?.gecko ||
        manifest.applications?.gecko;
      let id = bss?.id;
      // This is a basic type check.
      // When parseManifest is called, the ID is validated more thoroughly
      // because the id is defined to be an ExtensionID type in
      // toolkit/components/extensions/schemas/manifest.json
      if (typeof id == "string") {
        this.id = id;
      }
    }
  }

  // eslint-disable-next-line complexity
  async parseManifest() {
    await Promise.all([this.initializeAddonTypeAndID(), Management.lazyInit()]);

    let manifest = this.rawManifest;
    this.manifest = manifest;

    if (manifest.default_locale) {
      await this.initLocale();
    }

    if (manifest.l10n_resources) {
      if (this.isPrivileged) {
        // TODO (Bug 1733466): For historical reasons fluent isn't being used to
        // localize manifest properties read from the add-on manager (e.g., author,
        // homepage, etc.), the changes introduced by Bug 1734987 does now ensure
        // that isPrivileged will be set while parsing the manifest and so this
        // can be now supported but requires some additional changes, being tracked
        // by Bug 1733466.
        if (this.constructor != ExtensionData) {
          this.fluentL10n = new Localization(manifest.l10n_resources, true);
        }
      } else if (this.temporarilyInstalled) {
        this.manifestError(
          `Using 'l10n_resources' requires a privileged add-on. ` +
            PRIVILEGED_ADDONS_DEVDOCS_MESSAGE
        );
      } else {
        // Warn but don't make this fatal.
        this.manifestWarning(
          "Ignoring l10n_resources in unprivileged extension"
        );
      }
    }

    let normalized = await this._getNormalizedManifest();
    if (normalized.error) {
      this.manifestError(normalized.error);
      return null;
    }

    manifest = normalized.value;

    // browser_specific_settings is documented, but most internal code is written
    // using applications.  Use browser_specific_settings if it is in the manifest.  If
    // both are set, we probably should make it an error, but we don't know if addons
    // in the wild have done that, so let the chips fall where they may.
    if (manifest.browser_specific_settings?.gecko) {
      if (manifest.applications) {
        this.manifestWarning(
          `"applications" property ignored and overridden by "browser_specific_settings"`
        );
      }
      manifest.applications = manifest.browser_specific_settings;
    }

    if (
      this.manifestVersion < 3 &&
      manifest.background &&
      !this.eventPagesEnabled &&
      !manifest.background.persistent
    ) {
      this.logWarning("Event pages are not currently supported.");
    }

    let apiNames = new Set();
    let dependencies = new Set();
    let originPermissions = new Set();
    let permissions = new Set();
    let webAccessibleResources = [];

    let schemaPromises = new Map();

    // Note: this.id and this.type were computed in initializeAddonTypeAndID.
    // The format of `this.id` was confirmed to be a valid extensionID by the
    // Schema validation as part of the _getNormalizedManifest() call.
    let result = {
      apiNames,
      dependencies,
      id: this.id,
      manifest,
      modules: null,
      // Whether to treat all origin permissions (including content scripts)
      // from the manifestas as optional, and enable users to control them.
      originControls: this.manifestVersion >= 3,
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

      // Privileged and temporary extensions can opt out of originControls.
      if (
        (isPrivileged || this.temporarilyInstalled) &&
        manifest.granted_host_permissions
      ) {
        result.originControls = false;
      }

      let host_permissions = manifest.host_permissions ?? [];

      for (let perm of manifest.permissions.concat(host_permissions)) {
        if (perm === "geckoProfiler" && !isPrivileged) {
          const acceptedExtensions = Services.prefs.getStringPref(
            "extensions.geckoProfiler.acceptedExtensionIds",
            ""
          );
          if (!acceptedExtensions.split(",").includes(this.id)) {
            this.manifestError(
              "Only specific extensions are allowed to access the geckoProfiler."
            );
            continue;
          }
        }

        let type = classifyPermission(perm, restrictSchemes, isPrivileged);
        if (type.origin) {
          perm = type.origin;
          if (!result.originControls) {
            originPermissions.add(perm);
          }
        } else if (type.api) {
          apiNames.add(type.api);
        } else if (type.invalid) {
          // If EXPERIMENTS_ENABLED is not enabled prevent the install
          // to ensure developer awareness.
          if (this.temporarilyInstalled && type.privileged) {
            this.manifestError(
              `Using the privileged permission '${perm}' requires a privileged add-on. ` +
                PRIVILEGED_ADDONS_DEVDOCS_MESSAGE
            );
            continue;
          }
          this.manifestWarning(`Invalid extension permission: ${perm}`);
          continue;
        }

        // Unfortunately, we treat <all_urls> as an API permission as well.
        if (!type.origin || (perm === "<all_urls>" && !result.originControls)) {
          permissions.add(perm);
        }
      }

      if (this.id) {
        // An extension always gets permission to its own url.
        let matcher = new MatchPattern(this.getURL(), { ignorePath: true });
        originPermissions.add(matcher.pattern);

        // Apply optional permissions
        let perms = await lazy.ExtensionPermissions.get(this.id);
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

      if (manifest.experiment_apis) {
        if (this.canUseAPIExperiment()) {
          let parentModules = {};
          let childModules = {};

          for (let [name, data] of Object.entries(manifest.experiment_apis)) {
            let schema = this.getURL(data.schema);

            if (!schemaPromises.has(schema)) {
              schemaPromises.set(
                schema,
                this.readJSON(data.schema).then(json =>
                  lazy.Schemas.processSchema(json)
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
        } else if (this.temporarilyInstalled) {
          // Hard error for un-privileged temporary installs using experimental apis.
          this.manifestError(
            `Using 'experiment_apis' requires a privileged add-on. ` +
              PRIVILEGED_ADDONS_DEVDOCS_MESSAGE
          );
        } else {
          this.manifestWarning(
            `Using experimental APIs requires a privileged add-on.`
          );
        }
      }

      // Normalize all patterns to contain a single leading /
      if (manifest.web_accessible_resources) {
        // Normalize into V3 objects
        let wac =
          this.manifestVersion >= 3
            ? manifest.web_accessible_resources
            : [{ resources: manifest.web_accessible_resources }];
        webAccessibleResources.push(
          ...wac.map(obj => {
            obj.resources = obj.resources.map(path =>
              path.replace(/^\/*/, "/")
            );
            return obj;
          })
        );
      }
    } else if (this.type == "locale") {
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
      const entries = await this._readDirectory("localization");
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

        let dir = lazy.dirname(path);
        if (dir === ".") {
          dir = "";
        }
        let leafName = lazy.basename(path);
        let affixPath = leafName.slice(0, -3) + "aff";

        let entries = await this._readDirectory(dir);
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

    this.webAccessibleResources = manifestData.webAccessibleResources;

    this.originControls = manifestData.originControls;
    this.allowedOrigins = new MatchPatternSet(manifestData.originPermissions, {
      restrictSchemes: this.restrictSchemes,
    });

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

  // Returns true if an addon is builtin to Firefox or
  // distributed via Normandy into a system location.
  get isAppProvided() {
    return this.addonData.builtIn || this.addonData.isSystem;
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

    let entries = await this._readDirectory("_locales", true);
    for (let name of entries) {
      let locale = this.normalizeLocaleCode(name);
      locales.set(locale, name);
    }

    return locales;
  }

  _setupLocaleData(locales) {
    if (this.localeData) {
      return this.localeData.locales;
    }

    this.localeData = new lazy.LocaleData({
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
   * Classify host permissions
   * @param {array<string>} origins
   *                        permission origins
   * @param {boolean}       ignoreNonWebSchemes
   *                        return only these schemes: *, http, https, ws, wss
   *
   * @returns {object}
   *   @param {string} .allUrls   permission used to obtain all urls access
   *   @param {Set} .wildcards    set contains permissions with wildcards
   *   @param {Set} .sites        set contains explicit host permissions
   *   @param {Map} .wildcardsMap mapping origin wildcards to labels
   *   @param {Map} .sitesMap     mapping origin patterns to labels
   */
  static classifyOriginPermissions(origins = [], ignoreNonWebSchemes = false) {
    let allUrls = null,
      wildcards = new Set(),
      sites = new Set(),
      // TODO: use map.values() instead of these sets.  Note: account for two
      // match patterns producing the same permission string, see bug 1765828.
      wildcardsMap = new Map(),
      sitesMap = new Map();

    // https://searchfox.org/mozilla-central/rev/6f6cf28107/toolkit/components/extensions/MatchPattern.cpp#235
    const wildcardSchemes = ["*", "http", "https", "ws", "wss"];

    for (let permission of origins) {
      if (permission == "<all_urls>") {
        allUrls = permission;
        continue;
      }

      // Privileged extensions may request access to "about:"-URLs, such as
      // about:reader.
      let match = /^([a-z*]+):\/\/([^/]*)\/|^about:/.exec(permission);
      if (!match) {
        throw new Error(`Unparseable host permission ${permission}`);
      }

      // Note: the scheme is ignored in the permission warnings. If this ever
      // changes, update the comparePermissions method as needed.
      let [, scheme, host] = match;
      if (ignoreNonWebSchemes && !wildcardSchemes.includes(scheme)) {
        continue;
      }

      if (!host || host == "*") {
        if (!allUrls) {
          allUrls = permission;
        }
      } else if (host.startsWith("*.")) {
        wildcards.add(host.slice(2));
        // Using MatchPattern to normalize the pattern string.
        let pat = new MatchPattern(permission, { ignorePath: true });
        wildcardsMap.set(pat.pattern, `${scheme}://${host.slice(2)}`);
      } else {
        sites.add(host);
        let pat = new MatchPattern(permission, {
          ignorePath: true,
          // Safe because used just for normalization, not for granting access.
          restrictSchemes: false,
        });
        sitesMap.set(pat.pattern, `${scheme}://${host}`);
      }
    }
    return { allUrls, wildcards, sites, wildcardsMap, sitesMap };
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
   * @param {array<string>} info.optionalPermissions.origins
   *                        Optional origin permissions listed in the manifest.
   * @param {array<string>} info.optionalPermissions.permissions
   *                        Optional (non-origin) permissions listed in the manifest.
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
   * @param {boolean} options.buildOptionalOrigins
   *                  Wether to build optional origins Maps for permission
   *                  controls.  Defaults to false.
   * @param {function} options.getKeyForPermission
   *                   An optional callback function that returns the locale key for a given
   *                   permission name (set by default to a callback returning the locale
   *                   key following the default convention `webextPerms.description.PERMNAME`).
   *                   Overriding the default mapping can become necessary, when a permission
   *                   description needs to be modified and a non-default locale key has to be
   *                   used. There is at least one non-default locale key used in Thunderbird.
   *
   * @returns {object} An object with properties containing localized strings
   *                   for various elements of a permission dialog. The "header"
   *                   property on this object is the notification header text
   *                   and it has the string "<>" as a placeholder for the
   *                   addon name.
   *
   *                   "object.msgs" is an array of localized strings describing required permissions
   *
   *                   "object.optionalPermissions" is a map of permission name to localized
   *                   strings describing the permission.
   *
   *                   "object.optionalOrigins" is a map of a host permission to localized strings
   *                   describing the host permission, where appropriate.  Currently only
   *                   all url style permissions are included.
   */
  static formatPermissionStrings(
    info,
    bundle,
    {
      collapseOrigins = false,
      buildOptionalOrigins = false,
      getKeyForPermission = perm => `webextPerms.description.${perm}`,
    } = {}
  ) {
    let result = {
      msgs: [],
      optionalPermissions: {},
      optionalOrigins: {},
    };

    const haveAccessKeys = AppConstants.platform !== "android";

    let headerKey;
    result.text = "";
    result.listIntro = "";
    result.acceptText = bundle.GetStringFromName("webextPerms.add.label");
    result.cancelText = bundle.GetStringFromName("webextPerms.cancel.label");
    if (haveAccessKeys) {
      result.acceptKey = bundle.GetStringFromName("webextPerms.add.accessKey");
      result.cancelKey = bundle.GetStringFromName(
        "webextPerms.cancel.accessKey"
      );
    }

    // Generate a map of site_permission names to permission strings for site
    // permissions.  Since SitePermission addons cannot have regular permissions,
    // we reuse msgs to pass the strings to the permissions panel.
    if (info.sitePermissions) {
      for (let permission of info.sitePermissions) {
        try {
          result.msgs.push(
            bundle.GetStringFromName(
              `webextSitePerms.description.${permission}`
            )
          );
        } catch (err) {
          Cu.reportError(
            `site_permission ${permission} missing readable text property`
          );
          // We must never have a DOM api permission that is hidden so in
          // the case of any error, we'll use the plain permission string.
          // test_ext_sitepermissions.js tests for no missing messages, this
          // is just an extra fallback.
          result.msgs.push(permission);
        }
      }

      // Generate header message
      headerKey = info.unsigned
        ? "webextSitePerms.headerUnsignedWithPerms"
        : "webextSitePerms.headerWithPerms";
      // We simplify the origin to make it more user friendly.  The origin is
      // assured to be available via schema requirement.
      result.header = bundle.formatStringFromName(headerKey, [
        "<>",
        new URL(info.siteOrigin).hostname,
      ]);
      return result;
    }

    let perms = info.permissions || { origins: [], permissions: [] };
    let optional_permissions = info.optionalPermissions || {
      origins: [],
      permissions: [],
    };

    // First classify our host permissions
    let { allUrls, wildcards, sites } = ExtensionData.classifyOriginPermissions(
      perms.origins
    );

    // Format the host permissions.  If we have a wildcard for all urls,
    // a single string will suffice.  Otherwise, show domain wildcards
    // first, then individual host permissions.
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
            lazy.PluralForm.get(
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

    // Next, show the native messaging permission if it is present.
    const NATIVE_MSG_PERM = "nativeMessaging";
    if (perms.permissions.includes(NATIVE_MSG_PERM)) {
      result.msgs.push(
        bundle.formatStringFromName(getKeyForPermission(NATIVE_MSG_PERM), [
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
      if (permission == NATIVE_MSG_PERM) {
        continue;
      }
      try {
        result.msgs.push(
          bundle.GetStringFromName(getKeyForPermission(permission))
        );
      } catch (err) {
        // We deliberately do not include all permissions in the prompt.
        // So if we don't find one then just skip it.
      }
    }

    // Generate a map of permission names to permission strings for optional
    // permissions.  The key is necessary to handle toggling those permissions.
    for (let permission of optional_permissions.permissions) {
      if (permission == NATIVE_MSG_PERM) {
        result.optionalPermissions[
          permission
        ] = bundle.formatStringFromName(getKeyForPermission(permission), [
          info.appName,
        ]);
        continue;
      }
      try {
        result.optionalPermissions[permission] = bundle.GetStringFromName(
          getKeyForPermission(permission)
        );
      } catch (err) {
        // We deliberately do not have strings for all permissions.
        // So if we don't find one then just skip it.
      }
    }

    let optionalInfo = ExtensionData.classifyOriginPermissions(
      optional_permissions.origins,
      true
    );
    if (optionalInfo.allUrls) {
      result.optionalOrigins[optionalInfo.allUrls] = bundle.GetStringFromName(
        "webextPerms.hostDescription.allUrls"
      );
    }

    // Current UX controls are meant for developer testing with mv3.
    if (buildOptionalOrigins) {
      for (let [pattern, originLabel] of optionalInfo.wildcardsMap.entries()) {
        let key = "webextPerms.hostDescription.wildcard";
        let str = bundle.formatStringFromName(key, [originLabel]);
        result.optionalOrigins[pattern] = str;
      }
      for (let [pattern, originLabel] of optionalInfo.sitesMap.entries()) {
        let key = "webextPerms.hostDescription.oneSite";
        let str = bundle.formatStringFromName(key, [originLabel]);
        result.optionalOrigins[pattern] = str;
      }
    }

    if (info.type == "sideload") {
      headerKey = "webextPerms.sideloadHeader";
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
      headerKey = "webextPerms.updateText2";
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
      headerKey = "webextPerms.optionalPermsHeader";
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
    } else {
      headerKey = "webextPerms.header";
      if (result.msgs.length) {
        headerKey = info.unsigned
          ? "webextPerms.headerUnsignedWithPerms"
          : "webextPerms.headerWithPerms";
      } else if (info.unsigned) {
        headerKey = "webextPerms.headerUnsigned";
      }
    }
    result.header = bundle.formatStringFromName(headerKey, ["<>"]);
    return result;
  }
}

const PROXIED_EVENTS = new Set([
  "test-harness-message",
  "background-script-suspend",
  "background-script-suspend-canceled",
  "background-script-suspend-ignored",
]);

class BootstrapScope {
  install(data, reason) {}
  uninstall(data, reason) {
    lazy.AsyncShutdown.profileChangeTeardown.addBlocker(
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
    // For updates that happen during startup, such as sideloads
    // and staged updates, the extension startupReason will be
    // APP_STARTED.  In some situations, such as background and
    // persisted listeners, we also need to know that the addon
    // was updated.
    this.updateReason = this.BOOTSTRAP_REASON_TO_STRING_MAP[reason];
    // Retain any previously granted permissions that may have migrated
    // into the optional list.
    if (data.oldPermissions) {
      // New permissions may be null, ensure we have an empty
      // permission set in that case.
      let emptyPermissions = { permissions: [], origins: [] };
      await ExtensionData.migratePermissions(
        data.id,
        data.oldPermissions,
        data.oldOptionalPermissions,
        data.userPermissions || emptyPermissions,
        data.optionalPermissions || emptyPermissions
      );
    }

    return Management.emit("update", {
      id: data.id,
      resourceURI: data.resourceURI,
      isPrivileged: data.isPrivileged,
    });
  }

  startup(data, reason) {
    // eslint-disable-next-line no-use-before-define
    this.extension = new Extension(
      data,
      this.BOOTSTRAP_REASON_TO_STRING_MAP[reason],
      this.updateReason
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
    const { BOOTSTRAP_REASONS } = lazy.AddonManagerPrivate;

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

class LangpackBootstrapScope extends BootstrapScope {
  install(data, reason) {}
  uninstall(data, reason) {}
  update(data, reason) {}

  startup(data, reason) {
    // eslint-disable-next-line no-use-before-define
    this.langpack = new Langpack(data);
    return this.langpack.startup(this.BOOTSTRAP_REASON_TO_STRING_MAP[reason]);
  }

  shutdown(data, reason) {
    this.langpack.shutdown(this.BOOTSTRAP_REASON_TO_STRING_MAP[reason]);
    this.langpack = null;
  }
}

class SitePermissionBootstrapScope extends BootstrapScope {
  install(data, reason) {}
  uninstall(data, reason) {}

  startup(data, reason) {
    // eslint-disable-next-line no-use-before-define
    this.sitepermission = new SitePermission(data);
    return this.sitepermission.startup(
      this.BOOTSTRAP_REASON_TO_STRING_MAP[reason]
    );
  }

  shutdown(data, reason) {
    this.sitepermission.shutdown(this.BOOTSTRAP_REASON_TO_STRING_MAP[reason]);
    this.sitepermission = null;
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
  constructor(addonData, startupReason, updateReason) {
    super(addonData.resourceURI, addonData.isPrivileged);

    this.startupStates = new Set();
    this.state = "Not started";
    this.userContextIsolation = lazy.userContextIsolation;

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
    if (addonData.TEST_NO_DELAYED_STARTUP) {
      this.testNoDelayedStartup = true;
    }

    this.addonData = addonData;
    this.startupData = addonData.startupData || {};
    this.startupReason = startupReason;
    this.updateReason = updateReason;

    if (
      updateReason ||
      ["ADDON_UPGRADE", "ADDON_DOWNGRADE"].includes(startupReason)
    ) {
      StartupCache.clearAddonData(addonData.id);
    }

    this.remote = !WebExtensionPolicy.isExtensionProcess;
    this.remoteType = this.remote ? lazy.E10SUtils.EXTENSION_REMOTE_TYPE : null;

    if (this.remote && lazy.processCount !== 1) {
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

    this.allowedOrigins = null;
    this._optionalOrigins = null;
    this.webAccessibleResources = null;

    this.registeredContentScripts = new Map();

    this.emitter = new EventEmitter();

    if (this.startupData.lwtData && this.startupReason == "APP_STARTUP") {
      lazy.LightweightThemeManager.fallbackThemeData = this.startupData.lwtData;
    }

    /* eslint-disable mozilla/balanced-listeners */
    this.on("add-permissions", (ignoreEvent, permissions) => {
      for (let perm of permissions.permissions) {
        this.permissions.add(perm);
      }
      this.policy.permissions = Array.from(this.permissions);

      updateAllowedOrigins(this.policy, permissions.origins, /* isAdd */ true);
      this.allowedOrigins = this.policy.allowedOrigins;

      if (this.policy.active) {
        this.setSharedData("", this.serialize());
        Services.ppmm.sharedData.flush();
        this.broadcast("Extension:UpdatePermissions", {
          id: this.id,
          origins: permissions.origins,
          permissions: permissions.permissions,
          add: true,
        });
      }

      this.cachePermissions();
      this.updatePermissions();
    });

    this.on("remove-permissions", (ignoreEvent, permissions) => {
      for (let perm of permissions.permissions) {
        this.permissions.delete(perm);
      }
      this.policy.permissions = Array.from(this.permissions);

      updateAllowedOrigins(this.policy, permissions.origins, /* isAdd */ false);
      this.allowedOrigins = this.policy.allowedOrigins;

      if (this.policy.active) {
        this.setSharedData("", this.serialize());
        Services.ppmm.sharedData.flush();
        this.broadcast("Extension:UpdatePermissions", {
          id: this.id,
          origins: permissions.origins,
          permissions: permissions.permissions,
          add: false,
        });
      }

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

  // Some helpful properties added elsewhere:

  static getBootstrapScope() {
    return new BootstrapScope();
  }

  get browsingContextGroupId() {
    return this.policy.browsingContextGroupId;
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

  get backgroundContext() {
    for (let view of this.views) {
      if (
        view.viewType === "background" ||
        view.viewType === "background_worker"
      ) {
        return view;
      }
    }
    return undefined;
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

  get temporarilyInstalled() {
    return !!this.addonData.temporarilyInstalled;
  }

  saveStartupData() {
    if (this.dontSaveStartupData) {
      return;
    }
    lazy.AddonManagerPrivate.setAddonStartupData(this.id, this.startupData);
  }

  parseManifest() {
    return StartupCache.manifests.get(this.manifestCacheKey, () =>
      super.parseManifest()
    );
  }

  async cachePermissions() {
    let manifestData = await this.parseManifest();

    manifestData.originPermissions = this.allowedOrigins.patterns.map(
      pat => pat.pattern
    );
    manifestData.permissions = this.permissions;
    return StartupCache.manifests.set(this.manifestCacheKey, manifestData);
  }

  async loadManifest() {
    let manifest = await super.loadManifest();

    this.ensureNoErrors();

    return manifest;
  }

  get extensionPageCSP() {
    const { content_security_policy } = this.manifest;
    // While only manifest v3 should contain an object,
    // we'll remain lenient here.
    if (
      content_security_policy &&
      typeof content_security_policy === "object"
    ) {
      return content_security_policy.extension_pages;
    }
    return content_security_policy;
  }

  get backgroundScripts() {
    return this.manifest.background?.scripts;
  }

  get backgroundWorkerScript() {
    return this.manifest.background?.service_worker;
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

  // TODO bug 1699481: move this logic to WebExtensionPolicy
  canAccessContainer(userContextId) {
    userContextId = userContextId ?? 0; // firefox-default has userContextId as 0.
    let defaultRestrictedContainers = JSON.parse(
      lazy.userContextIsolationDefaultRestricted
    );
    let extensionRestrictedContainers = JSON.parse(
      Services.prefs.getStringPref(
        `extensions.userContextIsolation.${this.id}.restricted`,
        "[]"
      )
    );
    if (
      extensionRestrictedContainers.includes(userContextId) ||
      defaultRestrictedContainers.includes(userContextId)
    ) {
      return false;
    }

    return true;
  }

  // Representation of the extension to send to content
  // processes. This should include anything the content process might
  // need.
  serialize() {
    return {
      id: this.id,
      uuid: this.uuid,
      name: this.name,
      manifestVersion: this.manifestVersion,
      extensionPageCSP: this.extensionPageCSP,
      instanceId: this.instanceId,
      resourceURL: this.resourceURL,
      contentScripts: this.contentScripts,
      webAccessibleResources: this.webAccessibleResources,
      allowedOrigins: this.allowedOrigins.patterns.map(pat => pat.pattern),
      permissions: this.permissions,
      optionalPermissions: this.optionalPermissions,
      isPrivileged: this.isPrivileged,
      temporarilyInstalled: this.temporarilyInstalled,
    };
  }

  // Extended serialized data which is only needed in the extensions process,
  // and is never deserialized in web content processes.
  // Keep in sync with BrowserExtensionContent in ExtensionChild.jsm
  serializeExtended() {
    return {
      backgroundScripts: this.backgroundScripts,
      backgroundWorkerScript: this.backgroundWorkerScript,
      childModules: this.modules && this.modules.child,
      dependencies: this.dependencies,
      persistentBackground: this.persistentBackground,
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
   * Clear cached resources associated to the extension principal
   * when an extension is installed (in case we were unable to do that at
   * uninstall time) or when it is being upgraded or downgraded.
   *
   * @param {string|undefined} reason
   *        BOOTSTRAP_REASON string, if provided. The value is expected to be
   *        `undefined` for extension objects without a corresponding AddonManager
   *        addon wrapper (e.g. test extensions created using `ExtensionTestUtils`
   *        without `useAddonManager` optional property).
   *
   * @returns {Promise<void>}
   *        Promise resolved when the nsIClearDataService async method call
   *        has been completed.
   */
  async clearCache(reason) {
    switch (reason) {
      case "ADDON_INSTALL":
      case "ADDON_UPGRADE":
      case "ADDON_DOWNGRADE":
        return clearCacheForExtensionPrincipal(this.principal);
    }
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

  async startup() {
    this.state = "Startup";

    // readyPromise is resolved with the policy upon success,
    // and with null if startup was interrupted.
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
      temporarilyInstalled: this.temporarilyInstalled,
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

    lazy.ExtensionTelemetry.extensionStartup.stopwatchStart(this);
    try {
      this.state = "Startup: Loading manifest";
      await this.loadManifest();
      this.state = "Startup: Loaded manifest";

      if (!this.hasShutdown) {
        this.state = "Startup: Init locale";
        await this.initLocale();
        this.state = "Startup: Initted locale";
      }

      this.ensureNoErrors();

      if (this.hasShutdown) {
        // Startup was interrupted and shutdown() has taken care of unloading
        // the extension and running cleanup logic.
        return;
      }

      await this.clearCache(this.startupReason);

      // We automatically add permissions to system/built-in extensions.
      // Extensions expliticy stating not_allowed will never get permission.
      let isAllowed = this.permissions.has(PRIVATE_ALLOWED_PERMISSION);
      if (this.manifest.incognito === "not_allowed") {
        // If an extension previously had permission, but upgrades/downgrades to
        // a version that specifies "not_allowed" in manifest, remove the
        // permission.
        if (isAllowed) {
          lazy.ExtensionPermissions.remove(this.id, {
            permissions: [PRIVATE_ALLOWED_PERMISSION],
            origins: [],
          });
          this.permissions.delete(PRIVATE_ALLOWED_PERMISSION);
        }
      } else if (
        !isAllowed &&
        this.isPrivileged &&
        !this.temporarilyInstalled
      ) {
        // Add to EP so it is preserved after ADDON_INSTALL.  We don't wait on the add here
        // since we are pushing the value into this.permissions.  EP will eventually save.
        lazy.ExtensionPermissions.add(this.id, {
          permissions: [PRIVATE_ALLOWED_PERMISSION],
          origins: [],
        });
        this.permissions.add(PRIVATE_ALLOWED_PERMISSION);
      }

      // We only want to update the SVG_CONTEXT_PROPERTIES_PERMISSION during install and
      // upgrade/downgrade startups.
      if (INSTALL_AND_UPDATE_STARTUP_REASONS.has(this.startupReason)) {
        if (isMozillaExtension(this)) {
          // Add to EP so it is preserved after ADDON_INSTALL.  We don't wait on the add here
          // since we are pushing the value into this.permissions.  EP will eventually save.
          lazy.ExtensionPermissions.add(this.id, {
            permissions: [SVG_CONTEXT_PROPERTIES_PERMISSION],
            origins: [],
          });
          this.permissions.add(SVG_CONTEXT_PROPERTIES_PERMISSION);
        } else {
          lazy.ExtensionPermissions.remove(this.id, {
            permissions: [SVG_CONTEXT_PROPERTIES_PERMISSION],
            origins: [],
          });
          this.permissions.delete(SVG_CONTEXT_PROPERTIES_PERMISSION);
        }
      }

      // Ensure devtools permission is set
      if (
        this.manifest.devtools_page &&
        !this.manifest.optional_permissions.includes("devtools")
      ) {
        lazy.ExtensionPermissions.add(this.id, {
          permissions: ["devtools"],
          origins: [],
        });
        this.permissions.add("devtools");
      }

      GlobalManager.init(this);

      this.initSharedData();

      this.policy.active = false;
      this.policy = lazy.ExtensionProcessScript.initExtension(this);
      this.policy.extension = this;

      this.updatePermissions(this.startupReason);

      // Select the storage.local backend if it is already known,
      // and start the data migration if needed.
      if (this.hasPermission("storage")) {
        if (!lazy.ExtensionStorageIDB.isBackendEnabled) {
          this.setSharedData("storageIDBBackend", false);
        } else if (lazy.ExtensionStorageIDB.isMigratedExtension(this)) {
          this.setSharedData("storageIDBBackend", true);
          this.setSharedData(
            "storageIDBPrincipal",
            lazy.ExtensionStorageIDB.getStoragePrincipal(this)
          );
        } else if (
          this.startupReason === "ADDON_INSTALL" &&
          !Services.prefs.getBoolPref(LEAVE_STORAGE_PREF, false)
        ) {
          // If the extension has been just installed, set it as migrated,
          // because there will not be any data to migrate.
          lazy.ExtensionStorageIDB.setMigratedExtensionPref(this, true);
          this.setSharedData("storageIDBBackend", true);
          this.setSharedData(
            "storageIDBPrincipal",
            lazy.ExtensionStorageIDB.getStoragePrincipal(this)
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
    } catch (e) {
      this.state = `Startup: Error: ${e}`;

      Cu.reportError(e);

      if (this.policy) {
        this.policy.active = false;
      }

      this.cleanupGeneratedFile();

      throw e;
    } finally {
      lazy.ExtensionTelemetry.extensionStartup.stopwatchFinish(this);
      // Mark readyPromise as resolved in case it has not happened before,
      // e.g. due to an early return or an error.
      resolveReadyPromise(null);
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
      lazy.ExtensionStorageIDB.selectedBackendPromises.has(this)
    ) {
      this.state = "Shutdown: Storage";

      // Wait the data migration to complete.
      try {
        await lazy.ExtensionStorageIDB.selectedBackendPromises.get(this);
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

    // The service worker registrations related to the extensions are unregistered
    // only when the extension is not shutting down as part of the application
    // shutdown (a previously registered service worker is expected to stay
    // active across browser restarts), the service worker may have been
    // registered through the manifest.json background.service_worker property
    // or from an extension page through the service worker API if allowed
    // through the about:config pref.
    if (!isAppShutdown) {
      this.state = "Shutdown: ServiceWorkers";
      // TODO: ServiceWorkerCleanUp may go away once Bug 1183245 is fixed.
      await lazy.ServiceWorkerCleanUp.removeFromPrincipal(this.principal);
      this.state = "Shutdown: ServiceWorkers completed";
    }

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
      let { origins } = this.manifestOptionalPermissions;
      this._optionalOrigins = new MatchPatternSet(origins, {
        restrictSchemes: this.restrictSchemes,
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

      lazy.spellCheck.addDictionary(lang, uri);
    }

    Management.emit("ready", this);
  }

  async shutdown(reason) {
    if (reason !== "APP_SHUTDOWN") {
      lazy.AddonManagerPrivate.unregisterDictionaries(this.dictionaries);
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
      this.chromeRegistryHandle = lazy.aomStartup.registerChrome(
        manifestURI,
        this.startupData.chromeEntries
      );
    }

    const langpackId = this.startupData.langpackId;
    const l10nRegistrySources = this.startupData.l10nRegistrySources;

    lazy.resourceProtocol.setSubstitution(langpackId, this.rootURI);

    const fileSources = Object.entries(l10nRegistrySources).map(entry => {
      const [sourceName, basePath] = entry;
      return new L10nFileSource(
        `${sourceName}-${langpackId}`,
        langpackId,
        this.startupData.languages,
        `resource://${langpackId}/${basePath}localization/{locale}/`
      );
    });

    L10nRegistry.getInstance().registerSources(fileSources);

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

    const sourcesToRemove = Object.keys(
      this.startupData.l10nRegistrySources
    ).map(sourceName => `${sourceName}-${this.startupData.langpackId}`);
    L10nRegistry.getInstance().removeSources(sourcesToRemove);

    if (this.chromeRegistryHandle) {
      this.chromeRegistryHandle.destruct();
      this.chromeRegistryHandle = null;
    }

    lazy.resourceProtocol.setSubstitution(this.startupData.langpackId, null);
  }
}

class SitePermission extends ExtensionData {
  constructor(addonData, startupReason) {
    super(addonData.resourceURI);
    this.id = addonData.id;
    this.hasShutdown = false;
  }

  async loadManifest() {
    let [manifestData] = await Promise.all([this.parseManifest()]);

    if (!manifestData) {
      return;
    }

    this.manifest = manifestData.manifest;
    this.type = manifestData.type;
    this.sitePermissions = this.manifest.site_permissions;
    // 1 install_origins is mandatory for this addon type
    this.siteOrigin = this.manifest.install_origins[0];

    return this.manifest;
  }

  static getBootstrapScope() {
    return new SitePermissionBootstrapScope();
  }

  // Array of principals that may be set by the addon.
  getSupportedPrincipals() {
    if (!this.siteOrigin) {
      return [];
    }
    const uri = Services.io.newURI(this.siteOrigin);
    return [
      Services.scriptSecurityManager.createContentPrincipal(uri, {}),
      Services.scriptSecurityManager.createContentPrincipal(uri, {
        privateBrowsingId: 1,
      }),
    ];
  }

  async startup(reason) {
    await this.loadManifest();

    this.ensureNoErrors();

    let site_permissions = await lazy.SCHEMA_SITE_PERMISSIONS;
    let perms = await lazy.ExtensionPermissions.get(this.id);

    if (this.hasShutdown) {
      // Startup was interrupted and shutdown() has taken care of unloading
      // the extension and running cleanup logic.
      return;
    }

    let privateAllowed = perms.permissions.includes(PRIVATE_ALLOWED_PERMISSION);
    let principals = this.getSupportedPrincipals();

    // Remove any permissions not contained in site_permissions
    for (let principal of principals) {
      let existing = Services.perms.getAllForPrincipal(principal);
      for (let perm of existing) {
        if (
          site_permissions.includes(perm) &&
          !this.sitePermissions.includes(perm)
        ) {
          Services.perms.removeFromPrincipal(principal, perm);
        }
      }
    }

    // Ensure all permissions in site_permissions have been set, but do not
    // overwrite the permission so the user can override the values in preferences.
    for (let perm of this.sitePermissions) {
      for (let principal of principals) {
        let permission = Services.perms.testExactPermissionFromPrincipal(
          principal,
          perm
        );
        if (permission == Ci.nsIPermissionManager.UNKNOWN_ACTION) {
          let { privateBrowsingId } = principal.originAttributes;
          let allow = privateBrowsingId == 0 || privateAllowed;
          Services.perms.addFromPrincipal(
            principal,
            perm,
            allow ? Services.perms.ALLOW_ACTION : Services.perms.DENY_ACTION,
            Services.perms.EXPIRE_NEVER
          );
        }
      }
    }

    Services.obs.notifyObservers(
      { wrappedJSObject: { sitepermissions: this } },
      "webextension-sitepermissions-startup"
    );
  }

  async shutdown(reason) {
    this.hasShutdown = true;
    // Permissions are retained across restarts
    if (reason == "APP_SHUTDOWN") {
      return;
    }
    let principals = this.getSupportedPrincipals();

    for (let perm of this.sitePermissions || []) {
      for (let principal of principals) {
        Services.perms.removeFromPrincipal(principal, perm);
      }
    }
  }
}
