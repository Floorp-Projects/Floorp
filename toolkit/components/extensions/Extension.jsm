/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Extension", "ExtensionData"];

/* exported Extension, ExtensionData */
/* globals Extension ExtensionData */

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

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.importGlobalProperties(["TextEncoder"]);

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.jsm",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  ExtensionCommon: "resource://gre/modules/ExtensionCommon.jsm",
  ExtensionPermissions: "resource://gre/modules/ExtensionPermissions.jsm",
  ExtensionStorage: "resource://gre/modules/ExtensionStorage.jsm",
  ExtensionTestCommon: "resource://testing-common/ExtensionTestCommon.jsm",
  Log: "resource://gre/modules/Log.jsm",
  MessageChannel: "resource://gre/modules/MessageChannel.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  Schemas: "resource://gre/modules/Schemas.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  TelemetryStopwatch: "resource://gre/modules/TelemetryStopwatch.jsm",
});

XPCOMUtils.defineLazyGetter(
  this, "processScript",
  () => Cc["@mozilla.org/webextensions/extension-process-script;1"]
          .getService().wrappedJSObject);

Cu.import("resource://gre/modules/ExtensionParent.jsm");
Cu.import("resource://gre/modules/ExtensionUtils.jsm");

XPCOMUtils.defineLazyServiceGetters(this, {
  aomStartup: ["@mozilla.org/addons/addon-manager-startup;1", "amIAddonManagerStartup"],
  uuidGen: ["@mozilla.org/uuid-generator;1", "nsIUUIDGenerator"],
});

XPCOMUtils.defineLazyPreferenceGetter(this, "processCount", "dom.ipc.processCount.extension");
XPCOMUtils.defineLazyPreferenceGetter(this, "useRemoteWebExtensions",
                                      "extensions.webextensions.remote", false);

var {
  GlobalManager,
  ParentAPIManager,
  StartupCache,
  apiManager: Management,
} = ExtensionParent;

const {
  EventEmitter,
  getUniqueId,
} = ExtensionUtils;

XPCOMUtils.defineLazyGetter(this, "console", ExtensionUtils.getConsole);

XPCOMUtils.defineLazyGetter(this, "LocaleData", () => ExtensionCommon.LocaleData);

// The maximum time to wait for extension shutdown blockers to complete.
const SHUTDOWN_BLOCKER_MAX_MS = 1000;

// The list of properties that themes are allowed to contain.
XPCOMUtils.defineLazyGetter(this, "allowedThemeProperties", () => {
  Cu.import("resource://gre/modules/ExtensionParent.jsm");
  let propertiesInBaseManifest = ExtensionParent.baseManifestProperties;

  // The properties found in the base manifest contain all of the properties that
  // themes are allowed to have. However, the list also contains several properties
  // that aren't allowed, so we need to filter them out first before the list can
  // be used to validate themes.
  return propertiesInBaseManifest.filter(prop => {
    const propertiesToRemove = ["background", "content_scripts", "permissions"];
    return !propertiesToRemove.includes(prop);
  });
});

/**
 * Validates a theme to ensure it only contains static resources.
 *
 * @param {Array<string>} manifestProperties The list of top-level keys found in the
 *    the extension's manifest.
 * @returns {Array<string>} A list of invalid properties or an empty list
 *    if none are found.
 */
function validateThemeManifest(manifestProperties) {
  let invalidProps = [];
  for (let propName of manifestProperties) {
    if (propName != "theme" && !allowedThemeProperties.includes(propName)) {
      invalidProps.push(propName);
    }
  }
  return invalidProps;
}

/**
 * Classify an individual permission from a webextension manifest
 * as a host/origin permission, an api permission, or a regular permission.
 *
 * @param {string} perm  The permission string to classify
 *
 * @returns {object}
 *          An object with exactly one of the following properties:
 *          "origin" to indicate this is a host/origin permission.
 *          "api" to indicate this is an api permission
 *                (as used for webextensions experiments).
 *          "permission" to indicate this is a regular permission.
 */
function classifyPermission(perm) {
  let match = /^(\w+)(?:\.(\w+)(?:\.\w+)*)?$/.exec(perm);
  if (!match) {
    return {origin: perm};
  } else if (match[1] == "experiments" && match[2]) {
    return {api: match[2]};
  }
  return {permission: perm};
}

const LOGGER_ID_BASE = "addons.webextension.";
const UUID_MAP_PREF = "extensions.webextensions.uuids";
const LEAVE_STORAGE_PREF = "extensions.webextensions.keepStorageOnUninstall";
const LEAVE_UUID_PREF = "extensions.webextensions.keepUuidOnUninstall";

const COMMENT_REGEXP = new RegExp(String.raw`
    ^
    (
      (?:
        [^"\n] |
        " (?:[^"\\\n] | \\.)* "
      )*?
    )

    //.*
  `.replace(/\s+/g, ""), "gm");

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

// This is the old interface that UUIDMap replaced, to be removed when
// the references listed in bug 1291399 are updated.
/* exported getExtensionUUID */
function getExtensionUUID(id) {
  return UUIDMap.get(id, true);
}

// For extensions that have called setUninstallURL(), send an event
// so the browser can display the URL.
var UninstallObserver = {
  initialized: false,

  init() {
    if (!this.initialized) {
      AddonManager.addAddonListener(this);
      this.initialized = true;
    }
  },

  onUninstalling(addon) {
    let extension = GlobalManager.extensionMap.get(addon.id);
    if (extension) {
      // Let any other interested listeners respond
      // (e.g., display the uninstall URL)
      Management.emit("uninstall", extension);
    }
  },

  onUninstalled(addon) {
    let uuid = UUIDMap.get(addon.id, false);
    if (!uuid) {
      return;
    }

    if (!Services.prefs.getBoolPref(LEAVE_STORAGE_PREF, false)) {
      // Clear browser.local.storage
      AsyncShutdown.profileChangeTeardown.addBlocker(
        `Clear Extension Storage ${addon.id}`,
        ExtensionStorage.clear(addon.id));

      // Clear any IndexedDB storage created by the extension
      let baseURI = Services.io.newURI(`moz-extension://${uuid}/`);
      let principal = Services.scriptSecurityManager.createCodebasePrincipal(
        baseURI, {});
      Services.qms.clearStoragesForPrincipal(principal);

      // Clear localStorage created by the extension
      let storage = Services.domStorageManager.getStorage(null, principal);
      if (storage) {
        storage.clear();
      }

      // Remove any permissions related to the unlimitedStorage permission
      // if we are also removing all the data stored by the extension.
      Services.perms.removeFromPrincipal(principal, "WebExtensions-unlimitedStorage");
      Services.perms.removeFromPrincipal(principal, "indexedDB");
      Services.perms.removeFromPrincipal(principal, "persistent-storage");
    }

    if (!Services.prefs.getBoolPref(LEAVE_UUID_PREF, false)) {
      // Clear the entry in the UUID map
      UUIDMap.remove(addon.id);
    }
  },
};

UninstallObserver.init();

// Represents the data contained in an extension, contained either
// in a directory or a zip file, which may or may not be installed.
// This class implements the functionality of the Extension class,
// primarily related to manifest parsing and localization, which is
// useful prior to extension installation or initialization.
//
// No functionality of this class is guaranteed to work before
// |loadManifest| has been called, and completed.
this.ExtensionData = class {
  constructor(rootURI) {
    this.rootURI = rootURI;
    this.resourceURL = rootURI.spec;

    this.manifest = null;
    this.id = null;
    this.uuid = null;
    this.localeData = null;
    this._promiseLocales = null;

    this.apiNames = new Set();
    this.dependencies = new Set();
    this.permissions = new Set();

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

  // Report an error about the extension's manifest file.
  manifestError(message) {
    this.packagingError(`Reading manifest: ${message}`);
  }

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
      throw new Error("getURL may not be called before an `id` or `uuid` has been set");
    }
    if (!this.uuid) {
      this.uuid = UUIDMap.get(this.id);
    }
    return `moz-extension://${this.uuid}/${path}`;
  }

  async readDirectory(path) {
    if (this.rootURI instanceof Ci.nsIFileURL) {
      let uri = Services.io.newURI(this.rootURI.resolve("./" + path));
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
        Cu.reportError(e);
      }
      iter.close();

      return results;
    }

    let uri = this.rootURI.QueryInterface(Ci.nsIJARURI);
    let file = uri.JARFile.QueryInterface(Ci.nsIFileURL).file;

    // Normalize the directory path.
    path = `${uri.JAREntry}/${path}`;
    path = path.replace(/\/\/+/g, "/").replace(/^\/|\/$/g, "") + "/";

    // Escape pattern metacharacters.
    let pattern = path.replace(/[[\]()?*~|$\\]/g, "\\$&") + "*";

    let results = [];
    for (let name of aomStartup.enumerateZipFile(file, pattern)) {
      if (!name.startsWith(path)) {
        throw new Error("Unexpected ZipReader entry");
      }

      // The enumerator returns the full path of all entries.
      // Trim off the leading path, and filter out entries from
      // subdirectories.
      name = name.slice(path.length);
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

      NetUtil.asyncFetch({uri, loadUsingSystemPrincipal: true}, (inputStream, status) => {
        if (!Components.isSuccessCode(status)) {
          // Convert status code to a string
          let e = Components.Exception("", status);
          reject(new Error(`Error while loading '${uri}' (${e.name})`));
          return;
        }
        try {
          let text = NetUtil.readInputStreamToString(inputStream, inputStream.available(),
                                                     {charset: "utf-8"});

          text = text.replace(COMMENT_REGEXP, "$1");

          resolve(JSON.parse(text));
        } catch (e) {
          reject(e);
        }
      });
    });
  }

  // This method should return a structured representation of any
  // capabilities this extension has access to, as derived from the
  // manifest.  The current implementation just returns the contents
  // of the permissions attribute, if we add things like url_overrides,
  // they should also be added here.
  get userPermissions() {
    let result = {
      origins: this.whiteListedHosts.patterns.map(matcher => matcher.pattern),
      apis: [...this.apiNames],
    };

    if (Array.isArray(this.manifest.content_scripts)) {
      for (let entry of this.manifest.content_scripts) {
        result.origins.push(...entry.matches);
      }
    }
    const EXP_PATTERN = /^experiments\.\w+/;
    result.permissions = [...this.permissions]
      .filter(p => !result.origins.includes(p) && !EXP_PATTERN.test(p));
    return result;
  }

  // Compute the difference between two sets of permissions, suitable
  // for presenting to the user.
  static comparePermissions(oldPermissions, newPermissions) {
    // See bug 1331769: should we do something more complicated to
    // compare host permissions?
    // e.g., if we go from <all_urls> to a specific host or from
    // a *.domain.com to specific-host.domain.com that's actually a
    // drop in permissions but the simple test below will cause a prompt.
    return {
      origins: newPermissions.origins.filter(perm => !oldPermissions.origins.includes(perm)),
      permissions: newPermissions.permissions.filter(perm => !oldPermissions.permissions.includes(perm)),
    };
  }

  parseManifest() {
    return Promise.all([
      this.readJSON("manifest.json"),
      Management.lazyInit(),
    ]).then(([manifest]) => {
      this.manifest = manifest;
      this.rawManifest = manifest;

      if (manifest && manifest.default_locale) {
        return this.initLocale();
      }
    }).then(() => {
      let context = {
        url: this.baseURI && this.baseURI.spec,

        principal: this.principal,

        logError: error => {
          this.manifestWarning(error);
        },

        preprocessors: {},
      };

      if (this.manifest.theme) {
        let invalidProps = validateThemeManifest(Object.getOwnPropertyNames(this.manifest));

        if (invalidProps.length) {
          let message = `Themes defined in the manifest may only contain static resources. ` +
            `If you would like to use additional properties, please use the "theme" permission instead. ` +
            `(the invalid properties found are: ${invalidProps})`;
          this.manifestError(message);
        }
      }

      if (this.localeData) {
        context.preprocessors.localize = (value, context) => this.localize(value);
      }

      let normalized = Schemas.normalize(this.manifest, "manifest.WebExtensionManifest", context);
      if (normalized.error) {
        this.manifestError(normalized.error);
        return null;
      }

      let manifest = normalized.value;

      let id;
      try {
        if (manifest.applications.gecko.id) {
          id = manifest.applications.gecko.id;
        }
      } catch (e) {
        // Errors are handled by the type checks above.
      }

      let apiNames = new Set();
      let dependencies = new Set();
      let hostPermissions = new Set();
      let permissions = new Set();

      for (let perm of manifest.permissions) {
        if (perm === "geckoProfiler") {
          const acceptedExtensions = Services.prefs.getStringPref("extensions.geckoProfiler.acceptedExtensionIds", "");
          if (!acceptedExtensions.split(",").includes(id)) {
            this.manifestError("Only whitelisted extensions are allowed to access the geckoProfiler.");
            continue;
          }
        }

        let type = classifyPermission(perm);
        if (type.origin) {
          let matcher = new MatchPattern(perm, {ignorePath: true});

          perm = matcher.pattern;
          hostPermissions.add(perm);
        } else if (type.api) {
          apiNames.add(type.api);
        }

        permissions.add(perm);
      }

      // An extension always gets permission to its own url.
      if (this.id) {
        let matcher = new MatchPattern(this.getURL(), {ignorePath: true});
        hostPermissions.add(matcher.pattern);
      }

      for (let api of apiNames) {
        dependencies.add(`${api}@experiments.addons.mozilla.org`);
      }

      // Normalize all patterns to contain a single leading /
      let webAccessibleResources = (manifest.web_accessible_resources || [])
          .map(path => path.replace(/^\/*/, "/"));

      return {apiNames, dependencies, hostPermissions, id, manifest, permissions,
              webAccessibleResources};
    });
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
    this.dependencies = manifestData.dependencies;
    this.permissions = manifestData.permissions;

    this.webAccessibleResources = manifestData.webAccessibleResources.map(res => new MatchGlob(res));
    this.whiteListedHosts = new MatchPatternSet(manifestData.hostPermissions);

    return this.manifest;
  }

  localizeMessage(...args) {
    return this.localeData.localizeMessage(...args);
  }

  localize(...args) {
    return this.localeData.localize(...args);
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

    await Promise.all(Array.from(locales.keys(),
                                 locale => this.readLocaleFile(locale)));

    let defaultLocale = this.defaultLocale;
    if (defaultLocale) {
      if (!locales.has(defaultLocale)) {
        this.manifestError('Value for "default_locale" property must correspond to ' +
                           'a directory in "_locales/". Not found: ' +
                           JSON.stringify(`_locales/${this.manifest.default_locale}/`));
      }
    } else if (locales.size) {
      this.manifestError('The "default_locale" property is required when a ' +
                         '"_locales/" directory is present.');
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

    let {defaultLocale} = this;
    if (locale != defaultLocale && !this.localeData.has(defaultLocale)) {
      promises.push(this.readLocaleFile(defaultLocale));
    }

    let results = await Promise.all(promises);

    this.localeData.selectedLocale = locale;
    return results[0];
  }
};

const PROXIED_EVENTS = new Set(["test-harness-message", "add-permissions", "remove-permissions"]);

const shutdownPromises = new Map();

class BootstrapScope {
  install(data, reason) {}
  uninstall(data, reason) {}

  startup(data, reason) {
    this.extension = new Extension(data, this.BOOTSTRAP_REASON_TO_STRING_MAP[reason]);
    return this.extension.startup();
  }

  shutdown(data, reason) {
    this.extension.shutdown(this.BOOTSTRAP_REASON_TO_STRING_MAP[reason]);
    this.extension = null;
  }
}

XPCOMUtils.defineLazyGetter(BootstrapScope.prototype, "BOOTSTRAP_REASON_TO_STRING_MAP", () => {
  const {BOOTSTRAP_REASONS} = AddonManagerPrivate;

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
});

// We create one instance of this class per extension. |addonData|
// comes directly from bootstrap.js when initializing.
this.Extension = class extends ExtensionData {
  constructor(addonData, startupReason) {
    super(addonData.resourceURI);

    this.uuid = UUIDMap.get(addonData.id);
    this.instanceId = getUniqueId();

    this.MESSAGE_EMIT_EVENT = `Extension:EmitEvent:${this.instanceId}`;
    Services.ppmm.addMessageListener(this.MESSAGE_EMIT_EVENT, this);

    if (addonData.cleanupFile) {
      Services.obs.addObserver(this, "xpcom-shutdown");
      this.cleanupFile = addonData.cleanupFile || null;
      delete addonData.cleanupFile;
    }

    this.addonData = addonData;
    this.startupReason = startupReason;

    if (["ADDON_UPGRADE", "ADDON_DOWNGRADE"].includes(startupReason)) {
      StartupCache.clearAddonData(addonData.id);
    }

    this.remote = useRemoteWebExtensions;

    if (this.remote && processCount !== 1) {
      throw new Error("Out-of-process WebExtensions are not supported with multiple child processes");
    }

    // This is filled in the first time an extension child is created.
    this.parentMessageManager = null;

    this.id = addonData.id;
    this.version = addonData.version;
    this.baseURI = Services.io.newURI(this.getURL("")).QueryInterface(Ci.nsIURL);
    this.principal = this.createPrincipal();
    this.views = new Set();
    this._backgroundPageFrameLoader = null;

    this.onStartup = null;

    this.hasShutdown = false;
    this.onShutdown = new Set();

    this.uninstallURL = null;

    this.apis = [];
    this.whiteListedHosts = null;
    this._optionalOrigins = null;
    this.webAccessibleResources = null;

    this.emitter = new EventEmitter();

    /* eslint-disable mozilla/balanced-listeners */
    this.on("add-permissions", (ignoreEvent, permissions) => {
      for (let perm of permissions.permissions) {
        this.permissions.add(perm);
      }

      if (permissions.origins.length > 0) {
        let patterns = this.whiteListedHosts.patterns.map(host => host.pattern);

        this.whiteListedHosts = new MatchPatternSet([...patterns, ...permissions.origins],
                                                    {ignorePath: true});
      }

      this.policy.permissions = Array.from(this.permissions);
      this.policy.allowedOrigins = this.whiteListedHosts;
    });

    this.on("remove-permissions", (ignoreEvent, permissions) => {
      for (let perm of permissions.permissions) {
        this.permissions.delete(perm);
      }

      let origins = permissions.origins.map(
        origin => new MatchPattern(origin, {ignorePath: true}).pattern);

      this.whiteListedHosts = new MatchPatternSet(
        this.whiteListedHosts.patterns
            .filter(host => !origins.includes(host.pattern)));

      this.policy.permissions = Array.from(this.permissions);
      this.policy.allowedOrigins = this.whiteListedHosts;
    });
    /* eslint-enable mozilla/balanced-listeners */
  }

  static getBootstrapScope(id, file) {
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

  static generateXPI(data) {
    return ExtensionTestCommon.generateXPI(data);
  }

  static generateZipFile(files, baseName = "generated-extension.xpi") {
    return ExtensionTestCommon.generateZipFile(files, baseName);
  }

  static generate(data) {
    return ExtensionTestCommon.generate(data);
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
      Services.ppmm.broadcastAsyncMessage(this.MESSAGE_EMIT_EVENT, {event, args});
    }

    return this.emitter.emit(event, ...args);
  }

  receiveMessage({name, data}) {
    if (name === this.MESSAGE_EMIT_EVENT) {
      this.emitter.emit(data.event, ...data.args);
    }
  }

  testMessage(...args) {
    this.emit("test-harness-message", ...args);
  }

  createPrincipal(uri = this.baseURI) {
    return Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
  }

  // Checks that the given URL is a child of our baseURI.
  isExtensionURL(url) {
    let uri = Services.io.newURI(url);

    let common = this.baseURI.getCommonBaseSpec(uri);
    return common == this.baseURI.spec;
  }

  async promiseLocales(locale) {
    let locales = await StartupCache.locales
      .get([this.id, "@@all_locales"], () => this._promiseLocaleMap());

    return this._setupLocaleData(locales);
  }

  readLocaleFile(locale) {
    return StartupCache.locales.get([this.id, this.version, locale],
                                    () => super.readLocaleFile(locale))
      .then(result => {
        this.localeData.messages.set(locale, result);
      });
  }

  parseManifest() {
    return StartupCache.manifests.get([this.id, this.version, Services.locale.getAppLocaleAsLangTag()],
                                      () => super.parseManifest());
  }

  async loadManifest() {
    let manifest = await super.loadManifest();

    if (this.errors.length) {
      return Promise.reject({errors: this.errors});
    }

    if (this.apiNames.size) {
      // Load Experiments APIs that this extension depends on.
      let apis = await Promise.all(
        Array.from(this.apiNames, api => ExtensionCommon.ExtensionAPIs.load(api)));

      for (let API of apis) {
        this.apis.push(new API(this));
      }
    }

    return manifest;
  }

  // Representation of the extension to send to content
  // processes. This should include anything the content process might
  // need.
  serialize() {
    return {
      id: this.id,
      uuid: this.uuid,
      instanceId: this.instanceId,
      manifest: this.manifest,
      resourceURL: this.resourceURL,
      baseURL: this.baseURI.spec,
      contentScripts: this.contentScripts,
      webAccessibleResources: this.webAccessibleResources.map(res => res.glob),
      whiteListedHosts: this.whiteListedHosts.patterns.map(pat => pat.pattern),
      localeData: this.localeData.serialize(),
      permissions: this.permissions,
      principal: this.principal,
      optionalPermissions: this.manifest.optional_permissions,
    };
  }

  get contentScripts() {
    return this.manifest.content_scripts || [];
  }

  broadcast(msg, data) {
    return new Promise(resolve => {
      let {ppmm} = Services;
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

  runManifest(manifest) {
    let promises = [];
    for (let directive in manifest) {
      if (manifest[directive] !== null) {
        promises.push(Management.emit(`manifest_${directive}`, directive, this, manifest));

        promises.push(Management.asyncEmitManifestEntry(this, directive));
      }
    }

    let data = Services.ppmm.initialProcessData;
    if (!data["Extension:Extensions"]) {
      data["Extension:Extensions"] = [];
    }
    let serial = this.serialize();
    data["Extension:Extensions"].push(serial);

    return this.broadcast("Extension:Startup", serial).then(() => {
      return Promise.all(promises);
    });
  }

  callOnClose(obj) {
    this.onShutdown.add(obj);
  }

  forgetOnClose(obj) {
    this.onShutdown.delete(obj);
  }

  get builtinMessages() {
    return new Map([
      ["@@extension_id", this.uuid],
    ]);
  }

  // Reads the locale file for the given Gecko-compatible locale code, or if
  // no locale is given, the available locale closest to the UI locale.
  // Sets the currently selected locale on success.
  async initLocale(locale = undefined) {
    if (locale === undefined) {
      let locales = await this.promiseLocales();

      let matches = Services.locale.negotiateLanguages(
        Services.locale.getAppLocalesAsLangTags(),
        Array.from(locales.keys()),
        this.defaultLocale);

      locale = matches[0];
    }

    return super.initLocale(locale);
  }

  updatePermissions(reason) {
    const {principal} = this;

    const testPermission = perm =>
      Services.perms.testPermissionFromPrincipal(principal, perm);

    // Only update storage permissions when the extension changes in
    // some way.
    if (reason !== "APP_STARTUP" && reason !== "APP_SHUTDOWN") {
      if (this.hasPermission("unlimitedStorage")) {
        // Set the indexedDB permission and a custom "WebExtensions-unlimitedStorage" to remember
        // that the permission hasn't been selected manually by the user.
        Services.perms.addFromPrincipal(principal, "WebExtensions-unlimitedStorage",
                                        Services.perms.ALLOW_ACTION);
        Services.perms.addFromPrincipal(principal, "indexedDB", Services.perms.ALLOW_ACTION);
        Services.perms.addFromPrincipal(principal, "persistent-storage", Services.perms.ALLOW_ACTION);
      } else {
        // Remove the indexedDB permission if it has been enabled using the
        // unlimitedStorage WebExtensions permissions.
        Services.perms.removeFromPrincipal(principal, "WebExtensions-unlimitedStorage");
        Services.perms.removeFromPrincipal(principal, "indexedDB");
        Services.perms.removeFromPrincipal(principal, "persistent-storage");
      }
    }

    // Never change geolocation permissions at shutdown, since it uses a
    // session-only permission.
    if (reason !== "APP_SHUTDOWN") {
      if (this.hasPermission("geolocation")) {
        if (testPermission("geo") === Services.perms.UNKNOWN_ACTION) {
          Services.perms.addFromPrincipal(principal, "geo",
                                          Services.perms.ALLOW_ACTION,
                                          Services.perms.EXPIRE_SESSION);
        }
      } else if (reason !== "APP_STARTUP" &&
                 testPermission("geo") === Services.perms.ALLOW_ACTION) {
        Services.perms.removeFromPrincipal(principal, "geo");
      }
    }
  }

  startup() {
    this.startupPromise = this._startup();

    return this.startupPromise;
  }

  async _startup() {
    if (shutdownPromises.has(this.id)) {
      await shutdownPromises.get(this.id);
    }

    // Create a temporary policy object for the devtools and add-on
    // manager callers that depend on it being available early.
    this.policy = new WebExtensionPolicy({
      id: this.id,
      mozExtensionHostname: this.uuid,
      baseURL: this.baseURI.spec,
      allowedOrigins: new MatchPatternSet([]),
      localizeCallback() {},
    });
    if (!WebExtensionPolicy.getByID(this.id)) {
      // The add-on manager doesn't handle async startup and shutdown,
      // so during upgrades and add-on restarts, startup() gets called
      // before the last shutdown has completed, and this fails when
      // there's another active add-on with the same ID.
      this.policy.active = true;
    }

    TelemetryStopwatch.start("WEBEXT_EXTENSION_STARTUP_MS", this);
    try {
      let [perms] = await Promise.all([
        ExtensionPermissions.get(this),
        this.loadManifest(),
      ]);

      if (!this.hasShutdown) {
        await this.initLocale();
      }

      if (this.errors.length) {
        return Promise.reject({errors: this.errors});
      }

      if (this.hasShutdown) {
        return;
      }

      GlobalManager.init(this);

      // Apply optional permissions
      for (let perm of perms.permissions) {
        this.permissions.add(perm);
      }
      if (perms.origins.length > 0) {
        let patterns = this.whiteListedHosts.patterns.map(host => host.pattern);

        this.whiteListedHosts = new MatchPatternSet([...patterns, ...perms.origins],
                                                    {ignorePath: true});
      }

      this.policy.active = false;
      this.policy = processScript.initExtension(this);

      this.updatePermissions(this.startupReason);

      // The "startup" Management event sent on the extension instance itself
      // is emitted just before the Management "startup" event,
      // and it is used to run code that needs to be executed before
      // any of the "startup" listeners.
      this.emit("startup", this);
      Management.emit("startup", this);

      await this.runManifest(this.manifest);

      Management.emit("ready", this);
      this.emit("ready");
      TelemetryStopwatch.finish("WEBEXT_EXTENSION_STARTUP_MS", this);
    } catch (e) {
      dump(`Extension error: ${e.message || e} ${e.filename || e.fileName}:${e.lineNumber} :: ${e.stack || new Error().stack}\n`);
      Cu.reportError(e);

      if (this.policy) {
        this.policy.active = false;
      }

      this.cleanupGeneratedFile();

      throw e;
    }

    this.startupPromise = null;
  }

  cleanupGeneratedFile() {
    if (!this.cleanupFile) {
      return;
    }

    let file = this.cleanupFile;
    this.cleanupFile = null;

    Services.obs.removeObserver(this, "xpcom-shutdown");

    return this.broadcast("Extension:FlushJarCache", {path: file.path}).then(() => {
      // We can't delete this file until everyone using it has
      // closed it (because Windows is dumb). So we wait for all the
      // child processes (including the parent) to flush their JAR
      // caches. These caches may keep the file open.
      file.remove(false);
    }).catch(Cu.reportError);
  }

  async shutdown(reason) {
    let promise = this._shutdown(reason);

    let blocker = () => {
      return Promise.race([
        promise,
        new Promise(resolve => setTimeout(resolve, SHUTDOWN_BLOCKER_MAX_MS)),
      ]);
    };

    AsyncShutdown.profileChangeTeardown.addBlocker(
      `Extension Shutdown: ${this.id} (${this.manifest && this.name})`,
      blocker);

    // If we already have a shutdown promise for this extension, wait
    // for it to complete before replacing it with a new one. This can
    // sometimes happen during tests with rapid startup/shutdown cycles
    // of multiple versions.
    if (shutdownPromises.has(this.id)) {
      await shutdownPromises.get(this.id);
    }

    let cleanup = () => {
      shutdownPromises.delete(this.id);
      AsyncShutdown.profileChangeTeardown.removeBlocker(blocker);
    };
    shutdownPromises.set(this.id, promise.then(cleanup, cleanup));

    return Promise.resolve(promise);
  }

  async _shutdown(reason) {
    try {
      if (this.startupPromise) {
        await this.startupPromise;
      }
    } catch (e) {
      Cu.reportError(e);
    }

    this.shutdownReason = reason;
    this.hasShutdown = true;

    if (!this.policy) {
      return;
    }

    if (this.rootURI instanceof Ci.nsIJARURI) {
      let file = this.rootURI.JARFile.QueryInterface(Ci.nsIFileURL).file;
      Services.ppmm.broadcastAsyncMessage("Extension:FlushJarCache", {path: file.path});
    }

    if (this.cleanupFile ||
        ["ADDON_INSTALL", "ADDON_UNINSTALL", "ADDON_UPGRADE", "ADDON_DOWNGRADE"].includes(reason)) {
      StartupCache.clearAddonData(this.id);
    }

    let data = Services.ppmm.initialProcessData;
    data["Extension:Extensions"] = data["Extension:Extensions"].filter(e => e.id !== this.id);

    Services.ppmm.removeMessageListener(this.MESSAGE_EMIT_EVENT, this);

    this.updatePermissions(this.shutdownReason);

    if (!this.manifest) {
      this.policy.active = false;

      return this.cleanupGeneratedFile();
    }

    GlobalManager.uninit(this);

    for (let obj of this.onShutdown) {
      obj.close();
    }

    for (let api of this.apis) {
      api.destroy();
    }

    ParentAPIManager.shutdownExtension(this.id);

    Management.emit("shutdown", this);
    this.emit("shutdown");

    await this.broadcast("Extension:Shutdown", {id: this.id});

    MessageChannel.abortResponses({extensionId: this.id});

    this.policy.active = false;

    return this.cleanupGeneratedFile();
  }

  observe(subject, topic, data) {
    if (topic === "xpcom-shutdown") {
      this.cleanupGeneratedFile();
    }
  }

  hasPermission(perm, includeOptional = false) {
    let manifest_ = "manifest:";
    if (perm.startsWith(manifest_)) {
      return this.manifest[perm.substr(manifest_.length)] != null;
    }

    if (this.permissions.has(perm)) {
      return true;
    }

    if (includeOptional && this.manifest.optional_permissions.includes(perm)) {
      return true;
    }

    return false;
  }

  get name() {
    return this.manifest.name;
  }

  get optionalOrigins() {
    if (this._optionalOrigins == null) {
      let origins = this.manifest.optional_permissions.filter(perm => classifyPermission(perm).origin);
      this._optionalOrigins = new MatchPatternSet(origins, {ignorePath: true});
    }
    return this._optionalOrigins;
  }
};
