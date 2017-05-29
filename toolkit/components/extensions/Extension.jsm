/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Extension", "ExtensionData"];

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

/* globals processCount */

XPCOMUtils.defineLazyPreferenceGetter(this, "processCount", "dom.ipc.processCount.extension");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionAPIs",
                                  "resource://gre/modules/ExtensionAPI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionCommon",
                                  "resource://gre/modules/ExtensionCommon.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionPermissions",
                                  "resource://gre/modules/ExtensionPermissions.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionStorage",
                                  "resource://gre/modules/ExtensionStorage.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionTestCommon",
                                  "resource://testing-common/ExtensionTestCommon.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Locale",
                                  "resource://gre/modules/Locale.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Log",
                                  "resource://gre/modules/Log.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MatchGlobs",
                                  "resource://gre/modules/MatchPattern.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MatchPattern",
                                  "resource://gre/modules/MatchPattern.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MessageChannel",
                                  "resource://gre/modules/MessageChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "require",
                                  "resource://devtools/shared/Loader.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");

Cu.import("resource://gre/modules/ExtensionManagement.jsm");
Cu.import("resource://gre/modules/ExtensionParent.jsm");
Cu.import("resource://gre/modules/ExtensionUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "uuidGen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

var {
  GlobalManager,
  ParentAPIManager,
  apiManager: Management,
} = ExtensionParent;

const {
  classifyPermission,
  EventEmitter,
  StartupCache,
  getUniqueId,
} = ExtensionUtils;

XPCOMUtils.defineLazyGetter(this, "console", ExtensionUtils.getConsole);

XPCOMUtils.defineLazyGetter(this, "LocaleData", () => ExtensionCommon.LocaleData);


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
    let pref = Preferences.get(UUID_MAP_PREF, "{}");
    try {
      return JSON.parse(pref);
    } catch (e) {
      Cu.reportError(`Error parsing ${UUID_MAP_PREF}.`);
      return {};
    }
  },

  _write(map) {
    Preferences.set(UUID_MAP_PREF, JSON.stringify(map));
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
      XPCOMUtils.defineLazyPreferenceGetter(this, "leaveStorage", LEAVE_STORAGE_PREF, false);
      XPCOMUtils.defineLazyPreferenceGetter(this, "leaveUuid", LEAVE_UUID_PREF, false);
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

    if (!this.leaveStorage) {
      // Clear browser.local.storage
      ExtensionStorage.clear(addon.id);

      // Clear any IndexedDB storage created by the extension
      let baseURI = NetUtil.newURI(`moz-extension://${uuid}/`);
      let principal = Services.scriptSecurityManager.createCodebasePrincipal(
        baseURI, {});
      Services.qms.clearStoragesForPrincipal(principal);

      // Clear localStorage created by the extension
      let storage = Services.domStorageManager.getStorage(null, principal);
      if (storage) {
        storage.clear();
      }
    }

    if (!this.leaveUuid) {
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

    this.manifest = null;
    this.id = null;
    this.uuid = null;
    this.localeData = null;
    this._promiseLocales = null;

    this.apiNames = new Set();
    this.dependencies = new Set();
    this.permissions = new Set();

    this.errors = [];
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

  // Report an error about the extension's general packaging.
  packagingError(message) {
    this.errors.push(message);
    this.logger.error(`Loading extension '${this.id}': ${message}`);
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
      let uri = NetUtil.newURI(this.rootURI.resolve("./" + path));
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
      }
      iter.close();

      return results;
    }

    // FIXME: We need a way to do this without main thread IO.

    let uri = this.rootURI.QueryInterface(Ci.nsIJARURI);

    let file = uri.JARFile.QueryInterface(Ci.nsIFileURL).file;
    let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(Ci.nsIZipReader);
    zipReader.open(file);
    try {
      let results = [];

      // Normalize the directory path.
      path = `${uri.JAREntry}/${path}`;
      path = path.replace(/\/\/+/g, "/").replace(/^\/|\/$/g, "") + "/";

      // Escape pattern metacharacters.
      let pattern = path.replace(/[[\]()?*~|$\\]/g, "\\$&");

      let enumerator = zipReader.findEntries(pattern + "*");
      while (enumerator.hasMore()) {
        let name = enumerator.getNext();
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
    } finally {
      zipReader.close();
    }
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
      origins: this.whiteListedHosts.pat,
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
          this.logger.warn(`Loading extension '${this.id}': Reading manifest: ${error}`);
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
      } else {
        return normalized.value;
      }
    });
  }

  // Reads the extension's |manifest.json| file, and stores its
  // parsed contents in |this.manifest|.
  async loadManifest() {
    [this.manifest] = await Promise.all([
      this.parseManifest(),
      Management.lazyInit(),
    ]);

    if (!this.manifest) {
      return;
    }

    try {
      // Do not override the add-on id that has been already assigned.
      if (!this.id && this.manifest.applications.gecko.id) {
        this.id = this.manifest.applications.gecko.id;
      }
    } catch (e) {
      // Errors are handled by the type checks above.
    }

    let whitelist = [];
    for (let perm of this.manifest.permissions) {
      if (perm == "contextualIdentities" && !Preferences.get("privacy.userContext.enabled")) {
        continue;
      }

      if (perm === "geckoProfiler") {
        const acceptedExtensions = Preferences.get("extensions.geckoProfiler.acceptedExtensionIds");
        if (!acceptedExtensions.split(",").includes(this.id)) {
          this.manifestError("Only whitelisted extensions are allowed to access the geckoProfiler.");
          continue;
        }
      }

      this.permissions.add(perm);
      let type = classifyPermission(perm);
      if (type.origin) {
        whitelist.push(perm);
      } else if (type.api) {
        this.apiNames.add(type.api);
      }
    }
    this.whiteListedHosts = new MatchPattern(whitelist);

    for (let api of this.apiNames) {
      this.dependencies.add(`${api}@experiments.addons.mozilla.org`);
    }

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

  // Reads the list of locales available in the extension, and returns a
  // Promise which resolves to a Map upon completion.
  // Each map key is a Gecko-compatible locale code, and each value is the
  // "_locales" subdirectory containing that locale:
  //
  // Map(gecko-locale-code -> locale-directory-name)
  promiseLocales() {
    if (!this._promiseLocales) {
      this._promiseLocales = (async () => {
        let locales = new Map();

        let entries = await this.readDirectory("_locales");
        for (let file of entries) {
          if (file.isDir) {
            let locale = this.normalizeLocaleCode(file.name);
            locales.set(locale, file.name);
          }
        }

        this.localeData = new LocaleData({
          defaultLocale: this.defaultLocale,
          locales,
          builtinMessages: this.builtinMessages,
        });

        return locales;
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

    this.remote = ExtensionManagement.useRemoteWebExtensions;

    if (this.remote && processCount !== 1) {
      throw new Error("Out-of-process WebExtensions are not supported with multiple child processes");
    }
    // This is filled in the first time an extension child is created.
    this.parentMessageManager = null;

    this.id = addonData.id;
    this.baseURI = NetUtil.newURI(this.getURL("")).QueryInterface(Ci.nsIURL);
    this.principal = this.createPrincipal();

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
        this.whiteListedHosts = new MatchPattern(this.whiteListedHosts.pat.concat(...permissions.origins));
      }
    });

    this.on("remove-permissions", (ignoreEvent, permissions) => {
      for (let perm of permissions.permissions) {
        this.permissions.delete(perm);
      }

      for (let origin of permissions.origins) {
        this.whiteListedHosts.removeOne(origin);
      }
    });
    /* eslint-enable mozilla/balanced-listeners */
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

  readLocaleFile(locale) {
    return StartupCache.locales.get([this.id, locale],
                                    () => super.readLocaleFile(locale))
      .then(result => {
        this.localeData.messages.set(locale, result);
      });
  }

  parseManifest() {
    return StartupCache.manifests.get([this.id, Locale.getLocale()],
                                      () => super.parseManifest());
  }

  loadManifest() {
    return super.loadManifest().then(manifest => {
      if (this.errors.length) {
        return Promise.reject({errors: this.errors});
      }

      if (AppConstants.RELEASE_OR_BETA) {
        return manifest;
      }

      // Load Experiments APIs that this extension depends on.
      return Promise.all(
        Array.from(this.apiNames, api => ExtensionAPIs.load(api))
      ).then(apis => {
        for (let API of apis) {
          this.apis.push(new API(this));
        }

        return manifest;
      });
    });
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
      resourceURL: this.addonData.resourceURI.spec,
      baseURL: this.baseURI.spec,
      content_scripts: this.manifest.content_scripts || [],  // eslint-disable-line camelcase
      webAccessibleResources: this.webAccessibleResources.serialize(),
      whiteListedHosts: this.whiteListedHosts.serialize(),
      localeData: this.localeData.serialize(),
      permissions: this.permissions,
      principal: this.principal,
      optionalPermissions: this.manifest.optional_permissions,
    };
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
      ppmm.addMessageListener(msg + "Complete", listener);
      Services.obs.addObserver(observer, "message-manager-close");
      Services.obs.addObserver(observer, "message-manager-disconnect");

      ppmm.broadcastAsyncMessage(msg, data);
    });
  }

  runManifest(manifest) {
    // Strip leading slashes from web_accessible_resources.
    let strippedWebAccessibleResources = [];
    if (manifest.web_accessible_resources) {
      strippedWebAccessibleResources = manifest.web_accessible_resources.map(path => path.replace(/^\/+/, ""));
    }

    this.webAccessibleResources = new MatchGlobs(strippedWebAccessibleResources);

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

      let localeList = Array.from(locales.keys(), locale => {
        return {name: locale, locales: [locale]};
      });

      let match = Locale.findClosestLocale(localeList);
      locale = match ? match.name : this.defaultLocale;
    }

    return super.initLocale(locale);
  }

  startup() {
    this.startupPromise = this._startup();
    return this.startupPromise;
  }

  async _startup() {
    this.started = false;

    try {
      let [, perms] = await Promise.all([this.loadManifest(), ExtensionPermissions.get(this)]);

      ExtensionManagement.startupExtension(this.uuid, this.addonData.resourceURI, this);
      this.started = true;

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
        this.whiteListedHosts = new MatchPattern(this.whiteListedHosts.pat.concat(...perms.origins));
      }

      // The "startup" Management event sent on the extension instance itself
      // is emitted just before the Management "startup" event,
      // and it is used to run code that needs to be executed before
      // any of the "startup" listeners.
      this.emit("startup", this);
      Management.emit("startup", this);

      await this.runManifest(this.manifest);

      Management.emit("ready", this);
      this.emit("ready");
    } catch (e) {
      dump(`Extension error: ${e.message} ${e.filename || e.fileName}:${e.lineNumber} :: ${e.stack || new Error().stack}\n`);
      Cu.reportError(e);

      if (this.started) {
        this.started = false;
        ExtensionManagement.shutdownExtension(this.uuid);
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
    try {
      if (this.startupPromise) {
        await this.startupPromise;
      }
    } catch (e) {
      Cu.reportError(e);
    }

    this.shutdownReason = reason;
    this.hasShutdown = true;

    if (!this.started) {
      return;
    }

    if (this.cleanupFile ||
        ["ADDON_INSTALL", "ADDON_UNINSTALL", "ADDON_UPGRADE", "ADDON_DOWNGRADE"].includes(reason)) {
      StartupCache.clearAddonData(this.id);
    }

    let data = Services.ppmm.initialProcessData;
    data["Extension:Extensions"] = data["Extension:Extensions"].filter(e => e.id !== this.id);

    Services.ppmm.removeMessageListener(this.MESSAGE_EMIT_EVENT, this);

    if (!this.manifest) {
      ExtensionManagement.shutdownExtension(this.uuid);

      this.cleanupGeneratedFile();
      return;
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

    Services.ppmm.broadcastAsyncMessage("Extension:Shutdown", {id: this.id});

    if (this.rootURI.QueryInterface(Ci.nsIJARURI)) {
      let file = this.rootURI.JARFile.QueryInterface(Ci.nsIFileURL).file;
      Services.ppmm.broadcastAsyncMessage("Extension:FlushJarCache", {path: file.path});
    }

    MessageChannel.abortResponses({extensionId: this.id});

    ExtensionManagement.shutdownExtension(this.uuid);

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
      this._optionalOrigins = new MatchPattern(origins);
    }
    return this._optionalOrigins;
  }
};
