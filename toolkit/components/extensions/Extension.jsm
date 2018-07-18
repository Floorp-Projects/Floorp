/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["Dictionary", "Extension", "ExtensionData", "Langpack"];

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

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.jsm",
  AddonSettings: "resource://gre/modules/addons/AddonSettings.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  ContextualIdentityService: "resource://gre/modules/ContextualIdentityService.jsm",
  ExtensionPermissions: "resource://gre/modules/ExtensionPermissions.jsm",
  ExtensionStorage: "resource://gre/modules/ExtensionStorage.jsm",
  ExtensionTestCommon: "resource://testing-common/ExtensionTestCommon.jsm",
  FileSource: "resource://gre/modules/L10nRegistry.jsm",
  L10nRegistry: "resource://gre/modules/L10nRegistry.jsm",
  Log: "resource://gre/modules/Log.jsm",
  MessageChannel: "resource://gre/modules/MessageChannel.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  PluralForm: "resource://gre/modules/PluralForm.jsm",
  Schemas: "resource://gre/modules/Schemas.jsm",
  TelemetryStopwatch: "resource://gre/modules/TelemetryStopwatch.jsm",
  XPIProvider: "resource://gre/modules/addons/XPIProvider.jsm",
});

XPCOMUtils.defineLazyGetter(
  this, "processScript",
  () => Cc["@mozilla.org/webextensions/extension-process-script;1"]
          .getService().wrappedJSObject);

// This is used for manipulating jar entry paths, which always use Unix
// separators.
XPCOMUtils.defineLazyGetter(
  this, "OSPath", () => {
    let obj = {};
    ChromeUtils.import("resource://gre/modules/osfile/ospath_unix.jsm", obj);
    return obj;
  });

XPCOMUtils.defineLazyGetter(
  this, "resourceProtocol",
  () => Services.io.getProtocolHandler("resource")
          .QueryInterface(Ci.nsIResProtocolHandler));

ChromeUtils.import("resource://gre/modules/ExtensionCommon.jsm");
ChromeUtils.import("resource://gre/modules/ExtensionParent.jsm");
ChromeUtils.import("resource://gre/modules/ExtensionUtils.jsm");

XPCOMUtils.defineLazyServiceGetters(this, {
  aomStartup: ["@mozilla.org/addons/addon-manager-startup;1", "amIAddonManagerStartup"],
  spellCheck: ["@mozilla.org/spellchecker/engine;1", "mozISpellCheckingEngine"],
  uuidGen: ["@mozilla.org/uuid-generator;1", "nsIUUIDGenerator"],
});

XPCOMUtils.defineLazyPreferenceGetter(this, "processCount", "dom.ipc.processCount.extension");

var {
  GlobalManager,
  ParentAPIManager,
  StartupCache,
  apiManager: Management,
} = ExtensionParent;

const {
  getUniqueId,
  promiseTimeout,
} = ExtensionUtils;

const {
  EventEmitter,
} = ExtensionCommon;

XPCOMUtils.defineLazyGetter(this, "console", ExtensionCommon.getConsole);

XPCOMUtils.defineLazyGetter(this, "LocaleData", () => ExtensionCommon.LocaleData);

const {sharedData} = Services.ppmm;

// The userContextID reserved for the extension storage (its purpose is ensuring that the IndexedDB
// storage used by the browser.storage.local API is not directly accessible from the extension code).
XPCOMUtils.defineLazyGetter(this, "WEBEXT_STORAGE_USER_CONTEXT_ID", () => {
  return ContextualIdentityService.getDefaultPrivateIdentity(
    "userContextIdInternal.webextStorageLocal").userContextId;
});

// The maximum time to wait for extension child shutdown blockers to complete.
const CHILD_SHUTDOWN_TIMEOUT_MS = 8000;

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
const IDB_MIGRATED_PREF_BRANCH = "extensions.webextensions.ExtensionStorageIDB.migrated";

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
        ExtensionStorage.clear(addon.id, {shouldNotifyListeners: false}));

      // Clear any IndexedDB storage created by the extension
      let baseURI = Services.io.newURI(`moz-extension://${uuid}/`);
      let principal = Services.scriptSecurityManager.createCodebasePrincipal(
        baseURI, {});
      Services.qms.clearStoragesForPrincipal(principal);

      // Clear any storage.local data stored in the IDBBackend.
      let storagePrincipal = Services.scriptSecurityManager.createCodebasePrincipal(baseURI, {
        userContextId: WEBEXT_STORAGE_USER_CONTEXT_ID,
      });
      Services.qms.clearStoragesForPrincipal(storagePrincipal);

      // Clear the preference set for the extensions migrated to the IDBBackend.
      Services.prefs.clearUserPref(`${IDB_MIGRATED_PREF_BRANCH}.${addon.id}`);

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
      throw new Error("getURL may not be called before an `id` or `uuid` has been set");
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
    let file = uri.JARFile.QueryInterface(Ci.nsIFileURL).file;

    // Normalize the directory path.
    path = `${uri.JAREntry}/${path}`;
    path = path.replace(/\/\/+/g, "/").replace(/^\/|\/$/g, "") + "/";
    if (path === "/") {
      path = "";
    }

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

    let permissions = new Set();
    let origins = new Set();
    for (let perm of this.manifest.permissions || []) {
      let type = classifyPermission(perm);
      if (type.origin) {
        origins.add(perm);
      } else if (!type.api) {
        permissions.add(perm);
      }
    }

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
      origins: this.whiteListedHosts.patterns.map(matcher => matcher.pattern),
      apis: [...this.apiNames],
    };

    const EXP_PATTERN = /^experiments\.\w+/;
    result.permissions = [...this.permissions]
      .filter(p => !result.origins.includes(p) && !EXP_PATTERN.test(p));
    return result;
  }

  // Compute the difference between two sets of permissions, suitable
  // for presenting to the user.
  static comparePermissions(oldPermissions, newPermissions) {
    let oldMatcher = new MatchPatternSet(oldPermissions.origins);
    return {
      origins: newPermissions.origins.filter(perm => !oldMatcher.subsumes(new MatchPattern(perm))),
      permissions: newPermissions.permissions.filter(perm => !oldPermissions.permissions.includes(perm)),
    };
  }

  canUseExperiment(manifest) {
    return this.experimentsAllowed && manifest.experiment_apis;
  }

  // eslint-disable-next-line complexity
  async parseManifest() {
    let [manifest] = await Promise.all([
      this.readJSON("manifest.json"),
      Management.lazyInit(),
    ]);

    this.manifest = manifest;
    this.rawManifest = manifest;

    if (manifest && manifest.default_locale) {
      await this.initLocale();
    }

    let context = {
      url: this.baseURI && this.baseURI.spec,

      principal: this.principal,

      logError: error => {
        this.manifestWarning(error);
      },

      preprocessors: {},
    };

    let manifestType = "manifest.WebExtensionManifest";
    if (this.manifest.theme) {
      this.type = "theme";
      manifestType = "manifest.ThemeManifest";
    } else if (this.manifest.langpack_id) {
      this.type = "langpack";
      manifestType = "manifest.WebExtensionLangpackManifest";
    } else if (this.manifest.dictionaries) {
      this.type = "dictionary";
      manifestType = "manifest.WebExtensionDictionaryManifest";
    } else {
      this.type = "extension";
    }

    if (this.localeData) {
      context.preprocessors.localize = (value, context) => this.localize(value);
    }

    let normalized = Schemas.normalize(this.manifest, manifestType, context);
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
      let restrictSchemes = !(this.isPrivileged && manifest.permissions.includes("mozillaAddons"));

      for (let perm of manifest.permissions) {
        if (perm === "geckoProfiler" && !this.isPrivileged) {
          const acceptedExtensions = Services.prefs.getStringPref("extensions.geckoProfiler.acceptedExtensionIds", "");
          if (!acceptedExtensions.split(",").includes(id)) {
            this.manifestError("Only whitelisted extensions are allowed to access the geckoProfiler.");
            continue;
          }
        }

        let type = classifyPermission(perm);
        if (type.origin) {
          try {
            let matcher = new MatchPattern(perm, {restrictSchemes, ignorePath: true});

            perm = matcher.pattern;
            originPermissions.add(perm);
          } catch (e) {
            this.manifestWarning(`Invalid host permission: ${perm}`);
            continue;
          }
        } else if (type.api) {
          apiNames.add(type.api);
        }

        permissions.add(perm);
      }

      if (this.id) {
        // An extension always gets permission to its own url.
        let matcher = new MatchPattern(this.getURL(), {ignorePath: true});
        originPermissions.add(matcher.pattern);

        // Apply optional permissions
        let perms = await ExtensionPermissions.get(this);
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

      if (this.canUseExperiment(manifest)) {
        let parentModules = {};
        let childModules = {};

        for (let [name, data] of Object.entries(manifest.experiment_apis)) {
          let schema = this.getURL(data.schema);

          if (!schemaPromises.has(schema)) {
            schemaPromises.set(schema, this.readJSON(data.schema).then(json => Schemas.processSchema(json)));
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
        webAccessibleResources.push(...manifest.web_accessible_resources
          .map(path => path.replace(/^\/*/, "/")));
      }
    } else if (this.type == "langpack") {
      // Langpack startup is performance critical, so we want to compute as much
      // as possible here to make startup not trigger async DB reads.
      // We'll store the four items below in the startupData.

      // 1. Compute the chrome resources to be registered for this langpack.
      const platform = AppConstants.platform;
      const chromeEntries = [];
      for (const [language, entry] of Object.entries(manifest.languages)) {
        for (const [alias, path] of Object.entries(entry.chrome_resources || {})) {
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
      const langpackId =
        `langpack-${manifest.langpack_id}-${productCodeName}`;


      // 3. Compute L10nRegistry sources for this langpack.
      const l10nRegistrySources = {};

      // Check if there's a root directory `/localization` in the langpack.
      // If there is one, add it with the name `toolkit` as a FileSource.
      const entries = await this.readDirectory("localization");
      if (entries.length > 0) {
        l10nRegistrySources.toolkit = "";
      }

      // Add any additional sources listed in the manifest
      if (manifest.sources) {
        for (const [sourceName, {base_path}] of Object.entries(manifest.sources)) {
          l10nRegistrySources[sourceName] = base_path;
        }
      }

      // 4. Save the list of languages handled by this langpack.
      const languages = Object.keys(manifest.languages);


      this.startupData = {chromeEntries, langpackId, l10nRegistrySources, languages};
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

        let entries = Array.from(await this.readDirectory(dir), entry => entry.name);
        if (!entries.includes(leafName)) {
          this.manifestError(`Invalid dictionary path specified for '${lang}': ${path}`);
        }
        if (!entries.includes(affixPath)) {
          this.manifestError(`Invalid dictionary path specified for '${lang}': Missing affix file: ${path}`);
        }

        dictionaries[lang] = path;
      }

      this.startupData = {dictionaries};
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
    this.dependencies = manifestData.dependencies;
    this.permissions = manifestData.permissions;
    this.schemaURLs = manifestData.schemaURLs;
    this.type = manifestData.type;

    this.modules = manifestData.modules;

    this.apiManager = this.getAPIManager();
    await this.apiManager.lazyInit();

    this.webAccessibleResources = manifestData.webAccessibleResources.map(res => new MatchGlob(res));
    this.whiteListedHosts = new MatchPatternSet(manifestData.originPermissions, {restrictSchemes: !this.hasPermission("mozillaAddons")});

    return this.manifest;
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

  getAPIManager() {
    let apiManagers = [Management];

    for (let id of this.dependencies) {
      let policy = WebExtensionPolicy.getByID(id);
      if (policy) {
        apiManagers.push(policy.extension.experimentAPIManager);
      }
    }

    if (this.modules) {
      this.experimentAPIManager =
        new ExtensionCommon.LazyAPIManager("main", this.modules.parent, this.schemaURLs);

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
   *
   * @returns {object} An object with properties containing localized strings
   *                   for various elements of a permission dialog. The "header"
   *                   property on this object is the notification header text
   *                   and it has the string "<>" as a placeholder for the
   *                   addon name.
   */
  static formatPermissionStrings(info, bundle) {
    let result = {};

    let perms = info.permissions || {origins: [], permissions: []};

    // First classify our host permissions
    let allUrls = false, wildcards = new Set(), sites = new Set();
    for (let permission of perms.origins) {
      if (permission == "<all_urls>") {
        allUrls = true;
        break;
      }
      if (permission.startsWith("moz-extension:")) {
        continue;
      }
      let match = /^[a-z*]+:\/\/([^/]+)\//.exec(permission);
      if (!match) {
        Cu.reportError(`Unparseable host permission ${permission}`);
        continue;
      }
      if (match[1] == "*") {
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
      result.msgs.push(bundle.GetStringFromName("webextPerms.hostDescription.allUrls"));
    } else {
      // Formats a list of host permissions.  If we have 4 or fewer, display
      // them all, otherwise display the first 3 followed by an item that
      // says "...plus N others"
      let format = (list, itemKey, moreKey) => {
        function formatItems(items) {
          result.msgs.push(...items.map(item => bundle.formatStringFromName(itemKey, [item], 1)));
        }
        if (list.length < 5) {
          formatItems(list);
        } else {
          formatItems(list.slice(0, 3));

          let remaining = list.length - 3;
          result.msgs.push(PluralForm.get(remaining, bundle.GetStringFromName(moreKey))
                                     .replace("#1", remaining));
        }
      };

      format(Array.from(wildcards), "webextPerms.hostDescription.wildcard",
             "webextPerms.hostDescription.tooManyWildcards");
      format(Array.from(sites), "webextPerms.hostDescription.oneSite",
             "webextPerms.hostDescription.tooManySites");
    }

    let permissionKey = perm => `webextPerms.description.${perm}`;

    // Next, show the native messaging permission if it is present.
    const NATIVE_MSG_PERM = "nativeMessaging";
    if (perms.permissions.includes(NATIVE_MSG_PERM)) {
      result.msgs.push(bundle.formatStringFromName(permissionKey(NATIVE_MSG_PERM), [info.appName], 1));
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

    const haveAccessKeys = (AppConstants.platform !== "android");

    result.header = bundle.formatStringFromName("webextPerms.header", ["<>"], 1);
    result.text = info.unsigned ?
                  bundle.GetStringFromName("webextPerms.unsignedWarning") : "";
    result.listIntro = bundle.GetStringFromName("webextPerms.listIntro");

    result.acceptText = bundle.GetStringFromName("webextPerms.add.label");
    result.cancelText = bundle.GetStringFromName("webextPerms.cancel.label");
    if (haveAccessKeys) {
      result.acceptKey = bundle.GetStringFromName("webextPerms.add.accessKey");
      result.cancelKey = bundle.GetStringFromName("webextPerms.cancel.accessKey");
    }

    if (info.type == "sideload") {
      result.header = bundle.formatStringFromName("webextPerms.sideloadHeader", ["<>"], 1);
      let key = result.msgs.length == 0 ?
                "webextPerms.sideloadTextNoPerms" : "webextPerms.sideloadText2";
      result.text = bundle.GetStringFromName(key);
      result.acceptText = bundle.GetStringFromName("webextPerms.sideloadEnable.label");
      result.cancelText = bundle.GetStringFromName("webextPerms.sideloadCancel.label");
      if (haveAccessKeys) {
        result.acceptKey = bundle.GetStringFromName("webextPerms.sideloadEnable.accessKey");
        result.cancelKey = bundle.GetStringFromName("webextPerms.sideloadCancel.accessKey");
      }
    } else if (info.type == "update") {
      result.header = bundle.formatStringFromName("webextPerms.updateText", ["<>"], 1);
      result.text = "";
      result.acceptText = bundle.GetStringFromName("webextPerms.updateAccept.label");
      if (haveAccessKeys) {
        result.acceptKey = bundle.GetStringFromName("webextPerms.updateAccept.accessKey");
      }
    } else if (info.type == "optional") {
      result.header = bundle.formatStringFromName("webextPerms.optionalPermsHeader", ["<>"], 1);
      result.text = "";
      result.listIntro = bundle.GetStringFromName("webextPerms.optionalPermsListIntro");
      result.acceptText = bundle.GetStringFromName("webextPerms.optionalPermsAllow.label");
      result.cancelText = bundle.GetStringFromName("webextPerms.optionalPermsDeny.label");
      if (haveAccessKeys) {
        result.acceptKey = bundle.GetStringFromName("webextPerms.optionalPermsAllow.accessKey");
        result.cancelKey = bundle.GetStringFromName("webextPerms.optionalPermsDeny.accessKey");
      }
    }

    return result;
  }
}

const PROXIED_EVENTS = new Set(["test-harness-message", "add-permissions", "remove-permissions"]);

class BootstrapScope {
  install(data, reason) {}
  uninstall(data, reason) {
    AsyncShutdown.profileChangeTeardown.addBlocker(
      `Uninstalling add-on: ${data.id}`,
      Management.emit("uninstall", {id: data.id}).then(() => {
        Management.emit("uninstall-complete", {id: data.id});
      }));
  }

  update(data, reason) {
    Management.emit("update", {id: data.id, resourceURI: data.resourceURI});
  }

  startup(data, reason) {
    // eslint-disable-next-line no-use-before-define
    this.extension = new Extension(data, this.BOOTSTRAP_REASON_TO_STRING_MAP[reason]);
    return this.extension.startup();
  }

  shutdown(data, reason) {
    let result = this.extension.shutdown(this.BOOTSTRAP_REASON_TO_STRING_MAP[reason]);
    this.extension = null;
    return result;
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

/**
 * This class is the main representation of an active WebExtension
 * in the main process.
 * @extends ExtensionData
 */
class Extension extends ExtensionData {
  constructor(addonData, startupReason) {
    super(addonData.resourceURI);

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

    if (this.remote && processCount !== 1) {
      throw new Error("Out-of-process WebExtensions are not supported with multiple child processes");
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

    /* eslint-disable mozilla/balanced-listeners */
    this.on("add-permissions", (ignoreEvent, permissions) => {
      for (let perm of permissions.permissions) {
        this.permissions.add(perm);
      }

      if (permissions.origins.length > 0) {
        let patterns = this.whiteListedHosts.patterns.map(host => host.pattern);

        this.whiteListedHosts = new MatchPatternSet(new Set([...patterns, ...permissions.origins]),
                                                    {restrictSchemes: !this.hasPermission("mozillaAddons"), ignorePath: true});
      }

      this.policy.permissions = Array.from(this.permissions);
      this.policy.allowedOrigins = this.whiteListedHosts;

      this.cachePermissions();
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

      this.cachePermissions();
    });
    /* eslint-enable mozilla/balanced-listeners */
  }

  // Some helpful properties added elsewhere:
  /**
   * An object used to map between extension-visible tab ids and
   * native Tab object
   * @property {TabManager} tabManager
   */

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

  createPrincipal(uri = this.baseURI, originAttributes = {}) {
    return Services.scriptSecurityManager.createCodebasePrincipal(uri, originAttributes);
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

  get manifestCacheKey() {
    return [this.id, this.version, Services.locale.getAppLocaleAsLangTag()];
  }

  get isPrivileged() {
    return (this.addonData.signedState === AddonManager.SIGNEDSTATE_PRIVILEGED ||
            this.addonData.signedState === AddonManager.SIGNEDSTATE_SYSTEM ||
            this.addonData.builtIn ||
            (AppConstants.MOZ_ALLOW_LEGACY_EXTENSIONS &&
             this.addonData.temporarilyInstalled));
  }

  get experimentsAllowed() {
    return (AddonSettings.ALLOW_LEGACY_EXTENSIONS ||
            this.isPrivileged);
  }

  saveStartupData() {
    if (this.dontSaveStartupData) {
      return;
    }
    XPIProvider.setStartupData(this.id, this.startupData);
  }

  async _parseManifest() {
    let manifest = await super.parseManifest();
    if (manifest && manifest.permissions.has("mozillaAddons") &&
        !this.isPrivileged) {
      Cu.reportError(`Stripping mozillaAddons permission from ${this.id}`);
      manifest.permissions.delete("mozillaAddons");
      let i = manifest.manifest.permissions.indexOf("mozillaAddons");
      if (i >= 0) {
        manifest.manifest.permissions.splice(i, 1);
      } else {
        throw new Error("Could not find mozilaAddons in original permissions array");
      }
    }
    return manifest;
  }

  parseManifest() {
    return StartupCache.manifests.get(this.manifestCacheKey, () => this._parseManifest());
  }

  async cachePermissions() {
    let manifestData = await this.parseManifest();

    manifestData.originPermissions = this.whiteListedHosts.patterns.map(pat => pat.pattern);
    manifestData.permissions = this.permissions;
    return StartupCache.manifests.set(this.manifestCacheKey, manifestData);
  }

  async loadManifest() {
    let manifest = await super.loadManifest();

    if (this.errors.length) {
      return Promise.reject({errors: this.errors});
    }

    return manifest;
  }

  get contentSecurityPolicy() {
    return this.manifest.content_security_policy;
  }

  get backgroundScripts() {
    return (this.manifest.background &&
            this.manifest.background.scripts);
  }

  // Representation of the extension to send to content
  // processes. This should include anything the content process might
  // need.
  serialize() {
    return {
      id: this.id,
      uuid: this.uuid,
      name: this.name,
      contentSecurityPolicy: this.contentSecurityPolicy,
      instanceId: this.instanceId,
      resourceURL: this.resourceURL,
      contentScripts: this.contentScripts,
      webAccessibleResources: this.webAccessibleResources.map(res => res.glob),
      whiteListedHosts: this.whiteListedHosts.patterns.map(pat => pat.pattern),
      permissions: this.permissions,
      optionalPermissions: this.manifest.optional_permissions,
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
    for (let directive in manifest) {
      if (manifest[directive] !== null) {
        promises.push(Management.emit(`manifest_${directive}`, directive, this, manifest));

        promises.push(Management.asyncEmitManifestEntry(this, directive));
      }
    }

    activeExtensionIDs.add(this.id);
    sharedData.set("extensions/activeIDs", activeExtensionIDs);

    Services.ppmm.sharedData.flush();
    return this.broadcast("Extension:Startup", this.id).then(() => {
      return Promise.all(promises);
    });
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

  async startup() {
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

    this.policy.extension = this;

    TelemetryStopwatch.start("WEBEXT_EXTENSION_STARTUP_MS", this);
    try {
      await this.loadManifest();

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

      this.initSharedData();

      this.policy.active = false;
      this.policy = processScript.initExtension(this);
      this.policy.extension = this;

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
    } catch (errors) {
      for (let e of [].concat(errors)) {
        dump(`Extension error: ${e.message || e} ${e.filename || e.fileName}:${e.lineNumber} :: ${e.stack || new Error().stack}\n`);
        Cu.reportError(e);
      }

      if (this.policy) {
        this.policy.active = false;
      }

      this.cleanupGeneratedFile();

      throw errors;
    }
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

    activeExtensionIDs.delete(this.id);
    sharedData.set("extensions/activeIDs", activeExtensionIDs);

    for (let key of this.sharedDataKeys) {
      sharedData.delete(key);
    }

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

    ParentAPIManager.shutdownExtension(this.id);

    Management.emit("shutdown", this);
    this.emit("shutdown");

    const TIMED_OUT = Symbol();

    let result = await Promise.race([
      this.broadcast("Extension:Shutdown", {id: this.id}),
      promiseTimeout(CHILD_SHUTDOWN_TIMEOUT_MS).then(() => TIMED_OUT),
    ]);
    if (result === TIMED_OUT) {
      Cu.reportError(`Timeout while waiting for extension child to shutdown: ${this.policy.debugName}`);
    }

    MessageChannel.abortResponses({extensionId: this.id});

    this.policy.active = false;

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
      let origins = this.manifest.optional_permissions.filter(perm => classifyPermission(perm).origin);
      this._optionalOrigins = new MatchPatternSet(origins, {restrictSchemes: !this.hasPermission("mozillaAddons"), ignorePath: true});
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

  static getBootstrapScope(id, file) {
    return new DictionaryBootstrapScope();
  }

  async startup(reason) {
    this.dictionaries = {};
    for (let [lang, path] of Object.entries(this.startupData.dictionaries)) {
      let uri = Services.io.newURI(path.slice(0, -4) + ".aff", null, this.rootURI);
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

  static getBootstrapScope(id, file) {
    return new LangpackBootstrapScope();
  }

  async promiseLocales(locale) {
    let locales = await StartupCache.locales
      .get([this.id, "@@all_locales"], () => this._promiseLocaleMap());

    return this._setupLocaleData(locales);
  }

  parseManifest() {
    return StartupCache.manifests.get(this.manifestCacheKey,
                                      () => super.parseManifest());
  }

  async startup(reason) {
    this.chromeRegistryHandle = null;
    if (this.startupData.chromeEntries.length > 0) {
      const manifestURI = Services.io.newURI("manifest.json", null, this.rootURI);
      this.chromeRegistryHandle =
        aomStartup.registerChrome(manifestURI, this.startupData.chromeEntries);
    }

    const langpackId = this.startupData.langpackId;
    const l10nRegistrySources = this.startupData.l10nRegistrySources;

    resourceProtocol.setSubstitution(langpackId, this.rootURI);

    for (const [sourceName, basePath] of Object.entries(l10nRegistrySources)) {
      L10nRegistry.registerSource(new FileSource(
        `${sourceName}-${langpackId}`,
        this.startupData.languages,
        `resource://${langpackId}/${basePath}localization/{locale}/`
      ));
    }

    Services.obs.notifyObservers({wrappedJSObject: {langpack: this}},
                                 "webextension-langpack-startup");
  }

  async shutdown(reason) {
    for (const sourceName of Object.keys(this.startupData.l10nRegistrySources)) {
      L10nRegistry.removeSource(`${sourceName}-${this.startupData.langpackId}`);
    }
    if (this.chromeRegistryHandle) {
      this.chromeRegistryHandle.destruct();
      this.chromeRegistryHandle = null;
    }

    resourceProtocol.setSubstitution(this.startupData.langpackId, null);
  }
}
