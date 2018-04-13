 /* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["XPIProvider", "XPIInternal"];

/* globals WebExtensionPolicy */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/AddonManager.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonRepository: "resource://gre/modules/addons/AddonRepository.jsm",
  AddonSettings: "resource://gre/modules/addons/AddonSettings.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  Extension: "resource://gre/modules/Extension.jsm",
  Langpack: "resource://gre/modules/Extension.jsm",
  LightweightThemeManager: "resource://gre/modules/LightweightThemeManager.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  ZipUtils: "resource://gre/modules/ZipUtils.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  PermissionsUtils: "resource://gre/modules/PermissionsUtils.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  ConsoleAPI: "resource://gre/modules/Console.jsm",
  ProductAddonChecker: "resource://gre/modules/addons/ProductAddonChecker.jsm",
  UpdateUtils: "resource://gre/modules/UpdateUtils.jsm",
  JSONFile: "resource://gre/modules/JSONFile.jsm",
  LegacyExtensionsUtils: "resource://gre/modules/LegacyExtensionsUtils.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  clearTimeout: "resource://gre/modules/Timer.jsm",

  DownloadAddonInstall: "resource://gre/modules/addons/XPIInstall.jsm",
  LocalAddonInstall: "resource://gre/modules/addons/XPIInstall.jsm",
  UpdateChecker: "resource://gre/modules/addons/XPIInstall.jsm",
  XPIInstall: "resource://gre/modules/addons/XPIInstall.jsm",
  loadManifestFromFile: "resource://gre/modules/addons/XPIInstall.jsm",
  verifyBundleSignedState: "resource://gre/modules/addons/XPIInstall.jsm",
});

const {nsIBlocklistService} = Ci;

XPCOMUtils.defineLazyServiceGetters(this, {
  Blocklist: ["@mozilla.org/extensions/blocklist;1", "nsIBlocklistService"],
  AddonPolicyService: ["@mozilla.org/addons/policy-service;1", "nsIAddonPolicyService"],
  aomStartup: ["@mozilla.org/addons/addon-manager-startup;1", "amIAddonManagerStartup"],
});

XPCOMUtils.defineLazyGetter(this, "gTextDecoder", () => {
  return new TextDecoder();
});

Cu.importGlobalProperties(["URL"]);

const nsIFile = Components.Constructor("@mozilla.org/file/local;1", "nsIFile",
                                       "initWithPath");

const PREF_DB_SCHEMA                  = "extensions.databaseSchema";
const PREF_XPI_STATE                  = "extensions.xpiState";
const PREF_BLOCKLIST_ITEM_URL         = "extensions.blocklist.itemURL";
const PREF_BOOTSTRAP_ADDONS           = "extensions.bootstrappedAddons";
const PREF_PENDING_OPERATIONS         = "extensions.pendingOperations";
const PREF_EM_EXTENSION_FORMAT        = "extensions.";
const PREF_EM_ENABLED_SCOPES          = "extensions.enabledScopes";
const PREF_EM_STARTUP_SCAN_SCOPES     = "extensions.startupScanScopes";
const PREF_EM_SHOW_MISMATCH_UI        = "extensions.showMismatchUI";
const PREF_XPI_ENABLED                = "xpinstall.enabled";
const PREF_XPI_WHITELIST_REQUIRED     = "xpinstall.whitelist.required";
const PREF_XPI_DIRECT_WHITELISTED     = "xpinstall.whitelist.directRequest";
const PREF_XPI_FILE_WHITELISTED       = "xpinstall.whitelist.fileRequest";
// xpinstall.signatures.required only supported in dev builds
const PREF_XPI_SIGNATURES_REQUIRED    = "xpinstall.signatures.required";
const PREF_XPI_SIGNATURES_DEV_ROOT    = "xpinstall.signatures.dev-root";
const PREF_LANGPACK_SIGNATURES        = "extensions.langpacks.signatures.required";
const PREF_XPI_PERMISSIONS_BRANCH     = "xpinstall.";
const PREF_INSTALL_REQUIRESECUREORIGIN = "extensions.install.requireSecureOrigin";
const PREF_INSTALL_DISTRO_ADDONS      = "extensions.installDistroAddons";
const PREF_BRANCH_INSTALLED_ADDON     = "extensions.installedDistroAddon.";
const PREF_DISTRO_ADDONS_PERMS        = "extensions.distroAddons.promptForPermissions";
const PREF_SYSTEM_ADDON_SET           = "extensions.systemAddonSet";
const PREF_SYSTEM_ADDON_UPDATE_URL    = "extensions.systemAddon.update.url";
const PREF_ALLOW_LEGACY               = "extensions.legacy.enabled";

const PREF_EM_MIN_COMPAT_APP_VERSION      = "extensions.minCompatibleAppVersion";
const PREF_EM_MIN_COMPAT_PLATFORM_VERSION = "extensions.minCompatiblePlatformVersion";

const PREF_EM_LAST_APP_BUILD_ID       = "extensions.lastAppBuildId";

const DEFAULT_SKIN = "classic/1.0";

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

const ADDON_ID_DEFAULT_THEME          = "{972ce4c6-7e08-4474-a285-3208198ce6fd}";

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

const TEMPORARY_ADDON_SUFFIX = "@temporary-addon";

const STARTUP_MTIME_SCOPES = [KEY_APP_GLOBAL,
                              KEY_APP_SYSTEM_LOCAL,
                              KEY_APP_SYSTEM_SHARE,
                              KEY_APP_SYSTEM_USER];

const NOTIFICATION_FLUSH_PERMISSIONS  = "flush-pending-permissions";
const XPI_PERMISSION                  = "install";

const TOOLKIT_ID                      = "toolkit@mozilla.org";

const XPI_SIGNATURE_CHECK_PERIOD      = 24 * 60 * 60;

XPCOMUtils.defineConstant(this, "DB_SCHEMA", 24);

const NOTIFICATION_TOOLBOX_CONNECTION_CHANGE      = "toolbox-connection-change";

// Properties that exist in the install manifest
const PROP_LOCALE_SINGLE = ["name", "description", "creator", "homepageURL"];
const PROP_LOCALE_MULTI  = ["developers", "translators", "contributors"];

// Properties to cache and reload when an addon installation is pending
const PENDING_INSTALL_METADATA =
    ["syncGUID", "targetApplications", "userDisabled", "softDisabled",
     "existingAddonID", "sourceURI", "releaseNotesURI", "installDate",
     "updateDate", "applyBackgroundUpdates", "compatibilityOverrides"];

// Note: When adding/changing/removing items here, remember to change the
// DB schema version to ensure changes are picked up ASAP.
const STATIC_BLOCKLIST_PATTERNS = [
  { creator: "Mozilla Corp.",
    level: nsIBlocklistService.STATE_BLOCKED,
    blockID: "i162" },
  { creator: "Mozilla.org",
    level: nsIBlocklistService.STATE_BLOCKED,
    blockID: "i162" }
];

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
  "webextension-theme": "theme",
  "webextension-langpack": "locale",
};

const CHROME_TYPES = new Set([
  "extension",
  "experiment",
]);

const SIGNED_TYPES = new Set([
  "extension",
  "experiment",
  "webextension",
  "webextension-langpack",
  "webextension-theme",
]);

const LEGACY_TYPES = new Set([
  "extension",
  "theme",
]);

const ALL_EXTERNAL_TYPES = new Set([
  "dictionary",
  "extension",
  "experiment",
  "locale",
  "theme",
]);

// Whether add-on signing is required.
function mustSign(aType) {
  if (!SIGNED_TYPES.has(aType))
    return false;

  if (aType == "webextension-langpack") {
    return AddonSettings.LANGPACKS_REQUIRE_SIGNING;
  }

  return AddonSettings.REQUIRE_SIGNING;
}

// Keep track of where we are in startup for telemetry
// event happened during XPIDatabase.startup()
const XPI_STARTING = "XPIStarting";
// event happened after startup() but before the final-ui-startup event
const XPI_BEFORE_UI_STARTUP = "BeforeFinalUIStartup";
// event happened after final-ui-startup
const XPI_AFTER_UI_STARTUP = "AfterFinalUIStartup";

const COMPATIBLE_BY_DEFAULT_TYPES = {
  extension: true,
  dictionary: true
};

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

const LAZY_OBJECTS = ["XPIDatabase", "XPIDatabaseReconcile"];
/* globals XPIDatabase, XPIDatabaseReconcile*/

var gLazyObjectsLoaded = false;

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

function loadLazyObjects() {
  let uri = "resource://gre/modules/addons/XPIProviderUtils.js";
  let scope = Cu.Sandbox(Services.scriptSecurityManager.getSystemPrincipal(), {
    sandboxName: uri,
    wantGlobalProperties: ["ChromeUtils", "TextDecoder"],
  });

  Object.assign(scope, {
    ADDON_SIGNING: AddonSettings.ADDON_SIGNING,
    SIGNED_TYPES,
    BOOTSTRAP_REASONS,
    DB_SCHEMA,
    DEFAULT_SKIN,
    AddonInternal,
    XPIProvider,
    XPIStates,
    syncLoadManifestFromFile,
    isUsableAddon,
    recordAddonTelemetry,
    flushChromeCaches: XPIInstall.flushChromeCaches,
    descriptorToPath,
  });

  Services.scriptloader.loadSubScript(uri, scope);

  for (let name of LAZY_OBJECTS) {
    delete gGlobalScope[name];
    gGlobalScope[name] = scope[name];
  }
  gLazyObjectsLoaded = true;
  return scope;
}

LAZY_OBJECTS.forEach(name => {
  Object.defineProperty(gGlobalScope, name, {
    get() {
      let objs = loadLazyObjects();
      return objs[name];
    },
    configurable: true
  });
});

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


// Behaves like Promise.all except waits for all promises to resolve/reject
// before resolving/rejecting itself
function waitForAllPromises(promises) {
  return new Promise((resolve, reject) => {
    let shouldReject = false;
    let rejectValue = null;

    let newPromises = promises.map(
      p => p.catch(value => {
        shouldReject = true;
        rejectValue = value;
      })
    );
    Promise.all(newPromises)
           .then((results) => shouldReject ? reject(rejectValue) : resolve(results));
  });
}

function findMatchingStaticBlocklistItem(aAddon) {
  for (let item of STATIC_BLOCKLIST_PATTERNS) {
    if ("creator" in item && typeof item.creator == "string") {
      if ((aAddon.defaultLocale && aAddon.defaultLocale.creator == item.creator) ||
          (aAddon.selectedLocale && aAddon.selectedLocale.creator == item.creator)) {
        return item;
      }
    }
  }
  return null;
}

/**
 * Determine the reason to pass to an extension's bootstrap methods when
 * switch between versions.
 *
 * @param {string} oldVersion The version of the existing extension instance.
 * @param {string} newVersion The version of the extension being installed.
 *
 * @return {BOOSTRAP_REASONS.ADDON_UPGRADE|BOOSTRAP_REASONS.ADDON_DOWNGRADE}
 */
function newVersionReason(oldVersion, newVersion) {
  return Services.vc.compare(oldVersion, newVersion) <= 0 ?
         BOOTSTRAP_REASONS.ADDON_UPGRADE :
         BOOTSTRAP_REASONS.ADDON_DOWNGRADE;
}

/**
 * Converts an iterable of addon objects into a map with the add-on's ID as key.
 */
function addonMap(addons) {
  return new Map(addons.map(a => [a.id, a]));
}

/**
 * Helper function that determines whether an addon of a certain type is a
 * WebExtension.
 *
 * @param  {String} type
 * @return {Boolean}
 */
function isWebExtension(type) {
  return type == "webextension" || type == "webextension-theme";
}

var gThemeAliases = null;
/**
 * Helper function that determines whether an addon of a certain type is a
 * theme.
 *
 * @param  {String} type
 * @return {Boolean}
 */
function isTheme(type) {
  if (!gThemeAliases)
    gThemeAliases = getAllAliasesForTypes(["theme"]);
  return gThemeAliases.includes(type);
}

/**
 * Sets permissions on a file
 *
 * @param  aFile
 *         The file or directory to operate on.
 * @param  aPermissions
 *         The permissions to set
 */
function setFilePermissions(aFile, aPermissions) {
  try {
    aFile.permissions = aPermissions;
  } catch (e) {
    logger.warn("Failed to set permissions " + aPermissions.toString(8) + " on " +
         aFile.path, e);
  }
}

/**
 * Write a given string to a file
 *
 * @param  file
 *         The nsIFile instance to write into
 * @param  string
 *         The string to write
 */
function writeStringToFile(file, string) {
  let stream = Cc["@mozilla.org/network/file-output-stream;1"].
               createInstance(Ci.nsIFileOutputStream);
  let converter = Cc["@mozilla.org/intl/converter-output-stream;1"].
                  createInstance(Ci.nsIConverterOutputStream);

  try {
    stream.init(file, FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE |
                            FileUtils.MODE_TRUNCATE, FileUtils.PERMS_FILE,
                           0);
    converter.init(stream, "UTF-8");
    converter.writeString(string);
  } finally {
    converter.close();
    stream.close();
  }
}

/**
 * A safe way to install a file or the contents of a directory to a new
 * directory. The file or directory is moved or copied recursively and if
 * anything fails an attempt is made to rollback the entire operation. The
 * operation may also be rolled back to its original state after it has
 * completed by calling the rollback method.
 *
 * Operations can be chained. Calling move or copy multiple times will remember
 * the whole set and if one fails all of the operations will be rolled back.
 */
function SafeInstallOperation() {
  this._installedFiles = [];
  this._createdDirs = [];
}

SafeInstallOperation.prototype = {
  _installedFiles: null,
  _createdDirs: null,

  _installFile(aFile, aTargetDirectory, aCopy) {
    let oldFile = aCopy ? null : aFile.clone();
    let newFile = aFile.clone();
    try {
      if (aCopy) {
        newFile.copyTo(aTargetDirectory, null);
        // copyTo does not update the nsIFile with the new.
        newFile = getFile(aFile.leafName, aTargetDirectory);
        // Windows roaming profiles won't properly sync directories if a new file
        // has an older lastModifiedTime than a previous file, so update.
        newFile.lastModifiedTime = Date.now();
      } else {
        newFile.moveTo(aTargetDirectory, null);
      }
    } catch (e) {
      logger.error("Failed to " + (aCopy ? "copy" : "move") + " file " + aFile.path +
            " to " + aTargetDirectory.path, e);
      throw e;
    }
    this._installedFiles.push({ oldFile, newFile });
  },

  _installDirectory(aDirectory, aTargetDirectory, aCopy) {
    if (aDirectory.contains(aTargetDirectory)) {
      let err = new Error(`Not installing ${aDirectory} into its own descendent ${aTargetDirectory}`);
      logger.error(err);
      throw err;
    }

    let newDir = getFile(aDirectory.leafName, aTargetDirectory);
    try {
      newDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    } catch (e) {
      logger.error("Failed to create directory " + newDir.path, e);
      throw e;
    }
    this._createdDirs.push(newDir);

    // Use a snapshot of the directory contents to avoid possible issues with
    // iterating over a directory while removing files from it (the YAFFS2
    // embedded filesystem has this issue, see bug 772238), and to remove
    // normal files before their resource forks on OSX (see bug 733436).
    let entries = getDirectoryEntries(aDirectory, true);
    for (let entry of entries) {
      try {
        this._installDirEntry(entry, newDir, aCopy);
      } catch (e) {
        logger.error("Failed to " + (aCopy ? "copy" : "move") + " entry " +
                     entry.path, e);
        throw e;
      }
    }

    // If this is only a copy operation then there is nothing else to do
    if (aCopy)
      return;

    // The directory should be empty by this point. If it isn't this will throw
    // and all of the operations will be rolled back
    try {
      setFilePermissions(aDirectory, FileUtils.PERMS_DIRECTORY);
      aDirectory.remove(false);
    } catch (e) {
      logger.error("Failed to remove directory " + aDirectory.path, e);
      throw e;
    }

    // Note we put the directory move in after all the file moves so the
    // directory is recreated before all the files are moved back
    this._installedFiles.push({ oldFile: aDirectory, newFile: newDir });
  },

  _installDirEntry(aDirEntry, aTargetDirectory, aCopy) {
    let isDir = null;

    try {
      isDir = aDirEntry.isDirectory() && !aDirEntry.isSymlink();
    } catch (e) {
      // If the file has already gone away then don't worry about it, this can
      // happen on OSX where the resource fork is automatically moved with the
      // data fork for the file. See bug 733436.
      if (e.result == Cr.NS_ERROR_FILE_TARGET_DOES_NOT_EXIST)
        return;

      logger.error("Failure " + (aCopy ? "copying" : "moving") + " " + aDirEntry.path +
            " to " + aTargetDirectory.path);
      throw e;
    }

    try {
      if (isDir)
        this._installDirectory(aDirEntry, aTargetDirectory, aCopy);
      else
        this._installFile(aDirEntry, aTargetDirectory, aCopy);
    } catch (e) {
      logger.error("Failure " + (aCopy ? "copying" : "moving") + " " + aDirEntry.path +
            " to " + aTargetDirectory.path);
      throw e;
    }
  },

  /**
   * Moves a file or directory into a new directory. If an error occurs then all
   * files that have been moved will be moved back to their original location.
   *
   * @param  aFile
   *         The file or directory to be moved.
   * @param  aTargetDirectory
   *         The directory to move into, this is expected to be an empty
   *         directory.
   */
  moveUnder(aFile, aTargetDirectory) {
    try {
      this._installDirEntry(aFile, aTargetDirectory, false);
    } catch (e) {
      this.rollback();
      throw e;
    }
  },

  /**
   * Renames a file to a new location.  If an error occurs then all
   * files that have been moved will be moved back to their original location.
   *
   * @param  aOldLocation
   *         The old location of the file.
   * @param  aNewLocation
   *         The new location of the file.
   */
  moveTo(aOldLocation, aNewLocation) {
    try {
      let oldFile = aOldLocation.clone(), newFile = aNewLocation.clone();
      oldFile.moveTo(newFile.parent, newFile.leafName);
      this._installedFiles.push({ oldFile, newFile, isMoveTo: true});
    } catch (e) {
      this.rollback();
      throw e;
    }
  },

  /**
   * Copies a file or directory into a new directory. If an error occurs then
   * all new files that have been created will be removed.
   *
   * @param  aFile
   *         The file or directory to be copied.
   * @param  aTargetDirectory
   *         The directory to copy into, this is expected to be an empty
   *         directory.
   */
  copy(aFile, aTargetDirectory) {
    try {
      this._installDirEntry(aFile, aTargetDirectory, true);
    } catch (e) {
      this.rollback();
      throw e;
    }
  },

  /**
   * Rolls back all the moves that this operation performed. If an exception
   * occurs here then both old and new directories are left in an indeterminate
   * state
   */
  rollback() {
    while (this._installedFiles.length > 0) {
      let move = this._installedFiles.pop();
      if (move.isMoveTo) {
        move.newFile.moveTo(move.oldDir.parent, move.oldDir.leafName);
      } else if (move.newFile.isDirectory() && !move.newFile.isSymlink()) {
        let oldDir = getFile(move.oldFile.leafName, move.oldFile.parent);
        oldDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
      } else if (!move.oldFile) {
        // No old file means this was a copied file
        move.newFile.remove(true);
      } else {
        move.newFile.moveTo(move.oldFile.parent, null);
      }
    }

    while (this._createdDirs.length > 0)
      XPIInstall.recursiveRemove(this._createdDirs.pop());
  }
};

/**
 * Evaluates whether an add-on is allowed to run in safe mode.
 *
 * @param  aAddon
 *         The add-on to check
 * @return true if the add-on should run in safe mode
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
 * Determine if this addon should be disabled due to being legacy
 *
 * @param {Addon} addon The addon to check
 *
 * @returns {boolean} Whether the addon should be disabled for being legacy
 */
function isDisabledLegacy(addon) {
  return (!AddonSettings.ALLOW_LEGACY_EXTENSIONS &&
          LEGACY_TYPES.has(addon.type) &&

          // Legacy add-ons are allowed in the system location.
          !addon._installLocation.isSystem &&

          // Legacy extensions may be installed temporarily in
          // non-release builds.
          !(AppConstants.MOZ_ALLOW_LEGACY_EXTENSIONS &&
            addon._installLocation.name == KEY_APP_TEMPORARY) &&

          // Properly signed legacy extensions are allowed.
          addon.signedState !== AddonManager.SIGNEDSTATE_PRIVILEGED);
}

/**
 * Calculates whether an add-on should be appDisabled or not.
 *
 * @param  aAddon
 *         The add-on to check
 * @return true if the add-on should not be appDisabled
 */
function isUsableAddon(aAddon) {
  if (aAddon.type == "theme")
    return aAddon.internalName == DEFAULT_SKIN;

  if (mustSign(aAddon.type) && !aAddon.isCorrectlySigned) {
    logger.warn(`Add-on ${aAddon.id} is not correctly signed.`);
    if (Services.prefs.getBoolPref(PREF_XPI_SIGNATURES_DEV_ROOT, false)) {
      logger.warn(`Preference ${PREF_XPI_SIGNATURES_DEV_ROOT} is set.`);
    }
    return false;
  }

  if (aAddon.blocklistState == nsIBlocklistService.STATE_BLOCKED) {
    logger.warn(`Add-on ${aAddon.id} is blocklisted.`);
    return false;
  }

  // If we can't read it, it's not usable:
  if (aAddon.brokenManifest) {
    return false;
  }

  // Experiments are installed through an external mechanism that
  // limits target audience to compatible clients. We trust it knows what
  // it's doing and skip compatibility checks.
  //
  // This decision does forfeit defense in depth. If the experiments system
  // is ever wrong about targeting an add-on to a specific application
  // or platform, the client will likely see errors.
  if (aAddon.type == "experiment")
    return true;

  if (AddonManager.checkUpdateSecurity && !aAddon.providesUpdatesSecurely) {
    logger.warn(`Updates for add-on ${aAddon.id} must be provided over HTTPS.`);
    return false;
  }


  if (!aAddon.isPlatformCompatible) {
    logger.warn(`Add-on ${aAddon.id} is not compatible with platform.`);
    return false;
  }

  if (aAddon.dependencies.length) {
    let isActive = id => {
      let active = XPIProvider.activeAddons.get(id);
      return active && !active.disable;
    };

    if (aAddon.dependencies.some(id => !isActive(id)))
      return false;
  }

  if (isDisabledLegacy(aAddon)) {
    logger.warn(`disabling legacy extension ${aAddon.id}`);
    return false;
  }

  if (AddonManager.checkCompatibility) {
    if (!aAddon.isCompatible) {
      logger.warn(`Add-on ${aAddon.id} is not compatible with application version.`);
      return false;
    }
  } else {
    let app = aAddon.matchingTargetApplication;
    if (!app) {
      logger.warn(`Add-on ${aAddon.id} is not compatible with target application.`);
      return false;
    }
  }

  return true;
}

/**
 * Converts an internal add-on type to the type presented through the API.
 *
 * @param  aType
 *         The internal add-on type
 * @return an external add-on type
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
 * @param  aTypes
 *         An array of types or null for all types
 * @return an array of types or null for all types
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
 * A synchronous method for loading an add-on's manifest. This should only ever
 * be used during startup or a sync load of the add-ons DB
 */
function syncLoadManifestFromFile(aFile, aInstallLocation) {
  let success = undefined;
  let result = null;

  loadManifestFromFile(aFile, aInstallLocation).then(val => {
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
 * Gets an nsIURI for a file within another file, either a directory or an XPI
 * file. If aFile is a directory then this will return a file: URI, if it is an
 * XPI file then it will return a jar: URI.
 *
 * @param  aFile
 *         The file containing the resources, must be either a directory or an
 *         XPI file
 * @param  aPath
 *         The path to find the resource at, "/" separated. If aPath is empty
 *         then the uri to the root of the contained files will be returned
 * @return an nsIURI pointing at the resource
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
 * @param  aJarfile
 *         The ZIP file as an nsIFile
 * @param  aPath
 *         The path inside the ZIP file
 * @return an nsIURI for the file
 */
function buildJarURI(aJarfile, aPath) {
  let uri = Services.io.newFileURI(aJarfile);
  uri = "jar:" + uri.spec + "!/" + aPath;
  return Services.io.newURI(uri);
}

/**
 * Gets a snapshot of directory entries.
 *
 * @param  aDir
 *         Directory to look at
 * @param  aSortEntries
 *         True to sort entries by filename
 * @return An array of nsIFile, or an empty array if aDir is not a readable directory
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
 * Record a bit of per-addon telemetry
 * @param aAddon the addon to record
 */
function recordAddonTelemetry(aAddon) {
  let locale = aAddon.defaultLocale;
  if (locale) {
    if (locale.name)
      XPIProvider.setTelemetry(aAddon.id, "name", locale.name);
    if (locale.creator)
      XPIProvider.setTelemetry(aAddon.id, "creator", locale.creator);
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
   * @param {object} state
   *        The add-on's data from the xpiState preference.
   * @param {object} [bootstrapped]
   *        The add-on's data from the bootstrappedAddons preference, if
   *        applicable.
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

  // Compatibility shim getters for legacy callers in XPIProviderUtils:
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
   */
  toJSON() {
    let json = {
      enabled: this.enabled,
      lastModifiedTime: this.lastModifiedTime,
      path: this.relativePath,
      version: this.version,
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
   * @param aFile: nsIFile path of the add-on.
   * @param aId: The add-on ID.
   * @return True if the time stamp has changed.
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
   * Update the XPIState to match an XPIDatabase entry; if 'enabled' is changed to true,
   * update the last-modified time. This should probably be made async, but for now we
   * don't want to maintain parallel sync and async versions of the scan.
   * Caller is responsible for doing XPIStates.save() if necessary.
   * @param aDBAddon The DBAddonInternal for this add-on.
   * @param aUpdated The add-on was updated, so we must record new modified time.
   */
  syncWithDB(aDBAddon, aUpdated = false) {
    logger.debug("Updating XPIState for " + JSON.stringify(aDBAddon));
    // If the add-on changes from disabled to enabled, we should re-check the modified time.
    // If this is a newly found add-on, it won't have an 'enabled' field but we
    // did a full recursive scan in that case, so we don't need to do it again.
    // We don't use aDBAddon.active here because it's not updated until after restart.
    let mustGetMod = (aDBAddon.visible && !aDBAddon.disabled && !this.enabled);

    // We need to treat XUL themes specially here, since lightweight
    // themes require the default theme's chrome to be registered even
    // though we report it as disabled for UI purposes.
    if (aDBAddon.type == "theme") {
      this.enabled = aDBAddon.internalName == DEFAULT_SKIN;
    } else {
      this.enabled = aDBAddon.visible && !aDBAddon.disabled;
    }

    this.version = aDBAddon.version;
    this.type = aDBAddon.type;
    if (aDBAddon.startupData) {
      this.startupData = aDBAddon.startupData;
    }

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
      if (addon.type != "experiment") {
        json.addons[id] = addon;
      }
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
   * @param {bool} [ignoreSideloads = true]
   *        If true, ignore changes in scopes where we don't accept
   *        side-loads.
   *
   * @return true if anything has changed.
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
   * @param name The name of the install location.
   * @return XPIStateLocation (id -> XPIState) or null if there are no add-ons in the location.
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
   * @param aLocation The name of the location where the add-on is installed.
   * @param aId       The add-on ID
   * @return The XPIState entry for the add-on, or null.
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
   * @return {XPIState?}
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
   * @param aAddon DBAddonInternal for the new add-on.
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
   * @param aLocation  The name of the add-on location.
   * @param aId        The ID of the add-on.
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
  // True if all of the add-ons found during startup were installed in the
  // application install location
  allAppGlobal: true,
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
    return gLazyObjectsLoaded && XPIDatabase.initialized;
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
   * @param  aAppChanged
   *         A tri-state value. Undefined means the current profile was created
   *         for this session, true means the profile already existed but was
   *         last used with an application with a different version number,
   *         false means that the profile was last used by this version of the
   *         application.
   * @param  aOldAppVersion
   *         The version of the application last run with this profile or null
   *         if it is a new profile or the version is unknown
   * @param  aOldPlatformVersion
   *         The version of the platform last run with this profile or null
   *         if it is a new profile or the version is unknown
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

      if (aAppChanged && !this.allAppGlobal &&
          Services.prefs.getBoolPref(PREF_EM_SHOW_MISMATCH_UI, true) &&
          AddonManager.updateEnabled) {
        let addonsToUpdate = this.shouldForceUpdateCheck(aAppChanged);
        if (addonsToUpdate) {
          this.noLegacyStartupCheck(addonsToUpdate);
          flushCaches = true;
        }
      }

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
          Services.appinfo.annotateCrashReport("Theme", DEFAULT_SKIN);
        } catch (e) { }
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
                reason = newVersionReason(addon.version, existing.version);
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
          reason = newVersionReason(addon.version, existing.version);
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
   * If the application has been upgraded and there are add-ons outside the
   * application directory then we may need to synchronize compatibility
   * information but only if the mismatch UI isn't disabled.
   *
   * @returns null if no update check is needed, otherwise an array of add-on
   *          IDs to check for updates.
   */
  shouldForceUpdateCheck(aAppChanged) {
    AddonManagerPrivate.recordSimpleMeasure("XPIDB_metadata_age", AddonRepository.metadataAge());

    let startupChanges = AddonManager.getStartupChanges(AddonManager.STARTUP_CHANGE_DISABLED);
    logger.debug("shouldForceUpdateCheck startupChanges: " + startupChanges.toSource());
    AddonManagerPrivate.recordSimpleMeasure("XPIDB_startup_disabled", startupChanges.length);

    let forceUpdate = [];
    if (startupChanges.length > 0) {
    let addons = XPIDatabase.getAddons();
      for (let addon of addons) {
        if ((startupChanges.includes(addon.id)) &&
            (addon.permissions() & AddonManager.PERM_CAN_UPGRADE) &&
            (!addon.isCompatible || isDisabledLegacy(addon))) {
          logger.debug("shouldForceUpdateCheck: can upgrade disabled add-on " + addon.id);
          forceUpdate.push(addon.id);
        }
      }
    }

    if (forceUpdate.length > 0) {
      return forceUpdate;
    }

    return null;
  },

  /**
   * Perform startup check for updates of legacy extensions.
   * This runs during startup when an app update has made some add-ons
   * incompatible and legacy add-on support is diasabled.
   * In this case, we just do a quiet update check.
   *
   * @param {Array<string>} ids The ids of the addons to check for updates.
   *
   * @returns {Set<string>} The ids of any addons that were updated.  These
   *                        addons will have been started by the update
   *                        process so they should not be started by the
   *                        regular bootstrap startup code.
   */
  noLegacyStartupCheck(ids) {
    let started = new Set();
    const DIALOG = "chrome://mozapps/content/extensions/update.html";
    const SHOW_DIALOG_DELAY = 1000;
    const SHOW_CANCEL_DELAY = 30000;

    // Keep track of a value between 0 and 1 indicating the progress
    // for each addon.  Just combine these linearly into a single
    // value for the progress bar in the update dialog.
    let updateProgress = val => {};
    let progressByID = new Map();
    function setProgress(id, val) {
      progressByID.set(id, val);
      updateProgress(Array.from(progressByID.values()).reduce((a, b) => a + b) / progressByID.size);
    }

    // Do an update check for one addon and try to apply the update if
    // there is one.  Progress for the check is arbitrarily defined as
    // 10% done when the update check is done, between 10-90% during the
    // download, then 100% when the update has been installed.
    let checkOne = async (id) => {
      logger.debug(`Checking for updates to disabled addon ${id}\n`);

      setProgress(id, 0);

      let addon = await AddonManager.getAddonByID(id);
      let install = await new Promise(resolve => addon.findUpdates({
        onUpdateFinished() { resolve(null); },
        onUpdateAvailable(addon, install) { resolve(install); },
      }, AddonManager.UPDATE_WHEN_NEW_APP_INSTALLED));

      if (!install) {
        setProgress(id, 1);
        return;
      }

      setProgress(id, 0.1);

      let installPromise = new Promise(resolve => {
        let finish = () => {
          setProgress(id, 1);
          resolve();
        };
        install.addListener({
          onDownloadProgress() {
            if (install.maxProgress != 0) {
              setProgress(id, 0.1 + 0.8 * install.progress / install.maxProgress);
            }
          },
          onDownloadEnded() {
            setProgress(id, 0.9);
          },
          onDownloadFailed: finish,
          onInstallFailed: finish,
          onInstallEnded() {
            started.add(id);
            AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_CHANGED, id);
            finish();
          },
        });
      });
      install.install();
      await installPromise;
    };

    let finished = false;
    Promise.all(ids.map(checkOne)).then(() => { finished = true; });

    let window;
    let timer = setTimeout(() => {
      const FEATURES = "chrome,dialog,centerscreen,scrollbars=no";
      window = Services.ww.openWindow(null, DIALOG, "", FEATURES, null);

      let cancelDiv;
      window.addEventListener("DOMContentLoaded", e => {
        let progress = window.document.getElementById("progress");
        updateProgress = val => { progress.value = val; };

        cancelDiv = window.document.getElementById("cancel-section");
        cancelDiv.setAttribute("style", "display: none;");

        let cancelBtn = window.document.getElementById("cancel-btn");
        cancelBtn.addEventListener("click", e => { finished = true; });
      });

      timer = setTimeout(() => {
        cancelDiv.removeAttribute("style");
        window.sizeToContent();
      }, SHOW_CANCEL_DELAY - SHOW_DIALOG_DELAY);
    }, SHOW_DIALOG_DELAY);

    Services.tm.spinEventLoopUntil(() => finished);

    clearTimeout(timer);
    if (window) {
      window.close();
    }

    return started;
  },

  async updateSystemAddons() {
    let systemAddonLocation = XPIProvider.installLocationsByName[KEY_APP_SYSTEM_ADDONS];
    if (!systemAddonLocation)
      return;

    // Don't do anything in safe mode
    if (Services.appinfo.inSafeMode)
      return;

    // Download the list of system add-ons
    let url = Services.prefs.getStringPref(PREF_SYSTEM_ADDON_UPDATE_URL, null);
    if (!url) {
      await systemAddonLocation.cleanDirectories();
      return;
    }

    url = await UpdateUtils.formatUpdateURL(url);

    logger.info(`Starting system add-on update check from ${url}.`);
    let res = await ProductAddonChecker.getProductAddonList(url);

    // If there was no list then do nothing.
    if (!res || !res.gmpAddons) {
      logger.info("No system add-ons list was returned.");
      await systemAddonLocation.cleanDirectories();
      return;
    }

    let addonList = new Map(
      res.gmpAddons.map(spec => [spec.id, { spec, path: null, addon: null }]));

    let getAddonsInLocation = (location) => {
      return new Promise(resolve => {
        XPIDatabase.getAddonsInLocation(location, resolve);
      });
    };

    let setMatches = (wanted, existing) => {
      if (wanted.size != existing.size)
        return false;

      for (let [id, addon] of existing) {
        let wantedInfo = wanted.get(id);

        if (!wantedInfo)
          return false;
        if (wantedInfo.spec.version != addon.version)
          return false;
      }

      return true;
    };

    // If this matches the current set in the profile location then do nothing.
    let updatedAddons = addonMap(await getAddonsInLocation(KEY_APP_SYSTEM_ADDONS));
    if (setMatches(addonList, updatedAddons)) {
      logger.info("Retaining existing updated system add-ons.");
      await systemAddonLocation.cleanDirectories();
      return;
    }

    // If this matches the current set in the default location then reset the
    // updated set.
    let defaultAddons = addonMap(await getAddonsInLocation(KEY_APP_SYSTEM_DEFAULTS));
    if (setMatches(addonList, defaultAddons)) {
      logger.info("Resetting system add-ons.");
      systemAddonLocation.resetAddonSet();
      await systemAddonLocation.cleanDirectories();
      return;
    }

    // Download all the add-ons
    async function downloadAddon(item) {
      try {
        let sourceAddon = updatedAddons.get(item.spec.id);
        if (sourceAddon && sourceAddon.version == item.spec.version) {
          // Copying the file to a temporary location has some benefits. If the
          // file is locked and cannot be read then we'll fall back to
          // downloading a fresh copy. It also means we don't have to remember
          // whether to delete the temporary copy later.
          try {
            let path = OS.Path.join(OS.Constants.Path.tmpDir, "tmpaddon");
            let unique = await OS.File.openUnique(path);
            unique.file.close();
            await OS.File.copy(sourceAddon._sourceBundle.path, unique.path);
            // Make sure to update file modification times so this is detected
            // as a new add-on.
            await OS.File.setDates(unique.path);
            item.path = unique.path;
          } catch (e) {
            logger.warn(`Failed make temporary copy of ${sourceAddon._sourceBundle.path}.`, e);
          }
        }
        if (!item.path) {
          item.path = await ProductAddonChecker.downloadAddon(item.spec);
        }
        item.addon = await loadManifestFromFile(nsIFile(item.path), systemAddonLocation);
      } catch (e) {
        logger.error(`Failed to download system add-on ${item.spec.id}`, e);
      }
    }
    await Promise.all(Array.from(addonList.values()).map(downloadAddon));

    // The download promises all resolve regardless, now check if they all
    // succeeded
    let validateAddon = (item) => {
      if (item.spec.id != item.addon.id) {
        logger.warn(`Downloaded system add-on expected to be ${item.spec.id} but was ${item.addon.id}.`);
        return false;
      }

      if (item.spec.version != item.addon.version) {
        logger.warn(`Expected system add-on ${item.spec.id} to be version ${item.spec.version} but was ${item.addon.version}.`);
        return false;
      }

      if (!systemAddonLocation.isValidAddon(item.addon))
        return false;

      return true;
    };

    if (!Array.from(addonList.values()).every(item => item.path && item.addon && validateAddon(item))) {
      throw new Error("Rejecting updated system add-on set that either could not " +
                      "be downloaded or contained unusable add-ons.");
    }

    // Install into the install location
    logger.info("Installing new system add-on set");
    await systemAddonLocation.installAddonSet(Array.from(addonList.values())
      .map(a => a.addon));
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

        let disabled = XPIProvider.updateAddonDisabledState(addon);
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

    let data = Array.from(XPIStates.enabledAddons(),
                          a => encoded`${a.id}:${a.version}`).join(",");

    try {
      Services.appinfo.annotateCrashReport("Add-ons", data);
    } catch (e) { }

    let TelemetrySession =
      ChromeUtils.import("resource://gre/modules/TelemetrySession.jsm", {}).TelemetrySession;
    TelemetrySession.setAddOns(data);
  },

  /**
   * Check the staging directories of install locations for any add-ons to be
   * installed or add-ons to be uninstalled.
   *
   * @param  aManifests
   *         A dictionary to add detected install manifests to for the purpose
   *         of passing through updated compatibility information
   * @return true if an add-on was installed or uninstalled
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
      for (let [id, metadata] of state.getStagedAddons()) {
        state.unstageAddon(id);

        let source = getFile(`${id}.xpi`, location.getStagingDir());

        // Check that the directory's name is a valid ID.
        if (!gIDTest.test(id) || !source.exists() || !source.isFile()) {
          logger.warn("Ignoring invalid staging directory entry: ${id}", {id});
          cleanNames.push(source.leafName);
          continue;
        }

        changed = true;
        aManifests[location.name][id] = null;

        let addon;
        try {
          addon = syncLoadManifestFromFile(source, location);
        } catch (e) {
          logger.error(`Unable to read add-on manifest from ${source.path}`, e);
          cleanNames.push(source.leafName);
          continue;
        }

        if (mustSign(addon.type) &&
            addon.signedState <= AddonManager.SIGNEDSTATE_MISSING) {
          logger.warn(`Refusing to install staged add-on ${id} with signed state ${addon.signedState}`);
          cleanNames.push(source.leafName);
          continue;
        }

        addon.importMetadata(metadata);
        aManifests[location.name][id] = addon;

        var oldBootstrap = null;
        logger.debug(`Processing install of ${id} in ${location.name}`);
        let existingAddon = XPIStates.findAddon(id);
        if (existingAddon && existingAddon.bootstrapped) {
          try {
            var file = existingAddon.file;
            if (file.exists()) {
              oldBootstrap = existingAddon;

              // We'll be replacing a currently active bootstrapped add-on so
              // call its uninstall method
              let newVersion = addon.version;
              let oldVersion = existingAddon;
              let uninstallReason = newVersionReason(oldVersion, newVersion);

              this.callBootstrapMethod(existingAddon,
                                       file, "uninstall", uninstallReason,
                                       { newVersion });
              this.unloadBootstrapScope(id);
              XPIInstall.flushChromeCaches();
            }
          } catch (e) {
            Cu.reportError(e);
          }
        }

        try {
          addon._sourceBundle = location.installAddon({
            id, source, existingAddonID: id,
          });
          XPIStates.addAddon(addon);
        } catch (e) {
          logger.error("Failed to install staged add-on " + id + " in " + location.name,
                e);

          delete aManifests[location.name][id];

          if (oldBootstrap) {
            // Re-install the old add-on
            this.callBootstrapMethod(oldBootstrap, existingAddon, "install",
                                     BOOTSTRAP_REASONS.ADDON_INSTALL);
          }
        }
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
   * @param  aManifests
   *         A dictionary to add new install manifests to to save having to
   *         reload them later
   * @param  aAppChanged
   *         See checkForChanges
   * @return true if any new add-ons were installed
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
        if (id.substring(id.length - 4).toLowerCase() == ".xpi") {
          id = id.substring(0, id.length - 4);
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

      let addon;
      try {
        addon = syncLoadManifestFromFile(entry, profileLocation);
      } catch (e) {
        logger.warn("File entry " + entry.path + " contains an invalid add-on", e);
        continue;
      }

      if (addon.id != id) {
        logger.warn("File entry " + entry.path + " contains an add-on with an " +
             "incorrect ID");
        continue;
      }

      let existingEntry = null;
      try {
        existingEntry = profileLocation.getLocationForID(id);
      } catch (e) {
      }

      if (existingEntry) {
        let existingAddon;
        try {
          existingAddon = syncLoadManifestFromFile(existingEntry, profileLocation);

          if (Services.vc.compare(addon.version, existingAddon.version) <= 0)
            continue;
        } catch (e) {
          // Bad add-on in the profile so just proceed and install over the top
          logger.warn("Profile contains an add-on with a bad or missing install " +
               "manifest at " + existingEntry.path + ", overwriting", e);
        }
      } else if (Services.prefs.getBoolPref(PREF_BRANCH_INSTALLED_ADDON + id, false)) {
        continue;
      }

      // Install the add-on
      try {
        addon._sourceBundle = profileLocation.installAddon({ id, source: entry, action: "copy" });
        if (Services.prefs.getBoolPref(PREF_DISTRO_ADDONS_PERMS, false)) {
          addon.userDisabled = true;
          if (!this.newDistroAddons) {
            this.newDistroAddons = new Set();
          }
          this.newDistroAddons.add(id);
        }

        XPIStates.addAddon(addon);
        logger.debug("Installed distribution add-on " + id);

        Services.prefs.setBoolPref(PREF_BRANCH_INSTALLED_ADDON + id, true);

        // aManifests may contain a copy of a newly installed add-on's manifest
        // and we'll have overwritten that so instead cache our install manifest
        // which will later be put into the database in processFileChanges
        if (!(KEY_APP_PROFILE in aManifests))
          aManifests[KEY_APP_PROFILE] = {};
        aManifests[KEY_APP_PROFILE][id] = addon;
        changed = true;
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
   * Returns the add-on state data for the restartful extensions which
   * should be available in safe mode. In particular, this means the
   * default theme, and only the default theme.
   *
   * @returns {object}
   */
  getSafeModeExtensions() {
    let loc = XPIStates.getLocation(KEY_APP_GLOBAL);
    let state = loc.get(ADDON_ID_DEFAULT_THEME);

    // Use the default state data for the default theme, but always mark
    // it enabled, in case another theme is enabled in normal mode.
    let addonData = state.toJSON();
    addonData.enabled = true;

    return {
      [KEY_APP_GLOBAL]: {
        path: loc.path,
        addons: { [ADDON_ID_DEFAULT_THEME]: addonData },
      },
    };
  },

  /**
   * Checks for any changes that have occurred since the last time the
   * application was launched.
   *
   * @param  aAppChanged
   *         A tri-state value. Undefined means the current profile was created
   *         for this session, true means the profile already existed but was
   *         last used with an application with a different version number,
   *         false means that the profile was last used by this version of the
   *         application.
   * @param  aOldAppVersion
   *         The version of the application last run with this profile or null
   *         if it is a new profile or the version is unknown
   * @param  aOldPlatformVersion
   *         The version of the platform last run with this profile or null
   *         if it is a new profile or the version is unknown
   * @return true if a change requiring a restart was detected
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

      if (Services.appinfo.inSafeMode) {
        aomStartup.initializeExtensions(this.getSafeModeExtensions());
        logger.debug("Initialized safe mode add-ons");
        return false;
      }

      // If the application crashed before completing any pending operations then
      // we should perform them now.
      if (extensionListChanged || hasPendingChanges) {
        this._updateActiveAddons();

        // Serialize and deserialize so we get the expected JSON data.
        let state = JSON.parse(JSON.stringify(XPIStates));
        aomStartup.initializeExtensions(state);
        return true;
      }

      aomStartup.initializeExtensions(XPIStates.initialStateData);

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
   * @param  aMimetype
   *         The mimetype to check for
   * @return true if the mimetype is application/x-xpinstall
   */
  supportsMimetype(aMimetype) {
    return aMimetype == "application/x-xpinstall";
  },

  /**
   * Called to test whether installing XPI add-ons is enabled.
   *
   * @return true if installing is enabled
   */
  isInstallEnabled() {
    // Default to enabled if the preference does not exist
    return Services.prefs.getBoolPref(PREF_XPI_ENABLED, true);
  },

  /**
   * Called to test whether installing XPI add-ons by direct URL requests is
   * whitelisted.
   *
   * @return true if installing by direct requests is whitelisted
   */
  isDirectRequestWhitelisted() {
    // Default to whitelisted if the preference does not exist.
    return Services.prefs.getBoolPref(PREF_XPI_DIRECT_WHITELISTED, true);
  },

  /**
   * Called to test whether installing XPI add-ons from file referrers is
   * whitelisted.
   *
   * @return true if installing from file referrers is whitelisted
   */
  isFileRequestWhitelisted() {
    // Default to whitelisted if the preference does not exist.
    return Services.prefs.getBoolPref(PREF_XPI_FILE_WHITELISTED, true);
  },

  /**
   * Called to test whether installing XPI add-ons from a URI is allowed.
   *
   * @param  aInstallingPrincipal
   *         The nsIPrincipal that initiated the install
   * @return true if installing is allowed
   */
  isInstallAllowed(aInstallingPrincipal) {
    if (!this.isInstallEnabled())
      return false;

    let uri = aInstallingPrincipal.URI;

    // Direct requests without a referrer are either whitelisted or blocked.
    if (!uri)
      return this.isDirectRequestWhitelisted();

    // Local referrers can be whitelisted.
    if (this.isFileRequestWhitelisted() &&
        (uri.schemeIs("chrome") || uri.schemeIs("file")))
      return true;

    this.importPermissions();

    let permission = Services.perms.testPermissionFromPrincipal(aInstallingPrincipal, XPI_PERMISSION);
    if (permission == Ci.nsIPermissionManager.DENY_ACTION)
      return false;

    let requireWhitelist = Services.prefs.getBoolPref(PREF_XPI_WHITELIST_REQUIRED, true);
    if (requireWhitelist && (permission != Ci.nsIPermissionManager.ALLOW_ACTION))
      return false;

    let requireSecureOrigin = Services.prefs.getBoolPref(PREF_INSTALL_REQUIRESECUREORIGIN, true);
    let safeSchemes = ["https", "chrome", "file"];
    if (requireSecureOrigin && !safeSchemes.includes(uri.scheme))
      return false;

    return true;
  },

  // Identify temporary install IDs.
  isTemporaryInstallID(id) {
    return id.endsWith(TEMPORARY_ADDON_SUFFIX);
  },

  /**
   * Called to get an AddonInstall to download and install an add-on from a URL.
   *
   * @param  aUrl
   *         The URL to be installed
   * @param  aHash
   *         A hash for the install
   * @param  aName
   *         A name for the install
   * @param  aIcons
   *         Icon URLs for the install
   * @param  aVersion
   *         A version for the install
   * @param  aBrowser
   *         The browser performing the install
   * @param  aCallback
   *         A callback to pass the AddonInstall to
   */
  getInstallForURL(aUrl, aHash, aName, aIcons, aVersion, aBrowser,
                             aCallback) {
    let location = XPIProvider.installLocationsByName[KEY_APP_PROFILE];
    let url = Services.io.newURI(aUrl);

    let options = {
      hash: aHash,
      browser: aBrowser,
      name: aName,
      icons: aIcons,
      version: aVersion,
    };

    if (url instanceof Ci.nsIFileURL) {
      let install = new LocalAddonInstall(location, url, options);
      install.init().then(() => { aCallback(install.wrapper); });
    } else {
      let install = new DownloadAddonInstall(location, url, options);
      aCallback(install.wrapper);
    }
  },

  /**
   * Called to get an AddonInstall to install an add-on from a local file.
   *
   * @param  aFile
   *         The file to be installed
   * @param  aCallback
   *         A callback to pass the AddonInstall to
   */
  getInstallForFile(aFile, aCallback) {
    createLocalInstall(aFile).then(install => {
      aCallback(install ? install.wrapper : null);
    });
  },

  /**
   * Temporarily installs add-on from a local XPI file or directory.
   * As this is intended for development, the signature is not checked and
   * the add-on does not persist on application restart.
   *
   * @param aFile
   *        An nsIFile for the unpacked add-on directory or XPI file.
   *
   * @return See installAddonFromLocation return value.
   */
  installTemporaryAddon(aFile) {
    return this.installAddonFromLocation(aFile, TemporaryInstallLocation);
  },

  /**
   * Permanently installs add-on from a local XPI file or directory.
   * The signature is checked but the add-on persist on application restart.
   *
   * @param aFile
   *        An nsIFile for the unpacked add-on directory or XPI file.
   *
   * @return See installAddonFromLocation return value.
   */
  async installAddonFromSources(aFile) {
    let location = XPIProvider.installLocationsByName[KEY_APP_PROFILE];
    return this.installAddonFromLocation(aFile, location, "proxy");
  },

  /**
   * Installs add-on from a local XPI file or directory.
   *
   * @param aFile
   *        An nsIFile for the unpacked add-on directory or XPI file.
   * @param aInstallLocation
   *        Define a custom install location object to use for the install.
   * @param aInstallAction
   *        Optional action mode to use when installing the addon
   *        (see MutableDirectoryInstallLocation.installAddon)
   *
   * @return a Promise that resolves to an Addon object on success, or rejects
   *         if the add-on is not a valid restartless add-on or if the
   *         same ID is already installed.
   */
  async installAddonFromLocation(aFile, aInstallLocation, aInstallAction) {
    if (aFile.exists() && aFile.isFile()) {
      XPIInstall.flushJarCache(aFile);
    }
    let addon = await loadManifestFromFile(aFile, aInstallLocation);

    aInstallLocation.installAddon({ id: addon.id, source: aFile, action: aInstallAction });

    if (addon.appDisabled) {
      let message = `Add-on ${addon.id} is not compatible with application version.`;

      let app = addon.matchingTargetApplication;
      if (app) {
        if (app.minVersion) {
          message += ` add-on minVersion: ${app.minVersion}.`;
        }
        if (app.maxVersion) {
          message += ` add-on maxVersion: ${app.maxVersion}.`;
        }
      }
      throw new Error(message);
    }

    if (!addon.bootstrap) {
      throw new Error("Only restartless (bootstrap) add-ons"
                    + " can be installed from sources:", addon.id);
    }
    let installReason = BOOTSTRAP_REASONS.ADDON_INSTALL;
    let oldAddon = await new Promise(
                   resolve => XPIDatabase.getVisibleAddonForID(addon.id, resolve));
    let callUpdate = false;

    let extraParams = {};
    extraParams.temporarilyInstalled = aInstallLocation === TemporaryInstallLocation;
    if (oldAddon) {
      if (!oldAddon.bootstrap) {
        logger.warn("Non-restartless Add-on is already installed", addon.id);
        throw new Error("Non-restartless add-on with ID "
                        + oldAddon.id + " is already installed");
      } else {
        logger.warn("Addon with ID " + oldAddon.id + " already installed,"
                    + " older version will be disabled");

        addon.installDate = oldAddon.installDate;

        let existingAddonID = oldAddon.id;
        let existingAddon = oldAddon._sourceBundle;

        // We'll be replacing a currently active bootstrapped add-on so
        // call its uninstall method
        let newVersion = addon.version;
        let oldVersion = oldAddon.version;

        installReason = newVersionReason(oldVersion, newVersion);
        let uninstallReason = installReason;

        extraParams.newVersion = newVersion;
        extraParams.oldVersion = oldVersion;

        callUpdate = isWebExtension(oldAddon.type) && isWebExtension(addon.type);

        if (oldAddon.active) {
          XPIProvider.callBootstrapMethod(oldAddon, existingAddon,
                                          "shutdown", uninstallReason,
                                          extraParams);
        }

        if (!callUpdate) {
          this.callBootstrapMethod(oldAddon, existingAddon,
                                   "uninstall", uninstallReason, extraParams);
        }
        this.unloadBootstrapScope(existingAddonID);
        XPIInstall.flushChromeCaches();
      }
    } else {
      addon.installDate = Date.now();
    }

    let file = addon._sourceBundle;

    let method = callUpdate ? "update" : "install";
    XPIProvider.callBootstrapMethod(addon, file, method, installReason, extraParams);
    addon.state = AddonManager.STATE_INSTALLED;
    logger.debug("Install of temporary addon in " + aFile.path + " completed.");
    addon.visible = true;
    addon.enabled = true;
    addon.active = true;
    // WebExtension themes are installed as disabled, fix that here.
    addon.userDisabled = false;

    addon = XPIDatabase.addAddonMetadata(addon, file.path);

    XPIStates.addAddon(addon);
    XPIDatabase.saveChanges();
    XPIStates.save();

    AddonManagerPrivate.callAddonListeners("onInstalling", addon.wrapper,
                                           false);
    XPIProvider.callBootstrapMethod(addon, file, "startup", installReason, extraParams);
    AddonManagerPrivate.callInstallListeners("onExternalInstall",
                                             null, addon.wrapper,
                                             oldAddon ? oldAddon.wrapper : null,
                                             false);
    AddonManagerPrivate.callAddonListeners("onInstalled", addon.wrapper);

    // Notify providers that a new theme has been enabled.
    if (isTheme(addon.type))
      AddonManagerPrivate.notifyAddonChanged(addon.id, addon.type, false);

    return addon.wrapper;
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
   * @param aInstanceID
   *        An Addon Instance ID
   * @return {Promise}
   * @resolves The found Addon or null if no such add-on exists.
   * @rejects  Never
   * @throws if the aInstanceID argument is not specified
   */
   getAddonByInstanceID(aInstanceID) {
     if (!aInstanceID || typeof aInstanceID != "symbol")
       throw Components.Exception("aInstanceID must be a Symbol()",
                                  Cr.NS_ERROR_INVALID_ARG);

     for (let [id, val] of this.activeAddons) {
       if (aInstanceID == val.instanceID) {
         return new Promise(resolve => this.getAddonByID(id, resolve));
       }
     }

     return Promise.resolve(null);
   },

  /**
   * Removes an AddonInstall from the list of active installs.
   *
   * @param  install
   *         The AddonInstall to remove
   */
  removeActiveInstall(aInstall) {
    this.installs.delete(aInstall);
  },

  /**
   * Called to get an Addon with a particular ID.
   *
   * @param  aId
   *         The ID of the add-on to retrieve
   * @param  aCallback
   *         A callback to pass the Addon to
   */
  getAddonByID(aId, aCallback) {
    XPIDatabase.getVisibleAddonForID(aId, function(aAddon) {
      aCallback(aAddon ? aAddon.wrapper : null);
    });
  },

  /**
   * Called to get Addons of a particular type.
   *
   * @param  aTypes
   *         An array of types to fetch. Can be null to get all types.
   * @param  aCallback
   *         A callback to pass an array of Addons to
   */
  getAddonsByTypes(aTypes, aCallback) {
    let typesToGet = getAllAliasesForTypes(aTypes);
    if (typesToGet && !typesToGet.some(type => ALL_EXTERNAL_TYPES.has(type))) {
      aCallback([]);
      return;
    }

    XPIDatabase.getVisibleAddons(typesToGet, function(aAddons) {
      aCallback(aAddons.map(a => a.wrapper));
    });
  },

  /**
   * Called to get active Addons of a particular type
   *
   * @param  aTypes
   *         An array of types to fetch. Can be null to get all types.
   * @returns {Promise<Array<Addon>>}
   */
  getActiveAddons(aTypes) {
    // If we already have the database loaded, returning full info is fast.
    if (this.isDBLoaded) {
      return new Promise(resolve => {
        this.getAddonsByTypes(aTypes, addons => {
          // The thing with experiments is an ugly hack but we want
          // Experiments.jsm to use this interface instead of getAddonsByTypes.
          // They'll go away at some point and we can forget this ever happened.
          resolve({addons: addons.filter(addon => addon.isActive ||
                                       (addon.type == "experiment" && !addon.appDisabled)),
                   fullData: true
          });
        });
      });
    }

    // Construct addon-like objects with the information we already
    // have in memory.
    if (!XPIStates.db) {
      return Promise.reject(new Error("XPIStates not yet initialized"));
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

    return Promise.resolve({addons: result, fullData: false});
  },


  /**
   * Obtain an Addon having the specified Sync GUID.
   *
   * @param  aGUID
   *         String GUID of add-on to retrieve
   * @param  aCallback
   *         A callback to pass the Addon to. Receives null if not found.
   */
  getAddonBySyncGUID(aGUID, aCallback) {
    XPIDatabase.getAddonBySyncGUID(aGUID, function(aAddon) {
      aCallback(aAddon ? aAddon.wrapper : null);
    });
  },

  /**
   * Called to get Addons that have pending operations.
   *
   * @param  aTypes
   *         An array of types to fetch. Can be null to get all types
   * @param  aCallback
   *         A callback to pass an array of Addons to
   */
  getAddonsWithOperationsByTypes(aTypes, aCallback) {
    let typesToGet = getAllAliasesForTypes(aTypes);

    XPIDatabase.getVisibleAddonsWithPendingOperations(typesToGet, function(aAddons) {
      let results = aAddons.map(a => a.wrapper);
      for (let install of XPIProvider.installs) {
        if (install.state == AddonManager.STATE_INSTALLED &&
            !(install.addon.inDatabase))
          results.push(install.addon.wrapper);
      }
      aCallback(results);
    });
  },

  /**
   * Called to get the current AddonInstalls, optionally limiting to a list of
   * types.
   *
   * @param  aTypes
   *         An array of types or null to get all types
   * @param  aCallback
   *         A callback to pass the array of AddonInstalls to
   */
  getInstallsByTypes(aTypes, aCallback) {
    let results = [...this.installs];
    if (aTypes) {
      results = results.filter(install => {
        return aTypes.includes(getExternalType(install.type));
      });
    }

    aCallback(results.map(install => install.wrapper));
  },

  /**
   * Called when a new add-on has been enabled when only one add-on of that type
   * can be enabled.
   *
   * @param  aId
   *         The ID of the newly enabled add-on
   * @param  aType
   *         The type of the newly enabled add-on
   */
  addonChanged(aId, aType) {
    // We only care about themes in this provider
    if (!isTheme(aType))
      return;

    let addons = XPIDatabase.getAddonsByType("theme", "webextension-theme");
    for (let theme of addons) {
      if (isWebExtension(theme.type) && theme.visible && theme.id != aId)
        this.updateAddonDisabledState(theme, true, undefined);
    }

    let defaultTheme = XPIDatabase.getVisibleAddonForInternalName(DEFAULT_SKIN);
    this.updateAddonDisabledState(defaultTheme, aId && aId != defaultTheme.id);
  },

  /**
   * Update the appDisabled property for all add-ons.
   */
  updateAddonAppDisabledStates() {
    let addons = XPIDatabase.getAddons();
    for (let addon of addons) {
      this.updateAddonDisabledState(addon);
    }
  },

  /**
   * Update the repositoryAddon property for all add-ons.
   *
   * @param  aCallback
   *         Function to call when operation is complete.
   */
  updateAddonRepositoryData(aCallback) {
    XPIDatabase.getVisibleAddons(null, aAddons => {
      let pending = aAddons.length;
      logger.debug("updateAddonRepositoryData found " + pending + " visible add-ons");
      if (pending == 0) {
        aCallback();
        return;
      }

      function notifyComplete() {
        if (--pending == 0)
          aCallback();
      }

      for (let addon of aAddons) {
        AddonRepository.getCachedAddonByID(addon.id).then(aRepoAddon => {
          if (aRepoAddon || AddonRepository.getCompatibilityOverridesSync(addon.id)) {
            logger.debug("updateAddonRepositoryData got info for " + addon.id);
            addon._repositoryAddon = aRepoAddon;
            this.updateAddonDisabledState(addon);
          }

          notifyComplete();
        });
      }
    });
  },

  onDebugConnectionChange({what, connection}) {
    if (what != "opened")
      return;

    for (let [id, val] of this.activeAddons) {
      connection.setAddonOptions(
        id, { global: val.bootstrapScope });
    }
  },

  /**
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
   * @param  aId
   *         The add-on's ID
   * @param  aFile
   *         The nsIFile for the add-on
   * @param  aVersion
   *         The add-on's version
   * @param  aType
   *         The type for the add-on
   * @param  aRunInSafeMode
   *         Boolean indicating whether the add-on can run in safe mode.
   * @param  aDependencies
   *         An array of add-on IDs on which this add-on depends.
   * @param  hasEmbeddedWebExtension
   *         Boolean indicating whether the add-on has an embedded webextension.
   * @return a JavaScript scope
   */
  loadBootstrapScope(aId, aFile, aVersion, aType, aRunInSafeMode, aDependencies,
                     hasEmbeddedWebExtension) {
    this.activeAddons.set(aId, {
      bootstrapScope: null,
      // a Symbol passed to this add-on, which it can use to identify itself
      instanceID: Symbol(aId),
      started: false,
    });

    // Mark the add-on as active for the crash reporter before loading
    this.addAddonsToCrashReporter();

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
   * @param  aId
   *         The add-on's ID
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
   * @param  aAddon
   *         An object representing the add-on, with `id`, `type` and `version`
   * @param  aFile
   *         The nsIFile for the add-on
   * @param  aMethod
   *         The name of the bootstrap method to call
   * @param  aReason
   *         The reason flag to pass to the bootstrap's startup method
   * @param  aExtraParams
   *         An object of additional key/value pairs to pass to the method in
   *         the params argument
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
                                aAddon.hasEmbeddedWebExtension || false);
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
              this.updateAddonDisabledState(addon);
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
          this.updateAddonDisabledState(addon);
      }

      if (CHROME_TYPES.has(aAddon.type) && aMethod == "shutdown" && aReason != BOOTSTRAP_REASONS.APP_SHUTDOWN) {
        logger.debug("Removing manifest for " + aFile.path);
        Components.manager.removeBootstrappedManifestLocation(aFile);
      }
      this.setTelemetry(aAddon.id, aMethod + "_MS", new Date() - timeStart);
    }
  },

  /**
   * Updates the disabled state for an add-on. Its appDisabled property will be
   * calculated and if the add-on is changed the database will be saved and
   * appropriate notifications will be sent out to the registered AddonListeners.
   *
   * @param  aAddon
   *         The DBAddonInternal to update
   * @param  aUserDisabled
   *         Value for the userDisabled property. If undefined the value will
   *         not change
   * @param  aSoftDisabled
   *         Value for the softDisabled property. If undefined the value will
   *         not change. If true this will force userDisabled to be true
   * @return a tri-state indicating the action taken for the add-on:
   *           - undefined: The add-on did not change state
   *           - true: The add-on because disabled
   *           - false: The add-on became enabled
   * @throws if addon is not a DBAddonInternal
   */
  updateAddonDisabledState(aAddon, aUserDisabled, aSoftDisabled) {
    if (!(aAddon.inDatabase))
      throw new Error("Can only update addon states for installed addons.");
    if (aUserDisabled !== undefined && aSoftDisabled !== undefined) {
      throw new Error("Cannot change userDisabled and softDisabled at the " +
                      "same time");
    }

    if (aUserDisabled === undefined) {
      aUserDisabled = aAddon.userDisabled;
    } else if (!aUserDisabled) {
      // If enabling the add-on then remove softDisabled
      aSoftDisabled = false;
    }

    // If not changing softDisabled or the add-on is already userDisabled then
    // use the existing value for softDisabled
    if (aSoftDisabled === undefined || aUserDisabled)
      aSoftDisabled = aAddon.softDisabled;

    let appDisabled = !isUsableAddon(aAddon);
    // No change means nothing to do here
    if (aAddon.userDisabled == aUserDisabled &&
        aAddon.appDisabled == appDisabled &&
        aAddon.softDisabled == aSoftDisabled)
      return undefined;

    let wasDisabled = aAddon.disabled;
    let isDisabled = aUserDisabled || aSoftDisabled || appDisabled;

    // If appDisabled changes but addon.disabled doesn't,
    // no onDisabling/onEnabling is sent - so send a onPropertyChanged.
    let appDisabledChanged = aAddon.appDisabled != appDisabled;

    // Update the properties in the database.
    XPIDatabase.setAddonProperties(aAddon, {
      userDisabled: aUserDisabled,
      appDisabled,
      softDisabled: aSoftDisabled
    });

    let wrapper = aAddon.wrapper;

    if (appDisabledChanged) {
      AddonManagerPrivate.callAddonListeners("onPropertyChanged",
                                             wrapper,
                                             ["appDisabled"]);
    }

    // If the add-on is not visible or the add-on is not changing state then
    // there is no need to do anything else
    if (!aAddon.visible || (wasDisabled == isDisabled))
      return undefined;

    // Flag that active states in the database need to be updated on shutdown
    Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);

    // Sync with XPIStates.
    let xpiState = XPIStates.getAddon(aAddon.location, aAddon.id);
    if (xpiState) {
      xpiState.syncWithDB(aAddon);
      XPIStates.save();
    } else {
      // There should always be an xpiState
      logger.warn("No XPIState for ${id} in ${location}", aAddon);
    }

    // Have we just gone back to the current state?
    if (isDisabled != aAddon.active) {
      AddonManagerPrivate.callAddonListeners("onOperationCancelled", wrapper);
    } else {
      if (isDisabled) {
        AddonManagerPrivate.callAddonListeners("onDisabling", wrapper, false);
      } else {
        AddonManagerPrivate.callAddonListeners("onEnabling", wrapper, false);
      }

      XPIDatabase.updateAddonActive(aAddon, !isDisabled);

      if (isDisabled) {
        if (aAddon.bootstrap && this.activeAddons.has(aAddon.id)) {
          this.callBootstrapMethod(aAddon, aAddon._sourceBundle, "shutdown",
                                   BOOTSTRAP_REASONS.ADDON_DISABLE);
          this.unloadBootstrapScope(aAddon.id);
        }
        AddonManagerPrivate.callAddonListeners("onDisabled", wrapper);
      } else {
        if (aAddon.bootstrap) {
          this.callBootstrapMethod(aAddon, aAddon._sourceBundle, "startup",
                                   BOOTSTRAP_REASONS.ADDON_ENABLE);
        }
        AddonManagerPrivate.callAddonListeners("onEnabled", wrapper);
      }
    }

    // Notify any other providers that a new theme has been enabled
    if (isTheme(aAddon.type) && !isDisabled) {
      AddonManagerPrivate.notifyAddonChanged(aAddon.id, aAddon.type);

      if (xpiState) {
        xpiState.syncWithDB(aAddon);
        XPIStates.save();
      }
    }

    return isDisabled;
  },

  /**
   * Uninstalls an add-on, immediately if possible or marks it as pending
   * uninstall if not.
   *
   * @param  aAddon
   *         The DBAddonInternal to uninstall
   * @param  aForcePending
   *         Force this addon into the pending uninstall state (used
   *         e.g. while the add-on manager is open and offering an
   *         "undo" button)
   * @throws if the addon cannot be uninstalled because it is in an install
   *         location that does not allow it
   */
  uninstallAddon(aAddon, aForcePending) {
    if (!(aAddon.inDatabase))
      throw new Error("Cannot uninstall addon " + aAddon.id + " because it is not installed");

    if (aAddon._installLocation.locked)
      throw new Error("Cannot uninstall addon " + aAddon.id
          + " from locked install location " + aAddon._installLocation.name);

    if (aForcePending && aAddon.pendingUninstall)
      throw new Error("Add-on is already marked to be uninstalled");

    aAddon._hasResourceCache.clear();

    if (aAddon._updateCheck) {
      logger.debug("Cancel in-progress update check for " + aAddon.id);
      aAddon._updateCheck.cancel();
    }

    let wasPending = aAddon.pendingUninstall;

    if (aForcePending) {
      // We create an empty directory in the staging directory to indicate
      // that an uninstall is necessary on next startup. Temporary add-ons are
      // automatically uninstalled on shutdown anyway so there is no need to
      // do this for them.
      if (aAddon._installLocation.name != KEY_APP_TEMPORARY) {
        let stage = getFile(aAddon.id, aAddon._installLocation.getStagingDir());
        if (!stage.exists())
          stage.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
      }

      XPIDatabase.setAddonProperties(aAddon, {
        pendingUninstall: true
      });
      Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);
      let xpiState = XPIStates.getAddon(aAddon.location, aAddon.id);
      if (xpiState) {
        xpiState.enabled = false;
        XPIStates.save();
      } else {
        logger.warn("Can't find XPI state while uninstalling ${id} from ${location}", aAddon);
      }
    }

    // If the add-on is not visible then there is no need to notify listeners.
    if (!aAddon.visible)
      return;

    let wrapper = aAddon.wrapper;

    // If the add-on wasn't already pending uninstall then notify listeners.
    if (!wasPending) {
      AddonManagerPrivate.callAddonListeners("onUninstalling", wrapper,
                                             !!aForcePending);
    }

    let reason = BOOTSTRAP_REASONS.ADDON_UNINSTALL;
    let callUpdate = false;
    let existingAddon = XPIStates.findAddon(aAddon.id, loc =>
      loc.name != aAddon._installLocation.name);
    if (existingAddon) {
      reason = newVersionReason(aAddon.version, existingAddon.version);
      callUpdate = isWebExtension(aAddon.type) && isWebExtension(existingAddon.type);
    }

    if (!aForcePending) {
      if (aAddon.bootstrap) {
        if (aAddon.active) {
          this.callBootstrapMethod(aAddon, aAddon._sourceBundle, "shutdown",
                                   reason);
        }

        if (!callUpdate) {
          this.callBootstrapMethod(aAddon, aAddon._sourceBundle, "uninstall",
                                   reason);
        }
        XPIStates.disableAddon(aAddon.id);
        this.unloadBootstrapScope(aAddon.id);
        XPIInstall.flushChromeCaches();
      }
      aAddon._installLocation.uninstallAddon(aAddon.id);
      XPIDatabase.removeAddonMetadata(aAddon);
      XPIStates.removeAddon(aAddon.location, aAddon.id);
      AddonManagerPrivate.callAddonListeners("onUninstalled", wrapper);

      if (existingAddon) {
        XPIDatabase.getAddonInLocation(aAddon.id, existingAddon.location.name, existing => {
          XPIDatabase.makeAddonVisible(existing);

          let wrappedAddon = existing.wrapper;
          AddonManagerPrivate.callAddonListeners("onInstalling", wrappedAddon, false);

          if (!existing.disabled) {
            XPIDatabase.updateAddonActive(existing, true);
          }

          if (aAddon.bootstrap) {
            let method = callUpdate ? "update" : "install";
            XPIProvider.callBootstrapMethod(existing, existing._sourceBundle,
                                            method, reason);

            if (existing.active) {
              XPIProvider.callBootstrapMethod(existing, existing._sourceBundle,
                                              "startup", reason);
            } else {
              XPIProvider.unloadBootstrapScope(existing.id);
            }
          }

          AddonManagerPrivate.callAddonListeners("onInstalled", wrappedAddon);
        });
      }
    } else if (aAddon.bootstrap && aAddon.active) {
      this.callBootstrapMethod(aAddon, aAddon._sourceBundle, "shutdown", reason);
      XPIStates.disableAddon(aAddon.id);
      this.unloadBootstrapScope(aAddon.id);
      XPIDatabase.updateAddonActive(aAddon, false);
    }

    // Notify any other providers that a new theme has been enabled
    if (isTheme(aAddon.type) && aAddon.active)
      AddonManagerPrivate.notifyAddonChanged(null, aAddon.type);
  },

  /**
   * Cancels the pending uninstall of an add-on.
   *
   * @param  aAddon
   *         The DBAddonInternal to cancel uninstall for
   */
  cancelUninstallAddon(aAddon) {
    if (!(aAddon.inDatabase))
      throw new Error("Can only cancel uninstall for installed addons.");
    if (!aAddon.pendingUninstall)
      throw new Error("Add-on is not marked to be uninstalled");

    if (aAddon._installLocation.name != KEY_APP_TEMPORARY)
      aAddon._installLocation.cleanStagingDir([aAddon.id]);

    XPIDatabase.setAddonProperties(aAddon, {
      pendingUninstall: false
    });

    if (!aAddon.visible)
      return;

    XPIStates.getAddon(aAddon.location, aAddon.id).syncWithDB(aAddon);
    XPIStates.save();

    Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);

    // TODO hide hidden add-ons (bug 557710)
    let wrapper = aAddon.wrapper;
    AddonManagerPrivate.callAddonListeners("onOperationCancelled", wrapper);

    if (aAddon.bootstrap && !aAddon.disabled) {
      this.callBootstrapMethod(aAddon, aAddon._sourceBundle, "startup",
                               BOOTSTRAP_REASONS.ADDON_INSTALL);
      XPIDatabase.updateAddonActive(aAddon, true);
    }

    // Notify any other providers that this theme is now enabled again.
    if (isTheme(aAddon.type) && aAddon.active)
      AddonManagerPrivate.notifyAddonChanged(aAddon.id, aAddon.type, false);
  }
};

/**
 * Creates a new AddonInstall to install an add-on from a local file.
 *
 * @param  file
 *         The file to install
 * @param  location
 *         The location to install to
 * @returns Promise
 *          A Promise that resolves with the new install object.
 */
function createLocalInstall(file, location) {
  if (!location) {
    location = XPIProvider.installLocationsByName[KEY_APP_PROFILE];
  }
  let url = Services.io.newFileURI(file);

  try {
    let install = new LocalAddonInstall(location, url);
    return install.init().then(() => install);
  } catch (e) {
    logger.error("Error creating install", e);
    XPIProvider.removeActiveInstall(this);
    return Promise.resolve(null);
  }
}

// Maps instances of AddonInternal to AddonWrapper
const wrapperMap = new WeakMap();
let addonFor = wrapper => wrapperMap.get(wrapper);

/**
 * The AddonInternal is an internal only representation of add-ons. It may
 * have come from the database (see DBAddonInternal in XPIProviderUtils.jsm)
 * or an install manifest.
 */
function AddonInternal() {
  this._hasResourceCache = new Map();

  XPCOMUtils.defineLazyGetter(this, "wrapper", () => {
    return new AddonWrapper(this);
  });
}

AddonInternal.prototype = {
  _selectedLocale: null,
  _hasResourceCache: null,
  active: false,
  visible: false,
  userDisabled: false,
  appDisabled: false,
  softDisabled: false,
  blocklistState: Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
  blocklistURL: null,
  sourceURI: null,
  releaseNotesURI: null,
  foreignInstall: false,
  seen: true,
  skinnable: false,
  startupData: null,

  /**
   * @property {Array<string>} dependencies
   *   An array of bootstrapped add-on IDs on which this add-on depends.
   *   The add-on will remain appDisabled if any of the dependent
   *   add-ons is not installed and enabled.
   */
  dependencies: Object.freeze([]),
  hasEmbeddedWebExtension: false,

  get selectedLocale() {
    if (this._selectedLocale)
      return this._selectedLocale;

    /**
     * this.locales is a list of objects that have property `locales`.
     * It's value is an array of locale codes.
     *
     * First, we reduce this nested structure to a flat list of locale codes.
     */
    const locales = [].concat(...this.locales.map(loc => loc.locales));

    let requestedLocales = Services.locale.getRequestedLocales();

    /**
     * If en-US is not in the list, add it as the last fallback.
     */
    if (!requestedLocales.includes("en-US")) {
      requestedLocales.push("en-US");
    }

    /**
     * Then we negotiate best locale code matching the app locales.
     */
    let bestLocale = Services.locale.negotiateLanguages(
      requestedLocales,
      locales,
      "und",
      Services.locale.langNegStrategyLookup
    )[0];

    /**
     * If no match has been found, we'll assign the default locale as
     * the selected one.
     */
    if (bestLocale === "und") {
      this._selectedLocale = this.defaultLocale;
    } else {
      /**
       * Otherwise, we'll go through all locale entries looking for the one
       * that has the best match in it's locales list.
       */
      this._selectedLocale = this.locales.find(loc =>
        loc.locales.includes(bestLocale));
    }

    return this._selectedLocale;
  },

  get providesUpdatesSecurely() {
    return !this.updateURL || this.updateURL.startsWith("https:");
  },

  get isCorrectlySigned() {
    switch (this._installLocation.name) {
      case KEY_APP_SYSTEM_ADDONS:
        // System add-ons must be signed by the system key.
        return this.signedState == AddonManager.SIGNEDSTATE_SYSTEM;

      case KEY_APP_SYSTEM_DEFAULTS:
      case KEY_APP_TEMPORARY:
        // Temporary and built-in system add-ons do not require signing.
        return true;

      case KEY_APP_SYSTEM_SHARE:
      case KEY_APP_SYSTEM_LOCAL:
        // On UNIX platforms except OSX, an additional location for system
        // add-ons exists in /usr/{lib,share}/mozilla/extensions. Add-ons
        // installed there do not require signing.
        if (Services.appinfo.OS != "Darwin")
          return true;
        break;
    }

    if (this.signedState === AddonManager.SIGNEDSTATE_NOT_REQUIRED)
      return true;
    return this.signedState > AddonManager.SIGNEDSTATE_MISSING;
  },

  get unpack() {
    return this.type === "dictionary";
  },

  get isCompatible() {
    return this.isCompatibleWith();
  },

  get disabled() {
    return (this.userDisabled || this.appDisabled || this.softDisabled);
  },

  get isPlatformCompatible() {
    if (this.targetPlatforms.length == 0)
      return true;

    let matchedOS = false;

    // If any targetPlatform matches the OS and contains an ABI then we will
    // only match a targetPlatform that contains both the current OS and ABI
    let needsABI = false;

    // Some platforms do not specify an ABI, test against null in that case.
    let abi = null;
    try {
      abi = Services.appinfo.XPCOMABI;
    } catch (e) { }

    // Something is causing errors in here
    try {
      for (let platform of this.targetPlatforms) {
        if (platform.os == Services.appinfo.OS) {
          if (platform.abi) {
            needsABI = true;
            if (platform.abi === abi)
              return true;
          } else {
            matchedOS = true;
          }
        }
      }
    } catch (e) {
      let message = "Problem with addon " + this.id + " targetPlatforms "
                    + JSON.stringify(this.targetPlatforms);
      logger.error(message, e);
      AddonManagerPrivate.recordException("XPI", message, e);
      // don't trust this add-on
      return false;
    }

    return matchedOS && !needsABI;
  },

  isCompatibleWith(aAppVersion, aPlatformVersion) {
    let app = this.matchingTargetApplication;
    if (!app)
      return false;

    // set reasonable defaults for minVersion and maxVersion
    let minVersion = app.minVersion || "0";
    let maxVersion = app.maxVersion || "*";

    if (!aAppVersion)
      aAppVersion = Services.appinfo.version;
    if (!aPlatformVersion)
      aPlatformVersion = Services.appinfo.platformVersion;

    let version;
    if (app.id == Services.appinfo.ID)
      version = aAppVersion;
    else if (app.id == TOOLKIT_ID)
      version = aPlatformVersion;

    // Only extensions and dictionaries can be compatible by default; themes
    // and language packs always use strict compatibility checking.
    if (this.type in COMPATIBLE_BY_DEFAULT_TYPES &&
        !AddonManager.strictCompatibility && !this.strictCompatibility) {

      // The repository can specify compatibility overrides.
      // Note: For now, only blacklisting is supported by overrides.
      let overrides = AddonRepository.getCompatibilityOverridesSync(this.id);
      if (overrides) {
        let override = AddonRepository.findMatchingCompatOverride(this.version,
                                                                  overrides);
        if (override) {
          return false;
        }
      }

      // Extremely old extensions should not be compatible by default.
      let minCompatVersion;
      if (app.id == Services.appinfo.ID)
        minCompatVersion = XPIProvider.minCompatibleAppVersion;
      else if (app.id == TOOLKIT_ID)
        minCompatVersion = XPIProvider.minCompatiblePlatformVersion;

      if (minCompatVersion &&
          Services.vc.compare(minCompatVersion, maxVersion) > 0)
        return false;

      return Services.vc.compare(version, minVersion) >= 0;
    }

    return (Services.vc.compare(version, minVersion) >= 0) &&
           (Services.vc.compare(version, maxVersion) <= 0);
  },

  get matchingTargetApplication() {
    let app = null;
    for (let targetApp of this.targetApplications) {
      if (targetApp.id == Services.appinfo.ID)
        return targetApp;
      if (targetApp.id == TOOLKIT_ID)
        app = targetApp;
    }
    return app;
  },

  findBlocklistEntry() {
    let staticItem = findMatchingStaticBlocklistItem(this);
    if (staticItem) {
      let url = Services.urlFormatter.formatURLPref(PREF_BLOCKLIST_ITEM_URL);
      return {
        state: staticItem.level,
        url: url.replace(/%blockID%/g, staticItem.blockID)
      };
    }

    return Blocklist.getAddonBlocklistEntry(this.wrapper);
  },

  updateBlocklistState(options = {}) {
    let {applySoftBlock = true, oldAddon = null, updateDatabase = true} = options;

    if (oldAddon) {
      this.userDisabled = oldAddon.userDisabled;
      this.softDisabled = oldAddon.softDisabled;
      this.blocklistState = oldAddon.blocklistState;
    }
    let oldState = this.blocklistState;

    let entry = this.findBlocklistEntry();
    let newState = entry ? entry.state : Blocklist.STATE_NOT_BLOCKED;

    this.blocklistState = newState;
    this.blocklistURL = entry && entry.url;

    let userDisabled, softDisabled;
    // After a blocklist update, the blocklist service manually applies
    // new soft blocks after displaying a UI, in which cases we need to
    // skip updating it here.
    if (applySoftBlock && oldState != newState) {
      if (newState == Blocklist.STATE_SOFTBLOCKED) {
        if (this.type == "theme") {
          userDisabled = true;
        } else {
          softDisabled = !this.userDisabled;
        }
      } else {
        softDisabled = false;
      }
    }

    if (this.inDatabase && updateDatabase) {
      XPIProvider.updateAddonDisabledState(this, userDisabled, softDisabled);
      XPIDatabase.saveChanges();
    } else {
      this.appDisabled = !isUsableAddon(this);
      if (userDisabled !== undefined) {
        this.userDisabled = userDisabled;
      }
      if (softDisabled !== undefined) {
        this.softDisabled = softDisabled;
      }
    }
  },

  applyCompatibilityUpdate(aUpdate, aSyncCompatibility) {
    for (let targetApp of this.targetApplications) {
      for (let updateTarget of aUpdate.targetApplications) {
        if (targetApp.id == updateTarget.id && (aSyncCompatibility ||
            Services.vc.compare(targetApp.maxVersion, updateTarget.maxVersion) < 0)) {
          targetApp.minVersion = updateTarget.minVersion;
          targetApp.maxVersion = updateTarget.maxVersion;
        }
      }
    }
    this.appDisabled = !isUsableAddon(this);
  },

  /**
   * toJSON is called by JSON.stringify in order to create a filtered version
   * of this object to be serialized to a JSON file. A new object is returned
   * with copies of all non-private properties. Functions, getters and setters
   * are not copied.
   *
   * @param  aKey
   *         The key that this object is being serialized as in the JSON.
   *         Unused here since this is always the main object serialized
   *
   * @return an object containing copies of the properties of this object
   *         ignoring private properties, functions, getters and setters
   */
  toJSON(aKey) {
    let obj = {};
    for (let prop in this) {
      // Ignore the wrapper property
      if (prop == "wrapper")
        continue;

      // Ignore private properties
      if (prop.substring(0, 1) == "_")
        continue;

      // Ignore getters
      if (this.__lookupGetter__(prop))
        continue;

      // Ignore setters
      if (this.__lookupSetter__(prop))
        continue;

      // Ignore functions
      if (typeof this[prop] == "function")
        continue;

      obj[prop] = this[prop];
    }

    return obj;
  },

  /**
   * When an add-on install is pending its metadata will be cached in a file.
   * This method reads particular properties of that metadata that may be newer
   * than that in the install manifest, like compatibility information.
   *
   * @param  aObj
   *         A JS object containing the cached metadata
   */
  importMetadata(aObj) {
    for (let prop of PENDING_INSTALL_METADATA) {
      if (!(prop in aObj))
        continue;

      this[prop] = aObj[prop];
    }

    // Compatibility info may have changed so update appDisabled
    this.appDisabled = !isUsableAddon(this);
  },

  permissions() {
    let permissions = 0;

    // Add-ons that aren't installed cannot be modified in any way
    if (!(this.inDatabase))
      return permissions;

    if (!this.appDisabled) {
      if (this.userDisabled || this.softDisabled) {
        permissions |= AddonManager.PERM_CAN_ENABLE;
      } else if (this.type != "theme") {
        permissions |= AddonManager.PERM_CAN_DISABLE;
      }
    }

    // Add-ons that are in locked install locations, or are pending uninstall
    // cannot be upgraded or uninstalled
    if (!this._installLocation.locked && !this.pendingUninstall) {
      // Experiments cannot be upgraded.
      // System add-on upgrades are triggered through a different mechanism (see updateSystemAddons())
      let isSystem = this._installLocation.isSystem;
      // Add-ons that are installed by a file link cannot be upgraded.
      if (this.type != "experiment" &&
          !this._installLocation.isLinkedAddon(this.id) && !isSystem) {
        permissions |= AddonManager.PERM_CAN_UPGRADE;
      }

      permissions |= AddonManager.PERM_CAN_UNINSTALL;
    }

    if (Services.policies &&
        !Services.policies.isAllowed(`modify-extension:${this.id}`)) {
      permissions &= ~AddonManager.PERM_CAN_UNINSTALL;
      permissions &= ~AddonManager.PERM_CAN_DISABLE;
    }

    return permissions;
  },
};

/**
 * The AddonWrapper wraps an Addon to provide the data visible to consumers of
 * the public API.
 */
function AddonWrapper(aAddon) {
  wrapperMap.set(this, aAddon);
}

AddonWrapper.prototype = {
  get __AddonInternal__() {
    return AppConstants.DEBUG ? addonFor(this) : undefined;
  },

  get seen() {
    return addonFor(this).seen;
  },

  get hasEmbeddedWebExtension() {
    return addonFor(this).hasEmbeddedWebExtension;
  },

  markAsSeen() {
    addonFor(this).seen = true;
    XPIDatabase.saveChanges();
  },

  get type() {
    return getExternalType(addonFor(this).type);
  },

  get isWebExtension() {
    return isWebExtension(addonFor(this).type);
  },

  get temporarilyInstalled() {
    return addonFor(this)._installLocation == TemporaryInstallLocation;
  },

  get aboutURL() {
    return this.isActive ? addonFor(this).aboutURL : null;
  },

  get optionsURL() {
    if (!this.isActive) {
      return null;
    }

    let addon = addonFor(this);
    if (addon.optionsURL) {
      if (this.isWebExtension || this.hasEmbeddedWebExtension) {
        // The internal object's optionsURL property comes from the addons
        // DB and should be a relative URL.  However, extensions with
        // options pages installed before bug 1293721 was fixed got absolute
        // URLs in the addons db.  This code handles both cases.
        let policy = WebExtensionPolicy.getByID(addon.id);
        if (!policy) {
          return null;
        }
        let base = policy.getURL();
        return new URL(addon.optionsURL, base).href;
      }
      return addon.optionsURL;
    }

    return null;
  },

  get optionsType() {
    if (!this.isActive)
      return null;

    let addon = addonFor(this);
    let hasOptionsURL = !!this.optionsURL;

    if (addon.optionsType) {
      switch (parseInt(addon.optionsType, 10)) {
      case AddonManager.OPTIONS_TYPE_TAB:
      case AddonManager.OPTIONS_TYPE_INLINE_BROWSER:
        return hasOptionsURL ? addon.optionsType : null;
      }
      return null;
    }

    return null;
  },

  get optionsBrowserStyle() {
    let addon = addonFor(this);
    return addon.optionsBrowserStyle;
  },

  get iconURL() {
    return AddonManager.getPreferredIconURL(this, 48);
  },

  get icon64URL() {
    return AddonManager.getPreferredIconURL(this, 64);
  },

  get icons() {
    let addon = addonFor(this);
    let icons = {};

    if (addon._repositoryAddon) {
      for (let size in addon._repositoryAddon.icons) {
        icons[size] = addon._repositoryAddon.icons[size];
      }
    }

    if (addon.icons) {
      for (let size in addon.icons) {
        icons[size] = this.getResourceURI(addon.icons[size]).spec;
      }
    } else {
      // legacy add-on that did not update its icon data yet
      if (this.hasResource("icon.png")) {
        icons[32] = icons[48] = this.getResourceURI("icon.png").spec;
      }
      if (this.hasResource("icon64.png")) {
        icons[64] = this.getResourceURI("icon64.png").spec;
      }
    }

    let canUseIconURLs = this.isActive ||
      (addon.type == "theme" && addon.internalName == DEFAULT_SKIN);
    if (canUseIconURLs && addon.iconURL) {
      icons[32] = addon.iconURL;
      icons[48] = addon.iconURL;
    }

    if (canUseIconURLs && addon.icon64URL) {
      icons[64] = addon.icon64URL;
    }

    Object.freeze(icons);
    return icons;
  },

  get screenshots() {
    let addon = addonFor(this);
    let repositoryAddon = addon._repositoryAddon;
    if (repositoryAddon && ("screenshots" in repositoryAddon)) {
      let repositoryScreenshots = repositoryAddon.screenshots;
      if (repositoryScreenshots && repositoryScreenshots.length > 0)
        return repositoryScreenshots;
    }

    if (isTheme(addon.type) && this.hasResource("preview.png")) {
      let url = this.getResourceURI("preview.png").spec;
      return [new AddonManagerPrivate.AddonScreenshot(url)];
    }

    return null;
  },

  get applyBackgroundUpdates() {
    return addonFor(this).applyBackgroundUpdates;
  },
  set applyBackgroundUpdates(val) {
    let addon = addonFor(this);
    if (this.type == "experiment") {
      logger.warn("Setting applyBackgroundUpdates on an experiment is not supported.");
      return addon.applyBackgroundUpdates;
    }

    if (val != AddonManager.AUTOUPDATE_DEFAULT &&
        val != AddonManager.AUTOUPDATE_DISABLE &&
        val != AddonManager.AUTOUPDATE_ENABLE) {
      val = val ? AddonManager.AUTOUPDATE_DEFAULT :
                  AddonManager.AUTOUPDATE_DISABLE;
    }

    if (val == addon.applyBackgroundUpdates)
      return val;

    XPIDatabase.setAddonProperties(addon, {
      applyBackgroundUpdates: val
    });
    AddonManagerPrivate.callAddonListeners("onPropertyChanged", this, ["applyBackgroundUpdates"]);

    return val;
  },

  set syncGUID(val) {
    let addon = addonFor(this);
    if (addon.syncGUID == val)
      return val;

    if (addon.inDatabase)
      XPIDatabase.setAddonSyncGUID(addon, val);

    addon.syncGUID = val;

    return val;
  },

  get install() {
    let addon = addonFor(this);
    if (!("_install" in addon) || !addon._install)
      return null;
    return addon._install.wrapper;
  },

  get pendingUpgrade() {
    let addon = addonFor(this);
    return addon.pendingUpgrade ? addon.pendingUpgrade.wrapper : null;
  },

  get scope() {
    let addon = addonFor(this);
    if (addon._installLocation)
      return addon._installLocation.scope;

    return AddonManager.SCOPE_PROFILE;
  },

  get pendingOperations() {
    let addon = addonFor(this);
    let pending = 0;
    if (!(addon.inDatabase)) {
      // Add-on is pending install if there is no associated install (shouldn't
      // happen here) or if the install is in the process of or has successfully
      // completed the install. If an add-on is pending install then we ignore
      // any other pending operations.
      if (!addon._install || addon._install.state == AddonManager.STATE_INSTALLING ||
          addon._install.state == AddonManager.STATE_INSTALLED)
        return AddonManager.PENDING_INSTALL;
    } else if (addon.pendingUninstall) {
      // If an add-on is pending uninstall then we ignore any other pending
      // operations
      return AddonManager.PENDING_UNINSTALL;
    }

    if (addon.active && addon.disabled)
      pending |= AddonManager.PENDING_DISABLE;
    else if (!addon.active && !addon.disabled)
      pending |= AddonManager.PENDING_ENABLE;

    if (addon.pendingUpgrade)
      pending |= AddonManager.PENDING_UPGRADE;

    return pending;
  },

  get operationsRequiringRestart() {
    return 0;
  },

  get isDebuggable() {
    return this.isActive && addonFor(this).bootstrap;
  },

  get permissions() {
    return addonFor(this).permissions();
  },

  get isActive() {
    let addon = addonFor(this);
    if (!addon.active)
      return false;
    if (!Services.appinfo.inSafeMode)
      return true;
    return addon.bootstrap && canRunInSafeMode(addon);
  },

  get startupPromise() {
    let addon = addonFor(this);
    if (!addon.bootstrap || !this.isActive)
      return null;

    let activeAddon = XPIProvider.activeAddons.get(addon.id);
    if (activeAddon)
      return activeAddon.startupPromise || null;
    return null;
  },

  updateBlocklistState(applySoftBlock = true) {
    addonFor(this).updateBlocklistState({applySoftBlock});
  },

  get userDisabled() {
    let addon = addonFor(this);
    return addon.softDisabled || addon.userDisabled;
  },
  set userDisabled(val) {
    let addon = addonFor(this);
    if (val == this.userDisabled) {
      return val;
    }

    if (addon.inDatabase) {
      let theme = isTheme(addon.type);
      if (theme && val) {
        if (addon.internalName == DEFAULT_SKIN)
          throw new Error("Cannot disable the default theme");

        let defaultTheme = XPIDatabase.getVisibleAddonForInternalName(DEFAULT_SKIN);
        XPIProvider.updateAddonDisabledState(defaultTheme, false);
      }
      if (!(theme && val) || isWebExtension(addon.type)) {
        // hidden and system add-ons should not be user disasbled,
        // as there is no UI to re-enable them.
        if (this.hidden) {
          throw new Error(`Cannot disable hidden add-on ${addon.id}`);
        }
        XPIProvider.updateAddonDisabledState(addon, val);
      }
    } else {
      addon.userDisabled = val;
      // When enabling remove the softDisabled flag
      if (!val)
        addon.softDisabled = false;
    }

    return val;
  },

  set softDisabled(val) {
    let addon = addonFor(this);
    if (val == addon.softDisabled)
      return val;

    if (addon.inDatabase) {
      // When softDisabling a theme just enable the active theme
      if (isTheme(addon.type) && val && !addon.userDisabled) {
        if (addon.internalName == DEFAULT_SKIN)
          throw new Error("Cannot disable the default theme");
        if (isWebExtension(addon.type))
          XPIProvider.updateAddonDisabledState(addon, undefined, val);
      } else {
        XPIProvider.updateAddonDisabledState(addon, undefined, val);
      }
    } else if (!addon.userDisabled) {
      // Only set softDisabled if not already disabled
      addon.softDisabled = val;
    }

    return val;
  },

  get hidden() {
    let addon = addonFor(this);
    if (addon._installLocation.name == KEY_APP_TEMPORARY)
      return false;

    return addon._installLocation.isSystem;
  },

  get isSystem() {
    let addon = addonFor(this);
    return addon._installLocation.isSystem;
  },

  // Returns true if Firefox Sync should sync this addon. Only addons
  // in the profile install location are considered syncable.
  get isSyncable() {
    let addon = addonFor(this);
    return (addon._installLocation.name == KEY_APP_PROFILE);
  },

  get userPermissions() {
    return addonFor(this).userPermissions;
  },

  isCompatibleWith(aAppVersion, aPlatformVersion) {
    return addonFor(this).isCompatibleWith(aAppVersion, aPlatformVersion);
  },

  uninstall(alwaysAllowUndo) {
    let addon = addonFor(this);
    XPIProvider.uninstallAddon(addon, alwaysAllowUndo);
  },

  cancelUninstall() {
    let addon = addonFor(this);
    XPIProvider.cancelUninstallAddon(addon);
  },

  findUpdates(aListener, aReason, aAppVersion, aPlatformVersion) {
    // Short-circuit updates for experiments because updates are handled
    // through the Experiments Manager.
    if (this.type == "experiment") {
      AddonManagerPrivate.callNoUpdateListeners(this, aListener, aReason,
                                                aAppVersion, aPlatformVersion);
      return;
    }

    new UpdateChecker(addonFor(this), aListener, aReason, aAppVersion, aPlatformVersion);
  },

  // Returns true if there was an update in progress, false if there was no update to cancel
  cancelUpdate() {
    let addon = addonFor(this);
    if (addon._updateCheck) {
      addon._updateCheck.cancel();
      return true;
    }
    return false;
  },

  hasResource(aPath) {
    let addon = addonFor(this);
    if (addon._hasResourceCache.has(aPath))
      return addon._hasResourceCache.get(aPath);

    let bundle = addon._sourceBundle.clone();

    // Bundle may not exist any more if the addon has just been uninstalled,
    // but explicitly first checking .exists() results in unneeded file I/O.
    try {
      var isDir = bundle.isDirectory();
    } catch (e) {
      addon._hasResourceCache.set(aPath, false);
      return false;
    }

    if (isDir) {
      if (aPath)
        aPath.split("/").forEach(part => bundle.append(part));
      let result = bundle.exists();
      addon._hasResourceCache.set(aPath, result);
      return result;
    }

    let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                    createInstance(Ci.nsIZipReader);
    try {
      zipReader.open(bundle);
      let result = zipReader.hasEntry(aPath);
      addon._hasResourceCache.set(aPath, result);
      return result;
    } catch (e) {
      addon._hasResourceCache.set(aPath, false);
      return false;
    } finally {
      zipReader.close();
    }
  },

  /**
   * Reloads the add-on.
   *
   * For temporarily installed add-ons, this uninstalls and re-installs the
   * add-on. Otherwise, the addon is disabled and then re-enabled, and the cache
   * is flushed.
   *
   * @return Promise
   */
  reload() {
    return new Promise((resolve) => {
      const addon = addonFor(this);

      logger.debug(`reloading add-on ${addon.id}`);

      if (!this.temporarilyInstalled) {
        let addonFile = addon.getResourceURI;
        XPIProvider.updateAddonDisabledState(addon, true);
        Services.obs.notifyObservers(addonFile, "flush-cache-entry");
        XPIProvider.updateAddonDisabledState(addon, false);
        resolve();
      } else {
        // This function supports re-installing an existing add-on.
        resolve(AddonManager.installTemporaryAddon(addon._sourceBundle));
      }
    });
  },

  /**
   * Returns a URI to the selected resource or to the add-on bundle if aPath
   * is null. URIs to the bundle will always be file: URIs. URIs to resources
   * will be file: URIs if the add-on is unpacked or jar: URIs if the add-on is
   * still an XPI file.
   *
   * @param  aPath
   *         The path in the add-on to get the URI for or null to get a URI to
   *         the file or directory the add-on is installed as.
   * @return an nsIURI
   */
  getResourceURI(aPath) {
    let addon = addonFor(this);
    if (!aPath)
      return Services.io.newFileURI(addon._sourceBundle);

    return getURIForResourceInFile(addon._sourceBundle, aPath);
  }
};

function chooseValue(aAddon, aObj, aProp) {
  let repositoryAddon = aAddon._repositoryAddon;
  let objValue = aObj[aProp];

  if (repositoryAddon && (aProp in repositoryAddon) &&
      (objValue === undefined || objValue === null)) {
    return [repositoryAddon[aProp], true];
  }

  return [objValue, false];
}

function defineAddonWrapperProperty(name, getter) {
  Object.defineProperty(AddonWrapper.prototype, name, {
    get: getter,
    enumerable: true,
  });
}

["id", "syncGUID", "version", "isCompatible", "isPlatformCompatible",
 "providesUpdatesSecurely", "blocklistState", "blocklistURL", "appDisabled",
 "softDisabled", "skinnable", "size", "foreignInstall",
 "strictCompatibility", "updateURL", "dependencies",
 "signedState", "isCorrectlySigned"].forEach(function(aProp) {
   defineAddonWrapperProperty(aProp, function() {
     let addon = addonFor(this);
     return (aProp in addon) ? addon[aProp] : undefined;
   });
});

["fullDescription", "developerComments", "supportURL",
 "contributionURL", "averageRating", "reviewCount",
 "reviewURL", "weeklyDownloads"].forEach(function(aProp) {
  defineAddonWrapperProperty(aProp, function() {
    let addon = addonFor(this);
    if (addon._repositoryAddon)
      return addon._repositoryAddon[aProp];

    return null;
  });
});

["installDate", "updateDate"].forEach(function(aProp) {
  defineAddonWrapperProperty(aProp, function() {
    return new Date(addonFor(this)[aProp]);
  });
});

["sourceURI", "releaseNotesURI"].forEach(function(aProp) {
  defineAddonWrapperProperty(aProp, function() {
    let addon = addonFor(this);

    // Temporary Installed Addons do not have a "sourceURI",
    // But we can use the "_sourceBundle" as an alternative,
    // which points to the path of the addon xpi installed
    // or its source dir (if it has been installed from a
    // directory).
    if (aProp == "sourceURI" && this.temporarilyInstalled) {
      return Services.io.newFileURI(addon._sourceBundle);
    }

    let [target, fromRepo] = chooseValue(addon, addon, aProp);
    if (!target)
      return null;
    if (fromRepo)
      return target;
    return Services.io.newURI(target);
  });
});

PROP_LOCALE_SINGLE.forEach(function(aProp) {
  defineAddonWrapperProperty(aProp, function() {
    let addon = addonFor(this);
    // Override XPI creator if repository creator is defined
    if (aProp == "creator" &&
        addon._repositoryAddon && addon._repositoryAddon.creator) {
      return addon._repositoryAddon.creator;
    }

    let result = null;

    if (addon.active) {
      try {
        let pref = PREF_EM_EXTENSION_FORMAT + addon.id + "." + aProp;
        let value = Services.prefs.getPrefType(pref) != Ci.nsIPrefBranch.PREF_INVALID ? Services.prefs.getComplexValue(pref, Ci.nsIPrefLocalizedString).data : null;
        if (value)
          result = value;
      } catch (e) {
      }
    }

    if (result == null)
      [result] = chooseValue(addon, addon.selectedLocale, aProp);

    if (aProp == "creator")
      return result ? new AddonManagerPrivate.AddonAuthor(result) : null;

    return result;
  });
});

PROP_LOCALE_MULTI.forEach(function(aProp) {
  defineAddonWrapperProperty(aProp, function() {
    let addon = addonFor(this);
    let results = null;
    let usedRepository = false;

    if (addon.active) {
      let pref = PREF_EM_EXTENSION_FORMAT + addon.id + "." +
                 aProp.substring(0, aProp.length - 1);
      let list = Services.prefs.getChildList(pref, {});
      if (list.length > 0) {
        list.sort();
        results = [];
        for (let childPref of list) {
          let value = Services.prefs.getPrefType(childPref) != Ci.nsIPrefBranch.PREF_INVALID ? Services.prefs.getComplexValue(childPref, Ci.nsIPrefLocalizedString).data : null;
          if (value)
            results.push(value);
        }
      }
    }

    if (results == null)
      [results, usedRepository] = chooseValue(addon, addon.selectedLocale, aProp);

    if (results && !usedRepository) {
      results = results.map(function(aResult) {
        return new AddonManagerPrivate.AddonAuthor(aResult);
      });
    }

    return results;
  });
});

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
   * @param  aName
   *         The string identifier for the install location
   * @param  aDirectory
   *         The nsIFile directory for the install location
   * @param  aScope
   *         The scope of add-ons installed in this location
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
   * @param   file
   *          The file containing the directory path
   * @return  An nsIFile object representing the linked directory.
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
   * Gets an array of nsIFiles for add-ons installed in this location.
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
   * @param  aId
   *         The ID of the add-on
   * @return The nsIFile
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
   * @param aId
   *        The ID of the addon
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
   * @param  aName
   *         The string identifier for the install location
   * @param  aDirectory
   *         The nsIFile directory for the install location
   * @param  aScope
   *         The scope of add-ons installed in this location
   */
  constructor(aName, aDirectory, aScope) {
    super(aName, aDirectory, aScope);

    this.locked = false;
    this._stagingDirLock = 0;
  }

  /**
   * Gets the staging directory to put add-ons that are pending install and
   * uninstall into.
   *
   * @return an nsIFile
   */
  getStagingDir() {
    return getFile(DIR_STAGE, this._directory);
  }

  requestStagingDir() {
    this._stagingDirLock++;

    if (this._stagingDirPromise)
      return this._stagingDirPromise;

    OS.File.makeDir(this._directory.path);
    let stagepath = OS.Path.join(this._directory.path, DIR_STAGE);
    return this._stagingDirPromise = OS.File.makeDir(stagepath).catch((e) => {
      if (e instanceof OS.File.Error && e.becauseExists)
        return;
      logger.error("Failed to create staging directory", e);
      throw e;
    });
  }

  releaseStagingDir() {
    this._stagingDirLock--;

    if (this._stagingDirLock == 0) {
      this._stagingDirPromise = null;
      this.cleanStagingDir();
    }

    return Promise.resolve();
  }

  /**
   * Removes the specified files or directories in the staging directory and
   * then if the staging directory is empty attempts to remove it.
   *
   * @param  aLeafNames
   *         An array of file or directory to remove from the directory, the
   *         array may be empty
   */
  cleanStagingDir(aLeafNames = []) {
    let dir = this.getStagingDir();

    for (let name of aLeafNames) {
      let file = getFile(name, dir);
      XPIInstall.recursiveRemove(file);
    }

    if (this._stagingDirLock > 0)
      return;

    let dirEntries = dir.directoryEntries.QueryInterface(Ci.nsIDirectoryEnumerator);
    try {
      if (dirEntries.nextFile)
        return;
    } finally {
      dirEntries.close();
    }

    try {
      setFilePermissions(dir, FileUtils.PERMS_DIRECTORY);
      dir.remove(false);
    } catch (e) {
      logger.warn("Failed to remove staging dir", e);
      // Failing to remove the staging directory is ignorable
    }
  }

  /**
   * Returns a directory that is normally on the same filesystem as the rest of
   * the install location and can be used for temporarily storing files during
   * safe move operations. Calling this method will delete the existing trash
   * directory and its contents.
   *
   * @return an nsIFile
   */
  getTrashDir() {
    let trashDir = getFile(DIR_TRASH, this._directory);
    let trashDirExists = trashDir.exists();
    try {
      if (trashDirExists)
        XPIInstall.recursiveRemove(trashDir);
      trashDirExists = false;
    } catch (e) {
      logger.warn("Failed to remove trash directory", e);
    }
    if (!trashDirExists)
      trashDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

    return trashDir;
  }

  /**
   * Installs an add-on into the install location.
   *
   * @param  id
   *         The ID of the add-on to install
   * @param  source
   *         The source nsIFile to install from
   * @param  existingAddonID
   *         The ID of an existing add-on to uninstall at the same time
   * @param  action
   *         What to we do with the given source file:
   *           "move"
   *           Default action, the source files will be moved to the new
   *           location,
   *           "copy"
   *           The source files will be copied,
   *           "proxy"
   *           A "proxy file" is going to refer to the source file path
   * @return an nsIFile indicating where the add-on was installed to
   */
  installAddon({ id, source, existingAddonID, action = "move" }) {
    let trashDir = this.getTrashDir();

    let transaction = new SafeInstallOperation();

    let moveOldAddon = aId => {
      let file = getFile(aId, this._directory);
      if (file.exists())
        transaction.moveUnder(file, trashDir);

      file = getFile(`${aId}.xpi`, this._directory);
      if (file.exists()) {
        XPIInstall.flushJarCache(file);
        transaction.moveUnder(file, trashDir);
      }
    };

    // If any of these operations fails the finally block will clean up the
    // temporary directory
    try {
      moveOldAddon(id);
      if (existingAddonID && existingAddonID != id) {
        moveOldAddon(existingAddonID);

        {
          // Move the data directories.
          /* XXX ajvincent We can't use OS.File:  installAddon isn't compatible
           * with Promises, nor is SafeInstallOperation.  Bug 945540 has been filed
           * for porting to OS.File.
           */
          let oldDataDir = FileUtils.getDir(
            KEY_PROFILEDIR, ["extension-data", existingAddonID], false, true
          );

          if (oldDataDir.exists()) {
            let newDataDir = FileUtils.getDir(
              KEY_PROFILEDIR, ["extension-data", id], false, true
            );
            if (newDataDir.exists()) {
              let trashData = getFile("data-directory", trashDir);
              transaction.moveUnder(newDataDir, trashData);
            }

            transaction.moveTo(oldDataDir, newDataDir);
          }
        }
      }

      if (action == "copy") {
        transaction.copy(source, this._directory);
      } else if (action == "move") {
        if (source.isFile())
          XPIInstall.flushJarCache(source);

        transaction.moveUnder(source, this._directory);
      }
      // Do nothing for the proxy file as we sideload an addon permanently
    } finally {
      // It isn't ideal if this cleanup fails but it isn't worth rolling back
      // the install because of it.
      try {
        XPIInstall.recursiveRemove(trashDir);
      } catch (e) {
        logger.warn("Failed to remove trash directory when installing " + id, e);
      }
    }

    let newFile = this._directory.clone();

    if (action == "proxy") {
      // When permanently installing sideloaded addon, we just put a proxy file
      // referring to the addon sources
      newFile.append(id);

      writeStringToFile(newFile, source.path);
    } else {
      newFile.append(source.leafName);
    }

    try {
      newFile.lastModifiedTime = Date.now();
    } catch (e) {
      logger.warn("failed to set lastModifiedTime on " + newFile.path, e);
    }
    this._IDToFileMap[id] = newFile;

    if (existingAddonID && existingAddonID != id &&
        existingAddonID in this._IDToFileMap) {
      delete this._IDToFileMap[existingAddonID];
    }

    return newFile;
  }

  /**
   * Uninstalls an add-on from this location.
   *
   * @param  aId
   *         The ID of the add-on to uninstall
   * @throws if the ID does not match any of the add-ons installed
   */
  uninstallAddon(aId) {
    let file = this._IDToFileMap[aId];
    if (!file) {
      logger.warn("Attempted to remove " + aId + " from " +
           this._name + " but it was already gone");
      return;
    }

    file = getFile(aId, this._directory);
    if (!file.exists())
      file.leafName += ".xpi";

    if (!file.exists()) {
      logger.warn("Attempted to remove " + aId + " from " +
           this._name + " but it was already gone");

      delete this._IDToFileMap[aId];
      return;
    }

    let trashDir = this.getTrashDir();

    if (file.leafName != aId) {
      logger.debug("uninstallAddon: flushing jar cache " + file.path + " for addon " + aId);
      XPIInstall.flushJarCache(file);
    }

    let transaction = new SafeInstallOperation();

    try {
      transaction.moveUnder(file, trashDir);
    } finally {
      // It isn't ideal if this cleanup fails, but it is probably better than
      // rolling back the uninstall at this point
      try {
        XPIInstall.recursiveRemove(trashDir);
      } catch (e) {
        logger.warn("Failed to remove trash directory when uninstalling " + aId, e);
      }
    }

    XPIStates.removeAddon(this.name, aId);

    delete this._IDToFileMap[aId];
  }
}

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
    * @param  aName
    *         The string identifier for the install location
    * @param  aDirectory
    *         The nsIFile directory for the install location
    * @param  aScope
    *         The scope of add-ons installed in this location
    * @param  aResetSet
    *         True to throw away the current add-on set
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
   * Gets the staging directory to put add-ons that are pending install and
   * uninstall into.
   *
   * @return {nsIFile} - staging directory for system add-on upgrades.
   */
  getStagingDir() {
    this._addonSet = SystemAddonInstallLocation._loadAddonSet();
    let dir = null;
    if (this._addonSet.directory) {
      this._directory = getFile(this._addonSet.directory, this._baseDir);
      dir = getFile(DIR_STAGE, this._directory);
    } else {
      logger.info("SystemAddonInstallLocation directory is missing");
    }

    return dir;
  }

  requestStagingDir() {
    this._addonSet = SystemAddonInstallLocation._loadAddonSet();
    if (this._addonSet.directory) {
      this._directory = getFile(this._addonSet.directory, this._baseDir);
    }
    return super.requestStagingDir();
  }

  /**
   * Reads the current set of system add-ons
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

  /**
   * Saves the current set of system add-ons
   *
   * @param {Object} aAddonSet - object containing schema, directory and set
   *                 of system add-on IDs and versions.
   */
  static _saveAddonSet(aAddonSet) {
    Services.prefs.setStringPref(PREF_SYSTEM_ADDON_SET, JSON.stringify(aAddonSet));
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
   */
  isActive() {
    return this._directory != null;
  }

  isValidAddon(aAddon) {
    if (aAddon.appDisabled) {
      logger.warn(`System add-on ${aAddon.id} isn't compatible with the application.`);
      return false;
    }

    return true;
  }

  /**
   * Tests whether the loaded add-on information matches what is expected.
   */
  isValid(aAddons) {
    for (let id of Object.keys(this._addonSet.addons)) {
      if (!aAddons.has(id)) {
        logger.warn(`Expected add-on ${id} is missing from the system add-on location.`);
        return false;
      }

      let addon = aAddons.get(id);
      if (addon.version != this._addonSet.addons[id].version) {
        logger.warn(`Expected system add-on ${id} to be version ${this._addonSet.addons[id].version} but was ${addon.version}.`);
        return false;
      }

      if (!this.isValidAddon(addon))
        return false;
    }

    return true;
  }

  /**
   * Resets the add-on set so on the next startup the default set will be used.
   */
  resetAddonSet() {
    logger.info("Removing all system add-on upgrades.");

    // remove everything from the pref first, if uninstall
    // fails then at least they will not be re-activated on
    // next restart.
    this._addonSet = { schema: 1, addons: {} };
    SystemAddonInstallLocation._saveAddonSet(this._addonSet);

    // If this is running at app startup, the pref being cleared
    // will cause later stages of startup to notice that the
    // old updates are now gone.
    //
    // Updates will only be explicitly uninstalled if they are
    // removed restartlessly, for instance if they are no longer
    // part of the latest update set.
    if (this._addonSet) {
      for (let id of Object.keys(this._addonSet.addons)) {
        AddonManager.getAddonByID(id, addon => {
          if (addon) {
            addon.uninstall();
          }
        });
      }
    }
  }

  /**
   * Removes any directories not currently in use or pending use after a
   * restart. Any errors that happen here don't really matter as we'll attempt
   * to cleanup again next time.
   */
  async cleanDirectories() {
    // System add-ons directory does not exist
    if (!(await OS.File.exists(this._baseDir.path))) {
      return;
    }

    let iterator;
    try {
      iterator = new OS.File.DirectoryIterator(this._baseDir.path);
    } catch (e) {
      logger.error("Failed to clean updated system add-ons directories.", e);
      return;
    }

    try {
      for (;;) {
        let {value: entry, done} = await iterator.next();
        if (done) {
          break;
        }

        // Skip the directory currently in use
        if (this._directory && this._directory.path == entry.path) {
          continue;
        }

        // Skip the next directory
        if (this._nextDir && this._nextDir.path == entry.path) {
          continue;
        }

        if (entry.isDir) {
          await OS.File.removeDir(entry.path, {
            ignoreAbsent: true,
            ignorePermissions: true,
          });
        } else {
          await OS.File.remove(entry.path, {
            ignoreAbsent: true,
          });
        }
      }

    } catch (e) {
      logger.error("Failed to clean updated system add-ons directories.", e);
    } finally {
      iterator.close();
    }
  }

  /**
   * Installs a new set of system add-ons into the location and updates the
   * add-on set in prefs.
   *
   * @param {Array} aAddons - An array of addons to install.
   */
  async installAddonSet(aAddons) {
    // Make sure the base dir exists
    await OS.File.makeDir(this._baseDir.path, { ignoreExisting: true });

    let addonSet = SystemAddonInstallLocation._loadAddonSet();

    // Remove any add-ons that are no longer part of the set.
    for (let addonID of Object.keys(addonSet.addons)) {
      if (!aAddons.includes(addonID)) {
        AddonManager.getAddonByID(addonID, a => a.uninstall());
      }
    }

    let newDir = this._baseDir.clone();

    let uuidGen = Cc["@mozilla.org/uuid-generator;1"].
                  getService(Ci.nsIUUIDGenerator);
    newDir.append("blank");

    while (true) {
      newDir.leafName = uuidGen.generateUUID().toString();

      try {
        await OS.File.makeDir(newDir.path, { ignoreExisting: false });
        break;
      } catch (e) {
        logger.debug("Could not create new system add-on updates dir, retrying", e);
      }
    }

    // Record the new upgrade directory.
    let state = { schema: 1, directory: newDir.leafName, addons: {} };
    SystemAddonInstallLocation._saveAddonSet(state);

    this._nextDir = newDir;
    let location = this;

    let installs = [];
    for (let addon of aAddons) {
      let install = await createLocalInstall(addon._sourceBundle, location);
      installs.push(install);
    }

    async function installAddon(install) {
      // Make the new install own its temporary file.
      install.ownsTempFile = true;
      install.install();
    }

    async function postponeAddon(install) {
      let resumeFn;
      if (AddonManagerPrivate.hasUpgradeListener(install.addon.id)) {
        logger.info(`system add-on ${install.addon.id} has an upgrade listener, postponing upgrade set until restart`);
        resumeFn = () => {
          logger.info(`${install.addon.id} has resumed a previously postponed addon set`);
          install.installLocation.resumeAddonSet(installs);
        };
      }
      await install.postpone(resumeFn);
    }

    let previousState;

    try {
      // All add-ons in position, create the new state and store it in prefs
      state = { schema: 1, directory: newDir.leafName, addons: {} };
      for (let addon of aAddons) {
        state.addons[addon.id] = {
          version: addon.version
        };
      }

      previousState = SystemAddonInstallLocation._loadAddonSet();
      SystemAddonInstallLocation._saveAddonSet(state);

      let blockers = aAddons.filter(
        addon => AddonManagerPrivate.hasUpgradeListener(addon.id)
      );

      if (blockers.length > 0) {
        await waitForAllPromises(installs.map(postponeAddon));
      } else {
        await waitForAllPromises(installs.map(installAddon));
      }
    } catch (e) {
      // Roll back to previous upgrade set (if present) on restart.
      if (previousState) {
        SystemAddonInstallLocation._saveAddonSet(previousState);
      }
      // Otherwise, roll back to built-in set on restart.
      // TODO try to do these restartlessly
      this.resetAddonSet();

      try {
        await OS.File.removeDir(newDir.path, { ignorePermissions: true });
      } catch (e) {
        logger.warn(`Failed to remove failed system add-on directory ${newDir.path}.`, e);
      }
      throw e;
    }
  }

 /**
  * Resumes upgrade of a previously-delayed add-on set.
  */
  async resumeAddonSet(installs) {
    async function resumeAddon(install) {
      install.state = AddonManager.STATE_DOWNLOADED;
      install.installLocation.releaseStagingDir();
      install.install();
    }

    let blockers = installs.filter(
      install => AddonManagerPrivate.hasUpgradeListener(install.addon.id)
    );

    if (blockers.length > 1) {
      logger.warn("Attempted to resume system add-on install but upgrade blockers are still present");
    } else {
      await waitForAllPromises(installs.map(resumeAddon));
    }
  }

  /**
   * Returns a directory that is normally on the same filesystem as the rest of
   * the install location and can be used for temporarily storing files during
   * safe move operations. Calling this method will delete the existing trash
   * directory and its contents.
   *
   * @return an nsIFile
   */
  getTrashDir() {
    let trashDir = getFile(DIR_TRASH, this._directory);
    let trashDirExists = trashDir.exists();
    try {
      if (trashDirExists)
        XPIInstall.recursiveRemove(trashDir);
      trashDirExists = false;
    } catch (e) {
      logger.warn("Failed to remove trash directory", e);
    }
    if (!trashDirExists)
      trashDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

    return trashDir;
  }

  /**
   * Installs an add-on into the install location.
   *
   * @param  id
   *         The ID of the add-on to install
   * @param  source
   *         The source nsIFile to install from
   * @return an nsIFile indicating where the add-on was installed to
   */
  installAddon({id, source}) {
    let trashDir = this.getTrashDir();
    let transaction = new SafeInstallOperation();

    // If any of these operations fails the finally block will clean up the
    // temporary directory
    try {
      if (source.isFile()) {
        XPIInstall.flushJarCache(source);
      }

      transaction.moveUnder(source, this._directory);
    } finally {
      // It isn't ideal if this cleanup fails but it isn't worth rolling back
      // the install because of it.
      try {
        XPIInstall.recursiveRemove(trashDir);
      } catch (e) {
        logger.warn("Failed to remove trash directory when installing " + id, e);
      }
    }

    let newFile = getFile(source.leafName, this._directory);

    try {
      newFile.lastModifiedTime = Date.now();
    } catch (e) {
      logger.warn("failed to set lastModifiedTime on " + newFile.path, e);
    }
    this._IDToFileMap[id] = newFile;

    return newFile;
  }

  // old system add-on upgrade dirs get automatically removed
  uninstallAddon(aAddon) {}
}

/**
 * An object which identifies an install location for temporary add-ons.
 */
const TemporaryInstallLocation = {
  locked: false,
  name: KEY_APP_TEMPORARY,
  scope: AddonManager.SCOPE_TEMPORARY,
  getAddonLocations: () => [],
  isLinkedAddon: () => false,
  installAddon: () => {},
  uninstallAddon: (aAddon) => {},
  getStagingDir: () => {},
};

/**
 * An object that identifies a registry install location for add-ons. The location
 * consists of a registry key which contains string values mapping ID to the
 * path where an add-on is installed
 *
 */
class WinRegInstallLocation extends DirectoryInstallLocation {
  /**
    * @param  aName
    *         The string identifier of this Install Location.
    * @param  aRootKey
    *         The root key (one of the ROOT_KEY_ values from nsIWindowsRegKey).
    * @param  scope
    *         The scope of add-ons installed in this location
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
   * @param  key
   *         The key that contains the ID to path mapping
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

  /**
   * @see DirectoryInstallLocation
   */
  isLinkedAddon(aId) {
    return true;
  }
}

var XPIInternal = {
  AddonInternal,
  BOOTSTRAP_REASONS,
  KEY_APP_SYSTEM_ADDONS,
  KEY_APP_SYSTEM_DEFAULTS,
  KEY_APP_TEMPORARY,
  SIGNED_TYPES,
  TEMPORARY_ADDON_SUFFIX,
  TOOLKIT_ID,
  XPIStates,
  getExternalType,
  isTheme,
  isUsableAddon,
  isWebExtension,
  mustSign,
  recordAddonTelemetry,

  get XPIDatabase() { return gGlobalScope.XPIDatabase; },
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

// We only register experiments support if the application supports them.
// Ideally, we would install an observer to watch the pref. Installing
// an observer for this pref is not necessary here and may be buggy with
// regards to registering this XPIProvider twice.
if (Services.prefs.getBoolPref("experiments.supported", false)) {
  addonTypes.push(
    new AddonManagerPrivate.AddonType("experiment",
                                      URI_EXTENSION_STRINGS,
                                      "type.experiment.name",
                                      AddonManager.VIEW_TYPE_LIST, 11000,
                                      AddonManager.TYPE_UI_HIDE_EMPTY | AddonManager.TYPE_SUPPORTS_UNDO_RESTARTLESS_UNINSTALL));
}

AddonManagerPrivate.registerProvider(XPIProvider, addonTypes);
