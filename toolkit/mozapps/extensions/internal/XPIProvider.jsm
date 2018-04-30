 /* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This file contains most of the logic required to load and run
 * extensions at startup. Anything which is not required immediately at
 * startup should go in XPIInstall.jsm or XPIDatabase.jsm if at all
 * possible, in order to minimize the impact on startup performance.
 */

/**
 * @typedef {number} integer
 */

/* eslint "valid-jsdoc": [2, {requireReturn: false, requireReturnDescription: false, prefer: {return: "returns"}}] */

var EXPORTED_SYMBOLS = ["XPIProvider", "XPIInternal"];

/* globals WebExtensionPolicy */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/AddonManager.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonRepository: "resource://gre/modules/addons/AddonRepository.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  Dictionary: "resource://gre/modules/Extension.jsm",
  Extension: "resource://gre/modules/Extension.jsm",
  Langpack: "resource://gre/modules/Extension.jsm",
  LightweightThemeManager: "resource://gre/modules/LightweightThemeManager.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  PermissionsUtils: "resource://gre/modules/PermissionsUtils.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  ConsoleAPI: "resource://gre/modules/Console.jsm",
  JSONFile: "resource://gre/modules/JSONFile.jsm",
  LegacyExtensionsUtils: "resource://gre/modules/LegacyExtensionsUtils.jsm",
  TelemetrySession: "resource://gre/modules/TelemetrySession.jsm",

  XPIDatabase: "resource://gre/modules/addons/XPIDatabase.jsm",
  XPIDatabaseReconcile: "resource://gre/modules/addons/XPIDatabase.jsm",
  XPIInstall: "resource://gre/modules/addons/XPIInstall.jsm",
  verifyBundleSignedState: "resource://gre/modules/addons/XPIInstall.jsm",
});

XPCOMUtils.defineLazyServiceGetter(this, "aomStartup",
                                   "@mozilla.org/addons/addon-manager-startup;1",
                                   "amIAddonManagerStartup");

Cu.importGlobalProperties(["URL"]);

const nsIFile = Components.Constructor("@mozilla.org/file/local;1", "nsIFile",
                                       "initWithPath");

const PREF_DB_SCHEMA                  = "extensions.databaseSchema";
const PREF_XPI_STATE                  = "extensions.xpiState";
const PREF_BOOTSTRAP_ADDONS           = "extensions.bootstrappedAddons";
const PREF_PENDING_OPERATIONS         = "extensions.pendingOperations";
const PREF_EM_ENABLED_SCOPES          = "extensions.enabledScopes";
const PREF_EM_STARTUP_SCAN_SCOPES     = "extensions.startupScanScopes";
// xpinstall.signatures.required only supported in dev builds
const PREF_XPI_SIGNATURES_REQUIRED    = "xpinstall.signatures.required";
const PREF_LANGPACK_SIGNATURES        = "extensions.langpacks.signatures.required";
const PREF_XPI_PERMISSIONS_BRANCH     = "xpinstall.";
const PREF_INSTALL_DISTRO_ADDONS      = "extensions.installDistroAddons";
const PREF_BRANCH_INSTALLED_ADDON     = "extensions.installedDistroAddon.";
const PREF_SYSTEM_ADDON_SET           = "extensions.systemAddonSet";
const PREF_ALLOW_LEGACY               = "extensions.legacy.enabled";

const PREF_EM_MIN_COMPAT_APP_VERSION      = "extensions.minCompatibleAppVersion";
const PREF_EM_MIN_COMPAT_PLATFORM_VERSION = "extensions.minCompatiblePlatformVersion";

const PREF_EM_LAST_APP_BUILD_ID       = "extensions.lastAppBuildId";

// Specify a list of valid built-in add-ons to load.
const BUILT_IN_ADDONS_URI             = "chrome://browser/content/built_in_addons.json";

const OBSOLETE_PREFERENCES = [
  "extensions.bootstrappedAddons",
  "extensions.enabledAddons",
  "extensions.xpiState",
  "extensions.installCache",
];

const URI_EXTENSION_STRINGS           = "chrome://mozapps/locale/extensions/extensions.properties";

const DIR_EXTENSIONS                  = "extensions";
const DIR_SYSTEM_ADDONS               = "features";
const DIR_STAGE                       = "staged";
const DIR_TRASH                       = "trash";

const FILE_XPI_STATES                 = "addonStartup.json.lz4";
const FILE_DATABASE                   = "extensions.json";
const FILE_RDF_MANIFEST               = "install.rdf";
const FILE_WEB_MANIFEST               = "manifest.json";
const FILE_XPI_ADDONS_LIST            = "extensions.ini";

const KEY_PROFILEDIR                  = "ProfD";
const KEY_ADDON_APP_DIR               = "XREAddonAppDir";
const KEY_APP_DISTRIBUTION            = "XREAppDist";
const KEY_APP_FEATURES                = "XREAppFeat";

const KEY_APP_PROFILE                 = "app-profile";
const KEY_APP_SYSTEM_ADDONS           = "app-system-addons";
const KEY_APP_SYSTEM_DEFAULTS         = "app-system-defaults";
const KEY_APP_GLOBAL                  = "app-global";
const KEY_APP_SYSTEM_LOCAL            = "app-system-local";
const KEY_APP_SYSTEM_SHARE            = "app-system-share";
const KEY_APP_SYSTEM_USER             = "app-system-user";
const KEY_APP_TEMPORARY               = "app-temporary";

const DEFAULT_THEME_ID = "default-theme@mozilla.org";

const TEMPORARY_ADDON_SUFFIX = "@temporary-addon";

const STARTUP_MTIME_SCOPES = [KEY_APP_GLOBAL,
                              KEY_APP_SYSTEM_LOCAL,
                              KEY_APP_SYSTEM_SHARE,
                              KEY_APP_SYSTEM_USER];

const NOTIFICATION_FLUSH_PERMISSIONS  = "flush-pending-permissions";
const XPI_PERMISSION                  = "install";

const TOOLKIT_ID                      = "toolkit@mozilla.org";

const XPI_SIGNATURE_CHECK_PERIOD      = 24 * 60 * 60;

XPCOMUtils.defineConstant(this, "DB_SCHEMA", 25);

const NOTIFICATION_TOOLBOX_CONNECTION_CHANGE      = "toolbox-connection-change";

function encoded(strings, ...values) {
  let result = [];

  for (let [i, string] of strings.entries()) {
    result.push(string);
    if (i < values.length)
      result.push(encodeURIComponent(values[i]));
  }

  return result.join("");
}

const BOOTSTRAP_REASONS = {
  APP_STARTUP: 1,
  APP_SHUTDOWN: 2,
  ADDON_ENABLE: 3,
  ADDON_DISABLE: 4,
  ADDON_INSTALL: 5,
  ADDON_UNINSTALL: 6,
  ADDON_UPGRADE: 7,
  ADDON_DOWNGRADE: 8
};

// Some add-on types that we track internally are presented as other types
// externally
const TYPE_ALIASES = {
  "webextension": "extension",
  "webextension-dictionary": "dictionary",
  "webextension-langpack": "locale",
  "webextension-theme": "theme",
};

const CHROME_TYPES = new Set([
  "extension",
]);

const SIGNED_TYPES = new Set([
  "extension",
  "webextension",
  "webextension-langpack",
  "webextension-theme",
]);

const ALL_EXTERNAL_TYPES = new Set([
  "dictionary",
  "extension",
  "locale",
  "theme",
]);

// Keep track of where we are in startup for telemetry
// event happened during XPIDatabase.startup()
const XPI_STARTING = "XPIStarting";
// event happened after startup() but before the final-ui-startup event
const XPI_BEFORE_UI_STARTUP = "BeforeFinalUIStartup";
// event happened after final-ui-startup
const XPI_AFTER_UI_STARTUP = "AfterFinalUIStartup";

var gGlobalScope = this;

/**
 * Valid IDs fit this pattern.
 */
var gIDTest = /^(\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}|[a-z0-9-\._]*\@[a-z0-9-\._]+)$/i;

ChromeUtils.import("resource://gre/modules/Log.jsm");
const LOGGER_ID = "addons.xpi";

// Create a new logger for use by all objects in this Addons XPI Provider module
// (Requires AddonManager.jsm)
var logger = Log.repository.getLogger(LOGGER_ID);

XPCOMUtils.defineLazyGetter(this, "gStartupScanScopes", () => {
  let appBuildID = Services.appinfo.appBuildID;
  let oldAppBuildID = Services.prefs.getCharPref(PREF_EM_LAST_APP_BUILD_ID, "");
  Services.prefs.setCharPref(PREF_EM_LAST_APP_BUILD_ID, appBuildID);
  if (appBuildID !== oldAppBuildID) {
    // If the build id changed, scan all scopes
    return AddonManager.SCOPE_ALL;
  }

  return Services.prefs.getIntPref(PREF_EM_STARTUP_SCAN_SCOPES, 0);
});

/**
 * Spins the event loop until the given promise resolves, and then eiter returns
 * its success value or throws its rejection value.
 *
 * @param {Promise} promise
 *        The promise to await.
 * @returns {any}
 *        The promise's resolution value, if any.
 */
function awaitPromise(promise) {
  let success = undefined;
  let result = null;

  promise.then(val => {
    success = true;
    result = val;
  }, val => {
    success = false;
    result = val;
  });

  Services.tm.spinEventLoopUntil(() => success !== undefined);

  if (!success)
    throw result;
  return result;

}

/**
 * Returns a nsIFile instance for the given path, relative to the given
 * base file, if provided.
 *
 * @param {string} path
 *        The (possibly relative) path of the file.
 * @param {nsIFile} [base]
 *        An optional file to use as a base path if `path` is relative.
 * @returns {nsIFile}
 */
function getFile(path, base = null) {
  // First try for an absolute path, as we get in the case of proxy
  // files. Ideally we would try a relative path first, but on Windows,
  // paths which begin with a drive letter are valid as relative paths,
  // and treated as such.
  try {
    return new nsIFile(path);
  } catch (e) {
    // Ignore invalid relative paths. The only other error we should see
    // here is EOM, and either way, any errors that we care about should
    // be re-thrown below.
  }

  // If the path isn't absolute, we must have a base path.
  let file = base.clone();
  file.appendRelativePath(path);
  return file;
}

/**
 * Returns the modification time of the given file, or 0 if the file
 * does not exist, or cannot be accessed.
 *
 * @param {nsIFile} file
 *        The file to retrieve the modification time for.
 * @returns {double}
 *        The file's modification time, in seconds since the Unix epoch.
 */
function tryGetMtime(file) {
  try {
    // Clone the file object so we always get the actual mtime, rather
    // than whatever value it may have cached.
    return file.clone().lastModifiedTime;
  } catch (e) {
    return 0;
  }
}

/**
 * Returns the path to `file` relative to the directory `dir`, or an
 * absolute path to the file if no directory is passed, or the file is
 * not a descendant of it.
 *
 * @param {nsIFile} file
 *        The file to return a relative path to.
 * @param {nsIFile} [dir]
 *        If passed, return a path relative to this directory.
 * @returns {string}
 */
function getRelativePath(file, dir) {
  if (dir && dir.contains(file)) {
    let path = file.getRelativePath(dir);
    if (AppConstants.platform == "win") {
      path = path.replace(/\//g, "\\");
    }
    return path;
  }
  return file.path;
}

/**
 * Converts the given opaque descriptor string into an ordinary path string.
 *
 * @param {string} descriptor
 *        The opaque descriptor string to convert.
 * @param {nsIFile} [dir]
 *        If passed, return a path relative to this directory.
 * @returns {string}
 *        The file's path.
 */
function descriptorToPath(descriptor, dir) {
  try {
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.persistentDescriptor = descriptor;
    return getRelativePath(file, dir);
  } catch (e) {
    return null;
  }
}

/**
 * Helper function that determines whether an addon of a certain type is a
 * WebExtension.
 *
 * @param {string} type
 *        The add-on type to check.
 * @returns {boolean}
 */
function isWebExtension(type) {
  return type == "webextension" || type == "webextension-theme";
}

var gThemeAliases = null;
/**
 * Helper function that determines whether an addon of a certain type is a
 * theme.
 *
 * @param {string} type
 *        The add-on type to check.
 * @returns {boolean}
 */
function isTheme(type) {
  if (!gThemeAliases)
    gThemeAliases = getAllAliasesForTypes(["theme"]);
  return gThemeAliases.includes(type);
}

/**
 * Evaluates whether an add-on is allowed to run in safe mode.
 *
 * @param {AddonInternal} aAddon
 *        The add-on to check
 * @returns {boolean}
 *        True if the add-on should run in safe mode
 */
function canRunInSafeMode(aAddon) {
  // Even though the updated system add-ons aren't generally run in safe mode we
  // include them here so their uninstall functions get called when switching
  // back to the default set.

  // TODO product should make the call about temporary add-ons running
  // in safe mode. assuming for now that they are.
  let location = aAddon._installLocation || null;
  if (!location) {
    return false;
  }

  if (location.name == KEY_APP_TEMPORARY)
    return true;

  return location.isSystem;
}

/**
 * Converts an internal add-on type to the type presented through the API.
 *
 * @param {string} aType
 *        The internal add-on type
 * @returns {string}
 *        An external add-on type
 */
function getExternalType(aType) {
  if (aType in TYPE_ALIASES)
    return TYPE_ALIASES[aType];
  return aType;
}

function getManifestFileForDir(aDir) {
  let file = getFile(FILE_RDF_MANIFEST, aDir);
  if (file.exists() && file.isFile())
    return file;
  file.leafName = FILE_WEB_MANIFEST;
  if (file.exists() && file.isFile())
    return file;
  return null;
}

/**
 * Converts a list of API types to a list of API types and any aliases for those
 * types.
 *
 * @param {Array<string>?} aTypes
 *        An array of types or null for all types
 * @returns {Array<string>?}
 *        An array of types or null for all types
 */
function getAllAliasesForTypes(aTypes) {
  if (!aTypes)
    return null;

  // Build a set of all requested types and their aliases
  let typeset = new Set(aTypes);

  for (let alias of Object.keys(TYPE_ALIASES)) {
    // Ignore any requested internal types
    typeset.delete(alias);

    // Add any alias for the internal type
    if (typeset.has(TYPE_ALIASES[alias]))
      typeset.add(alias);
  }

  return [...typeset];
}

/**
 * Gets an nsIURI for a file within another file, either a directory or an XPI
 * file. If aFile is a directory then this will return a file: URI, if it is an
 * XPI file then it will return a jar: URI.
 *
 * @param {nsIFile} aFile
 *        The file containing the resources, must be either a directory or an
 *        XPI file
 * @param {string} aPath
 *        The path to find the resource at, "/" separated. If aPath is empty
 *        then the uri to the root of the contained files will be returned
 * @returns {nsIURI}
 *        An nsIURI pointing at the resource
 */
function getURIForResourceInFile(aFile, aPath) {
  if (aFile.exists() && aFile.isDirectory()) {
    let resource = aFile.clone();
    if (aPath)
      aPath.split("/").forEach(part => resource.append(part));

    return Services.io.newFileURI(resource);
  }

  return buildJarURI(aFile, aPath);
}

/**
 * Creates a jar: URI for a file inside a ZIP file.
 *
 * @param {nsIFile} aJarfile
 *        The ZIP file as an nsIFile
 * @param {string} aPath
 *        The path inside the ZIP file
 * @returns {nsIURI}
 *        An nsIURI for the file
 */
function buildJarURI(aJarfile, aPath) {
  let uri = Services.io.newFileURI(aJarfile);
  uri = "jar:" + uri.spec + "!/" + aPath;
  return Services.io.newURI(uri);
}

/**
 * Gets a snapshot of directory entries.
 *
 * @param {nsIURI} aDir
 *        Directory to look at
 * @param {boolean} aSortEntries
 *        True to sort entries by filename
 * @returns {Array<nsIFile>}
 *        An array of nsIFile, or an empty array if aDir is not a readable directory
 */
function getDirectoryEntries(aDir, aSortEntries) {
  let dirEnum;
  try {
    dirEnum = aDir.directoryEntries.QueryInterface(Ci.nsIDirectoryEnumerator);
    let entries = [];
    while (dirEnum.hasMoreElements())
      entries.push(dirEnum.nextFile);

    if (aSortEntries) {
      entries.sort(function(a, b) {
        return a.path > b.path ? -1 : 1;
      });
    }

    return entries;
  } catch (e) {
    if (aDir.exists()) {
      logger.warn("Can't iterate directory " + aDir.path, e);
    }
    return [];
  } finally {
    if (dirEnum) {
      dirEnum.close();
    }
  }
}

/**
 * The on-disk state of an individual XPI, created from an Object
 * as stored in the addonStartup.json file.
 */
const JSON_FIELDS = Object.freeze([
  "bootstrapped",
  "changed",
  "dependencies",
  "enabled",
  "file",
  "hasEmbeddedWebExtension",
  "lastModifiedTime",
  "path",
  "runInSafeMode",
  "startupData",
  "telemetryKey",
  "type",
  "version",
]);

const BOOTSTRAPPED_FIELDS = Object.freeze([
  "dependencies",
  "hasEmbeddedWebExtension",
  "runInSafeMode",
  "type",
  "version",
]);

class XPIState {
  constructor(location, id, saved = {}) {
    this.location = location;
    this.id = id;

    // Set default values.
    this.type = "extension";
    this.bootstrapped = false;

    for (let prop of JSON_FIELDS) {
      if (prop in saved) {
        this[prop] = saved[prop];
      }
    }

    if (!this.telemetryKey) {
      this.telemetryKey = this.getTelemetryKey();
    }

    if (saved.currentModifiedTime && saved.currentModifiedTime != this.lastModifiedTime) {
      this.lastModifiedTime = saved.currentModifiedTime;
      this.changed = true;
    }
  }

  /**
   * Migrates an add-on's data from xpiState and bootstrappedAddons
   * preferences, and returns an XPIState object for it.
   *
   * @param {XPIStateLocation} location
   *        The location of the add-on.
   * @param {string} id
   *        The ID of the add-on to migrate.
   * @param {object} saved
   *        The add-on's data from the xpiState preference.
   * @param {object} [bootstrapped]
   *        The add-on's data from the bootstrappedAddons preference, if
   *        applicable.
   * @returns {XPIState}
   */
  static migrate(location, id, saved, bootstrapped) {
    let data = {
      enabled: saved.e,
      path: descriptorToPath(saved.d, location.dir),
      lastModifiedTime: saved.mt || saved.st,
      version: saved.v,
    };

    if (bootstrapped) {
      data.bootstrapped = true;
      data.enabled = true;
      data.path = descriptorToPath(bootstrapped.descriptor, location.dir);

      for (let field of BOOTSTRAPPED_FIELDS) {
        if (field in bootstrapped) {
          data[field] = bootstrapped[field];
        }
      }
    }

    return new XPIState(location, id, data);
  }

  // Compatibility shim getters for legacy callers in XPIDatabase.jsm.
  get mtime() {
    return this.lastModifiedTime;
  }
  get active() {
    return this.enabled;
  }


  /**
   * @property {string} path
   *        The full on-disk path of the add-on.
   */
  get path() {
    return this.file && this.file.path;
  }
  set path(path) {
    this.file = getFile(path, this.location.dir);
  }

  /**
   * @property {string} relativePath
   *        The path to the add-on relative to its parent location, or
   *        the full path if its parent location has no on-disk path.
   */
  get relativePath() {
    if (this.location.dir && this.location.dir.contains(this.file)) {
      let path = this.file.getRelativePath(this.location.dir);
      if (AppConstants.platform == "win") {
        path = path.replace(/\//g, "\\");
      }
      return path;
    }
    return this.path;
  }

  /**
   * Returns a JSON-compatible representation of this add-on's state
   * data, to be saved to addonStartup.json.
   *
   * @returns {Object}
   */
  toJSON() {
    let json = {
      enabled: this.enabled,
      lastModifiedTime: this.lastModifiedTime,
      path: this.relativePath,
      version: this.version,
      telemetryKey: this.telemetryKey,
    };
    if (this.type != "extension") {
      json.type = this.type;
    }
    if (this.bootstrapped) {
      json.bootstrapped = true;
      json.dependencies = this.dependencies;
      json.runInSafeMode = this.runInSafeMode;
      json.hasEmbeddedWebExtension = this.hasEmbeddedWebExtension;
    }
    if (this.startupData) {
      json.startupData = this.startupData;
    }
    return json;
  }

  /**
   * Update the last modified time for an add-on on disk.
   *
   * @param {nsIFile} aFile
   *        The location of the add-on.
   * @param {string} aId
   *        The add-on ID.
   * @returns {boolean}
   *       True if the time stamp has changed.
   */
  getModTime(aFile, aId) {
    // Modified time is the install manifest time, if any. If no manifest
    // exists, we assume this is a packed .xpi and use the time stamp of
    // {path}
    let mtime = (tryGetMtime(getManifestFileForDir(aFile)) ||
                 tryGetMtime(aFile));
    if (!mtime) {
      logger.warn("Can't get modified time of ${file}", {file: aFile.path});
    }

    this.changed = mtime != this.lastModifiedTime;
    this.lastModifiedTime = mtime;
    return this.changed;
  }

  /**
   * Returns a string key by which to identify this add-on in telemetry
   * and crash reports.
   *
   * @returns {string}
   */
  getTelemetryKey() {
    return encoded`${this.id}:${this.version}`;
  }

  /**
   * Update the XPIState to match an XPIDatabase entry; if 'enabled' is changed to true,
   * update the last-modified time. This should probably be made async, but for now we
   * don't want to maintain parallel sync and async versions of the scan.
   *
   * Caller is responsible for doing XPIStates.save() if necessary.
   *
   * @param {DBAddonInternal} aDBAddon
   *        The DBAddonInternal for this add-on.
   * @param {boolean} [aUpdated = false]
   *        The add-on was updated, so we must record new modified time.
   */
  syncWithDB(aDBAddon, aUpdated = false) {
    logger.debug("Updating XPIState for " + JSON.stringify(aDBAddon));
    // If the add-on changes from disabled to enabled, we should re-check the modified time.
    // If this is a newly found add-on, it won't have an 'enabled' field but we
    // did a full recursive scan in that case, so we don't need to do it again.
    // We don't use aDBAddon.active here because it's not updated until after restart.
    let mustGetMod = (aDBAddon.visible && !aDBAddon.disabled && !this.enabled);

    this.enabled = aDBAddon.visible && !aDBAddon.disabled;

    this.version = aDBAddon.version;
    this.type = aDBAddon.type;
    if (aDBAddon.startupData) {
      this.startupData = aDBAddon.startupData;
    }

    this.telemetryKey = this.getTelemetryKey();

    this.bootstrapped = !!aDBAddon.bootstrap;
    if (this.bootstrapped) {
      this.hasEmbeddedWebExtension = aDBAddon.hasEmbeddedWebExtension;
      this.dependencies = aDBAddon.dependencies;
      this.runInSafeMode = canRunInSafeMode(aDBAddon);
    }

    if (aUpdated || mustGetMod) {
      this.getModTime(this.file, aDBAddon.id);
      if (this.lastModifiedTime != aDBAddon.updateDate) {
        aDBAddon.updateDate = this.lastModifiedTime;
        if (XPIDatabase.initialized) {
          XPIDatabase.saveChanges();
        }
      }
    }
  }
}

/**
 * Manages the state data for add-ons in a given install location.
 *
 * @param {string} name
 *        The name of the install location (e.g., "app-profile").
 * @param {string?} path
 *        The on-disk path of the install location. May be null for some
 *        locations which do not map to a specific on-disk path.
 * @param {object} [saved = {}]
 *        The persisted JSON state data to restore.
 */
class XPIStateLocation extends Map {
  constructor(name, path, saved = {}) {
    super();

    this.name = name;
    this.path = path || saved.path || null;
    this.staged = saved.staged || {};
    this.changed = saved.changed || false;
    this.dir = this.path && new nsIFile(this.path);

    for (let [id, data] of Object.entries(saved.addons || {})) {
      let xpiState = this._addState(id, data);
      // Make a note that this state was restored from saved data.
      xpiState.wasRestored = true;
    }
  }

  /**
   * Returns a JSON-compatible representation of this location's state
   * data, to be saved to addonStartup.json.
   *
   * @returns {Object}
   */
  toJSON() {
    let json = {
      addons: {},
      staged: this.staged,
    };

    if (this.path) {
      json.path = this.path;
    }

    if (STARTUP_MTIME_SCOPES.includes(this.name)) {
      json.checkStartupModifications = true;
    }

    for (let [id, addon] of this.entries()) {
      json.addons[id] = addon;
    }
    return json;
  }

  get hasStaged() {
    for (let key in this.staged) {
      return true;
    }
    return false;
  }

  _addState(addonId, saved) {
    let xpiState = new XPIState(this, addonId, saved);
    this.set(addonId, xpiState);
    return xpiState;
  }

  /**
   * Adds state data for the given DB add-on to the DB.
   *
   * @param {DBAddon} addon
   *        The DBAddon to add.
   */
  addAddon(addon) {
    logger.debug("XPIStates adding add-on ${id} in ${location}: ${path}", addon);

    let xpiState = this._addState(addon.id, {file: addon._sourceBundle});
    xpiState.syncWithDB(addon, true);

    XPIProvider.setTelemetry(addon.id, "location", this.name);
  }

  /**
   * Adds stub state data for the local file to the DB.
   *
   * @param {string} addonId
   *        The ID of the add-on represented by the given file.
   * @param {nsIFile} file
   *        The local file or directory containing the add-on.
   * @returns {XPIState}
   */
  addFile(addonId, file) {
    let xpiState = this._addState(addonId, {enabled: false, file: file.clone()});
    xpiState.getModTime(xpiState.file, addonId);
    return xpiState;
  }

  /**
   * Adds metadata for a staged install which should be performed after
   * the next restart.
   *
   * @param {string} addonId
   *        The ID of the staged install. The leaf name of the XPI
   *        within the location's staging directory must correspond to
   *        this ID.
   * @param {object} metadata
   *        The JSON metadata of the parsed install, to be used during
   *        the next startup.
   */
  stageAddon(addonId, metadata) {
    this.staged[addonId] = metadata;
    XPIStates.save();
  }

  /**
   * Removes staged install metadata for the given add-on ID.
   *
   * @param {string} addonId
   *        The ID of the staged install.
   */
  unstageAddon(addonId) {
    if (addonId in this.staged) {
      delete this.staged[addonId];
      XPIStates.save();
    }
  }

  * getStagedAddons() {
    for (let [id, metadata] of Object.entries(this.staged)) {
      yield [id, metadata];
    }
  }

  /**
   * Migrates saved state data for the given add-on from the values
   * stored in xpiState and bootstrappedAddons preferences, and adds it to
   * the DB.
   *
   * @param {string} id
   *        The ID of the add-on to migrate.
   * @param {object} state
   *        The add-on's data from the xpiState preference.
   * @param {object} [bootstrapped]
   *        The add-on's data from the bootstrappedAddons preference, if
   *        applicable.
   */
  migrateAddon(id, state, bootstrapped) {
    this.set(id, XPIState.migrate(this, id, state, bootstrapped));
  }
}

/**
 * Keeps track of the state of XPI add-ons on the file system.
 */
var XPIStates = {
  // Map(location name -> Map(add-on ID -> XPIState))
  db: null,

  _jsonFile: null,

  /**
   * @property {Map<string, XPIState>} sideLoadedAddons
   *        A map of new add-ons detected during install location
   *        directory scans. Keys are add-on IDs, values are XPIState
   *        objects corresponding to those add-ons.
   */
  sideLoadedAddons: new Map(),

  get size() {
    let count = 0;
    if (this.db) {
      for (let location of this.db.values()) {
        count += location.size;
      }
    }
    return count;
  },

  /**
   * Migrates state data from the xpiState and bootstrappedAddons
   * preferences and adds it to the DB. Returns a JSON-compatible
   * representation of the current state of the DB.
   *
   * @returns {object}
   */
  migrateStateFromPrefs() {
    logger.info("No addonStartup.json found. Attempting to migrate data from preferences");

    let state;
    // Try to migrate state data from old storage locations.
    let bootstrappedAddons;
    try {
      state = JSON.parse(Services.prefs.getStringPref(PREF_XPI_STATE));
      bootstrappedAddons = JSON.parse(Services.prefs.getStringPref(PREF_BOOTSTRAP_ADDONS, "{}"));
    } catch (e) {
      logger.warn("Error parsing extensions.xpiState and " +
                  "extensions.bootstrappedAddons: ${error}",
                  {error: e});

    }

    for (let [locName, addons] of Object.entries(state)) {
      for (let [id, addon] of Object.entries(addons)) {
        let loc = this.getLocation(locName);
        if (loc) {
          loc.migrateAddon(id, addon, bootstrappedAddons[id] || null);
        }
      }
    }

    // Clear out old state data.
    for (let pref of OBSOLETE_PREFERENCES) {
      Services.prefs.clearUserPref(pref);
    }
    OS.File.remove(OS.Path.join(OS.Constants.Path.profileDir,
                                FILE_XPI_ADDONS_LIST));

    // Serialize and deserialize so we get the expected JSON data.
    let data = JSON.parse(JSON.stringify(this));

    logger.debug("Migrated data: ${}", data);

    return data;
  },

  /**
   * Load extension state data from addonStartup.json, or migrates it
   * from legacy state preferences, if they exist.
   *
   * @returns {Object}
   */
  loadExtensionState() {
    let state;
    try {
      state = aomStartup.readStartupData();
    } catch (e) {
      logger.warn("Error parsing extensions state: ${error}",
                  {error: e});
    }

    if (!state && Services.prefs.getPrefType(PREF_XPI_STATE) != Ci.nsIPrefBranch.PREF_INVALID) {
      try {
        state = this.migrateStateFromPrefs();
      } catch (e) {
        logger.warn("Error migrating extensions.xpiState and " +
                    "extensions.bootstrappedAddons: ${error}",
                    {error: e});
      }
    }

    logger.debug("Loaded add-on state: ${}", state);
    return state || {};
  },

  /**
   * Walk through all install locations, highest priority first,
   * comparing the on-disk state of extensions to what is stored in prefs.
   *
   * @param {boolean} [ignoreSideloads = true]
   *        If true, ignore changes in scopes where we don't accept
   *        side-loads.
   *
   * @returns {boolean}
   *        True if anything has changed.
   */
  getInstallState(ignoreSideloads = true) {
    if (!this.db) {
      this.db = new Map();
    }

    let oldState = this.initialStateData || this.loadExtensionState();
    this.initialStateData = oldState;

    let changed = false;
    let oldLocations = new Set(Object.keys(oldState));

    for (let location of XPIProvider.installLocations) {
      oldLocations.delete(location.name);

      // The results of scanning this location.
      let loc = this.getLocation(location.name, location.path || null,
                                 oldState[location.name] || undefined);
      changed = changed || loc.changed;

      // Don't bother checking scopes where we don't accept side-loads.
      if (ignoreSideloads && !(location.scope & gStartupScanScopes)) {
        continue;
      }

      if (location.name == KEY_APP_TEMPORARY) {
        continue;
      }

      let knownIds = new Set(loc.keys());
      for (let [id, file] of location.getAddonLocations(true)) {
        knownIds.delete(id);

        let xpiState = loc.get(id);
        if (!xpiState) {
          logger.debug("New add-on ${id} in ${location}", {id, location: location.name});

          changed = true;
          xpiState = loc.addFile(id, file);
          if (!location.isSystem) {
            this.sideLoadedAddons.set(id, xpiState);
          }
        } else {
          let addonChanged = (xpiState.getModTime(file, id) ||
                              file.path != xpiState.path);
          xpiState.file = file.clone();

          if (addonChanged) {
            changed = true;
            logger.debug("Changed add-on ${id} in ${location}", {id, location: location.name});
          } else {
            logger.debug("Existing add-on ${id} in ${location}", {id, location: location.name});
          }
        }
        XPIProvider.setTelemetry(id, "location", location.name);
      }

      // Anything left behind in oldState was removed from the file system.
      for (let id of knownIds) {
        loc.delete(id);
        changed = true;
      }
    }

    // If there's anything left in oldState, an install location that held add-ons
    // was removed from the browser configuration.
    changed = changed || oldLocations.size > 0;

    logger.debug("getInstallState changed: ${rv}, state: ${state}",
        {rv: changed, state: this.db});
    return changed;
  },

  /**
   * Get the Map of XPI states for a particular location.
   *
   * @param {string} name
   *        The name of the install location.
   * @param {string?} [path]
   *        The expected path of the location, if known.
   * @param {Object?} [saved]
   *        The saved data for the location, as read from the
   *        addonStartup.json file.
   *
   * @returns {XPIStateLocation?}
   *        (id -> XPIState) or null if there are no add-ons in the location.
   */
  getLocation(name, path, saved) {
    let location = this.db.get(name);

    if (path && location && location.path != path) {
      location = null;
      saved = null;
    }

    if (!location || (path && location.path != path)) {
      let loc = XPIProvider.installLocationsByName[name];
      if (loc) {
        location = new XPIStateLocation(name, path || loc.path || null, saved);
        this.db.set(name, location);
      }
    }
    return location;
  },

  /**
   * Get the XPI state for a specific add-on in a location.
   * If the state is not in our cache, return null.
   *
   * @param {string} aLocation
   *        The name of the location where the add-on is installed.
   * @param {string} aId
   *        The add-on ID
   *
   * @returns {XPIState?}
   *        The XPIState entry for the add-on, or null.
   */
  getAddon(aLocation, aId) {
    let location = this.db.get(aLocation);
    return location && location.get(aId);
  },

  /**
   * Find the highest priority location of an add-on by ID and return the
   * XPIState.
   * @param {string} aId
   *        The add-on IDa
   * @param {function} aFilter
   *        An optional filter to apply to install locations.  If provided,
   *        addons in locations that do not match the filter are not considered.
   *
   * @returns {XPIState?}
   */
  findAddon(aId, aFilter = location => true) {
    // Fortunately the Map iterator returns in order of insertion, which is
    // also our highest -> lowest priority order.
    for (let location of this.db.values()) {
      if (!aFilter(location)) {
        continue;
      }
      if (location.has(aId)) {
        return location.get(aId);
      }
    }
    return undefined;
  },

  /**
   * Iterates over the list of all enabled add-ons in any location.
   */
  * enabledAddons() {
    for (let location of this.db.values()) {
      for (let entry of location.values()) {
        if (entry.enabled) {
          yield entry;
        }
      }
    }
  },

  /**
   * Iterates over the list of all add-ons which were initially restored
   * from the startup state cache.
   */
  * initialEnabledAddons() {
    for (let addon of this.enabledAddons()) {
      if (addon.wasRestored) {
        yield addon;
      }
    }
  },

  /**
   * Iterates over all enabled bootstrapped add-ons, in any location.
   */
  * bootstrappedAddons() {
    for (let addon of this.enabledAddons()) {
      if (addon.bootstrapped) {
        yield addon;
      }
    }
  },

  /**
   * Add a new XPIState for an add-on and synchronize it with the DBAddonInternal.
   *
   * @param {DBAddonInternal} aAddon
   *        The add-on to add.
   */
  addAddon(aAddon) {
    let location = this.getLocation(aAddon._installLocation.name);
    location.addAddon(aAddon);
  },

  /**
   * Save the current state of installed add-ons.
   */
  save() {
    if (!this._jsonFile) {
      this._jsonFile = new JSONFile({
        path: OS.Path.join(OS.Constants.Path.profileDir, FILE_XPI_STATES),
        finalizeAt: AddonManager.shutdown,
        compression: "lz4",
      });
      this._jsonFile.data = this;
    }

    this._jsonFile.saveSoon();
  },

  toJSON() {
    let data = {};
    for (let [key, loc] of this.db.entries()) {
      if (key != TemporaryInstallLocation.name && (loc.size || loc.hasStaged)) {
        data[key] = loc;
      }
    }
    return data;
  },

  /**
   * Remove the XPIState for an add-on and save the new state.
   *
   * @param {string} aLocation
   *        The name of the add-on location.
   * @param {string} aId
   *        The ID of the add-on.
   *
   */
  removeAddon(aLocation, aId) {
    logger.debug("Removing XPIState for " + aLocation + ":" + aId);
    let location = this.db.get(aLocation);
    if (location) {
      location.delete(aId);
      if (location.size == 0) {
        this.db.delete(aLocation);
      }
      this.save();
    }
  },

  /**
   * Disable the XPIState for an add-on.
   *
   * @param {string} aId
   *        The ID of the add-on.
   */
  disableAddon(aId) {
    logger.debug(`Disabling XPIState for ${aId}`);
    let state = this.findAddon(aId);
    if (state) {
      state.enabled = false;
    }
  },
};

var XPIProvider = {
  get name() {
    return "XPIProvider";
  },

  BOOTSTRAP_REASONS: Object.freeze(BOOTSTRAP_REASONS),

  // An array of known install locations
  installLocations: null,
  // A dictionary of known install locations by name
  installLocationsByName: null,
  // An array of currently active AddonInstalls
  installs: null,
  // The value of the minCompatibleAppVersion preference
  minCompatibleAppVersion: null,
  // The value of the minCompatiblePlatformVersion preference
  minCompatiblePlatformVersion: null,
  // A Map of active addons to their bootstrapScope by ID
  activeAddons: new Map(),
  // True if the platform could have activated extensions
  extensionsActive: false,
  // New distribution addons awaiting permissions approval
  newDistroAddons: null,
  // Keep track of startup phases for telemetry
  runPhase: XPI_STARTING,
  // Per-addon telemetry information
  _telemetryDetails: {},
  // Have we started shutting down bootstrap add-ons?
  _closing: false,

  // Check if the XPIDatabase has been loaded (without actually
  // triggering unwanted imports or I/O)
  get isDBLoaded() {
    // Make sure we don't touch the XPIDatabase getter before it's
    // actually loaded, and force an early load.
    return (Object.getOwnPropertyDescriptor(gGlobalScope, "XPIDatabase").value &&
            XPIDatabase.initialized) || false;
  },

  /**
   * Returns true if the add-on with the given ID is currently active,
   * without forcing the add-ons database to load.
   *
   * @param {string} addonId
   *        The ID of the add-on to check.
   * @returns {boolean}
   */
  addonIsActive(addonId) {
    let state = XPIStates.findAddon(addonId);
    return state && state.enabled;
  },

  /**
   * Returns an array of the add-on values in `bootstrappedAddons`,
   * sorted so that all of an add-on's dependencies appear in the array
   * before itself.
   *
   * @returns {Array<object>}
   *   A sorted array of add-on objects. Each value is a copy of the
   *   corresponding value in the `bootstrappedAddons` object, with an
   *   additional `id` property, which corresponds to the key in that
   *   object, which is the same as the add-ons ID.
   */
  sortBootstrappedAddons() {
    function compare(a, b) {
      if (a === b) {
        return 0;
      }
      return (a < b) ? -1 : 1;
    }

    // Sort the list so that ordering is deterministic.
    let list = Array.from(XPIStates.bootstrappedAddons());
    list.sort((a, b) => compare(a.id, b.id));

    let addons = {};
    for (let entry of list) {
      addons[entry.id] = entry;
    }

    let res = new Set();
    let seen = new Set();

    let add = addon => {
      seen.add(addon.id);

      for (let id of addon.dependencies || []) {
        if (id in addons && !seen.has(id)) {
          add(addons[id]);
        }
      }

      res.add(addon.id);
    };

    Object.values(addons).forEach(add);

    return Array.from(res, id => addons[id]);
  },

  /*
   * Set a value in the telemetry hash for a given ID
   */
  setTelemetry(aId, aName, aValue) {
    if (!this._telemetryDetails[aId])
      this._telemetryDetails[aId] = {};
    this._telemetryDetails[aId][aName] = aValue;
  },

  // Keep track of in-progress operations that support cancel()
  _inProgress: [],

  doing(aCancellable) {
    this._inProgress.push(aCancellable);
  },

  done(aCancellable) {
    let i = this._inProgress.indexOf(aCancellable);
    if (i != -1) {
      this._inProgress.splice(i, 1);
      return true;
    }
    return false;
  },

  cancelAll() {
    // Cancelling one may alter _inProgress, so don't use a simple iterator
    while (this._inProgress.length > 0) {
      let c = this._inProgress.shift();
      try {
        c.cancel();
      } catch (e) {
        logger.warn("Cancel failed", e);
      }
    }
  },

  /**
   * Starts the XPI provider initializes the install locations and prefs.
   *
   * @param {boolean?} aAppChanged
   *        A tri-state value. Undefined means the current profile was created
   *        for this session, true means the profile already existed but was
   *        last used with an application with a different version number,
   *        false means that the profile was last used by this version of the
   *        application.
   * @param {string?} [aOldAppVersion]
   *        The version of the application last run with this profile or null
   *        if it is a new profile or the version is unknown
   * @param {string?} [aOldPlatformVersion]
   *        The version of the platform last run with this profile or null
   *        if it is a new profile or the version is unknown
   */
  startup(aAppChanged, aOldAppVersion, aOldPlatformVersion) {
    function addDirectoryInstallLocation(aName, aKey, aPaths, aScope, aLocked) {
      try {
        var dir = FileUtils.getDir(aKey, aPaths);
      } catch (e) {
        // Some directories aren't defined on some platforms, ignore them
        logger.debug("Skipping unavailable install location " + aName);
        return;
      }

      try {
        var location = aLocked ? new DirectoryInstallLocation(aName, dir, aScope)
                               : new MutableDirectoryInstallLocation(aName, dir, aScope);
      } catch (e) {
        logger.warn("Failed to add directory install location " + aName, e);
        return;
      }

      XPIProvider.installLocations.push(location);
      XPIProvider.installLocationsByName[location.name] = location;
    }

    function addBuiltInInstallLocation(name, key, paths, scope) {
      let dir;
      try {
        dir = FileUtils.getDir(key, paths);
      } catch (e) {
        return;
      }
      try {
        let location = new BuiltInInstallLocation(name, dir, scope);

        XPIProvider.installLocations.push(location);
        XPIProvider.installLocationsByName[location.name] = location;
      } catch (e) {
        logger.warn(`Failed to add built-in install location ${name}`, e);
      }
    }

    function addSystemAddonInstallLocation(aName, aKey, aPaths, aScope) {
      try {
        var dir = FileUtils.getDir(aKey, aPaths);
      } catch (e) {
        // Some directories aren't defined on some platforms, ignore them
        logger.debug("Skipping unavailable install location " + aName);
        return;
      }

      try {
        var location = new SystemAddonInstallLocation(aName, dir, aScope, aAppChanged !== false);
      } catch (e) {
        logger.warn("Failed to add system add-on install location " + aName, e);
        return;
      }

      XPIProvider.installLocations.push(location);
      XPIProvider.installLocationsByName[location.name] = location;
    }

    function addRegistryInstallLocation(aName, aRootkey, aScope) {
      try {
        var location = new WinRegInstallLocation(aName, aRootkey, aScope);
      } catch (e) {
        logger.warn("Failed to add registry install location " + aName, e);
        return;
      }

      XPIProvider.installLocations.push(location);
      XPIProvider.installLocationsByName[location.name] = location;
    }

    try {
      AddonManagerPrivate.recordTimestamp("XPI_startup_begin");

      logger.debug("startup");
      this.runPhase = XPI_STARTING;
      this.installs = new Set();
      this.installLocations = [];
      this.installLocationsByName = {};

      // Clear this at startup for xpcshell test restarts
      this._telemetryDetails = {};
      // Register our details structure with AddonManager
      AddonManagerPrivate.setTelemetryDetails("XPI", this._telemetryDetails);

      let hasRegistry = ("nsIWindowsRegKey" in Ci);

      let enabledScopes = Services.prefs.getIntPref(PREF_EM_ENABLED_SCOPES,
                                                    AddonManager.SCOPE_ALL);

      // These must be in order of priority, highest to lowest,
      // for processFileChanges etc. to work

      XPIProvider.installLocations.push(TemporaryInstallLocation);
      XPIProvider.installLocationsByName[TemporaryInstallLocation.name] =
        TemporaryInstallLocation;

      // The profile location is always enabled
      addDirectoryInstallLocation(KEY_APP_PROFILE, KEY_PROFILEDIR,
                                  [DIR_EXTENSIONS],
                                  AddonManager.SCOPE_PROFILE, false);

      addSystemAddonInstallLocation(KEY_APP_SYSTEM_ADDONS, KEY_PROFILEDIR,
                                    [DIR_SYSTEM_ADDONS],
                                    AddonManager.SCOPE_PROFILE);

      addBuiltInInstallLocation(KEY_APP_SYSTEM_DEFAULTS, KEY_APP_FEATURES,
                                [], AddonManager.SCOPE_PROFILE);

      if (enabledScopes & AddonManager.SCOPE_USER) {
        addDirectoryInstallLocation(KEY_APP_SYSTEM_USER, "XREUSysExt",
                                    [Services.appinfo.ID],
                                    AddonManager.SCOPE_USER, true);
        if (hasRegistry) {
          addRegistryInstallLocation("winreg-app-user",
                                     Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                                     AddonManager.SCOPE_USER);
        }
      }

      addDirectoryInstallLocation(KEY_APP_GLOBAL, KEY_ADDON_APP_DIR,
                                  [DIR_EXTENSIONS],
                                  AddonManager.SCOPE_APPLICATION, true);

      if (enabledScopes & AddonManager.SCOPE_SYSTEM) {
        addDirectoryInstallLocation(KEY_APP_SYSTEM_SHARE, "XRESysSExtPD",
                                    [Services.appinfo.ID],
                                    AddonManager.SCOPE_SYSTEM, true);
        addDirectoryInstallLocation(KEY_APP_SYSTEM_LOCAL, "XRESysLExtPD",
                                    [Services.appinfo.ID],
                                    AddonManager.SCOPE_SYSTEM, true);
        if (hasRegistry) {
          addRegistryInstallLocation("winreg-app-global",
                                     Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                                     AddonManager.SCOPE_SYSTEM);
        }
      }

      this.minCompatibleAppVersion = Services.prefs.getStringPref(PREF_EM_MIN_COMPAT_APP_VERSION,
                                                                  null);
      this.minCompatiblePlatformVersion = Services.prefs.getStringPref(PREF_EM_MIN_COMPAT_PLATFORM_VERSION,
                                                                       null);

      Services.prefs.addObserver(PREF_EM_MIN_COMPAT_APP_VERSION, this);
      Services.prefs.addObserver(PREF_EM_MIN_COMPAT_PLATFORM_VERSION, this);
      if (!AppConstants.MOZ_REQUIRE_SIGNING || Cu.isInAutomation)
        Services.prefs.addObserver(PREF_XPI_SIGNATURES_REQUIRED, this);
      Services.prefs.addObserver(PREF_LANGPACK_SIGNATURES, this);
      Services.prefs.addObserver(PREF_ALLOW_LEGACY, this);
      Services.obs.addObserver(this, NOTIFICATION_FLUSH_PERMISSIONS);
      Services.obs.addObserver(this, NOTIFICATION_TOOLBOX_CONNECTION_CHANGE);


      let flushCaches = this.checkForChanges(aAppChanged, aOldAppVersion,
                                             aOldPlatformVersion);

      AddonManagerPrivate.markProviderSafe(this);

      if (flushCaches) {
        Services.obs.notifyObservers(null, "startupcache-invalidate");
        // UI displayed early in startup (like the compatibility UI) may have
        // caused us to cache parts of the skin or locale in memory. These must
        // be flushed to allow extension provided skins and locales to take full
        // effect
        Services.obs.notifyObservers(null, "chrome-flush-skin-caches");
        Services.obs.notifyObservers(null, "chrome-flush-caches");
      }

      if (AppConstants.MOZ_CRASHREPORTER) {
        // Annotate the crash report with relevant add-on information.
        try {
          Services.appinfo.annotateCrashReport("EMCheckCompatibility",
                                               AddonManager.checkCompatibility);
        } catch (e) { }
        this.addAddonsToCrashReporter();
      }

      try {
        AddonManagerPrivate.recordTimestamp("XPI_bootstrap_addons_begin");

        for (let addon of this.sortBootstrappedAddons()) {
          // The startup update check above may have already started some
          // extensions, make sure not to try to start them twice.
          let activeAddon = this.activeAddons.get(addon.id);
          if (activeAddon && activeAddon.started) {
            continue;
          }
          try {
            let reason = BOOTSTRAP_REASONS.APP_STARTUP;
            // Eventually set INSTALLED reason when a bootstrap addon
            // is dropped in profile folder and automatically installed
            if (AddonManager.getStartupChanges(AddonManager.STARTUP_CHANGE_INSTALLED)
                            .includes(addon.id))
              reason = BOOTSTRAP_REASONS.ADDON_INSTALL;
            this.callBootstrapMethod(addon, addon.file, "startup", reason);
          } catch (e) {
            logger.error("Failed to load bootstrap addon " + addon.id + " from " +
                         addon.descriptor, e);
          }
        }
        AddonManagerPrivate.recordTimestamp("XPI_bootstrap_addons_end");
      } catch (e) {
        logger.error("bootstrap startup failed", e);
        AddonManagerPrivate.recordException("XPI-BOOTSTRAP", "startup failed", e);
      }

      // Let these shutdown a little earlier when they still have access to most
      // of XPCOM
      Services.obs.addObserver({
        observe(aSubject, aTopic, aData) {
          XPIProvider.cleanupTemporaryAddons();
          XPIProvider._closing = true;
          for (let addon of XPIProvider.sortBootstrappedAddons().reverse()) {
            // If no scope has been loaded for this add-on then there is no need
            // to shut it down (should only happen when a bootstrapped add-on is
            // pending enable)
            let activeAddon = XPIProvider.activeAddons.get(addon.id);
            if (!activeAddon || !activeAddon.started) {
              continue;
            }

            // If the add-on was pending disable then shut it down and remove it
            // from the persisted data.
            let reason = BOOTSTRAP_REASONS.APP_SHUTDOWN;
            if (addon.disable) {
              reason = BOOTSTRAP_REASONS.ADDON_DISABLE;
            } else if (addon.location.name == KEY_APP_TEMPORARY) {
              reason = BOOTSTRAP_REASONS.ADDON_UNINSTALL;
              let existing = XPIStates.findAddon(addon.id, loc =>
                loc.name != TemporaryInstallLocation.name);
              if (existing) {
                reason = XPIInstall.newVersionReason(addon.version, existing.version);
              }
            }
            XPIProvider.callBootstrapMethod(addon, addon.file,
                                            "shutdown", reason);
          }
          Services.obs.removeObserver(this, "quit-application-granted");
        }
      }, "quit-application-granted");

      // Detect final-ui-startup for telemetry reporting
      Services.obs.addObserver({
        observe(aSubject, aTopic, aData) {
          AddonManagerPrivate.recordTimestamp("XPI_finalUIStartup");
          XPIProvider.runPhase = XPI_AFTER_UI_STARTUP;
          Services.obs.removeObserver(this, "final-ui-startup");
        }
      }, "final-ui-startup");

      // If we haven't yet loaded the XPI database, schedule loading it
      // to occur once other important startup work is finished.  We want
      // this to happen relatively quickly after startup so the telemetry
      // environment has complete addon information.
      //
      // Unfortunately we have to use a variety of ways do detect when it
      // is time to load.  In a regular browser process we just wait for
      // sessionstore-windows-restored.  In a browser toolbox process
      // we wait for the toolbox to show up, based on xul-window-visible
      // and a visible toolbox window.
      // Finally, we have a test-only event called test-load-xpi-database
      // as a temporary workaround for bug 1372845.  The latter can be
      // cleaned up when that bug is resolved.
      if (!this.isDBLoaded) {
        const EVENTS = [ "sessionstore-windows-restored", "xul-window-visible", "test-load-xpi-database" ];
        let observer = {
          observe(subject, topic, data) {
            if (topic == "xul-window-visible" &&
                !Services.wm.getMostRecentWindow("devtools:toolbox")) {
              return;
            }

            for (let event of EVENTS) {
              Services.obs.removeObserver(observer, event);
            }

            // It would be nice to defer some of the work here until we
            // have idle time but we can't yet use requestIdleCallback()
            // from chrome.  See bug 1358476.
            XPIDatabase.asyncLoadDB();
          },
        };
        for (let event of EVENTS) {
          Services.obs.addObserver(observer, event);
        }
      }

      AddonManagerPrivate.recordTimestamp("XPI_startup_end");

      this.extensionsActive = true;
      this.runPhase = XPI_BEFORE_UI_STARTUP;

      let timerManager = Cc["@mozilla.org/updates/timer-manager;1"].
                         getService(Ci.nsIUpdateTimerManager);
      timerManager.registerTimer("xpi-signature-verification", () => {
        this.verifySignatures();
      }, XPI_SIGNATURE_CHECK_PERIOD);
    } catch (e) {
      logger.error("startup failed", e);
      AddonManagerPrivate.recordException("XPI", "startup failed", e);
    }
  },

  /**
   * Shuts down the database and releases all references.
   * Return: Promise{integer} resolves / rejects with the result of
   *                          flushing the XPI Database if it was loaded,
   *                          0 otherwise.
   */
  async shutdown() {
    logger.debug("shutdown");

    // Stop anything we were doing asynchronously
    this.cancelAll();

    this.activeAddons.clear();
    this.allAppGlobal = true;

    for (let install of this.installs) {
      if (install.onShutdown()) {
        install.onShutdown();
      }
    }

    // If there are pending operations then we must update the list of active
    // add-ons
    if (Services.prefs.getBoolPref(PREF_PENDING_OPERATIONS, false)) {
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_pending_ops", 1);
      XPIDatabase.updateActiveAddons();
      Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, false);
    }

    // Ugh, if we reach this point without loading the xpi database,
    // we need to load it know, otherwise the telemetry shutdown blocker
    // will never resolve.
    if (!XPIDatabase.initialized) {
      await XPIDatabase.asyncLoadDB();
    }

    this.installs = null;
    this.installLocations = null;
    this.installLocationsByName = null;

    // This is needed to allow xpcshell tests to simulate a restart
    this.extensionsActive = false;

    await XPIDatabase.shutdown();
  },

  cleanupTemporaryAddons() {
    let tempLocation = XPIStates.getLocation(TemporaryInstallLocation.name);
    if (tempLocation) {
      for (let [id, addon] of tempLocation.entries()) {
        tempLocation.delete(id);

        let reason = BOOTSTRAP_REASONS.ADDON_UNINSTALL;

        let existing = XPIStates.findAddon(id, loc => loc != tempLocation);
        let callUpdate = false;
        if (existing) {
          reason = XPIInstall.newVersionReason(addon.version, existing.version);
          callUpdate = (isWebExtension(addon.type) && isWebExtension(existing.type));
        }

        this.callBootstrapMethod(addon, addon.file, "shutdown", reason);
        if (!callUpdate) {
          this.callBootstrapMethod(addon, addon.file, "uninstall", reason);
        }
        this.unloadBootstrapScope(id);
        TemporaryInstallLocation.uninstallAddon(id);
        XPIStates.removeAddon(TemporaryInstallLocation.name, id);

        if (existing) {
          let newAddon = XPIDatabase.makeAddonLocationVisible(id, existing.location.name);

          let file = new nsIFile(newAddon.path);

          let data = {oldVersion: addon.version};
          let method = callUpdate ? "update" : "install";
          this.callBootstrapMethod(newAddon, file, method, reason, data);
        }
      }
    }
  },

  /**
   * Verifies that all installed add-ons are still correctly signed.
   */
  async verifySignatures() {
    try {
      let addons = await XPIDatabase.getAddonList(a => true);

      let changes = {
        enabled: [],
        disabled: []
      };

      for (let addon of addons) {
        // The add-on might have vanished, we'll catch that on the next startup
        if (!addon._sourceBundle.exists())
          continue;

        let signedState = await verifyBundleSignedState(addon._sourceBundle, addon);

        if (signedState != addon.signedState) {
          addon.signedState = signedState;
          AddonManagerPrivate.callAddonListeners("onPropertyChanged",
                                                 addon.wrapper,
                                                 ["signedState"]);
        }

        let disabled = XPIDatabase.updateAddonDisabledState(addon);
        if (disabled !== undefined)
          changes[disabled ? "disabled" : "enabled"].push(addon.id);
      }

      XPIDatabase.saveChanges();

      Services.obs.notifyObservers(null, "xpi-signature-changed", JSON.stringify(changes));
    } catch (err) {
      logger.error("XPI_verifySignature: " + err);
    }
  },

  /**
   * Adds a list of currently active add-ons to the next crash report.
   */
  addAddonsToCrashReporter() {
    if (!(Services.appinfo instanceof Ci.nsICrashReporter) ||
        !AppConstants.MOZ_CRASHREPORTER) {
      return;
    }

    // In safe mode no add-ons are loaded so we should not include them in the
    // crash report
    if (Services.appinfo.inSafeMode) {
      return;
    }

    let data = Array.from(XPIStates.enabledAddons(), a => a.telemetryKey).join(",");

    try {
      Services.appinfo.annotateCrashReport("Add-ons", data);
    } catch (e) { }

    TelemetrySession.setAddOns(data);
  },

  /**
   * Check the staging directories of install locations for any add-ons to be
   * installed or add-ons to be uninstalled.
   *
   * @param {Object} aManifests
   *         A dictionary to add detected install manifests to for the purpose
   *         of passing through updated compatibility information
   * @returns {boolean}
   *        True if an add-on was installed or uninstalled
   */
  processPendingFileChanges(aManifests) {
    let changed = false;
    for (let location of this.installLocations) {
      aManifests[location.name] = {};
      // We can't install or uninstall anything in locked locations
      if (location.locked) {
        continue;
      }

      let state = XPIStates.getLocation(location.name);

      let cleanNames = [];
      let promises = [];
      for (let [id, metadata] of state.getStagedAddons()) {
        state.unstageAddon(id);

        aManifests[location.name][id] = null;
        promises.push(
          XPIInstall.installStagedAddon(id, metadata, location).then(
            addon => {
              aManifests[location.name][id] = addon;
            },
            error => {
              delete aManifests[location.name][id];
              cleanNames.push(`${id}.xpi`);

              logger.error(`Failed to install staged add-on ${id} in ${location.name}`,
                           error);
            }));
      }

      if (promises.length) {
        changed = true;
        awaitPromise(Promise.all(promises));
      }

      try {
        if (cleanNames.length) {
          location.cleanStagingDir(cleanNames);
        }
      } catch (e) {
        // Non-critical, just saves some perf on startup if we clean this up.
        logger.debug("Error cleaning staging dir", e);
      }
    }
    return changed;
  },

  /**
   * Installs any add-ons located in the extensions directory of the
   * application's distribution specific directory into the profile unless a
   * newer version already exists or the user has previously uninstalled the
   * distributed add-on.
   *
   * @param {Object} aManifests
   *        A dictionary to add new install manifests to to save having to
   *        reload them later
   * @param {string} [aAppChanged]
   *        See checkForChanges
   * @returns {boolean}
   *        True if any new add-ons were installed
   */
  installDistributionAddons(aManifests, aAppChanged) {
    let distroDir;
    try {
      distroDir = FileUtils.getDir(KEY_APP_DISTRIBUTION, [DIR_EXTENSIONS]);
    } catch (e) {
      return false;
    }

    if (!distroDir.exists())
      return false;

    if (!distroDir.isDirectory())
      return false;

    let changed = false;
    let profileLocation = this.installLocationsByName[KEY_APP_PROFILE];

    let entries = distroDir.directoryEntries
                           .QueryInterface(Ci.nsIDirectoryEnumerator);
    let entry;
    while ((entry = entries.nextFile)) {

      let id = entry.leafName;

      if (entry.isFile()) {
        if (id.endsWith(".xpi")) {
          id = id.slice(0, -4);
        } else {
          logger.debug("Ignoring distribution add-on that isn't an XPI: " + entry.path);
          continue;
        }
      } else if (!entry.isDirectory()) {
        logger.debug("Ignoring distribution add-on that isn't a file or directory: " +
            entry.path);
        continue;
      }

      if (!gIDTest.test(id)) {
        logger.debug("Ignoring distribution add-on whose name is not a valid add-on ID: " +
            entry.path);
        continue;
      }

      /* If this is not an upgrade and we've already handled this extension
       * just continue */
      if (!aAppChanged && Services.prefs.prefHasUserValue(PREF_BRANCH_INSTALLED_ADDON + id)) {
        continue;
      }

      try {
        let addon = awaitPromise(XPIInstall.installDistributionAddon(id, entry, profileLocation));

        if (addon) {
          // aManifests may contain a copy of a newly installed add-on's manifest
          // and we'll have overwritten that so instead cache our install manifest
          // which will later be put into the database in processFileChanges
          if (!(KEY_APP_PROFILE in aManifests))
            aManifests[KEY_APP_PROFILE] = {};
          aManifests[KEY_APP_PROFILE][id] = addon;
          changed = true;
        }
      } catch (e) {
        logger.error("Failed to install distribution add-on " + entry.path, e);
      }
    }

    entries.close();

    return changed;
  },

  getNewDistroAddons() {
    let addons = this.newDistroAddons;
    this.newDistroAddons = null;
    return addons;
  },

  /**
   * Imports the xpinstall permissions from preferences into the permissions
   * manager for the user to change later.
   */
  importPermissions() {
    PermissionsUtils.importFromPrefs(PREF_XPI_PERMISSIONS_BRANCH,
                                     XPI_PERMISSION);
  },

  getDependentAddons(aAddon) {
    return Array.from(XPIDatabase.getAddons())
                .filter(addon => addon.dependencies.includes(aAddon.id));
  },

  /**
   * Checks for any changes that have occurred since the last time the
   * application was launched.
   *
   * @param {boolean?} [aAppChanged]
   *        A tri-state value. Undefined means the current profile was created
   *        for this session, true means the profile already existed but was
   *        last used with an application with a different version number,
   *        false means that the profile was last used by this version of the
   *        application.
   * @param {string?} [aOldAppVersion]
   *        The version of the application last run with this profile or null
   *        if it is a new profile or the version is unknown
   * @param {string?} [aOldPlatformVersion]
   *        The version of the platform last run with this profile or null
   *        if it is a new profile or the version is unknown
   * @returns {boolean}
   *        True if a change requiring a restart was detected
   */
  checkForChanges(aAppChanged, aOldAppVersion, aOldPlatformVersion) {
    logger.debug("checkForChanges");

    // Keep track of whether and why we need to open and update the database at
    // startup time.
    let updateReasons = [];
    if (aAppChanged) {
      updateReasons.push("appChanged");
    }

    let installChanged = XPIStates.getInstallState(aAppChanged === false);
    if (installChanged) {
      updateReasons.push("directoryState");
    }

    // First install any new add-ons into the locations, if there are any
    // changes then we must update the database with the information in the
    // install locations
    let manifests = {};
    let updated = this.processPendingFileChanges(manifests);
    if (updated) {
      updateReasons.push("pendingFileChanges");
    }

    // This will be true if the previous session made changes that affect the
    // active state of add-ons but didn't commit them properly (normally due
    // to the application crashing)
    let hasPendingChanges = Services.prefs.getBoolPref(PREF_PENDING_OPERATIONS, false);
    if (hasPendingChanges) {
      updateReasons.push("hasPendingChanges");
    }

    // If the application has changed then check for new distribution add-ons
    if (Services.prefs.getBoolPref(PREF_INSTALL_DISTRO_ADDONS, true)) {
      updated = this.installDistributionAddons(manifests, aAppChanged);
      if (updated) {
        updateReasons.push("installDistributionAddons");
      }
    }

    let haveAnyAddons = (XPIStates.size > 0);

    // If the schema appears to have changed then we should update the database
    if (DB_SCHEMA != Services.prefs.getIntPref(PREF_DB_SCHEMA, 0)) {
      // If we don't have any add-ons, just update the pref, since we don't need to
      // write the database
      if (!haveAnyAddons) {
        logger.debug("Empty XPI database, setting schema version preference to " + DB_SCHEMA);
        Services.prefs.setIntPref(PREF_DB_SCHEMA, DB_SCHEMA);
      } else {
        updateReasons.push("schemaChanged");
      }
    }

    // If the database doesn't exist and there are add-ons installed then we
    // must update the database however if there are no add-ons then there is
    // no need to update the database.
    let dbFile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_DATABASE], true);
    if (!dbFile.exists() && haveAnyAddons) {
      updateReasons.push("needNewDatabase");
    }

    // Catch and log any errors during the main startup
    try {
      let extensionListChanged = false;
      // If the database needs to be updated then open it and then update it
      // from the filesystem
      if (updateReasons.length > 0) {
        AddonManagerPrivate.recordSimpleMeasure("XPIDB_startup_load_reasons", updateReasons);
        XPIDatabase.syncLoadDB(false);
        try {
          extensionListChanged = XPIDatabaseReconcile.processFileChanges(manifests,
                                                                         aAppChanged,
                                                                         aOldAppVersion,
                                                                         aOldPlatformVersion,
                                                                         updateReasons.includes("schemaChanged"));
        } catch (e) {
          logger.error("Failed to process extension changes at startup", e);
        }
      }

      // If the application crashed before completing any pending operations then
      // we should perform them now.
      if (extensionListChanged || hasPendingChanges) {
        this._updateActiveAddons();
        return true;
      }

      logger.debug("No changes found");
    } catch (e) {
      logger.error("Error during startup file checks", e);
    }

    return false;
  },

  _updateActiveAddons() {
    logger.debug("Updating database with changes to installed add-ons");
    XPIDatabase.updateActiveAddons();
    Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, false);
  },

  /**
   * Gets an array of add-ons which were placed in a known install location
   * prior to startup of the current session, were detected by a directory scan
   * of those locations, and are currently disabled.
   *
   * @returns {Promise<Array<Addon>>}
   */
  async getNewSideloads() {
    if (XPIStates.getInstallState(false)) {
      // We detected changes. Update the database to account for them.
      await XPIDatabase.asyncLoadDB(false);
      XPIDatabaseReconcile.processFileChanges({}, false);
      this._updateActiveAddons();
    }

    let addons = await Promise.all(
      Array.from(XPIStates.sideLoadedAddons.keys(),
                 id => AddonManager.getAddonByID(id)));

    return addons.filter(addon => (addon.seen === false &&
                                   addon.permissions & AddonManager.PERM_CAN_ENABLE));
  },

  /**
   * Called to test whether this provider supports installing a particular
   * mimetype.
   *
   * @param {string} aMimetype
   *        The mimetype to check for
   * @returns {boolean}
   *        True if the mimetype is application/x-xpinstall
   */
  supportsMimetype(aMimetype) {
    return aMimetype == "application/x-xpinstall";
  },

  // Identify temporary install IDs.
  isTemporaryInstallID(id) {
    return id.endsWith(TEMPORARY_ADDON_SUFFIX);
  },

  /**
   * Sets startupData for the given addon.  The provided data will be stored
   * in addonsStartup.json so it is available early during browser startup.
   * Note that this file is read synchronously at startup, so startupData
   * should be used with care.
   *
   * @param {string} aID
   *         The id of the addon to save startup data for.
   * @param {any} aData
   *        The data to store.  Must be JSON serializable.
   */
  setStartupData(aID, aData) {
    let state = XPIStates.findAddon(aID);
    state.startupData = aData;
    XPIStates.save();
  },

  /**
   * Returns an Addon corresponding to an instance ID.
   *
   * @param {Symbol} aInstanceID
   *        An Addon Instance ID
   *
   * @returns {AddonInternal?}
   *
   * @throws if the aInstanceID argument is not valid.
   */
   getAddonByInstanceID(aInstanceID) {
     let id = this.getAddonIDByInstanceID(aInstanceID);
     if (id) {
       return this.syncGetAddonByID(id);
     }

     return null;
   },

   getAddonIDByInstanceID(aInstanceID) {
     if (!aInstanceID || typeof aInstanceID != "symbol")
       throw Components.Exception("aInstanceID must be a Symbol()",
                                  Cr.NS_ERROR_INVALID_ARG);

     for (let [id, val] of this.activeAddons) {
       if (aInstanceID == val.instanceID) {
         return id;
       }
     }

     return null;
   },

  /**
   * Removes an AddonInstall from the list of active installs.
   *
   * @param {AddonInstall} aInstall
   *        The AddonInstall to remove
   */
  removeActiveInstall(aInstall) {
    this.installs.delete(aInstall);
  },

  /**
   * Called to get an Addon with a particular ID.
   *
   * @param {string} aId
   *        The ID of the add-on to retrieve
   * @returns {Addon?}
   */
  async getAddonByID(aId) {
    let aAddon = await XPIDatabase.getVisibleAddonForID(aId);
    return aAddon ? aAddon.wrapper : null;
  },

  /**
   * Synchronously returns the Addon object for the add-on with the
   * given ID.
   *
   * *DO NOT USE THIS IF YOU CAN AT ALL AVOID IT*
   *
   * This will always return null if the add-on database has not been
   * loaded, and the resulting Addon object may not yet include a
   * reference to its corresponding repository add-on object.
   *
   * @param {string} aId
   *        The ID of the add-on to return.
   * @returns {DBAddonInternal?}
   *        The Addon object, if available.
   */
  syncGetAddonByID(aId) {
    let aAddon = XPIDatabase.syncGetVisibleAddonForID(aId);
    return aAddon ? aAddon.wrapper : null;
  },

  /**
   * Called to get Addons of a particular type.
   *
   * @param {Array<string>?} aTypes
   *        An array of types to fetch. Can be null to get all types.
   * @returns {Addon[]}
   */
  async getAddonsByTypes(aTypes) {
    let typesToGet = getAllAliasesForTypes(aTypes);
    if (typesToGet && !typesToGet.some(type => ALL_EXTERNAL_TYPES.has(type))) {
      return [];
    }

    let addons = await XPIDatabase.getVisibleAddons(typesToGet);
    return addons.map(a => a.wrapper);
  },

  /**
   * Called to get active Addons of a particular type
   *
   * @param {Array<string>?} aTypes
   *        An array of types to fetch. Can be null to get all types.
   * @returns {Promise<Array<Addon>>}
   */
  async getActiveAddons(aTypes) {
    // If we already have the database loaded, returning full info is fast.
    if (this.isDBLoaded) {
      let addons = await this.getAddonsByTypes(aTypes);
      return {
        addons: addons.filter(addon => addon.isActive),
        fullData: true,
      };
    }

    // Construct addon-like objects with the information we already
    // have in memory.
    if (!XPIStates.db) {
      throw new Error("XPIStates not yet initialized");
    }

    let result = [];
    for (let addon of XPIStates.enabledAddons()) {
      if (aTypes && !aTypes.includes(addon.type)) {
        continue;
      }
      let location = this.installLocationsByName[addon.location.name];
      let scope, isSystem;
      if (location) {
        ({scope, isSystem} = location);
      }
      result.push({
        id: addon.id,
        version: addon.version,
        type: addon.type,
        updateDate: addon.lastModifiedTime,
        scope,
        isSystem,
        isWebExtension: isWebExtension(addon),
      });
    }

    return {addons: result, fullData: false};
  },


  /**
   * Obtain an Addon having the specified Sync GUID.
   *
   * @param {string} aGUID
   *        String GUID of add-on to retrieve
   * @returns {Addon?}
   */
  async getAddonBySyncGUID(aGUID) {
    let addon = await XPIDatabase.getAddonBySyncGUID(aGUID);
    return addon ? addon.wrapper : null;
  },

  /**
   * Called to get Addons that have pending operations.
   *
   * @param {Array<string>?} aTypes
   *        An array of types to fetch. Can be null to get all types
   * @returns {Addon[]}
   */
  async getAddonsWithOperationsByTypes(aTypes) {
    let typesToGet = getAllAliasesForTypes(aTypes);

    let aAddons = await XPIDatabase.getVisibleAddonsWithPendingOperations(typesToGet);
    let results = aAddons.map(a => a.wrapper);
    for (let install of XPIProvider.installs) {
      if (install.state == AddonManager.STATE_INSTALLED &&
          !(install.addon.inDatabase))
        results.push(install.addon.wrapper);
    }
    return results;
  },

  /**
   * Called to get the current AddonInstalls, optionally limiting to a list of
   * types.
   *
   * @param {Array<string>?} aTypes
   *        An array of types or null to get all types
   * @returns {AddonInstall[]}
   */
  getInstallsByTypes(aTypes) {
    let results = [...this.installs];
    if (aTypes) {
      results = results.filter(install => {
        return aTypes.includes(getExternalType(install.type));
      });
    }

    return results.map(install => install.wrapper);
  },

  /**
   * Called when a new add-on has been enabled when only one add-on of that type
   * can be enabled.
   *
   * @param {string} aId
   *        The ID of the newly enabled add-on
   * @param {string} aType
   *        The type of the newly enabled add-on
   */
  addonChanged(aId, aType) {
    // We only care about themes in this provider
    if (!isTheme(aType))
      return;

    let addons = XPIDatabase.getAddonsByType("webextension-theme");
    for (let theme of addons) {
      if (theme.visible && theme.id != aId)
        XPIDatabase.updateAddonDisabledState(theme, true, undefined, true);
    }

    if (!aId && (!LightweightThemeManager.currentTheme ||
                 LightweightThemeManager.currentTheme !== DEFAULT_THEME_ID)) {
      let theme = LightweightThemeManager.getUsedTheme(DEFAULT_THEME_ID);
      // This can only ever be null in tests.
      // This can all go away once lightweight themes are gone.
      if (theme) {
        LightweightThemeManager.currentTheme = theme;
      }
    }
  },

  /**
   * Update the appDisabled property for all add-ons.
   */
  updateAddonAppDisabledStates() {
    let addons = XPIDatabase.getAddons();
    for (let addon of addons) {
      XPIDatabase.updateAddonDisabledState(addon);
    }
  },

  /**
   * Update the repositoryAddon property for all add-ons.
   */
  async updateAddonRepositoryData() {
    let addons = await XPIDatabase.getVisibleAddons(null);
    logger.debug("updateAddonRepositoryData found " + addons.length + " visible add-ons");

    await Promise.all(addons.map(addon =>
      AddonRepository.getCachedAddonByID(addon.id).then(aRepoAddon => {
        if (aRepoAddon || AddonRepository.getCompatibilityOverridesSync(addon.id)) {
          logger.debug("updateAddonRepositoryData got info for " + addon.id);
          addon._repositoryAddon = aRepoAddon;
          XPIDatabase.updateAddonDisabledState(addon);
        }
      })));
  },

  onDebugConnectionChange({what, connection}) {
    if (what != "opened")
      return;

    for (let [id, val] of this.activeAddons) {
      connection.setAddonOptions(
        id, { global: val.bootstrapScope });
    }
  },

  /*
   * Notified when a preference we're interested in has changed.
   *
   * @see nsIObserver
   */
  observe(aSubject, aTopic, aData) {
    if (aTopic == NOTIFICATION_FLUSH_PERMISSIONS) {
      if (!aData || aData == XPI_PERMISSION) {
        this.importPermissions();
      }
      return;
    } else if (aTopic == NOTIFICATION_TOOLBOX_CONNECTION_CHANGE) {
      this.onDebugConnectionChange(aSubject.wrappedJSObject);
      return;
    }

    if (aTopic == "nsPref:changed") {
      switch (aData) {
      case PREF_EM_MIN_COMPAT_APP_VERSION:
        this.minCompatibleAppVersion = Services.prefs.getStringPref(PREF_EM_MIN_COMPAT_APP_VERSION,
                                                                    null);
        this.updateAddonAppDisabledStates();
        break;
      case PREF_EM_MIN_COMPAT_PLATFORM_VERSION:
        this.minCompatiblePlatformVersion = Services.prefs.getStringPref(PREF_EM_MIN_COMPAT_PLATFORM_VERSION,
                                                                         null);
        this.updateAddonAppDisabledStates();
        break;
      case PREF_XPI_SIGNATURES_REQUIRED:
      case PREF_LANGPACK_SIGNATURES:
      case PREF_ALLOW_LEGACY:
        this.updateAddonAppDisabledStates();
        break;
      }
    }
  },

  /**
   * Loads a bootstrapped add-on's bootstrap.js into a sandbox and the reason
   * values as constants in the scope. This will also add information about the
   * add-on to the bootstrappedAddons dictionary and notify the crash reporter
   * that new add-ons have been loaded.
   *
   * @param {string} aId
   *        The add-on's ID
   * @param {nsIFile} aFile
   *        The nsIFile for the add-on
   * @param {string} aVersion
   *        The add-on's version
   * @param {string} aType
   *        The type for the add-on
   * @param {boolean} aRunInSafeMode
   *        Boolean indicating whether the add-on can run in safe mode.
   * @param {string[]} aDependencies
   *        An array of add-on IDs on which this add-on depends.
   * @param {boolean} hasEmbeddedWebExtension
   *        Boolean indicating whether the add-on has an embedded webextension.
   * @param {integer?} [aReason]
   *        The reason this bootstrap is being loaded, as passed to a
   *        bootstrap method.
   */
  loadBootstrapScope(aId, aFile, aVersion, aType, aRunInSafeMode, aDependencies,
                     hasEmbeddedWebExtension, aReason) {
    this.activeAddons.set(aId, {
      bootstrapScope: null,
      // a Symbol passed to this add-on, which it can use to identify itself
      instanceID: Symbol(aId),
      started: false,
    });

    // Mark the add-on as active for the crash reporter before loading.
    // But not at app startup, since we'll already have added all of our
    // annotations before starting any loads.
    if (aReason !== BOOTSTRAP_REASONS.APP_STARTUP) {
      this.addAddonsToCrashReporter();
    }

    let activeAddon = this.activeAddons.get(aId);

    logger.debug("Loading bootstrap scope from " + aFile.path);

    let principal = Cc["@mozilla.org/systemprincipal;1"].
                    createInstance(Ci.nsIPrincipal);

    if (!aFile.exists()) {
      activeAddon.bootstrapScope =
        new Cu.Sandbox(principal, { sandboxName: aFile.path,
                                    addonId: aId,
                                    wantGlobalProperties: ["ChromeUtils"],
                                    metadata: { addonID: aId } });
      logger.error("Attempted to load bootstrap scope from missing directory " + aFile.path);
      return;
    }

    if (isWebExtension(aType)) {
      activeAddon.bootstrapScope = Extension.getBootstrapScope(aId, aFile);
    } else if (aType === "webextension-langpack") {
      activeAddon.bootstrapScope = Langpack.getBootstrapScope(aId, aFile);
    } else if (aType === "webextension-dictionary") {
      activeAddon.bootstrapScope = Dictionary.getBootstrapScope(aId, aFile);
    } else {
      let uri = getURIForResourceInFile(aFile, "bootstrap.js").spec;
      if (aType == "dictionary")
        uri = "resource://gre/modules/addons/SpellCheckDictionaryBootstrap.js";

      activeAddon.bootstrapScope =
        new Cu.Sandbox(principal, { sandboxName: uri,
                                    addonId: aId,
                                    wantGlobalProperties: ["ChromeUtils"],
                                    metadata: { addonID: aId, URI: uri } });

      try {
        // Copy the reason values from the global object into the bootstrap scope.
        for (let name in BOOTSTRAP_REASONS)
          activeAddon.bootstrapScope[name] = BOOTSTRAP_REASONS[name];

        // Add other stuff that extensions want.
        Object.assign(activeAddon.bootstrapScope, {Worker, ChromeWorker});

        // Define a console for the add-on
        XPCOMUtils.defineLazyGetter(
          activeAddon.bootstrapScope, "console",
          () => new ConsoleAPI({ consoleID: "addon/" + aId }));

        activeAddon.bootstrapScope.__SCRIPT_URI_SPEC__ = uri;
        Services.scriptloader.loadSubScript(uri, activeAddon.bootstrapScope);
      } catch (e) {
        logger.warn("Error loading bootstrap.js for " + aId, e);
      }
    }

    // Notify the BrowserToolboxProcess that a new addon has been loaded.
    let wrappedJSObject = { id: aId, options: { global: activeAddon.bootstrapScope }};
    Services.obs.notifyObservers({ wrappedJSObject }, "toolbox-update-addon-options");
  },

  /**
   * Unloads a bootstrap scope by dropping all references to it and then
   * updating the list of active add-ons with the crash reporter.
   *
   * @param {string} aId
   *        The add-on's ID
   */
  unloadBootstrapScope(aId) {
    this.activeAddons.delete(aId);
    this.addAddonsToCrashReporter();

    // Notify the BrowserToolboxProcess that an addon has been unloaded.
    let wrappedJSObject = { id: aId, options: { global: null }};
    Services.obs.notifyObservers({ wrappedJSObject }, "toolbox-update-addon-options");
  },

  /**
   * Calls a bootstrap method for an add-on.
   *
   * @param {Object} aAddon
   *        An object representing the add-on, with `id`, `type` and `version`
   * @param {nsIFile} aFile
   *        The nsIFile for the add-on
   * @param {string} aMethod
   *        The name of the bootstrap method to call
   * @param {integer} aReason
   *        The reason flag to pass to the bootstrap's startup method
   * @param {Object?} [aExtraParams]
   *        An object of additional key/value pairs to pass to the method in
   *        the params argument
   */
  callBootstrapMethod(aAddon, aFile, aMethod, aReason, aExtraParams) {
    if (!aAddon.id || !aAddon.version || !aAddon.type) {
      throw new Error("aAddon must include an id, version, and type");
    }

    // Only run in safe mode if allowed to
    let runInSafeMode = "runInSafeMode" in aAddon ? aAddon.runInSafeMode : canRunInSafeMode(aAddon);
    if (Services.appinfo.inSafeMode && !runInSafeMode)
      return;

    let timeStart = new Date();
    if (CHROME_TYPES.has(aAddon.type) && aMethod == "startup") {
      logger.debug("Registering manifest for " + aFile.path);
      Components.manager.addBootstrappedManifestLocation(aFile);
    }

    try {
      // Load the scope if it hasn't already been loaded
      let activeAddon = this.activeAddons.get(aAddon.id);
      if (!activeAddon) {
        this.loadBootstrapScope(aAddon.id, aFile, aAddon.version, aAddon.type,
                                runInSafeMode, aAddon.dependencies,
                                aAddon.hasEmbeddedWebExtension || false,
                                aReason);
        activeAddon = this.activeAddons.get(aAddon.id);
      }

      if (aMethod == "startup" || aMethod == "shutdown") {
        if (!aExtraParams) {
          aExtraParams = {};
        }
        aExtraParams.instanceID = this.activeAddons.get(aAddon.id).instanceID;
      }

      let method = undefined;
      let scope = activeAddon.bootstrapScope;
      try {
        method = scope[aMethod] || Cu.evalInSandbox(`${aMethod};`, scope);
      } catch (e) {
        // An exception will be caught if the expected method is not defined.
        // That will be logged below.
      }

      if (aMethod == "startup") {
        activeAddon.started = true;
      } else if (aMethod == "shutdown") {
        activeAddon.started = false;

        // Extensions are automatically deinitialized in the correct order at shutdown.
        if (aReason != BOOTSTRAP_REASONS.APP_SHUTDOWN) {
          activeAddon.disable = true;
          for (let addon of this.getDependentAddons(aAddon)) {
            if (addon.active)
              XPIDatabase.updateAddonDisabledState(addon);
          }
        }
      }

      let installLocation = aAddon._installLocation || null;
      let params = {
        id: aAddon.id,
        version: aAddon.version,
        installPath: aFile.clone(),
        resourceURI: getURIForResourceInFile(aFile, ""),
        signedState: aAddon.signedState,
        temporarilyInstalled: installLocation == TemporaryInstallLocation,
        builtIn: installLocation instanceof BuiltInInstallLocation,
      };

      if (aMethod == "startup" && aAddon.startupData) {
        params.startupData = aAddon.startupData;
      }

      if (aExtraParams) {
        for (let key in aExtraParams) {
          params[key] = aExtraParams[key];
        }
      }

      if (aAddon.hasEmbeddedWebExtension) {
        let reason = Object.keys(BOOTSTRAP_REASONS).find(
          key => BOOTSTRAP_REASONS[key] == aReason
        );

        if (aMethod == "startup") {
          const webExtension = LegacyExtensionsUtils.getEmbeddedExtensionFor(params);
          params.webExtension = {
            startup: () => webExtension.startup(reason),
          };
        } else if (aMethod == "shutdown") {
          LegacyExtensionsUtils.getEmbeddedExtensionFor(params).shutdown(reason);
        }
      }

      if (!method) {
        logger.warn("Add-on " + aAddon.id + " is missing bootstrap method " + aMethod);
      } else {
        logger.debug("Calling bootstrap method " + aMethod + " on " + aAddon.id + " version " +
                     aAddon.version);

        let result;
        try {
          result = method.call(scope, params, aReason);
        } catch (e) {
          logger.warn("Exception running bootstrap method " + aMethod + " on " + aAddon.id, e);
        }

        if (aMethod == "startup") {
          activeAddon.startupPromise = Promise.resolve(result);
          activeAddon.startupPromise.catch(Cu.reportError);
        }
      }
    } finally {
      // Extensions are automatically initialized in the correct order at startup.
      if (aMethod == "startup" && aReason != BOOTSTRAP_REASONS.APP_STARTUP) {
        for (let addon of this.getDependentAddons(aAddon))
          XPIDatabase.updateAddonDisabledState(addon);
      }

      if (CHROME_TYPES.has(aAddon.type) && aMethod == "shutdown" && aReason != BOOTSTRAP_REASONS.APP_SHUTDOWN) {
        logger.debug("Removing manifest for " + aFile.path);
        Components.manager.removeBootstrappedManifestLocation(aFile);
      }
      this.setTelemetry(aAddon.id, aMethod + "_MS", new Date() - timeStart);
    }
  },
};

for (let meth of ["cancelUninstallAddon", "getInstallForFile",
                  "getInstallForURL", "installAddonFromLocation",
                  "installAddonFromSources", "installTemporaryAddon",
                  "isInstallAllowed", "isInstallEnabled", "uninstallAddon",
                  "updateSystemAddons"]) {
  XPIProvider[meth] = function() {
    return XPIInstall[meth](...arguments);
  };
}

function forwardInstallMethods(cls, methods) {
  let {prototype} = cls;
  for (let meth of methods) {
    prototype[meth] = function() {
      return XPIInstall[cls.name].prototype[meth].apply(this, arguments);
    };
  }
}

/**
 * An object which identifies a directory install location for add-ons. The
 * location consists of a directory which contains the add-ons installed in the
 * location.
 *
 */
class DirectoryInstallLocation {
  /**
   * Each add-on installed in the location is either a directory containing the
   * add-on's files or a text file containing an absolute path to the directory
   * containing the add-ons files. The directory or text file must have the same
   * name as the add-on's ID.
   *
   * @param {string} aName
   *        The string identifier for the install location
   * @param {nsIFile} aDirectory
   *        The nsIFile directory for the install location
   * @param {integer} aScope
   *        The scope of add-ons installed in this location
  */
  constructor(aName, aDirectory, aScope) {
    this._name = aName;
    this.locked = true;
    this._directory = aDirectory;
    this._scope = aScope;
    this._IDToFileMap = {};
    this._linkedAddons = [];

    this.isSystem = (aName == KEY_APP_SYSTEM_ADDONS ||
                     aName == KEY_APP_SYSTEM_DEFAULTS);

    if (!aDirectory || !aDirectory.exists())
      return;
    if (!aDirectory.isDirectory())
      throw new Error("Location must be a directory.");

    this.initialized = false;
  }

  get path() {
    return this._directory && this._directory.path;
  }

  /**
   * Reads a directory linked to in a file.
   *
   * @param {nsIFile} aFile
   *        The file containing the directory path
   * @returns {nsIFile?}
   *        An nsIFile object representing the linked directory, or null
   *        on error.
   */
  _readDirectoryFromFile(aFile) {
    let linkedDirectory;
    if (aFile.isSymlink()) {
      linkedDirectory = aFile.clone();
      try {
        linkedDirectory.normalize();
      } catch (e) {
        logger.warn("Symbolic link " + aFile.path + " points to a path" +
             " which does not exist");
        return null;
      }
    } else {
      let fis = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
      fis.init(aFile, -1, -1, false);
      let line = { value: "" };
      if (fis instanceof Ci.nsILineInputStream)
        fis.readLine(line);
      fis.close();
      if (line.value) {
        linkedDirectory = Cc["@mozilla.org/file/local;1"].
                              createInstance(Ci.nsIFile);

        try {
          linkedDirectory.initWithPath(line.value);
        } catch (e) {
          linkedDirectory.setRelativeDescriptor(aFile.parent, line.value);
        }
      }
    }

    if (linkedDirectory) {
      if (!linkedDirectory.exists()) {
        logger.warn("File pointer " + aFile.path + " points to " + linkedDirectory.path +
             " which does not exist");
        return null;
      }

      if (!linkedDirectory.isDirectory()) {
        logger.warn("File pointer " + aFile.path + " points to " + linkedDirectory.path +
             " which is not a directory");
        return null;
      }

      return linkedDirectory;
    }

    logger.warn("File pointer " + aFile.path + " does not contain a path");
    return null;
  }

  /**
   * Finds all the add-ons installed in this location.
   *
   * @param {boolean} [rescan = false]
   *        True if the directory should be re-scanned, even if it has
   *        already been initialized.
   */
  _readAddons(rescan = false) {
    if ((this.initialized && !rescan) || !this._directory) {
      return;
    }
    this.initialized = true;

    // Use a snapshot of the directory contents to avoid possible issues with
    // iterating over a directory while removing files from it (the YAFFS2
    // embedded filesystem has this issue, see bug 772238).
    let entries = getDirectoryEntries(this._directory);
    for (let entry of entries) {
      let id = entry.leafName;

      if (id == DIR_STAGE || id == DIR_TRASH)
        continue;

      let directLoad = false;
      if (entry.isFile() &&
          id.substring(id.length - 4).toLowerCase() == ".xpi") {
        directLoad = true;
        id = id.substring(0, id.length - 4);
      }

      if (!gIDTest.test(id)) {
        logger.debug("Ignoring file entry whose name is not a valid add-on ID: " +
             entry.path);
        continue;
      }

      if (!directLoad && (entry.isFile() || entry.isSymlink())) {
        let newEntry = this._readDirectoryFromFile(entry);
        if (!newEntry) {
          logger.debug("Deleting stale pointer file " + entry.path);
          try {
            entry.remove(true);
          } catch (e) {
            logger.warn("Failed to remove stale pointer file " + entry.path, e);
            // Failing to remove the stale pointer file is ignorable
          }
          continue;
        }

        entry = newEntry;
        this._linkedAddons.push(id);
      }

      this._IDToFileMap[id] = entry;
    }
  }

  /**
   * Gets the name of this install location.
   */
  get name() {
    return this._name;
  }

  /**
   * Gets the scope of this install location.
   */
  get scope() {
    return this._scope;
  }

  /**
   * Gets an map of files for add-ons installed in this location.
   *
   * @param {boolean} [rescan = false]
   *        True if the directory should be re-scanned, even if it has
   *        already been initialized.
   *
   * @returns {Map<string, nsIFile>}
   *        A map of all add-ons in the location, with each add-on's ID
   *        as the key and an nsIFile for its location as the value.
   */
  getAddonLocations(rescan = false) {
    this._readAddons(rescan);

    let locations = new Map();
    for (let id in this._IDToFileMap) {
      locations.set(id, this._IDToFileMap[id].clone());
    }
    return locations;
  }

  /**
   * Gets the directory that the add-on with the given ID is installed in.
   *
   * @param {string} aId
   *        The ID of the add-on
   * @returns {nsIFile}
   * @throws if the ID does not match any of the add-ons installed
   */
  getLocationForID(aId) {
    if (!(aId in this._IDToFileMap))
      this._readAddons();

    if (aId in this._IDToFileMap)
      return this._IDToFileMap[aId].clone();
    throw new Error("Unknown add-on ID " + aId);
  }

  /**
   * Returns true if the given addon was installed in this location by a text
   * file pointing to its real path.
   *
   * @param {string} aId
   *        The ID of the addon
   * @returns {boolean}
   */
  isLinkedAddon(aId) {
    return this._linkedAddons.includes(aId);
  }
}

/**
 * An extension of DirectoryInstallLocation which adds methods to installing
 * and removing add-ons from the directory at runtime.
 */
class MutableDirectoryInstallLocation extends DirectoryInstallLocation {
  /**
   * @param {string} aName
   *        The string identifier for the install location
   * @param {nsIFile} aDirectory
   *        The nsIFile directory for the install location
   * @param {integer} aScope
   *        The scope of add-ons installed in this location
   */
  constructor(aName, aDirectory, aScope) {
    super(aName, aDirectory, aScope);

    this.locked = false;
    this._stagingDirLock = 0;
  }
}
forwardInstallMethods(MutableDirectoryInstallLocation,
                      ["cleanStagingDir", "getStagingDir", "getTrashDir",
                       "installAddon", "releaseStagingDir", "requestStagingDir",
                       "uninstallAddon"]);

/**
 * An object which identifies a built-in install location for add-ons, such
 * as default system add-ons.
 *
 * This location should point either to a XPI, or a directory in a local build.
 */
class BuiltInInstallLocation extends DirectoryInstallLocation {
  /**
   * Read the manifest of allowed add-ons and build a mapping between ID and URI
   * for each.
   */
  _readAddons() {
    let manifest;
    try {
      let url = Services.io.newURI(BUILT_IN_ADDONS_URI);
      let data = Cu.readUTF8URI(url);
      manifest = JSON.parse(data);
    } catch (e) {
      logger.warn("List of valid built-in add-ons could not be parsed.", e);
      return;
    }

    if (!("system" in manifest)) {
      logger.warn("No list of valid system add-ons found.");
      return;
    }

    for (let id of manifest.system) {
      let file = new FileUtils.File(this._directory.path);
      file.append(`${id}.xpi`);

      // Only attempt to load unpacked directory if unofficial build.
      if (!AppConstants.MOZILLA_OFFICIAL && !file.exists()) {
        file = new FileUtils.File(this._directory.path);
        file.append(`${id}`);
      }

      this._IDToFileMap[id] = file;
    }
  }
}

/**
 * An object which identifies a directory install location for system add-ons
 * updates.
 */
class SystemAddonInstallLocation extends MutableDirectoryInstallLocation {
  /**
    * The location consists of a directory which contains the add-ons installed.
    *
    * @param {string} aName
    *        The string identifier for the install location
    * @param {nsIFile} aDirectory
    *        The nsIFile directory for the install location
    * @param {integer} aScope
    *        The scope of add-ons installed in this location
    * @param {boolean} aResetSet
    *        True to throw away the current add-on set
    */
  constructor(aName, aDirectory, aScope, aResetSet) {
    let addonSet = SystemAddonInstallLocation._loadAddonSet();
    let directory = null;

    // The system add-on update directory is stored in a pref.
    // Therefore, this is looked up before calling the
    // constructor on the superclass.
    if (addonSet.directory) {
      directory = getFile(addonSet.directory, aDirectory);
      logger.info("SystemAddonInstallLocation scanning directory " + directory.path);
    } else {
      logger.info("SystemAddonInstallLocation directory is missing");
    }

    super(aName, directory, aScope);

    this._addonSet = addonSet;
    this._baseDir = aDirectory;
    this._nextDir = null;
    this._directory = directory;

    this._stagingDirLock = 0;

    if (aResetSet) {
      this.resetAddonSet();
    }

    this.locked = false;
  }

  /**
   * Reads the current set of system add-ons
   *
   * @returns {Object}
   */
  static _loadAddonSet() {
    try {
      let setStr = Services.prefs.getStringPref(PREF_SYSTEM_ADDON_SET, null);
      if (setStr) {
        let addonSet = JSON.parse(setStr);
        if ((typeof addonSet == "object") && addonSet.schema == 1) {
          return addonSet;
        }
      }
    } catch (e) {
      logger.error("Malformed system add-on set, resetting.");
    }

    return { schema: 1, addons: {} };
  }

  getAddonLocations() {
    // Updated system add-ons are ignored in safe mode
    if (Services.appinfo.inSafeMode) {
      return new Map();
    }

    let addons = super.getAddonLocations();

    // Strip out any unexpected add-ons from the list
    for (let id of addons.keys()) {
      if (!(id in this._addonSet.addons)) {
        addons.delete(id);
      }
    }

    return addons;
  }

  /**
   * Tests whether updated system add-ons are expected.
   *
   * @returns {boolean}
   */
  isActive() {
    return this._directory != null;
  }
}

forwardInstallMethods(SystemAddonInstallLocation,
                      ["cleanDirectories", "cleanStagingDir", "getStagingDir",
                       "getTrashDir", "installAddon", "installAddon",
                       "installAddonSet", "isValid", "isValidAddon",
                       "releaseStagingDir", "requestStagingDir",
                       "resetAddonSet", "resumeAddonSet", "uninstallAddon",
                       "uninstallAddon"]);

/** An object which identifies an install location for temporary add-ons.
 */
const TemporaryInstallLocation = { locked: false, name: KEY_APP_TEMPORARY,
  scope: AddonManager.SCOPE_TEMPORARY,
  getAddonLocations: () => [], isLinkedAddon: () => false, installAddon:
    () => {}, uninstallAddon: (aAddon) => {}, getStagingDir: () => {},
};

/**
 * An object that identifies a registry install location for add-ons. The location
 * consists of a registry key which contains string values mapping ID to the
 * path where an add-on is installed
 *
 */
class WinRegInstallLocation extends DirectoryInstallLocation {
  /**
    * @param {string} aName
    *        The string identifier of this Install Location.
    * @param {integer} aRootKey
    *        The root key (one of the ROOT_KEY_ values from nsIWindowsRegKey).
    * @param {integer} aScope
    *        The scope of add-ons installed in this location
    */
  constructor(aName, aRootKey, aScope) {
    super(aName, undefined, aScope);

    this.locked = true;
    this._name = aName;
    this._rootKey = aRootKey;
    this._scope = aScope;
    this._IDToFileMap = {};

    let path = this._appKeyPath + "\\Extensions";
    let key = Cc["@mozilla.org/windows-registry-key;1"].
              createInstance(Ci.nsIWindowsRegKey);

    // Reading the registry may throw an exception, and that's ok.  In error
    // cases, we just leave ourselves in the empty state.
    try {
      key.open(this._rootKey, path, Ci.nsIWindowsRegKey.ACCESS_READ);
    } catch (e) {
      return;
    }

    this._readAddons(key);
    key.close();
  }

  /**
   * Retrieves the path of this Application's data key in the registry.
   */
  get _appKeyPath() {
    let appVendor = Services.appinfo.vendor;
    let appName = Services.appinfo.name;

    // XXX Thunderbird doesn't specify a vendor string
    if (AppConstants.MOZ_APP_NAME == "thunderbird" && appVendor == "")
      appVendor = "Mozilla";

    // XULRunner-based apps may intentionally not specify a vendor
    if (appVendor != "")
      appVendor += "\\";

    return "SOFTWARE\\" + appVendor + appName;
  }

  /**
   * Read the registry and build a mapping between ID and path for each
   * installed add-on.
   *
   * @param {nsIWindowsRegKey} aKey
   *        The key that contains the ID to path mapping
   */
  _readAddons(aKey) {
    let count = aKey.valueCount;
    for (let i = 0; i < count; ++i) {
      let id = aKey.getValueName(i);

      let file = new nsIFile(aKey.readStringValue(id));

      if (!file.exists()) {
        logger.warn("Ignoring missing add-on in " + file.path);
        continue;
      }

      this._IDToFileMap[id] = file;
    }
  }

  /**
   * Gets the name of this install location.
   */
  get name() {
    return this._name;
  }

  /*
   * @see DirectoryInstallLocation
   */
  isLinkedAddon(aId) {
    return true;
  }
}

var XPIInternal = {
  BOOTSTRAP_REASONS,
  DB_SCHEMA,
  KEY_APP_SYSTEM_ADDONS,
  KEY_APP_SYSTEM_DEFAULTS,
  KEY_APP_TEMPORARY,
  PREF_BRANCH_INSTALLED_ADDON,
  PREF_SYSTEM_ADDON_SET,
  SIGNED_TYPES,
  SystemAddonInstallLocation,
  TEMPORARY_ADDON_SUFFIX,
  TOOLKIT_ID,
  TemporaryInstallLocation,
  XPIProvider,
  XPIStates,
  XPI_PERMISSION,
  awaitPromise,
  canRunInSafeMode,
  descriptorToPath,
  getExternalType,
  getURIForResourceInFile,
  isTheme,
  isWebExtension,
};

var addonTypes = [
  new AddonManagerPrivate.AddonType("extension", URI_EXTENSION_STRINGS,
                                    "type.extension.name",
                                    AddonManager.VIEW_TYPE_LIST, 4000,
                                    AddonManager.TYPE_SUPPORTS_UNDO_RESTARTLESS_UNINSTALL),
  new AddonManagerPrivate.AddonType("theme", URI_EXTENSION_STRINGS,
                                    "type.themes.name",
                                    AddonManager.VIEW_TYPE_LIST, 5000),
  new AddonManagerPrivate.AddonType("dictionary", URI_EXTENSION_STRINGS,
                                    "type.dictionary.name",
                                    AddonManager.VIEW_TYPE_LIST, 7000,
                                    AddonManager.TYPE_UI_HIDE_EMPTY | AddonManager.TYPE_SUPPORTS_UNDO_RESTARTLESS_UNINSTALL),
  new AddonManagerPrivate.AddonType("locale", URI_EXTENSION_STRINGS,
                                    "type.locale.name",
                                    AddonManager.VIEW_TYPE_LIST, 8000,
                                    AddonManager.TYPE_UI_HIDE_EMPTY | AddonManager.TYPE_SUPPORTS_UNDO_RESTARTLESS_UNINSTALL),
];

AddonManagerPrivate.registerProvider(XPIProvider, addonTypes);
