 /* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

this.EXPORTED_SYMBOLS = ["XPIProvider"];

const CONSTANTS = {};
Cu.import("resource://gre/modules/addons/AddonConstants.jsm", CONSTANTS);
const { ADDON_SIGNING, REQUIRE_SIGNING } = CONSTANTS

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");
/*globals AddonManagerPrivate*/
Cu.import("resource://gre/modules/Preferences.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonRepository",
                                  "resource://gre/modules/addons/AddonRepository.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ChromeManifestParser",
                                  "resource://gre/modules/ChromeManifestParser.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeManager",
                                  "resource://gre/modules/LightweightThemeManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionData",
                                  "resource://gre/modules/Extension.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionManagement",
                                  "resource://gre/modules/ExtensionManagement.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Locale",
                                  "resource://gre/modules/Locale.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ZipUtils",
                                  "resource://gre/modules/ZipUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PermissionsUtils",
                                  "resource://gre/modules/PermissionsUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserToolboxProcess",
                                  "resource://devtools/client/framework/ToolboxProcess.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ConsoleAPI",
                                  "resource://gre/modules/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ProductAddonChecker",
                                  "resource://gre/modules/addons/ProductAddonChecker.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateUtils",
                                  "resource://gre/modules/UpdateUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "isAddonPartOfE10SRollout",
                                  "resource://gre/modules/addons/E10SAddonsRollout.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "Blocklist",
                                   "@mozilla.org/extensions/blocklist;1",
                                   Ci.nsIBlocklistService);
XPCOMUtils.defineLazyServiceGetter(this,
                                   "ChromeRegistry",
                                   "@mozilla.org/chrome/chrome-registry;1",
                                   "nsIChromeRegistry");
XPCOMUtils.defineLazyServiceGetter(this,
                                   "ResProtocolHandler",
                                   "@mozilla.org/network/protocol;1?name=resource",
                                   "nsIResProtocolHandler");
XPCOMUtils.defineLazyServiceGetter(this,
                                   "AddonPolicyService",
                                   "@mozilla.org/addons/policy-service;1",
                                   "nsIAddonPolicyService");
XPCOMUtils.defineLazyServiceGetter(this,
                                   "AddonPathService",
                                   "@mozilla.org/addon-path-service;1",
                                   "amIAddonPathService");

XPCOMUtils.defineLazyGetter(this, "CertUtils", function() {
  let certUtils = {};
  Components.utils.import("resource://gre/modules/CertUtils.jsm", certUtils);
  return certUtils;
});

Cu.importGlobalProperties(["URL"]);

const nsIFile = Components.Constructor("@mozilla.org/file/local;1", "nsIFile",
                                       "initWithPath");

const PREF_DB_SCHEMA                  = "extensions.databaseSchema";
const PREF_INSTALL_CACHE              = "extensions.installCache";
const PREF_XPI_STATE                  = "extensions.xpiState";
const PREF_BOOTSTRAP_ADDONS           = "extensions.bootstrappedAddons";
const PREF_PENDING_OPERATIONS         = "extensions.pendingOperations";
const PREF_EM_DSS_ENABLED             = "extensions.dss.enabled";
const PREF_DSS_SWITCHPENDING          = "extensions.dss.switchPending";
const PREF_DSS_SKIN_TO_SELECT         = "extensions.lastSelectedSkin";
const PREF_GENERAL_SKINS_SELECTEDSKIN = "general.skins.selectedSkin";
const PREF_EM_UPDATE_URL              = "extensions.update.url";
const PREF_EM_UPDATE_BACKGROUND_URL   = "extensions.update.background.url";
const PREF_EM_ENABLED_ADDONS          = "extensions.enabledAddons";
const PREF_EM_EXTENSION_FORMAT        = "extensions.";
const PREF_EM_ENABLED_SCOPES          = "extensions.enabledScopes";
const PREF_EM_SHOW_MISMATCH_UI        = "extensions.showMismatchUI";
const PREF_XPI_ENABLED                = "xpinstall.enabled";
const PREF_XPI_WHITELIST_REQUIRED     = "xpinstall.whitelist.required";
const PREF_XPI_DIRECT_WHITELISTED     = "xpinstall.whitelist.directRequest";
const PREF_XPI_FILE_WHITELISTED       = "xpinstall.whitelist.fileRequest";
// xpinstall.signatures.required only supported in dev builds
const PREF_XPI_SIGNATURES_REQUIRED    = "xpinstall.signatures.required";
const PREF_XPI_SIGNATURES_DEV_ROOT    = "xpinstall.signatures.dev-root";
const PREF_XPI_PERMISSIONS_BRANCH     = "xpinstall.";
const PREF_XPI_UNPACK                 = "extensions.alwaysUnpack";
const PREF_INSTALL_REQUIREBUILTINCERTS = "extensions.install.requireBuiltInCerts";
const PREF_INSTALL_REQUIRESECUREORIGIN = "extensions.install.requireSecureOrigin";
const PREF_INSTALL_DISTRO_ADDONS      = "extensions.installDistroAddons";
const PREF_BRANCH_INSTALLED_ADDON     = "extensions.installedDistroAddon.";
const PREF_INTERPOSITION_ENABLED      = "extensions.interposition.enabled";
const PREF_SYSTEM_ADDON_SET           = "extensions.systemAddonSet";
const PREF_SYSTEM_ADDON_UPDATE_URL    = "extensions.systemAddon.update.url";
const PREF_E10S_BLOCK_ENABLE          = "extensions.e10sBlocksEnabling";
const PREF_E10S_ADDON_BLOCKLIST       = "extensions.e10s.rollout.blocklist";
const PREF_E10S_ADDON_POLICY          = "extensions.e10s.rollout.policy";
const PREF_E10S_HAS_NONEXEMPT_ADDON   = "extensions.e10s.rollout.hasAddon";

const PREF_EM_MIN_COMPAT_APP_VERSION      = "extensions.minCompatibleAppVersion";
const PREF_EM_MIN_COMPAT_PLATFORM_VERSION = "extensions.minCompatiblePlatformVersion";

const PREF_CHECKCOMAT_THEMEOVERRIDE   = "extensions.checkCompatibility.temporaryThemeOverride_minAppVersion";

const PREF_EM_HOTFIX_ID               = "extensions.hotfix.id";
const PREF_EM_CERT_CHECKATTRIBUTES    = "extensions.hotfix.cert.checkAttributes";
const PREF_EM_HOTFIX_CERTS            = "extensions.hotfix.certs.";

const URI_EXTENSION_UPDATE_DIALOG     = "chrome://mozapps/content/extensions/update.xul";
const URI_EXTENSION_STRINGS           = "chrome://mozapps/locale/extensions/extensions.properties";

const STRING_TYPE_NAME                = "type.%ID%.name";

const DIR_EXTENSIONS                  = "extensions";
const DIR_SYSTEM_ADDONS               = "features";
const DIR_STAGE                       = "staged";
const DIR_TRASH                       = "trash";

const FILE_DATABASE                   = "extensions.json";
const FILE_OLD_CACHE                  = "extensions.cache";
const FILE_RDF_MANIFEST               = "install.rdf";
const FILE_WEB_MANIFEST               = "manifest.json";
const FILE_XPI_ADDONS_LIST            = "extensions.ini";

const KEY_PROFILEDIR                  = "ProfD";
const KEY_ADDON_APP_DIR               = "XREAddonAppDir";
const KEY_TEMPDIR                     = "TmpD";
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

const NOTIFICATION_FLUSH_PERMISSIONS  = "flush-pending-permissions";
const XPI_PERMISSION                  = "install";

const RDFURI_INSTALL_MANIFEST_ROOT    = "urn:mozilla:install-manifest";
const PREFIX_NS_EM                    = "http://www.mozilla.org/2004/em-rdf#";

const TOOLKIT_ID                      = "toolkit@mozilla.org";

const XPI_SIGNATURE_CHECK_PERIOD      = 24 * 60 * 60;

XPCOMUtils.defineConstant(this, "DB_SCHEMA", 17);

const NOTIFICATION_TOOLBOXPROCESS_LOADED      = "ToolboxProcessLoaded";

// Properties that exist in the install manifest
const PROP_METADATA      = ["id", "version", "type", "internalName", "updateURL",
                            "updateKey", "optionsURL", "optionsType", "aboutURL",
                            "iconURL", "icon64URL"];
const PROP_LOCALE_SINGLE = ["name", "description", "creator", "homepageURL"];
const PROP_LOCALE_MULTI  = ["developers", "translators", "contributors"];
const PROP_TARGETAPP     = ["id", "minVersion", "maxVersion"];

// Properties to cache and reload when an addon installation is pending
const PENDING_INSTALL_METADATA =
    ["syncGUID", "targetApplications", "userDisabled", "softDisabled",
     "existingAddonID", "sourceURI", "releaseNotesURI", "installDate",
     "updateDate", "applyBackgroundUpdates", "compatibilityOverrides"];

// Note: When adding/changing/removing items here, remember to change the
// DB schema version to ensure changes are picked up ASAP.
const STATIC_BLOCKLIST_PATTERNS = [
  { creator: "Mozilla Corp.",
    level: Blocklist.STATE_BLOCKED,
    blockID: "i162" },
  { creator: "Mozilla.org",
    level: Blocklist.STATE_BLOCKED,
    blockID: "i162" }
];


const BOOTSTRAP_REASONS = {
  APP_STARTUP     : 1,
  APP_SHUTDOWN    : 2,
  ADDON_ENABLE    : 3,
  ADDON_DISABLE   : 4,
  ADDON_INSTALL   : 5,
  ADDON_UNINSTALL : 6,
  ADDON_UPGRADE   : 7,
  ADDON_DOWNGRADE : 8
};

// Map new string type identifiers to old style nsIUpdateItem types
const TYPES = {
  extension: 2,
  theme: 4,
  locale: 8,
  multipackage: 32,
  dictionary: 64,
  experiment: 128,
};

if (!AppConstants.RELEASE_BUILD)
  TYPES.apiextension = 256;

// Some add-on types that we track internally are presented as other types
// externally
const TYPE_ALIASES = {
  "webextension": "extension",
  "apiextension": "extension",
};

const CHROME_TYPES = new Set([
  "extension",
  "locale",
  "experiment",
]);

const RESTARTLESS_TYPES = new Set([
  "webextension",
  "dictionary",
  "experiment",
  "locale",
  "apiextension",
]);

const SIGNED_TYPES = new Set([
  "webextension",
  "extension",
  "experiment",
  "apiextension",
]);

// This is a random number array that can be used as "salt" when generating
// an automatic ID based on the directory path of an add-on. It will prevent
// someone from creating an ID for a permanent add-on that could be replaced
// by a temporary add-on (because that would be confusing, I guess).
const TEMP_INSTALL_ID_GEN_SESSION =
  new Uint8Array(Float64Array.of(Math.random()).buffer);

// Whether add-on signing is required.
function mustSign(aType) {
  if (!SIGNED_TYPES.has(aType))
    return false;
  return REQUIRE_SIGNING || Preferences.get(PREF_XPI_SIGNATURES_REQUIRED, false);
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

const MSG_JAR_FLUSH = "AddonJarFlush";
const MSG_MESSAGE_MANAGER_CACHES_FLUSH = "AddonMessageManagerCachesFlush";

var gGlobalScope = this;

/**
 * Valid IDs fit this pattern.
 */
var gIDTest = /^(\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}|[a-z0-9-\._]*\@[a-z0-9-\._]+)$/i;

Cu.import("resource://gre/modules/Log.jsm");
const LOGGER_ID = "addons.xpi";

// Create a new logger for use by all objects in this Addons XPI Provider module
// (Requires AddonManager.jsm)
var logger = Log.repository.getLogger(LOGGER_ID);

const LAZY_OBJECTS = ["XPIDatabase", "XPIDatabaseReconcile"];
/*globals XPIDatabase, XPIDatabaseReconcile*/

var gLazyObjectsLoaded = false;

function loadLazyObjects() {
  let uri = "resource://gre/modules/addons/XPIProviderUtils.js";
  let scope = Cu.Sandbox(Services.scriptSecurityManager.getSystemPrincipal(), {
    sandboxName: uri,
    wantGlobalProperties: ["TextDecoder"],
  });

  let shared = {
    ADDON_SIGNING,
    SIGNED_TYPES,
    BOOTSTRAP_REASONS,
    DB_SCHEMA,
    AddonInternal,
    XPIProvider,
    XPIStates,
    syncLoadManifestFromFile,
    isUsableAddon,
    recordAddonTelemetry,
    applyBlocklistChanges,
    flushChromeCaches,
    canRunInSafeMode,
  }

  for (let key of Object.keys(shared))
    scope[key] = shared[key];

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
    get: function() {
      let objs = loadLazyObjects();
      return objs[name];
    },
    configurable: true
  });
});


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
 * Converts an iterable of addon objects into a map with the add-on's ID as key.
 */
function addonMap(addons) {
  return new Map(addons.map(a => [a.id, a]));
}

/**
 * Sets permissions on a file
 *
 * @param  aFile
 *         The file or directory to operate on.
 * @param  aPermissions
 *         The permisions to set
 */
function setFilePermissions(aFile, aPermissions) {
  try {
    aFile.permissions = aPermissions;
  }
  catch (e) {
    logger.warn("Failed to set permissions " + aPermissions.toString(8) + " on " +
         aFile.path, e);
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

  _installFile: function(aFile, aTargetDirectory, aCopy) {
    let oldFile = aCopy ? null : aFile.clone();
    let newFile = aFile.clone();
    try {
      if (aCopy) {
        newFile.copyTo(aTargetDirectory, null);
        // copyTo does not update the nsIFile with the new.
        newFile = aTargetDirectory.clone();
        newFile.append(aFile.leafName);
        // Windows roaming profiles won't properly sync directories if a new file
        // has an older lastModifiedTime than a previous file, so update.
        newFile.lastModifiedTime = Date.now();
      }
      else {
        newFile.moveTo(aTargetDirectory, null);
      }
    }
    catch (e) {
      logger.error("Failed to " + (aCopy ? "copy" : "move") + " file " + aFile.path +
            " to " + aTargetDirectory.path, e);
      throw e;
    }
    this._installedFiles.push({ oldFile: oldFile, newFile: newFile });
  },

  _installDirectory: function(aDirectory, aTargetDirectory, aCopy) {
    if (aDirectory.contains(aTargetDirectory)) {
      let err = new Error(`Not installing ${aDirectory} into its own descendent ${aTargetDirectory}`);
      logger.error(err);
      throw err;
    }

    let newDir = aTargetDirectory.clone();
    newDir.append(aDirectory.leafName);
    try {
      newDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    }
    catch (e) {
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
      }
      catch (e) {
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
    }
    catch (e) {
      logger.error("Failed to remove directory " + aDirectory.path, e);
      throw e;
    }

    // Note we put the directory move in after all the file moves so the
    // directory is recreated before all the files are moved back
    this._installedFiles.push({ oldFile: aDirectory, newFile: newDir });
  },

  _installDirEntry: function(aDirEntry, aTargetDirectory, aCopy) {
    let isDir = null;

    try {
      isDir = aDirEntry.isDirectory() && !aDirEntry.isSymlink();
    }
    catch (e) {
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
    }
    catch (e) {
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
  moveUnder: function(aFile, aTargetDirectory) {
    try {
      this._installDirEntry(aFile, aTargetDirectory, false);
    }
    catch (e) {
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
  moveTo: function(aOldLocation, aNewLocation) {
    try {
      let oldFile = aOldLocation.clone(), newFile = aNewLocation.clone();
      oldFile.moveTo(newFile.parent, newFile.leafName);
      this._installedFiles.push({ oldFile: oldFile, newFile: newFile, isMoveTo: true});
    }
    catch (e) {
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
  copy: function(aFile, aTargetDirectory) {
    try {
      this._installDirEntry(aFile, aTargetDirectory, true);
    }
    catch (e) {
      this.rollback();
      throw e;
    }
  },

  /**
   * Rolls back all the moves that this operation performed. If an exception
   * occurs here then both old and new directories are left in an indeterminate
   * state
   */
  rollback: function() {
    while (this._installedFiles.length > 0) {
      let move = this._installedFiles.pop();
      if (move.isMoveTo) {
        move.newFile.moveTo(move.oldDir.parent, move.oldDir.leafName);
      }
      else if (move.newFile.isDirectory() && !move.newFile.isSymlink()) {
        let oldDir = move.oldFile.parent.clone();
        oldDir.append(move.oldFile.leafName);
        oldDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
      }
      else if (!move.oldFile) {
        // No old file means this was a copied file
        move.newFile.remove(true);
      }
      else {
        move.newFile.moveTo(move.oldFile.parent, null);
      }
    }

    while (this._createdDirs.length > 0)
      recursiveRemove(this._createdDirs.pop());
  }
};

/**
 * Sets the userDisabled and softDisabled properties of an add-on based on what
 * values those properties had for a previous instance of the add-on. The
 * previous instance may be a previous install or in the case of an application
 * version change the same add-on.
 *
 * NOTE: this may modify aNewAddon in place; callers should save the database if
 * necessary
 *
 * @param  aOldAddon
 *         The previous instance of the add-on
 * @param  aNewAddon
 *         The new instance of the add-on
 * @param  aAppVersion
 *         The optional application version to use when checking the blocklist
 *         or undefined to use the current application
 * @param  aPlatformVersion
 *         The optional platform version to use when checking the blocklist or
 *         undefined to use the current platform
 */
function applyBlocklistChanges(aOldAddon, aNewAddon, aOldAppVersion,
                               aOldPlatformVersion) {
  // Copy the properties by default
  aNewAddon.userDisabled = aOldAddon.userDisabled;
  aNewAddon.softDisabled = aOldAddon.softDisabled;

  let oldBlocklistState = Blocklist.getAddonBlocklistState(aOldAddon.wrapper,
                                                           aOldAppVersion,
                                                           aOldPlatformVersion);
  let newBlocklistState = Blocklist.getAddonBlocklistState(aNewAddon.wrapper);

  // If the blocklist state hasn't changed then the properties don't need to
  // change
  if (newBlocklistState == oldBlocklistState)
    return;

  if (newBlocklistState == Blocklist.STATE_SOFTBLOCKED) {
    if (aNewAddon.type != "theme") {
      // The add-on has become softblocked, set softDisabled if it isn't already
      // userDisabled
      aNewAddon.softDisabled = !aNewAddon.userDisabled;
    }
    else {
      // Themes just get userDisabled to switch back to the default theme
      aNewAddon.userDisabled = true;
    }
  }
  else {
    // If the new add-on is not softblocked then it cannot be softDisabled
    aNewAddon.softDisabled = false;
  }
}

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
  if (aAddon._installLocation.name == KEY_APP_TEMPORARY)
    return true;

  return aAddon._installLocation.name == KEY_APP_SYSTEM_DEFAULTS ||
         aAddon._installLocation.name == KEY_APP_SYSTEM_ADDONS;
}

/**
 * Calculates whether an add-on should be appDisabled or not.
 *
 * @param  aAddon
 *         The add-on to check
 * @return true if the add-on should not be appDisabled
 */
function isUsableAddon(aAddon) {
  // Hack to ensure the default theme is always usable
  if (aAddon.type == "theme" && aAddon.internalName == XPIProvider.defaultSkin)
    return true;

  if (aAddon._installLocation.name == KEY_APP_SYSTEM_ADDONS &&
      aAddon.signedState != AddonManager.SIGNEDSTATE_SYSTEM) {
    logger.warn(`System add-on update ${aAddon.id} not signed with the system key.`);
    return false;
  }
  // Temporary and built-in system add-ons do not require signing.
  // On UNIX platforms except OSX, an additional location for system add-ons
  // exists in /usr/{lib,share}/mozilla/extensions. Add-ons installed there
  // do not require signing either.
  if (((aAddon._installLocation.scope != AddonManager.SCOPE_SYSTEM ||
        Services.appinfo.OS == "Darwin") &&
       aAddon._installLocation.name != KEY_APP_SYSTEM_DEFAULTS &&
       aAddon._installLocation.name != KEY_APP_TEMPORARY) &&
       mustSign(aAddon.type)) {
    if (aAddon.signedState <= AddonManager.SIGNEDSTATE_MISSING) {
      logger.warn(`Add-on ${aAddon.id} not signed.`);
      return false;
    }
  }

  if (aAddon.blocklistState == Blocklist.STATE_BLOCKED) {
    logger.warn(`Add-on ${aAddon.id} is blocklisted.`);
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

  if (AddonManager.checkCompatibility) {
    if (!aAddon.isCompatible) {
      logger.warn(`Add-on ${aAddon.id} is not compatible with application version.`);
      return false;
    }
  }
  else {
    let app = aAddon.matchingTargetApplication;
    if (!app) {
      logger.warn(`Add-on ${aAddon.id} is not compatible with target application.`);
      return false;
    }

    // XXX Temporary solution to let applications opt-in to make themes safer
    //     following significant UI changes even if checkCompatibility=false has
    //     been set, until we get bug 962001.
    if (aAddon.type == "theme" && app.id == Services.appinfo.ID) {
      try {
        let minCompatVersion = Services.prefs.getCharPref(PREF_CHECKCOMAT_THEMEOVERRIDE);
        if (minCompatVersion &&
            Services.vc.compare(minCompatVersion, app.maxVersion) > 0) {
          logger.warn(`Theme ${aAddon.id} is not compatible with application version.`);
          return false;
        }
      } catch (e) {}
    }
  }

  return true;
}

XPCOMUtils.defineLazyServiceGetter(this, "gRDF", "@mozilla.org/rdf/rdf-service;1",
                                   Ci.nsIRDFService);

function EM_R(aProperty) {
  return gRDF.GetResource(PREFIX_NS_EM + aProperty);
}

function createAddonDetails(id, aAddon) {
  return {
    id: id || aAddon.id,
    type: aAddon.type,
    version: aAddon.version,
    multiprocessCompatible: aAddon.multiprocessCompatible,
    runInSafeMode: aAddon.runInSafeMode,
    dependencies: aAddon.dependencies,
  };
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
  let file = aDir.clone();
  file.append(FILE_RDF_MANIFEST);
  if (file.exists() && file.isFile())
    return file;
  file.leafName = FILE_WEB_MANIFEST;
  if (file.exists() && file.isFile())
    return file;
  return null;
}

function getManifestEntryForZipReader(aZipReader) {
  if (aZipReader.hasEntry(FILE_RDF_MANIFEST))
    return FILE_RDF_MANIFEST;
  if (aZipReader.hasEntry(FILE_WEB_MANIFEST))
    return FILE_WEB_MANIFEST;
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
 * Converts an RDF literal, resource or integer into a string.
 *
 * @param  aLiteral
 *         The RDF object to convert
 * @return a string if the object could be converted or null
 */
function getRDFValue(aLiteral) {
  if (aLiteral instanceof Ci.nsIRDFLiteral)
    return aLiteral.Value;
  if (aLiteral instanceof Ci.nsIRDFResource)
    return aLiteral.Value;
  if (aLiteral instanceof Ci.nsIRDFInt)
    return aLiteral.Value;
  return null;
}

/**
 * Gets an RDF property as a string
 *
 * @param  aDs
 *         The RDF datasource to read the property from
 * @param  aResource
 *         The RDF resource to read the property from
 * @param  aProperty
 *         The property to read
 * @return a string if the property existed or null
 */
function getRDFProperty(aDs, aResource, aProperty) {
  return getRDFValue(aDs.GetTarget(aResource, EM_R(aProperty), true));
}

/**
 * Reads an AddonInternal object from a manifest stream.
 *
 * @param  aUri
 *         A |file:| or |jar:| URL for the manifest
 * @return an AddonInternal object
 * @throws if the install manifest in the stream is corrupt or could not
 *         be read
 */
var loadManifestFromWebManifest = Task.async(function*(aUri) {
  // We're passed the URI for the manifest file. Get the URI for its
  // parent directory.
  let uri = NetUtil.newURI("./", null, aUri);

  let extension = new ExtensionData(uri);

  let manifest = yield extension.readManifest();

  // Read the list of available locales, and pre-load messages for
  // all locales.
  let locales = yield extension.initAllLocales();

  // If there were any errors loading the extension, bail out now.
  if (extension.errors.length)
    throw new Error("Extension is invalid");

  let bss = (manifest.browser_specific_settings && manifest.browser_specific_settings.gecko)
      || (manifest.applications && manifest.applications.gecko) || {};
  if (manifest.browser_specific_settings && manifest.applications) {
    logger.warn("Ignoring applications property in manifest");
  }

  let addon = new AddonInternal();
  addon.id = bss.id;
  addon.version = manifest.version;
  addon.type = "webextension";
  addon.unpack = false;
  addon.strictCompatibility = true;
  addon.bootstrap = true;
  addon.hasBinaryComponents = false;
  addon.multiprocessCompatible = true;
  addon.internalName = null;
  addon.updateURL = bss.update_url;
  addon.updateKey = null;
  addon.optionsURL = null;
  addon.optionsType = null;
  addon.aboutURL = null;
  addon.dependencies = Object.freeze(Array.from(extension.dependencies));

  if (manifest.options_ui) {
    // Store just the relative path here, the AddonWrapper getURL
    // wrapper maps this to a full URL.
    addon.optionsURL = manifest.options_ui.page;
    if (manifest.options_ui.open_in_tab)
      addon.optionsType = AddonManager.OPTIONS_TYPE_TAB;
    else
      addon.optionsType = AddonManager.OPTIONS_TYPE_INLINE_BROWSER;
  }

  // WebExtensions don't use iconURLs
  addon.iconURL = null;
  addon.icon64URL = null;
  addon.icons = manifest.icons || {};

  addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DEFAULT;

  function getLocale(aLocale) {
    // Use the raw manifest, here, since we need values with their
    // localization placeholders still in place.
    let rawManifest = extension.rawManifest;

    let result = {
      name: extension.localize(rawManifest.name, aLocale),
      description: extension.localize(rawManifest.description, aLocale),
      creator: extension.localize(rawManifest.creator, aLocale),
      homepageURL: extension.localize(rawManifest.homepage_url, aLocale),

      developers: null,
      translators: null,
      contributors: null,
      locales: [aLocale],
    };
    return result;
  }

  addon.defaultLocale = getLocale(extension.defaultLocale);
  addon.locales = Array.from(locales.keys(), getLocale);

  delete addon.defaultLocale.locales;

  addon.targetApplications = [{
    id: TOOLKIT_ID,
    minVersion: (bss.strict_min_version ||
                 AddonManagerPrivate.webExtensionsMinPlatformVersion),
    maxVersion: bss.strict_max_version || "*",
  }];

  addon.targetPlatforms = [];
  addon.userDisabled = false;
  addon.softDisabled = addon.blocklistState == Blocklist.STATE_SOFTBLOCKED;

  return addon;
});

/**
 * Reads an AddonInternal object from an RDF stream.
 *
 * @param  aUri
 *         The URI that the manifest is being read from
 * @param  aStream
 *         An open stream to read the RDF from
 * @return an AddonInternal object
 * @throws if the install manifest in the RDF stream is corrupt or could not
 *         be read
 */
function loadManifestFromRDF(aUri, aStream) {
  function getPropertyArray(aDs, aSource, aProperty) {
    let values = [];
    let targets = aDs.GetTargets(aSource, EM_R(aProperty), true);
    while (targets.hasMoreElements())
      values.push(getRDFValue(targets.getNext()));

    return values;
  }

  /**
   * Reads locale properties from either the main install manifest root or
   * an em:localized section in the install manifest.
   *
   * @param  aDs
   *         The nsIRDFDatasource to read from
   * @param  aSource
   *         The nsIRDFResource to read the properties from
   * @param  isDefault
   *         True if the locale is to be read from the main install manifest
   *         root
   * @param  aSeenLocales
   *         An array of locale names already seen for this install manifest.
   *         Any locale names seen as a part of this function will be added to
   *         this array
   * @return an object containing the locale properties
   */
  function readLocale(aDs, aSource, isDefault, aSeenLocales) {
    let locale = { };
    if (!isDefault) {
      locale.locales = [];
      let targets = ds.GetTargets(aSource, EM_R("locale"), true);
      while (targets.hasMoreElements()) {
        let localeName = getRDFValue(targets.getNext());
        if (!localeName) {
          logger.warn("Ignoring empty locale in localized properties");
          continue;
        }
        if (aSeenLocales.indexOf(localeName) != -1) {
          logger.warn("Ignoring duplicate locale in localized properties");
          continue;
        }
        aSeenLocales.push(localeName);
        locale.locales.push(localeName);
      }

      if (locale.locales.length == 0) {
        logger.warn("Ignoring localized properties with no listed locales");
        return null;
      }
    }

    for (let prop of PROP_LOCALE_SINGLE) {
      locale[prop] = getRDFProperty(aDs, aSource, prop);
    }

    for (let prop of PROP_LOCALE_MULTI) {
      // Don't store empty arrays
      let props = getPropertyArray(aDs, aSource,
                                   prop.substring(0, prop.length - 1));
      if (props.length > 0)
        locale[prop] = props;
    }

    return locale;
  }

  let rdfParser = Cc["@mozilla.org/rdf/xml-parser;1"].
                  createInstance(Ci.nsIRDFXMLParser)
  let ds = Cc["@mozilla.org/rdf/datasource;1?name=in-memory-datasource"].
           createInstance(Ci.nsIRDFDataSource);
  let listener = rdfParser.parseAsync(ds, aUri);
  let channel = Cc["@mozilla.org/network/input-stream-channel;1"].
                createInstance(Ci.nsIInputStreamChannel);
  channel.setURI(aUri);
  channel.contentStream = aStream;
  channel.QueryInterface(Ci.nsIChannel);
  channel.contentType = "text/xml";

  listener.onStartRequest(channel, null);

  try {
    let pos = 0;
    let count = aStream.available();
    while (count > 0) {
      listener.onDataAvailable(channel, null, aStream, pos, count);
      pos += count;
      count = aStream.available();
    }
    listener.onStopRequest(channel, null, Components.results.NS_OK);
  }
  catch (e) {
    listener.onStopRequest(channel, null, e.result);
    throw e;
  }

  let root = gRDF.GetResource(RDFURI_INSTALL_MANIFEST_ROOT);
  let addon = new AddonInternal();
  for (let prop of PROP_METADATA) {
    addon[prop] = getRDFProperty(ds, root, prop);
  }
  addon.unpack = getRDFProperty(ds, root, "unpack") == "true";

  if (!addon.type) {
    addon.type = addon.internalName ? "theme" : "extension";
  }
  else {
    let type = addon.type;
    addon.type = null;
    for (let name in TYPES) {
      if (TYPES[name] == type) {
        addon.type = name;
        break;
      }
    }
  }

  if (!(addon.type in TYPES))
    throw new Error("Install manifest specifies unknown type: " + addon.type);

  if (addon.type != "multipackage") {
    if (!addon.id)
      throw new Error("No ID in install manifest");
    if (!gIDTest.test(addon.id))
      throw new Error("Illegal add-on ID " + addon.id);
    if (!addon.version)
      throw new Error("No version in install manifest");
  }

  addon.strictCompatibility = !(addon.type in COMPATIBLE_BY_DEFAULT_TYPES) ||
                              getRDFProperty(ds, root, "strictCompatibility") == "true";

  // Only read these properties for extensions.
  if (addon.type == "extension") {
    addon.bootstrap = getRDFProperty(ds, root, "bootstrap") == "true";
    addon.multiprocessCompatible = getRDFProperty(ds, root, "multiprocessCompatible") == "true";
    if (addon.optionsType &&
        addon.optionsType != AddonManager.OPTIONS_TYPE_DIALOG &&
        addon.optionsType != AddonManager.OPTIONS_TYPE_INLINE &&
        addon.optionsType != AddonManager.OPTIONS_TYPE_TAB &&
        addon.optionsType != AddonManager.OPTIONS_TYPE_INLINE_INFO) {
      throw new Error("Install manifest specifies unknown type: " + addon.optionsType);
    }
  }
  else {
    // Some add-on types are always restartless.
    if (RESTARTLESS_TYPES.has(addon.type)) {
      addon.bootstrap = true;
    }

    // Only extensions are allowed to provide an optionsURL, optionsType or aboutURL. For
    // all other types they are silently ignored
    addon.optionsURL = null;
    addon.optionsType = null;
    addon.aboutURL = null;

    if (addon.type == "theme") {
      if (!addon.internalName)
        throw new Error("Themes must include an internalName property");
      addon.skinnable = getRDFProperty(ds, root, "skinnable") == "true";
    }
  }

  addon.defaultLocale = readLocale(ds, root, true);

  let seenLocales = [];
  addon.locales = [];
  let targets = ds.GetTargets(root, EM_R("localized"), true);
  while (targets.hasMoreElements()) {
    let target = targets.getNext().QueryInterface(Ci.nsIRDFResource);
    let locale = readLocale(ds, target, false, seenLocales);
    if (locale)
      addon.locales.push(locale);
  }

  let dependencies = new Set();
  targets = ds.GetTargets(root, EM_R("dependency"), true);
  while (targets.hasMoreElements()) {
    let target = targets.getNext().QueryInterface(Ci.nsIRDFResource);
    let id = getRDFProperty(ds, target, "id");
    dependencies.add(id);
  }
  addon.dependencies = Object.freeze(Array.from(dependencies));

  let seenApplications = [];
  addon.targetApplications = [];
  targets = ds.GetTargets(root, EM_R("targetApplication"), true);
  while (targets.hasMoreElements()) {
    let target = targets.getNext().QueryInterface(Ci.nsIRDFResource);
    let targetAppInfo = {};
    for (let prop of PROP_TARGETAPP) {
      targetAppInfo[prop] = getRDFProperty(ds, target, prop);
    }
    if (!targetAppInfo.id || !targetAppInfo.minVersion ||
        !targetAppInfo.maxVersion) {
      logger.warn("Ignoring invalid targetApplication entry in install manifest");
      continue;
    }
    if (seenApplications.indexOf(targetAppInfo.id) != -1) {
      logger.warn("Ignoring duplicate targetApplication entry for " + targetAppInfo.id +
           " in install manifest");
      continue;
    }
    seenApplications.push(targetAppInfo.id);
    addon.targetApplications.push(targetAppInfo);
  }

  // Note that we don't need to check for duplicate targetPlatform entries since
  // the RDF service coalesces them for us.
  let targetPlatforms = getPropertyArray(ds, root, "targetPlatform");
  addon.targetPlatforms = [];
  for (let targetPlatform of targetPlatforms) {
    let platform = {
      os: null,
      abi: null
    };

    let pos = targetPlatform.indexOf("_");
    if (pos != -1) {
      platform.os = targetPlatform.substring(0, pos);
      platform.abi = targetPlatform.substring(pos + 1);
    }
    else {
      platform.os = targetPlatform;
    }

    addon.targetPlatforms.push(platform);
  }

  // A theme's userDisabled value is true if the theme is not the selected skin
  // or if there is an active lightweight theme. We ignore whether softblocking
  // is in effect since it would change the active theme.
  if (addon.type == "theme") {
    addon.userDisabled = !!LightweightThemeManager.currentTheme ||
                         addon.internalName != XPIProvider.selectedSkin;
  }
  else if (addon.type == "experiment") {
    // Experiments are disabled by default. It is up to the Experiments Manager
    // to enable them (it drives installation).
    addon.userDisabled = true;
  }
  else {
    addon.userDisabled = false;
  }

  addon.softDisabled = addon.blocklistState == Blocklist.STATE_SOFTBLOCKED;
  addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DEFAULT;

  // Experiments are managed and updated through an external "experiments
  // manager." So disable some built-in mechanisms.
  if (addon.type == "experiment") {
    addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DISABLE;
    addon.updateURL = null;
    addon.updateKey = null;
  }

  // icons will be filled by the calling function
  addon.icons = {};

  return addon;
}

function defineSyncGUID(aAddon) {
  // Define .syncGUID as a lazy property which is also settable
  Object.defineProperty(aAddon, "syncGUID", {
    get: () => {
      // Generate random GUID used for Sync.
      let guid = Cc["@mozilla.org/uuid-generator;1"]
          .getService(Ci.nsIUUIDGenerator)
          .generateUUID().toString();

      delete aAddon.syncGUID;
      aAddon.syncGUID = guid;
      return guid;
    },
    set: (val) => {
      delete aAddon.syncGUID;
      aAddon.syncGUID = val;
    },
    configurable: true,
    enumerable: true,
  });
}

/**
 * Loads an AddonInternal object from an add-on extracted in a directory.
 *
 * @param  aDir
 *         The nsIFile directory holding the add-on
 * @return an AddonInternal object
 * @throws if the directory does not contain a valid install manifest
 */
var loadManifestFromDir = Task.async(function*(aDir, aInstallLocation) {
  function getFileSize(aFile) {
    if (aFile.isSymlink())
      return 0;

    if (!aFile.isDirectory())
      return aFile.fileSize;

    let size = 0;
    let entries = aFile.directoryEntries.QueryInterface(Ci.nsIDirectoryEnumerator);
    let entry;
    while ((entry = entries.nextFile))
      size += getFileSize(entry);
    entries.close();
    return size;
  }

  function loadFromRDF(aUri) {
    let fis = Cc["@mozilla.org/network/file-input-stream;1"].
              createInstance(Ci.nsIFileInputStream);
    fis.init(aUri.file, -1, -1, false);
    let bis = Cc["@mozilla.org/network/buffered-input-stream;1"].
              createInstance(Ci.nsIBufferedInputStream);
    bis.init(fis, 4096);
    try {
      var addon = loadManifestFromRDF(aUri, bis);
    } finally {
      bis.close();
      fis.close();
    }

    let iconFile = aDir.clone();
    iconFile.append("icon.png");

    if (iconFile.exists()) {
      addon.icons[32] = "icon.png";
      addon.icons[48] = "icon.png";
    }

    let icon64File = aDir.clone();
    icon64File.append("icon64.png");

    if (icon64File.exists()) {
      addon.icons[64] = "icon64.png";
    }

    let file = aDir.clone();
    file.append("chrome.manifest");
    let chromeManifest = ChromeManifestParser.parseSync(Services.io.newFileURI(file));
    addon.hasBinaryComponents = ChromeManifestParser.hasType(chromeManifest,
                                                             "binary-component");
    return addon;
  }

  let file = getManifestFileForDir(aDir);
  if (!file) {
    throw new Error("Directory " + aDir.path + " does not contain a valid " +
                    "install manifest");
  }

  let uri = Services.io.newFileURI(file).QueryInterface(Ci.nsIFileURL);

  let addon;
  if (file.leafName == FILE_WEB_MANIFEST) {
    addon = yield loadManifestFromWebManifest(uri);
    if (!addon.id) {
      if (aInstallLocation == TemporaryInstallLocation) {
        // Generate a unique ID based on the directory path of
        // this temporary add-on location.
        const hasher = Cc["@mozilla.org/security/hash;1"]
          .createInstance(Ci.nsICryptoHash);
        hasher.init(hasher.SHA1);
        const data = new TextEncoder().encode(aDir.path);
        // Make it so this ID cannot be guessed.
        const sess = TEMP_INSTALL_ID_GEN_SESSION;
        hasher.update(sess, sess.length);
        hasher.update(data, data.length);
        addon.id = `${getHashStringForCrypto(hasher)}@temporary-addon`;
        logger.info(
          `Generated temp id ${addon.id} (${sess.join("")}) for ${aDir.path}`);
      } else {
        addon.id = aDir.leafName;
      }
    }
  } else {
    addon = loadFromRDF(uri);
  }

  addon._sourceBundle = aDir.clone();
  addon._installLocation = aInstallLocation;
  addon.size = getFileSize(aDir);
  addon.signedState = yield verifyDirSignedState(aDir, addon)
    .then(({signedState}) => signedState);
  addon.appDisabled = !isUsableAddon(addon);

  defineSyncGUID(addon);

  return addon;
});

/**
 * Loads an AddonInternal object from an nsIZipReader for an add-on.
 *
 * @param  aZipReader
 *         An open nsIZipReader for the add-on's files
 * @return an AddonInternal object
 * @throws if the XPI file does not contain a valid install manifest
 */
var loadManifestFromZipReader = Task.async(function*(aZipReader, aInstallLocation) {
  function loadFromRDF(aUri) {
    let zis = aZipReader.getInputStream(entry);
    let bis = Cc["@mozilla.org/network/buffered-input-stream;1"].
              createInstance(Ci.nsIBufferedInputStream);
    bis.init(zis, 4096);
    try {
      var addon = loadManifestFromRDF(aUri, bis);
    } finally {
      bis.close();
      zis.close();
    }

    if (aZipReader.hasEntry("icon.png")) {
      addon.icons[32] = "icon.png";
      addon.icons[48] = "icon.png";
    }

    if (aZipReader.hasEntry("icon64.png")) {
      addon.icons[64] = "icon64.png";
    }

    // Binary components can only be loaded from unpacked addons.
    if (addon.unpack) {
      let uri = buildJarURI(aZipReader.file, "chrome.manifest");
      let chromeManifest = ChromeManifestParser.parseSync(uri);
      addon.hasBinaryComponents = ChromeManifestParser.hasType(chromeManifest,
                                                               "binary-component");
    } else {
      addon.hasBinaryComponents = false;
    }

    return addon;
  }

  let entry = getManifestEntryForZipReader(aZipReader);
  if (!entry) {
    throw new Error("File " + aZipReader.file.path + " does not contain a valid " +
                    "install manifest");
  }

  let uri = buildJarURI(aZipReader.file, entry);

  let isWebExtension = (entry == FILE_WEB_MANIFEST);

  let addon = isWebExtension ?
              yield loadManifestFromWebManifest(uri) :
              loadFromRDF(uri);

  addon._sourceBundle = aZipReader.file;
  addon._installLocation = aInstallLocation;

  addon.size = 0;
  let entries = aZipReader.findEntries(null);
  while (entries.hasMore())
    addon.size += aZipReader.getEntry(entries.getNext()).realSize;

  let {signedState, cert} = yield verifyZipSignedState(aZipReader.file, addon);
  addon.signedState = signedState;
  if (isWebExtension && !addon.id && cert) {
    addon.id = cert.commonName;
    if (!gIDTest.test(addon.id)) {
      throw new Error(`Webextension is signed with an invalid id (${addon.id})`);
    }
  }
  addon.appDisabled = !isUsableAddon(addon);

  defineSyncGUID(addon);

  return addon;
});

/**
 * Loads an AddonInternal object from an add-on in an XPI file.
 *
 * @param  aXPIFile
 *         An nsIFile pointing to the add-on's XPI file
 * @return an AddonInternal object
 * @throws if the XPI file does not contain a valid install manifest
 */
var loadManifestFromZipFile = Task.async(function*(aXPIFile, aInstallLocation) {
  let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                  createInstance(Ci.nsIZipReader);
  try {
    zipReader.open(aXPIFile);

    // Can't return this promise because that will make us close the zip reader
    // before it has finished loading the manifest. Wait for the result and then
    // return.
    let manifest = yield loadManifestFromZipReader(zipReader, aInstallLocation);
    return manifest;
  }
  finally {
    zipReader.close();
  }
});

function loadManifestFromFile(aFile, aInstallLocation) {
  if (aFile.isFile())
    return loadManifestFromZipFile(aFile, aInstallLocation);
  return loadManifestFromDir(aFile, aInstallLocation);
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
    result = val
  });

  let thread = Services.tm.currentThread;

  while (success === undefined)
    thread.processNextEvent(true);

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
  if (aFile.isDirectory()) {
    let resource = aFile.clone();
    if (aPath)
      aPath.split("/").forEach(part => resource.append(part));

    return NetUtil.newURI(resource);
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
  return NetUtil.newURI(uri);
}

/**
 * Sends local and remote notifications to flush a JAR file cache entry
 *
 * @param aJarFile
 *        The ZIP/XPI/JAR file as a nsIFile
 */
function flushJarCache(aJarFile) {
  Services.obs.notifyObservers(aJarFile, "flush-cache-entry", null);
  Services.mm.broadcastAsyncMessage(MSG_JAR_FLUSH, aJarFile.path);
}

function flushChromeCaches() {
  // Init this, so it will get the notification.
  Services.obs.notifyObservers(null, "startupcache-invalidate", null);
  // Flush message manager cached scripts
  Services.obs.notifyObservers(null, "message-manager-flush-caches", null);
  // Also dispatch this event to child processes
  Services.mm.broadcastAsyncMessage(MSG_MESSAGE_MANAGER_CACHES_FLUSH, null);
}

/**
 * Creates and returns a new unique temporary file. The caller should delete
 * the file when it is no longer needed.
 *
 * @return an nsIFile that points to a randomly named, initially empty file in
 *         the OS temporary files directory
 */
function getTemporaryFile() {
  let file = FileUtils.getDir(KEY_TEMPDIR, []);
  let random = Math.random().toString(36).replace(/0./, '').substr(-3);
  file.append("tmp-" + random + ".xpi");
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

  return file;
}

/**
 * Verifies that a zip file's contents are all signed by the same principal.
 * Directory entries and anything in the META-INF directory are not checked.
 *
 * @param  aZip
 *         A nsIZipReader to check
 * @param  aCertificate
 *         The nsIX509Cert to compare against
 * @return true if all the contents that should be signed were signed by the
 *         principal
 */
function verifyZipSigning(aZip, aCertificate) {
  var count = 0;
  var entries = aZip.findEntries(null);
  while (entries.hasMore()) {
    var entry = entries.getNext();
    // Nothing in META-INF is in the manifest.
    if (entry.substr(0, 9) == "META-INF/")
      continue;
    // Directory entries aren't in the manifest.
    if (entry.substr(-1) == "/")
      continue;
    count++;
    var entryCertificate = aZip.getSigningCert(entry);
    if (!entryCertificate || !aCertificate.equals(entryCertificate)) {
      return false;
    }
  }
  return aZip.manifestEntriesCount == count;
}

/**
 * Returns the signedState for a given return code and certificate by verifying
 * it against the expected ID.
 */
function getSignedStatus(aRv, aCert, aAddonID) {
  let expectedCommonName = aAddonID;
  if (aAddonID && aAddonID.length > 64) {
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                    createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    let data = converter.convertToByteArray(aAddonID, {});

    let crypto = Cc["@mozilla.org/security/hash;1"].
                 createInstance(Ci.nsICryptoHash);
    crypto.init(Ci.nsICryptoHash.SHA256);
    crypto.update(data, data.length);
    expectedCommonName = getHashStringForCrypto(crypto);
  }

  switch (aRv) {
    case Cr.NS_OK:
      if (expectedCommonName && expectedCommonName != aCert.commonName)
        return AddonManager.SIGNEDSTATE_BROKEN;

      let hotfixID = Preferences.get(PREF_EM_HOTFIX_ID, undefined);
      if (hotfixID && hotfixID == aAddonID && Preferences.get(PREF_EM_CERT_CHECKATTRIBUTES, false)) {
        // The hotfix add-on has some more rigorous certificate checks
        try {
          CertUtils.validateCert(aCert,
                                 CertUtils.readCertPrefs(PREF_EM_HOTFIX_CERTS));
        }
        catch (e) {
          logger.warn("The hotfix add-on was not signed by the expected " +
                      "certificate and so will not be installed.", e);
          return AddonManager.SIGNEDSTATE_BROKEN;
        }
      }

      if (aCert.organizationalUnit == "Mozilla Components")
        return AddonManager.SIGNEDSTATE_SYSTEM;

      return /preliminary/i.test(aCert.organizationalUnit)
               ? AddonManager.SIGNEDSTATE_PRELIMINARY
               : AddonManager.SIGNEDSTATE_SIGNED;
      break;
    case Cr.NS_ERROR_SIGNED_JAR_NOT_SIGNED:
      return AddonManager.SIGNEDSTATE_MISSING;
      break;
    case Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID:
    case Cr.NS_ERROR_SIGNED_JAR_ENTRY_INVALID:
    case Cr.NS_ERROR_SIGNED_JAR_ENTRY_MISSING:
    case Cr.NS_ERROR_SIGNED_JAR_ENTRY_TOO_LARGE:
    case Cr.NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY:
    case Cr.NS_ERROR_SIGNED_JAR_MODIFIED_ENTRY:
      return AddonManager.SIGNEDSTATE_BROKEN;
      break;
    default:
      // Any other error indicates that either the add-on isn't signed or it
      // is signed by a signature that doesn't chain to the trusted root.
      return AddonManager.SIGNEDSTATE_UNKNOWN;
      break;
  }
}

function shouldVerifySignedState(aAddon) {
  // Updated system add-ons should always have their signature checked
  if (aAddon._installLocation.name == KEY_APP_SYSTEM_ADDONS)
    return true;

  // We don't care about signatures for default system add-ons
  if (aAddon._installLocation.name == KEY_APP_SYSTEM_DEFAULTS)
    return false;

  // Hotfixes should always have their signature checked
  let hotfixID = Preferences.get(PREF_EM_HOTFIX_ID, undefined);
  if (hotfixID && aAddon.id == hotfixID)
    return true;

  // Otherwise only check signatures if signing is enabled and the add-on is one
  // of the signed types.
  return ADDON_SIGNING && SIGNED_TYPES.has(aAddon.type);
}

let gCertDB = Cc["@mozilla.org/security/x509certdb;1"]
              .getService(Ci.nsIX509CertDB);

/**
 * Verifies that a zip file's contents are all correctly signed by an
 * AMO-issued certificate
 *
 * @param  aFile
 *         the xpi file to check
 * @param  aAddon
 *         the add-on object to verify
 * @return a Promise that resolves to an object with properties:
 *         signedState: an AddonManager.SIGNEDSTATE_* constant
 *         cert: an nsIX509Cert
 */
function verifyZipSignedState(aFile, aAddon) {
  if (!shouldVerifySignedState(aAddon))
    return Promise.resolve({
      signedState: AddonManager.SIGNEDSTATE_NOT_REQUIRED,
      cert: null
    });

  let root = Ci.nsIX509CertDB.AddonsPublicRoot;
  if (!REQUIRE_SIGNING && Preferences.get(PREF_XPI_SIGNATURES_DEV_ROOT, false))
    root = Ci.nsIX509CertDB.AddonsStageRoot;

  return new Promise(resolve => {
    let callback = {
      openSignedAppFileFinished: function(aRv, aZipReader, aCert) {
        if (aZipReader)
          aZipReader.close();
        resolve({
          signedState: getSignedStatus(aRv, aCert, aAddon.id),
          cert: aCert
        });
      }
    };
    // This allows the certificate DB to get the raw JS callback object so the
    // test code can pass through objects that XPConnect would reject.
    callback.wrappedJSObject = callback;

    gCertDB.openSignedAppFileAsync(root, aFile, callback);
  });
}

/**
 * Verifies that a directory's contents are all correctly signed by an
 * AMO-issued certificate
 *
 * @param  aDir
 *         the directory to check
 * @param  aAddon
 *         the add-on object to verify
 * @return a Promise that resolves to an object with properties:
 *         signedState: an AddonManager.SIGNEDSTATE_* constant
 *         cert: an nsIX509Cert
 */
function verifyDirSignedState(aDir, aAddon) {
  if (!shouldVerifySignedState(aAddon))
    return Promise.resolve({
      signedState: AddonManager.SIGNEDSTATE_NOT_REQUIRED,
      cert: null,
    });

  let root = Ci.nsIX509CertDB.AddonsPublicRoot;
  if (!REQUIRE_SIGNING && Preferences.get(PREF_XPI_SIGNATURES_DEV_ROOT, false))
    root = Ci.nsIX509CertDB.AddonsStageRoot;

  return new Promise(resolve => {
    let callback = {
      verifySignedDirectoryFinished: function(aRv, aCert) {
        resolve({
          signedState: getSignedStatus(aRv, aCert, aAddon.id),
          cert: null,
        });
      }
    };
    // This allows the certificate DB to get the raw JS callback object so the
    // test code can pass through objects that XPConnect would reject.
    callback.wrappedJSObject = callback;

    gCertDB.verifySignedDirectoryAsync(root, aDir, callback);
  });
}

/**
 * Verifies that a bundle's contents are all correctly signed by an
 * AMO-issued certificate
 *
 * @param  aBundle
 *         the nsIFile for the bundle to check, either a directory or zip file
 * @param  aAddon
 *         the add-on object to verify
 * @return a Promise that resolves to an AddonManager.SIGNEDSTATE_* constant.
 */
function verifyBundleSignedState(aBundle, aAddon) {
  let promise = aBundle.isFile() ? verifyZipSignedState(aBundle, aAddon)
      : verifyDirSignedState(aBundle, aAddon);
  return promise.then(({signedState}) => signedState);
}

/**
 * Replaces %...% strings in an addon url (update and updateInfo) with
 * appropriate values.
 *
 * @param  aAddon
 *         The AddonInternal representing the add-on
 * @param  aUri
 *         The uri to escape
 * @param  aUpdateType
 *         An optional number representing the type of update, only applicable
 *         when creating a url for retrieving an update manifest
 * @param  aAppVersion
 *         The optional application version to use for %APP_VERSION%
 * @return the appropriately escaped uri.
 */
function escapeAddonURI(aAddon, aUri, aUpdateType, aAppVersion)
{
  let uri = AddonManager.escapeAddonURI(aAddon, aUri, aAppVersion);

  // If there is an updateType then replace the UPDATE_TYPE string
  if (aUpdateType)
    uri = uri.replace(/%UPDATE_TYPE%/g, aUpdateType);

  // If this add-on has compatibility information for either the current
  // application or toolkit then replace the ITEM_MAXAPPVERSION with the
  // maxVersion
  let app = aAddon.matchingTargetApplication;
  if (app)
    var maxVersion = app.maxVersion;
  else
    maxVersion = "";
  uri = uri.replace(/%ITEM_MAXAPPVERSION%/g, maxVersion);

  let compatMode = "normal";
  if (!AddonManager.checkCompatibility)
    compatMode = "ignore";
  else if (AddonManager.strictCompatibility)
    compatMode = "strict";
  uri = uri.replace(/%COMPATIBILITY_MODE%/g, compatMode);

  return uri;
}

function removeAsync(aFile) {
  return Task.spawn(function*() {
    let info = null;
    try {
      info = yield OS.File.stat(aFile.path);
      if (info.isDir)
        yield OS.File.removeDir(aFile.path);
      else
        yield OS.File.remove(aFile.path);
    }
    catch (e) {
      if (!(e instanceof OS.File.Error) || ! e.becauseNoSuchFile)
        throw e;
      // The file has already gone away
      return;
    }
  });
}

/**
 * Recursively removes a directory or file fixing permissions when necessary.
 *
 * @param  aFile
 *         The nsIFile to remove
 */
function recursiveRemove(aFile) {
  let isDir = null;

  try {
    isDir = aFile.isDirectory();
  }
  catch (e) {
    // If the file has already gone away then don't worry about it, this can
    // happen on OSX where the resource fork is automatically moved with the
    // data fork for the file. See bug 733436.
    if (e.result == Cr.NS_ERROR_FILE_TARGET_DOES_NOT_EXIST)
      return;
    if (e.result == Cr.NS_ERROR_FILE_NOT_FOUND)
      return;

    throw e;
  }

  setFilePermissions(aFile, isDir ? FileUtils.PERMS_DIRECTORY
                                  : FileUtils.PERMS_FILE);

  try {
    aFile.remove(true);
    return;
  }
  catch (e) {
    if (!aFile.isDirectory() || aFile.isSymlink()) {
      logger.error("Failed to remove file " + aFile.path, e);
      throw e;
    }
  }

  // Use a snapshot of the directory contents to avoid possible issues with
  // iterating over a directory while removing files from it (the YAFFS2
  // embedded filesystem has this issue, see bug 772238), and to remove
  // normal files before their resource forks on OSX (see bug 733436).
  let entries = getDirectoryEntries(aFile, true);
  entries.forEach(recursiveRemove);

  try {
    aFile.remove(true);
  }
  catch (e) {
    logger.error("Failed to remove empty directory " + aFile.path, e);
    throw e;
  }
}

/**
 * Returns the timestamp and leaf file name of the most recently modified
 * entry in a directory,
 * or simply the file's own timestamp if it is not a directory.
 * Also returns the total number of items (directories and files) visited in the scan
 *
 * @param  aFile
 *         A non-null nsIFile object
 * @return [File Name, Epoch time, items visited], as described above.
 */
function recursiveLastModifiedTime(aFile) {
  try {
    let modTime = aFile.lastModifiedTime;
    let fileName = aFile.leafName;
    if (aFile.isFile())
      return [fileName, modTime, 1];

    if (aFile.isDirectory()) {
      let entries = aFile.directoryEntries.QueryInterface(Ci.nsIDirectoryEnumerator);
      let entry;
      let totalItems = 1;
      while ((entry = entries.nextFile)) {
        let [subName, subTime, items] = recursiveLastModifiedTime(entry);
        totalItems += items;
        if (subTime > modTime) {
          modTime = subTime;
          fileName = subName;
        }
      }
      entries.close();
      return [fileName, modTime, totalItems];
    }
  }
  catch (e) {
    logger.warn("Problem getting last modified time for " + aFile.path, e);
  }

  // If the file is something else, just ignore it.
  return ["", 0, 0];
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

    return entries
  }
  catch (e) {
    logger.warn("Can't iterate directory " + aDir.path, e);
    return [];
  }
  finally {
    if (dirEnum) {
      dirEnum.close();
    }
  }
}

/**
 * Wraps a function in an exception handler to protect against exceptions inside callbacks
 * @param aFunction function(args...)
 * @return function(args...), a function that takes the same arguments as aFunction
 *         and returns the same result unless aFunction throws, in which case it logs
 *         a warning and returns undefined.
 */
function makeSafe(aFunction) {
  return function(...aArgs) {
    try {
      return aFunction(...aArgs);
    }
    catch (ex) {
      logger.warn("XPIProvider callback failed", ex);
    }
    return undefined;
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
 * as stored in the 'extensions.xpiState' pref.
 */
function XPIState(saved) {
  for (let [short, long] of XPIState.prototype.fields) {
    if (short in saved) {
      this[long] = saved[short];
    }
  }
}

XPIState.prototype = {
  fields: [['d', 'descriptor'],
           ['e', 'enabled'],
           ['v', 'version'],
           ['st', 'scanTime'],
           ['mt', 'manifestTime']],
  /**
   * Return the last modified time, based on enabled/disabled
   */
  get mtime() {
    if (!this.enabled && ('manifestTime' in this) && this.manifestTime > this.scanTime) {
      return this.manifestTime;
    }
    return this.scanTime;
  },

  toJSON() {
    let json = {};
    for (let [short, long] of XPIState.prototype.fields) {
      if (long in this) {
        json[short] = this[long];
      }
    }
    return json;
  },

  /**
   * Update the last modified time for an add-on on disk.
   * @param aFile: nsIFile path of the add-on.
   * @param aId: The add-on ID.
   * @return True if the time stamp has changed.
   */
  getModTime(aFile, aId) {
    let changed = false;
    let scanStarted = Cu.now();
    // For an unknown or enabled add-on, we do a full recursive scan.
    if (!('scanTime' in this) || this.enabled) {
      logger.debug('getModTime: Recursive scan of ' + aId);
      let [modFile, modTime, items] = recursiveLastModifiedTime(aFile);
      XPIProvider._mostRecentlyModifiedFile[aId] = modFile;
      XPIProvider.setTelemetry(aId, "scan_items", items);
      if (modTime != this.scanTime) {
        this.scanTime = modTime;
        changed = true;
      }
    }
    // if the add-on is disabled, modified time is the install manifest time, if
    // any. If no manifest exists, we assume this is a packed .xpi and use
    // the time stamp of {path}
    try {
      // Get the install manifest update time, if any.
      let maniFile = getManifestFileForDir(aFile);
      if (!(aId in XPIProvider._mostRecentlyModifiedFile)) {
        XPIProvider._mostRecentlyModifiedFile[aId] = maniFile.leafName;
      }
      let maniTime = maniFile.lastModifiedTime;
      if (maniTime != this.manifestTime) {
        this.manifestTime = maniTime;
        changed = true;
      }
    } catch (e) {
      // No manifest
      delete this.manifestTime;
      try {
        let dtime = aFile.lastModifiedTime;
        if (dtime != this.scanTime) {
          changed = true;
          this.scanTime = dtime;
        }
      } catch (e) {
        logger.warn("Can't get modified time of ${file}: ${e}", {file: aFile.path, e: e});
        changed = true;
        this.scanTime = 0;
      }
    }
    // Record duration of file-modified check
    XPIProvider.setTelemetry(aId, "scan_MS", Math.round(Cu.now() - scanStarted));

    return changed;
  },

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
    this.enabled = (aDBAddon.visible && !aDBAddon.disabled);
    this.version = aDBAddon.version;
    // XXX Eventually also copy bootstrap, etc.
    if (aUpdated || mustGetMod) {
      this.getModTime(new nsIFile(this.descriptor), aDBAddon.id);
      if (this.scanTime != aDBAddon.updateDate) {
        aDBAddon.updateDate = this.scanTime;
        XPIDatabase.saveChanges();
      }
    }
  },
};

// Constructor for an ES6 Map that knows how to convert itself into a
// regular object for toJSON().
function SerializableMap() {
  let m = new Map();
  m.toJSON = function() {
    let out = {}
    for (let [key, val] of m) {
      out[key] = val;
    }
    return out;
  };
  return m;
}

/**
 * Keeps track of the state of XPI add-ons on the file system.
 */
this.XPIStates = {
  // Map(location name -> Map(add-on ID -> XPIState))
  db: null,

  get size() {
    if (!this.db) {
      return 0;
    }
    let count = 0;
    for (let location of this.db.values()) {
      count += location.size;
    }
    return count;
  },

  /**
   * Load extension state data from preferences.
   */
  loadExtensionState() {
    let state = {};

    // Clear out old directory state cache.
    Preferences.reset(PREF_INSTALL_CACHE);

    let cache = Preferences.get(PREF_XPI_STATE, "{}");
    try {
      state = JSON.parse(cache);
    } catch (e) {
      logger.warn("Error parsing extensions.xpiState ${state}: ${error}",
          {state: cache, error: e});
    }
    logger.debug("Loaded add-on state from prefs: ${}", state);
    return state;
  },

  /**
   * Walk through all install locations, highest priority first,
   * comparing the on-disk state of extensions to what is stored in prefs.
   * @return true if anything has changed.
   */
  getInstallState() {
    let oldState = this.loadExtensionState();
    let changed = false;
    this.db = new SerializableMap();

    for (let location of XPIProvider.installLocations) {
      // The list of add-on like file/directory names in the install location.
      let addons = location.getAddonLocations();
      // The results of scanning this location.
      let foundAddons = new SerializableMap();

      // What our old state thinks should be in this location.
      let locState = {};
      if (location.name in oldState) {
        locState = oldState[location.name];
        // We've seen this location.
        delete oldState[location.name];
      }

      for (let [id, file] of addons) {
        if (!(id in locState)) {
          logger.debug("New add-on ${id} in ${location}", {id: id, location: location.name});
          let xpiState = new XPIState({d: file.persistentDescriptor});
          changed = xpiState.getModTime(file, id) || changed;
          foundAddons.set(id, xpiState);
        } else {
          let xpiState = new XPIState(locState[id]);
          // We found this add-on in the file system
          delete locState[id];

          changed = xpiState.getModTime(file, id) || changed;

          if (file.persistentDescriptor != xpiState.descriptor) {
            xpiState.descriptor = file.persistentDescriptor;
            changed = true;
          }
          if (changed) {
            logger.debug("Changed add-on ${id} in ${location}", {id: id, location: location.name});
          }
          else {
            logger.debug("Existing add-on ${id} in ${location}", {id: id, location: location.name});
          }
          foundAddons.set(id, xpiState);
        }
        XPIProvider.setTelemetry(id, "location", location.name);
      }

      // Anything left behind in oldState was removed from the file system.
      for (let id in locState) {
        changed = true;
        break;
      }
      // If we found anything, add this location to our database.
      if (foundAddons.size != 0) {
        this.db.set(location.name, foundAddons);
      }
    }

    // If there's anything left in oldState, an install location that held add-ons
    // was removed from the browser configuration.
    for (let location in oldState) {
      changed = true;
      break;
    }

    logger.debug("getInstallState changed: ${rv}, state: ${state}",
        {rv: changed, state: this.db});
    return changed;
  },

  /**
   * Get the Map of XPI states for a particular location.
   * @param aLocation The name of the install location.
   * @return Map (id -> XPIState) or null if there are no add-ons in the location.
   */
  getLocation(aLocation) {
    return this.db.get(aLocation);
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
    if (!location) {
      return null;
    }
    return location.get(aId);
  },

  /**
   * Find the highest priority location of an add-on by ID and return the
   * location and the XPIState.
   * @param aId   The add-on ID
   * @return [locationName, XPIState] if the add-on is found, [undefined, undefined]
   *         if the add-on is not found.
   */
  findAddon(aId) {
    // Fortunately the Map iterator returns in order of insertion, which is
    // also our highest -> lowest priority order.
    for (let [name, location] of this.db) {
      if (location.has(aId)) {
        return [name, location.get(aId)];
      }
    }
    return [undefined, undefined];
  },

  /**
   * Add a new XPIState for an add-on and synchronize it with the DBAddonInternal.
   * @param aAddon DBAddonInternal for the new add-on.
   */
  addAddon(aAddon) {
    let location = this.db.get(aAddon.location);
    if (!location) {
      // First add-on in this location.
      location = new SerializableMap();
      this.db.set(aAddon.location, location);
    }
    logger.debug("XPIStates adding add-on ${id} in ${location}: ${descriptor}", aAddon);
    let xpiState = new XPIState({d: aAddon.descriptor});
    location.set(aAddon.id, xpiState);
    xpiState.syncWithDB(aAddon, true);
    XPIProvider.setTelemetry(aAddon.id, "location", aAddon.location);
  },

  /**
   * Save the current state of installed add-ons.
   * XXX this *totally* should be a .json file using DeferredSave...
   */
  save() {
    let cache = JSON.stringify(this.db);
    Services.prefs.setCharPref(PREF_XPI_STATE, cache);
  },

  /**
   * Remove the XPIState for an add-on and save the new state.
   * @param aLocation  The name of the add-on location.
   * @param aId        The ID of the add-on.
   */
  removeAddon(aLocation, aId) {
    logger.debug("Removing XPIState for " + aLocation + ":" + aId);
    let location = this.db.get(aLocation);
    if (!location) {
      return;
    }
    location.delete(aId);
    if (location.size == 0) {
      this.db.delete(aLocation);
    }
    this.save();
  },
};

this.XPIProvider = {
  get name() {
    return "XPIProvider";
  },

  // An array of known install locations
  installLocations: null,
  // A dictionary of known install locations by name
  installLocationsByName: null,
  // An array of currently active AddonInstalls
  installs: null,
  // The default skin for the application
  defaultSkin: "classic/1.0",
  // The current skin used by the application
  currentSkin: null,
  // The selected skin to be used by the application when it is restarted. This
  // will be the same as currentSkin when it is the skin to be used when the
  // application is restarted
  selectedSkin: null,
  // The value of the minCompatibleAppVersion preference
  minCompatibleAppVersion: null,
  // The value of the minCompatiblePlatformVersion preference
  minCompatiblePlatformVersion: null,
  // A dictionary of the file descriptors for bootstrappable add-ons by ID
  bootstrappedAddons: {},
  // A Map of active addons to their bootstrapScope by ID
  activeAddons: new Map(),
  // True if the platform could have activated extensions
  extensionsActive: false,
  // True if all of the add-ons found during startup were installed in the
  // application install location
  allAppGlobal: true,
  // A string listing the enabled add-ons for annotating crash reports
  enabledAddons: null,
  // Keep track of startup phases for telemetry
  runPhase: XPI_STARTING,
  // Keep track of the newest file in each add-on, in case we want to
  // report it to telemetry.
  _mostRecentlyModifiedFile: {},
  // Per-addon telemetry information
  _telemetryDetails: {},
  // A Map from an add-on install to its ID
  _addonFileMap: new Map(),
  // Flag to know if ToolboxProcess.jsm has already been loaded by someone or not
  _toolboxProcessLoaded: false,
  // Have we started shutting down bootstrap add-ons?
  _closing: false,

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
  sortBootstrappedAddons: function() {
    let addons = {};

    // Sort the list of IDs so that the ordering is deterministic.
    for (let id of Object.keys(this.bootstrappedAddons).sort()) {
      addons[id] = Object.assign({id}, this.bootstrappedAddons[id]);
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
    }

    Object.values(addons).forEach(add);

    return Array.from(res, id => addons[id]);
  },

  /*
   * Set a value in the telemetry hash for a given ID
   */
  setTelemetry: function(aId, aName, aValue) {
    if (!this._telemetryDetails[aId])
      this._telemetryDetails[aId] = {};
    this._telemetryDetails[aId][aName] = aValue;
  },

  // Keep track of in-progress operations that support cancel()
  _inProgress: [],

  doing: function(aCancellable) {
    this._inProgress.push(aCancellable);
  },

  done: function(aCancellable) {
    let i = this._inProgress.indexOf(aCancellable);
    if (i != -1) {
      this._inProgress.splice(i, 1);
      return true;
    }
    return false;
  },

  cancelAll: function() {
    // Cancelling one may alter _inProgress, so don't use a simple iterator
    while (this._inProgress.length > 0) {
      let c = this._inProgress.shift();
      try {
        c.cancel();
      }
      catch (e) {
        logger.warn("Cancel failed", e);
      }
    }
  },

  /**
   * Adds or updates a URI mapping for an Addon.id.
   *
   * Mappings should not be removed at any point. This is so that the mappings
   * will be still valid after an add-on gets disabled or uninstalled, as
   * consumers may still have URIs of (leaked) resources they want to map.
   */
  _addURIMapping: function(aID, aFile) {
    logger.info("Mapping " + aID + " to " + aFile.path);
    this._addonFileMap.set(aID, aFile.path);

    AddonPathService.insertPath(aFile.path, aID);
  },

  /**
   * Resolve a URI back to physical file.
   *
   * Of course, this works only for URIs pointing to local resources.
   *
   * @param  aURI
   *         URI to resolve
   * @return
   *         resolved nsIFileURL
   */
  _resolveURIToFile: function(aURI) {
    switch (aURI.scheme) {
      case "jar":
      case "file":
        if (aURI instanceof Ci.nsIJARURI) {
          return this._resolveURIToFile(aURI.JARFile);
        }
        return aURI;

      case "chrome":
        aURI = ChromeRegistry.convertChromeURL(aURI);
        return this._resolveURIToFile(aURI);

      case "resource":
        aURI = Services.io.newURI(ResProtocolHandler.resolveURI(aURI), null,
                                  null);
        return this._resolveURIToFile(aURI);

      case "view-source":
        aURI = Services.io.newURI(aURI.path, null, null);
        return this._resolveURIToFile(aURI);

      case "about":
        if (aURI.spec == "about:blank") {
          // Do not attempt to map about:blank
          return null;
        }

        let chan;
        try {
          chan = NetUtil.newChannel({
            uri: aURI,
            loadUsingSystemPrincipal: true
          });
        }
        catch (ex) {
          return null;
        }
        // Avoid looping
        if (chan.URI.equals(aURI)) {
          return null;
        }
        // We want to clone the channel URI to avoid accidentially keeping
        // unnecessary references to the channel or implementation details
        // around.
        return this._resolveURIToFile(chan.URI.clone());

      default:
        return null;
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
  startup: function(aAppChanged, aOldAppVersion, aOldPlatformVersion) {
    function addDirectoryInstallLocation(aName, aKey, aPaths, aScope, aLocked) {
      try {
        var dir = FileUtils.getDir(aKey, aPaths);
      }
      catch (e) {
        // Some directories aren't defined on some platforms, ignore them
        logger.debug("Skipping unavailable install location " + aName);
        return;
      }

      try {
        var location = aLocked ? new DirectoryInstallLocation(aName, dir, aScope)
                               : new MutableDirectoryInstallLocation(aName, dir, aScope);
      }
      catch (e) {
        logger.warn("Failed to add directory install location " + aName, e);
        return;
      }

      XPIProvider.installLocations.push(location);
      XPIProvider.installLocationsByName[location.name] = location;
    }

    function addSystemAddonInstallLocation(aName, aKey, aPaths, aScope) {
      try {
        var dir = FileUtils.getDir(aKey, aPaths);
      }
      catch (e) {
        // Some directories aren't defined on some platforms, ignore them
        logger.debug("Skipping unavailable install location " + aName);
        return;
      }

      try {
        var location = new SystemAddonInstallLocation(aName, dir, aScope, aAppChanged !== false);
      }
      catch (e) {
        logger.warn("Failed to add system add-on install location " + aName, e);
        return;
      }

      XPIProvider.installLocations.push(location);
      XPIProvider.installLocationsByName[location.name] = location;
    }

    function addRegistryInstallLocation(aName, aRootkey, aScope) {
      try {
        var location = new WinRegInstallLocation(aName, aRootkey, aScope);
      }
      catch (e) {
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
      this.installs = [];
      this.installLocations = [];
      this.installLocationsByName = {};
      // Hook for tests to detect when saving database at shutdown time fails
      this._shutdownError = null;
      // Clear this at startup for xpcshell test restarts
      this._telemetryDetails = {};
      // Register our details structure with AddonManager
      AddonManagerPrivate.setTelemetryDetails("XPI", this._telemetryDetails);

      let hasRegistry = ("nsIWindowsRegKey" in Ci);

      let enabledScopes = Preferences.get(PREF_EM_ENABLED_SCOPES,
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

      addDirectoryInstallLocation(KEY_APP_SYSTEM_DEFAULTS, KEY_APP_FEATURES,
                                  [], AddonManager.SCOPE_PROFILE, true);

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

      let defaultPrefs = new Preferences({ defaultBranch: true });
      this.defaultSkin = defaultPrefs.get(PREF_GENERAL_SKINS_SELECTEDSKIN,
                                          "classic/1.0");
      this.currentSkin = Preferences.get(PREF_GENERAL_SKINS_SELECTEDSKIN,
                                         this.defaultSkin);
      this.selectedSkin = this.currentSkin;
      this.applyThemeChange();

      this.minCompatibleAppVersion = Preferences.get(PREF_EM_MIN_COMPAT_APP_VERSION,
                                                     null);
      this.minCompatiblePlatformVersion = Preferences.get(PREF_EM_MIN_COMPAT_PLATFORM_VERSION,
                                                          null);
      this.enabledAddons = "";

      Services.prefs.addObserver(PREF_EM_MIN_COMPAT_APP_VERSION, this, false);
      Services.prefs.addObserver(PREF_EM_MIN_COMPAT_PLATFORM_VERSION, this, false);
      Services.prefs.addObserver(PREF_E10S_ADDON_BLOCKLIST, this, false);
      Services.prefs.addObserver(PREF_E10S_ADDON_POLICY, this, false);
      if (!REQUIRE_SIGNING)
        Services.prefs.addObserver(PREF_XPI_SIGNATURES_REQUIRED, this, false);
      Services.obs.addObserver(this, NOTIFICATION_FLUSH_PERMISSIONS, false);

      // Cu.isModuleLoaded can fail here for external XUL apps where there is
      // no chrome.manifest that defines resource://devtools.
      if (ResProtocolHandler.hasSubstitution("devtools")) {
        if (Cu.isModuleLoaded("resource://devtools/client/framework/ToolboxProcess.jsm")) {
          // If BrowserToolboxProcess is already loaded, set the boolean to true
          // and do whatever is needed
          this._toolboxProcessLoaded = true;
          BrowserToolboxProcess.on("connectionchange",
                                   this.onDebugConnectionChange.bind(this));
        } else {
          // Else, wait for it to load
          Services.obs.addObserver(this, NOTIFICATION_TOOLBOXPROCESS_LOADED, false);
        }
      }

      let flushCaches = this.checkForChanges(aAppChanged, aOldAppVersion,
                                             aOldPlatformVersion);

      // Changes to installed extensions may have changed which theme is selected
      this.applyThemeChange();

      AddonManagerPrivate.markProviderSafe(this);

      if (aAppChanged && !this.allAppGlobal &&
          Preferences.get(PREF_EM_SHOW_MISMATCH_UI, true)) {
        let addonsToUpdate = this.shouldForceUpdateCheck(aAppChanged);
        if (addonsToUpdate) {
          this.showUpgradeUI(addonsToUpdate);
          flushCaches = true;
        }
      }

      if (flushCaches) {
        Services.obs.notifyObservers(null, "startupcache-invalidate", null);
        // UI displayed early in startup (like the compatibility UI) may have
        // caused us to cache parts of the skin or locale in memory. These must
        // be flushed to allow extension provided skins and locales to take full
        // effect
        Services.obs.notifyObservers(null, "chrome-flush-skin-caches", null);
        Services.obs.notifyObservers(null, "chrome-flush-caches", null);
      }

      this.enabledAddons = Preferences.get(PREF_EM_ENABLED_ADDONS, "");

      if ("nsICrashReporter" in Ci &&
          Services.appinfo instanceof Ci.nsICrashReporter) {
        // Annotate the crash report with relevant add-on information.
        try {
          Services.appinfo.annotateCrashReport("Theme", this.currentSkin);
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
          try {
            let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
            file.persistentDescriptor = addon.descriptor;
            let reason = BOOTSTRAP_REASONS.APP_STARTUP;
            // Eventually set INSTALLED reason when a bootstrap addon
            // is dropped in profile folder and automatically installed
            if (AddonManager.getStartupChanges(AddonManager.STARTUP_CHANGE_INSTALLED)
                            .indexOf(addon.id) !== -1)
              reason = BOOTSTRAP_REASONS.ADDON_INSTALL;
            this.callBootstrapMethod(createAddonDetails(addon.id, addon),
                                     file, "startup", reason);
          }
          catch (e) {
            logger.error("Failed to load bootstrap addon " + addon.id + " from " +
                         addon.descriptor, e);
          }
        }
        AddonManagerPrivate.recordTimestamp("XPI_bootstrap_addons_end");
      }
      catch (e) {
        logger.error("bootstrap startup failed", e);
        AddonManagerPrivate.recordException("XPI-BOOTSTRAP", "startup failed", e);
      }

      // Let these shutdown a little earlier when they still have access to most
      // of XPCOM
      Services.obs.addObserver({
        observe: function(aSubject, aTopic, aData) {
          XPIProvider._closing = true;
          for (let addon of XPIProvider.sortBootstrappedAddons().reverse()) {
            // If no scope has been loaded for this add-on then there is no need
            // to shut it down (should only happen when a bootstrapped add-on is
            // pending enable)
            if (!XPIProvider.activeAddons.has(addon.id))
              continue;

            let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
            file.persistentDescriptor = addon.descriptor;
            let addonDetails = createAddonDetails(addon.id, addon);

            // If the add-on was pending disable then shut it down and remove it
            // from the persisted data.
            if (addon.disable) {
              XPIProvider.callBootstrapMethod(addonDetails, file, "shutdown",
                                              BOOTSTRAP_REASONS.ADDON_DISABLE);
              delete XPIProvider.bootstrappedAddons[addon.id];
            }
            else {
              XPIProvider.callBootstrapMethod(addonDetails, file, "shutdown",
                                              BOOTSTRAP_REASONS.APP_SHUTDOWN);
            }
          }
          Services.obs.removeObserver(this, "quit-application-granted");
        }
      }, "quit-application-granted", false);

      // Detect final-ui-startup for telemetry reporting
      Services.obs.addObserver({
        observe: function(aSubject, aTopic, aData) {
          AddonManagerPrivate.recordTimestamp("XPI_finalUIStartup");
          XPIProvider.runPhase = XPI_AFTER_UI_STARTUP;
          Services.obs.removeObserver(this, "final-ui-startup");
        }
      }, "final-ui-startup", false);

      AddonManagerPrivate.recordTimestamp("XPI_startup_end");

      this.extensionsActive = true;
      this.runPhase = XPI_BEFORE_UI_STARTUP;

      let timerManager = Cc["@mozilla.org/updates/timer-manager;1"].
                         getService(Ci.nsIUpdateTimerManager);
      timerManager.registerTimer("xpi-signature-verification", () => {
        this.verifySignatures();
      }, XPI_SIGNATURE_CHECK_PERIOD);
    }
    catch (e) {
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
  shutdown: function() {
    logger.debug("shutdown");

    // Stop anything we were doing asynchronously
    this.cancelAll();

    this.bootstrappedAddons = {};
    this.activeAddons.clear();
    this.enabledAddons = null;
    this.allAppGlobal = true;

    // If there are pending operations then we must update the list of active
    // add-ons
    if (Preferences.get(PREF_PENDING_OPERATIONS, false)) {
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_pending_ops", 1);
      XPIDatabase.updateActiveAddons();
      Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS,
                                 !XPIDatabase.writeAddonsList());
    }

    this.installs = null;
    this.installLocations = null;
    this.installLocationsByName = null;

    // This is needed to allow xpcshell tests to simulate a restart
    this.extensionsActive = false;
    this._addonFileMap.clear();

    if (gLazyObjectsLoaded) {
      let done = XPIDatabase.shutdown();
      done.then(
        ret => {
          logger.debug("Notifying XPI shutdown observers");
          Services.obs.notifyObservers(null, "xpi-provider-shutdown", null);
        },
        err => {
          logger.debug("Notifying XPI shutdown observers");
          this._shutdownError = err;
          Services.obs.notifyObservers(null, "xpi-provider-shutdown", err);
        }
      );
      return done;
    }
    logger.debug("Notifying XPI shutdown observers");
    Services.obs.notifyObservers(null, "xpi-provider-shutdown", null);
    return undefined;
  },

  /**
   * Applies any pending theme change to the preferences.
   */
  applyThemeChange: function() {
    if (!Preferences.get(PREF_DSS_SWITCHPENDING, false))
      return;

    // Tell the Chrome Registry which Skin to select
    try {
      this.selectedSkin = Preferences.get(PREF_DSS_SKIN_TO_SELECT);
      Services.prefs.setCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN,
                                 this.selectedSkin);
      Services.prefs.clearUserPref(PREF_DSS_SKIN_TO_SELECT);
      logger.debug("Changed skin to " + this.selectedSkin);
      this.currentSkin = this.selectedSkin;
    }
    catch (e) {
      logger.error("Error applying theme change", e);
    }
    Services.prefs.clearUserPref(PREF_DSS_SWITCHPENDING);
  },

  /**
   * If the application has been upgraded and there are add-ons outside the
   * application directory then we may need to synchronize compatibility
   * information but only if the mismatch UI isn't disabled.
   *
   * @returns False if no update check is needed, otherwise an array of add-on
   *          IDs to check for updates. Array may be empty if no add-ons can be/need
   *           to be updated, but the metadata check needs to be performed.
   */
  shouldForceUpdateCheck: function(aAppChanged) {
    AddonManagerPrivate.recordSimpleMeasure("XPIDB_metadata_age", AddonRepository.metadataAge());

    let startupChanges = AddonManager.getStartupChanges(AddonManager.STARTUP_CHANGE_DISABLED);
    logger.debug("shouldForceUpdateCheck startupChanges: " + startupChanges.toSource());
    AddonManagerPrivate.recordSimpleMeasure("XPIDB_startup_disabled", startupChanges.length);

    let forceUpdate = [];
    if (startupChanges.length > 0) {
    let addons = XPIDatabase.getAddons();
      for (let addon of addons) {
        if ((startupChanges.indexOf(addon.id) != -1) &&
            (addon.permissions() & AddonManager.PERM_CAN_UPGRADE) &&
            !addon.isCompatible) {
          logger.debug("shouldForceUpdateCheck: can upgrade disabled add-on " + addon.id);
          forceUpdate.push(addon.id);
        }
      }
    }

    if (AddonRepository.isMetadataStale()) {
      logger.debug("shouldForceUpdateCheck: metadata is stale");
      return forceUpdate;
    }
    if (forceUpdate.length > 0) {
      return forceUpdate;
    }

    return false;
  },

  /**
   * Shows the "Compatibility Updates" UI.
   *
   * @param  aAddonIDs
   *         Array opf addon IDs that were disabled by the application update, and
   *         should therefore be checked for updates.
   */
  showUpgradeUI: function(aAddonIDs) {
    logger.debug("XPI_showUpgradeUI: " + aAddonIDs.toSource());
    Services.telemetry.getHistogramById("ADDON_MANAGER_UPGRADE_UI_SHOWN").add(1);

    // Flip a flag to indicate that we interrupted startup with an interactive prompt
    Services.startup.interrupted = true;

    var variant = Cc["@mozilla.org/variant;1"].
                  createInstance(Ci.nsIWritableVariant);
    variant.setFromVariant(aAddonIDs);

    // This *must* be modal as it has to block startup.
    var features = "chrome,centerscreen,dialog,titlebar,modal";
    var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
             getService(Ci.nsIWindowWatcher);
    ww.openWindow(null, URI_EXTENSION_UPDATE_DIALOG, "", features, variant);

    // Ensure any changes to the add-ons list are flushed to disk
    Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS,
                               !XPIDatabase.writeAddonsList());
  },

  updateSystemAddons: Task.async(function*() {
    let systemAddonLocation = XPIProvider.installLocationsByName[KEY_APP_SYSTEM_ADDONS];
    if (!systemAddonLocation)
      return;

    // Don't do anything in safe mode
    if (Services.appinfo.inSafeMode)
      return;

    // Download the list of system add-ons
    let url = Preferences.get(PREF_SYSTEM_ADDON_UPDATE_URL, null);
    if (!url) {
      yield systemAddonLocation.cleanDirectories();
      return;
    }

    url = UpdateUtils.formatUpdateURL(url);

    logger.info(`Starting system add-on update check from ${url}.`);
    let addonList = yield ProductAddonChecker.getProductAddonList(url);

    // If there was no list then do nothing.
    if (!addonList) {
      logger.info("No system add-ons list was returned.");
      yield systemAddonLocation.cleanDirectories();
      return;
    }

    addonList = new Map(
      addonList.map(spec => [spec.id, { spec, path: null, addon: null }]));

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
    let updatedAddons = addonMap(yield getAddonsInLocation(KEY_APP_SYSTEM_ADDONS));
    if (setMatches(addonList, updatedAddons)) {
      logger.info("Retaining existing updated system add-ons.");
      yield systemAddonLocation.cleanDirectories();
      return;
    }

    // If this matches the current set in the default location then reset the
    // updated set.
    let defaultAddons = addonMap(yield getAddonsInLocation(KEY_APP_SYSTEM_DEFAULTS));
    if (setMatches(addonList, defaultAddons)) {
      logger.info("Resetting system add-ons.");
      systemAddonLocation.resetAddonSet();
      yield systemAddonLocation.cleanDirectories();
      return;
    }

    // Download all the add-ons
    // Bug 1204158: If we already have some of these locally then just use those
    let downloadAddon = Task.async(function*(item) {
      try {
        let sourceAddon = updatedAddons.get(item.spec.id);
        if (sourceAddon && sourceAddon.version == item.spec.version) {
          // Copying the file to a temporary location has some benefits. If the
          // file is locked and cannot be read then we'll fall back to
          // downloading a fresh copy. It also means we don't have to remember
          // whether to delete the temporary copy later.
          try {
            let path = OS.Path.join(OS.Constants.Path.tmpDir, "tmpaddon");
            let unique = yield OS.File.openUnique(path);
            unique.file.close();
            yield OS.File.copy(sourceAddon._sourceBundle.path, unique.path);
            // Make sure to update file modification times so this is detected
            // as a new add-on.
            yield OS.File.setDates(unique.path);
            item.path = unique.path;
          }
          catch (e) {
            logger.warn(`Failed make temporary copy of ${sourceAddon._sourceBundle.path}.`, e);
          }
        }
        if (!item.path) {
          item.path = yield ProductAddonChecker.downloadAddon(item.spec);
        }
        item.addon = yield loadManifestFromFile(nsIFile(item.path), systemAddonLocation);
      }
      catch (e) {
        logger.error(`Failed to download system add-on ${item.spec.id}`, e);
      }
    });
    yield Promise.all(Array.from(addonList.values()).map(downloadAddon));

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
    }

    try {
      if (!Array.from(addonList.values()).every(item => item.path && item.addon && validateAddon(item))) {
        throw new Error("Rejecting updated system add-on set that either could not " +
                        "be downloaded or contained unusable add-ons.");
      }

      // Install into the install location
      logger.info("Installing new system add-on set");
      yield systemAddonLocation.installAddonSet(Array.from(addonList.values())
        .map(a => a.addon));

      // Bug 1204156: Switch to the new system add-ons without requiring a restart
    }
    finally {
      // Delete the temporary files
      logger.info("Deleting temporary files");
      for (let item of addonList.values()) {
        // If this item downloaded delete the temporary file.
        if (item.path) {
          try {
            yield OS.File.remove(item.path);
          }
          catch (e) {
            logger.warn(`Failed to remove temporary file ${item.path}.`, e);
          }
        }
      }

      yield systemAddonLocation.cleanDirectories();
    }
  }),

  /**
   * Verifies that all installed add-ons are still correctly signed.
   */
  verifySignatures: function() {
    XPIDatabase.getAddonList(a => true, (addons) => {
      Task.spawn(function*() {
        let changes = {
          enabled: [],
          disabled: []
        };

        for (let addon of addons) {
          // The add-on might have vanished, we'll catch that on the next startup
          if (!addon._sourceBundle.exists())
            continue;

          let signedState = yield verifyBundleSignedState(addon._sourceBundle, addon);

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
      }).then(null, err => {
        logger.error("XPI_verifySignature: " + err);
      })
    });
  },

  /**
   * Persists changes to XPIProvider.bootstrappedAddons to its store (a pref).
   */
  persistBootstrappedAddons: function() {
    // Experiments are disabled upon app load, so don't persist references.
    let filtered = {};
    for (let id in this.bootstrappedAddons) {
      let entry = this.bootstrappedAddons[id];
      if (entry.type == "experiment") {
        continue;
      }

      filtered[id] = entry;
    }

    Services.prefs.setCharPref(PREF_BOOTSTRAP_ADDONS,
                               JSON.stringify(filtered));
  },

  /**
   * Adds a list of currently active add-ons to the next crash report.
   */
  addAddonsToCrashReporter: function() {
    if (!("nsICrashReporter" in Ci) ||
        !(Services.appinfo instanceof Ci.nsICrashReporter))
      return;

    // In safe mode no add-ons are loaded so we should not include them in the
    // crash report
    if (Services.appinfo.inSafeMode)
      return;

    let data = this.enabledAddons;
    for (let id in this.bootstrappedAddons) {
      data += (data ? "," : "") + encodeURIComponent(id) + ":" +
              encodeURIComponent(this.bootstrappedAddons[id].version);
    }

    try {
      Services.appinfo.annotateCrashReport("Add-ons", data);
    }
    catch (e) { }

    let TelemetrySession =
      Cu.import("resource://gre/modules/TelemetrySession.jsm", {}).TelemetrySession;
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
  processPendingFileChanges: function(aManifests) {
    let changed = false;
    for (let location of this.installLocations) {
      aManifests[location.name] = {};
      // We can't install or uninstall anything in locked locations
      if (location.locked)
        continue;

      let stagingDir = location.getStagingDir();

      try {
        if (!stagingDir || !stagingDir.exists() || !stagingDir.isDirectory())
          continue;
      }
      catch (e) {
        logger.warn("Failed to find staging directory", e);
        continue;
      }

      let seenFiles = [];
      // Use a snapshot of the directory contents to avoid possible issues with
      // iterating over a directory while removing files from it (the YAFFS2
      // embedded filesystem has this issue, see bug 772238), and to remove
      // normal files before their resource forks on OSX (see bug 733436).
      let stagingDirEntries = getDirectoryEntries(stagingDir, true);
      for (let stageDirEntry of stagingDirEntries) {
        let id = stageDirEntry.leafName;

        let isDir;
        try {
          isDir = stageDirEntry.isDirectory();
        }
        catch (e) {
          if (e.result != Cr.NS_ERROR_FILE_TARGET_DOES_NOT_EXIST)
            throw e;
          // If the file has already gone away then don't worry about it, this
          // can happen on OSX where the resource fork is automatically moved
          // with the data fork for the file. See bug 733436.
          continue;
        }

        if (!isDir) {
          if (id.substring(id.length - 4).toLowerCase() == ".xpi") {
            id = id.substring(0, id.length - 4);
          }
          else {
            if (id.substring(id.length - 5).toLowerCase() != ".json") {
              logger.warn("Ignoring file: " + stageDirEntry.path);
              seenFiles.push(stageDirEntry.leafName);
            }
            continue;
          }
        }

        // Check that the directory's name is a valid ID.
        if (!gIDTest.test(id)) {
          logger.warn("Ignoring directory whose name is not a valid add-on ID: " +
               stageDirEntry.path);
          seenFiles.push(stageDirEntry.leafName);
          continue;
        }

        changed = true;

        if (isDir) {
          // Check if the directory contains an install manifest.
          let manifest = getManifestFileForDir(stageDirEntry);

          // If the install manifest doesn't exist uninstall this add-on in this
          // install location.
          if (!manifest) {
            logger.debug("Processing uninstall of " + id + " in " + location.name);

            try {
              let addonFile = location.getLocationForID(id);
              let addonToUninstall = syncLoadManifestFromFile(addonFile, location);
              if (addonToUninstall.bootstrap) {
                this.callBootstrapMethod(addonToUninstall, addonToUninstall._sourceBundle,
                                         "uninstall", BOOTSTRAP_REASONS.ADDON_UNINSTALL);
              }
            }
            catch (e) {
              logger.warn("Failed to call uninstall for " + id, e);
            }

            try {
              location.uninstallAddon(id);
              seenFiles.push(stageDirEntry.leafName);
            }
            catch (e) {
              logger.error("Failed to uninstall add-on " + id + " in " + location.name, e);
            }
            // The file check later will spot the removal and cleanup the database
            continue;
          }
        }

        aManifests[location.name][id] = null;
        let existingAddonID = id;

        let jsonfile = stagingDir.clone();
        jsonfile.append(id + ".json");
        // Assume this was a foreign install if there is no cached metadata file
        let foreignInstall = !jsonfile.exists();
        let addon;

        try {
          addon = syncLoadManifestFromFile(stageDirEntry, location);
        }
        catch (e) {
          logger.error("Unable to read add-on manifest from " + stageDirEntry.path, e);
          // This add-on can't be installed so just remove it now
          seenFiles.push(stageDirEntry.leafName);
          seenFiles.push(jsonfile.leafName);
          continue;
        }

        if (mustSign(addon.type) &&
            addon.signedState <= AddonManager.SIGNEDSTATE_MISSING) {
          logger.warn("Refusing to install staged add-on " + id + " with signed state " + addon.signedState);
          seenFiles.push(stageDirEntry.leafName);
          seenFiles.push(jsonfile.leafName);
          continue;
        }

        // Check for a cached metadata for this add-on, it may contain updated
        // compatibility information
        if (!foreignInstall) {
          logger.debug("Found updated metadata for " + id + " in " + location.name);
          let fis = Cc["@mozilla.org/network/file-input-stream;1"].
                       createInstance(Ci.nsIFileInputStream);
          let json = Cc["@mozilla.org/dom/json;1"].
                     createInstance(Ci.nsIJSON);

          try {
            fis.init(jsonfile, -1, 0, 0);
            let metadata = json.decodeFromStream(fis, jsonfile.fileSize);
            addon.importMetadata(metadata);

            // Pass this through to addMetadata so it knows this add-on was
            // likely installed through the UI
            aManifests[location.name][id] = addon;
          }
          catch (e) {
            // If some data can't be recovered from the cached metadata then it
            // is unlikely to be a problem big enough to justify throwing away
            // the install, just log and error and continue
            logger.error("Unable to read metadata from " + jsonfile.path, e);
          }
          finally {
            fis.close();
          }
        }
        seenFiles.push(jsonfile.leafName);

        existingAddonID = addon.existingAddonID || id;

        var oldBootstrap = null;
        logger.debug("Processing install of " + id + " in " + location.name);
        if (existingAddonID in this.bootstrappedAddons) {
          try {
            var existingAddon = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
            existingAddon.persistentDescriptor = this.bootstrappedAddons[existingAddonID].descriptor;
            if (existingAddon.exists()) {
              oldBootstrap = this.bootstrappedAddons[existingAddonID];

              // We'll be replacing a currently active bootstrapped add-on so
              // call its uninstall method
              let newVersion = addon.version;
              let oldVersion = oldBootstrap.version;
              let uninstallReason = Services.vc.compare(oldVersion, newVersion) < 0 ?
                                    BOOTSTRAP_REASONS.ADDON_UPGRADE :
                                    BOOTSTRAP_REASONS.ADDON_DOWNGRADE;

              this.callBootstrapMethod(createAddonDetails(existingAddonID, oldBootstrap),
                                       existingAddon, "uninstall", uninstallReason,
                                       { newVersion: newVersion });
              this.unloadBootstrapScope(existingAddonID);
              flushChromeCaches();
            }
          }
          catch (e) {
          }
        }

        try {
          addon._sourceBundle = location.installAddon(id, stageDirEntry,
                                                       existingAddonID);
        }
        catch (e) {
          logger.error("Failed to install staged add-on " + id + " in " + location.name,
                e);
          // Re-create the staged install
          AddonInstall.createStagedInstall(location, stageDirEntry,
                                           addon);
          // Make sure not to delete the cached manifest json file
          seenFiles.pop();

          delete aManifests[location.name][id];

          if (oldBootstrap) {
            // Re-install the old add-on
            this.callBootstrapMethod(createAddonDetails(existingAddonID, oldBootstrap),
                                     existingAddon, "install",
                                     BOOTSTRAP_REASONS.ADDON_INSTALL);
          }
          continue;
        }
      }

      try {
        location.cleanStagingDir(seenFiles);
      }
      catch (e) {
        // Non-critical, just saves some perf on startup if we clean this up.
        logger.debug("Error cleaning staging dir " + stagingDir.path, e);
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
  installDistributionAddons: function(aManifests, aAppChanged) {
    let distroDir;
    try {
      distroDir = FileUtils.getDir(KEY_APP_DISTRIBUTION, [DIR_EXTENSIONS]);
    }
    catch (e) {
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
        }
        else {
          logger.debug("Ignoring distribution add-on that isn't an XPI: " + entry.path);
          continue;
        }
      }
      else if (!entry.isDirectory()) {
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
      if (!aAppChanged && Preferences.isSet(PREF_BRANCH_INSTALLED_ADDON + id)) {
        continue;
      }

      let addon;
      try {
        addon = syncLoadManifestFromFile(entry, profileLocation);
      }
      catch (e) {
        logger.warn("File entry " + entry.path + " contains an invalid add-on", e);
        continue;
      }

      if (addon.id != id) {
        logger.warn("File entry " + entry.path + " contains an add-on with an " +
             "incorrect ID")
        continue;
      }

      let existingEntry = null;
      try {
        existingEntry = profileLocation.getLocationForID(id);
      }
      catch (e) {
      }

      if (existingEntry) {
        let existingAddon;
        try {
          existingAddon = syncLoadManifestFromFile(existingEntry, profileLocation);

          if (Services.vc.compare(addon.version, existingAddon.version) <= 0)
            continue;
        }
        catch (e) {
          // Bad add-on in the profile so just proceed and install over the top
          logger.warn("Profile contains an add-on with a bad or missing install " +
               "manifest at " + existingEntry.path + ", overwriting", e);
        }
      }
      else if (Preferences.get(PREF_BRANCH_INSTALLED_ADDON + id, false)) {
        continue;
      }

      // Install the add-on
      try {
        addon._sourceBundle = profileLocation.installAddon(id, entry, null, true);
        logger.debug("Installed distribution add-on " + id);

        Services.prefs.setBoolPref(PREF_BRANCH_INSTALLED_ADDON + id, true)

        // aManifests may contain a copy of a newly installed add-on's manifest
        // and we'll have overwritten that so instead cache our install manifest
        // which will later be put into the database in processFileChanges
        if (!(KEY_APP_PROFILE in aManifests))
          aManifests[KEY_APP_PROFILE] = {};
        aManifests[KEY_APP_PROFILE][id] = addon;
        changed = true;
      }
      catch (e) {
        logger.error("Failed to install distribution add-on " + entry.path, e);
      }
    }

    entries.close();

    return changed;
  },

  /**
   * Imports the xpinstall permissions from preferences into the permissions
   * manager for the user to change later.
   */
  importPermissions: function() {
    PermissionsUtils.importFromPrefs(PREF_XPI_PERMISSIONS_BRANCH,
                                     XPI_PERMISSION);
  },

  getDependentAddons: function(aAddon) {
    return Array.from(XPIDatabase.getAddons())
                .filter(addon => addon.dependencies.includes(aAddon.id));
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
  checkForChanges: function(aAppChanged, aOldAppVersion,
                                                aOldPlatformVersion) {
    logger.debug("checkForChanges");

    // Keep track of whether and why we need to open and update the database at
    // startup time.
    let updateReasons = [];
    if (aAppChanged) {
      updateReasons.push("appChanged");
    }

    // Load the list of bootstrapped add-ons first so processFileChanges can
    // modify it
    // XXX eventually get rid of bootstrappedAddons
    try {
      this.bootstrappedAddons = JSON.parse(Preferences.get(PREF_BOOTSTRAP_ADDONS,
                                           "{}"));
    } catch (e) {
      logger.warn("Error parsing enabled bootstrapped extensions cache", e);
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
    let hasPendingChanges = Preferences.get(PREF_PENDING_OPERATIONS);
    if (hasPendingChanges) {
      updateReasons.push("hasPendingChanges");
    }

    // If the application has changed then check for new distribution add-ons
    if (Preferences.get(PREF_INSTALL_DISTRO_ADDONS, true))
    {
      updated = this.installDistributionAddons(manifests, aAppChanged);
      if (updated) {
        updateReasons.push("installDistributionAddons");
      }
    }

    // Telemetry probe added around getInstallState() to check perf
    let telemetryCaptureTime = Cu.now();
    let installChanged = XPIStates.getInstallState();
    let telemetry = Services.telemetry;
    telemetry.getHistogramById("CHECK_ADDONS_MODIFIED_MS").add(Math.round(Cu.now() - telemetryCaptureTime));
    if (installChanged) {
      updateReasons.push("directoryState");
    }

    let haveAnyAddons = (XPIStates.size > 0);

    // If the schema appears to have changed then we should update the database
    if (DB_SCHEMA != Preferences.get(PREF_DB_SCHEMA, 0)) {
      // If we don't have any add-ons, just update the pref, since we don't need to
      // write the database
      if (!haveAnyAddons) {
        logger.debug("Empty XPI database, setting schema version preference to " + DB_SCHEMA);
        Services.prefs.setIntPref(PREF_DB_SCHEMA, DB_SCHEMA);
      }
      else {
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

    // XXX This will go away when we fold bootstrappedAddons into XPIStates.
    if (updateReasons.length == 0) {
      let bootstrapDescriptors = new Set(Object.keys(this.bootstrappedAddons)
        .map(b => this.bootstrappedAddons[b].descriptor));

      for (let location of XPIStates.db.values()) {
        for (let state of location.values()) {
          bootstrapDescriptors.delete(state.descriptor);
        }
      }

      if (bootstrapDescriptors.size > 0) {
        logger.warn("Bootstrap state is invalid (missing add-ons: "
            + Array.from(bootstrapDescriptors).join(", ") + ")");
        updateReasons.push("missingBootstrapAddon");
      }
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
                                                                         aOldPlatformVersion);
        }
        catch (e) {
          logger.error("Failed to process extension changes at startup", e);
        }
      }

      if (aAppChanged) {
        // When upgrading the app and using a custom skin make sure it is still
        // compatible otherwise switch back the default
        if (this.currentSkin != this.defaultSkin) {
          let oldSkin = XPIDatabase.getVisibleAddonForInternalName(this.currentSkin);
          if (!oldSkin || oldSkin.disabled)
            this.enableDefaultTheme();
        }

        // When upgrading remove the old extensions cache to force older
        // versions to rescan the entire list of extensions
        let oldCache = FileUtils.getFile(KEY_PROFILEDIR, [FILE_OLD_CACHE], true);
        try {
          if (oldCache.exists())
            oldCache.remove(true);
        }
        catch (e) {
          logger.warn("Unable to remove old extension cache " + oldCache.path, e);
        }
      }

      // If the application crashed before completing any pending operations then
      // we should perform them now.
      if (extensionListChanged || hasPendingChanges) {
        logger.debug("Updating database with changes to installed add-ons");
        XPIDatabase.updateActiveAddons();
        Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS,
                                   !XPIDatabase.writeAddonsList());
        Services.prefs.setCharPref(PREF_BOOTSTRAP_ADDONS,
                                   JSON.stringify(this.bootstrappedAddons));
        return true;
      }

      logger.debug("No changes found");
    }
    catch (e) {
      logger.error("Error during startup file checks", e);
    }

    // Check that the add-ons list still exists
    let addonsList = FileUtils.getFile(KEY_PROFILEDIR, [FILE_XPI_ADDONS_LIST],
                                       true);
    // the addons list file should exist if and only if we have add-ons installed
    if (addonsList.exists() != haveAnyAddons) {
      logger.debug("Add-ons list is invalid, rebuilding");
      XPIDatabase.writeAddonsList();
    }

    return false;
  },

  /**
   * Called to test whether this provider supports installing a particular
   * mimetype.
   *
   * @param  aMimetype
   *         The mimetype to check for
   * @return true if the mimetype is application/x-xpinstall
   */
  supportsMimetype: function(aMimetype) {
    return aMimetype == "application/x-xpinstall";
  },

  /**
   * Called to test whether installing XPI add-ons is enabled.
   *
   * @return true if installing is enabled
   */
  isInstallEnabled: function() {
    // Default to enabled if the preference does not exist
    return Preferences.get(PREF_XPI_ENABLED, true);
  },

  /**
   * Called to test whether installing XPI add-ons by direct URL requests is
   * whitelisted.
   *
   * @return true if installing by direct requests is whitelisted
   */
  isDirectRequestWhitelisted: function() {
    // Default to whitelisted if the preference does not exist.
    return Preferences.get(PREF_XPI_DIRECT_WHITELISTED, true);
  },

  /**
   * Called to test whether installing XPI add-ons from file referrers is
   * whitelisted.
   *
   * @return true if installing from file referrers is whitelisted
   */
  isFileRequestWhitelisted: function() {
    // Default to whitelisted if the preference does not exist.
    return Preferences.get(PREF_XPI_FILE_WHITELISTED, true);
  },

  /**
   * Called to test whether installing XPI add-ons from a URI is allowed.
   *
   * @param  aInstallingPrincipal
   *         The nsIPrincipal that initiated the install
   * @return true if installing is allowed
   */
  isInstallAllowed: function(aInstallingPrincipal) {
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

    let requireWhitelist = Preferences.get(PREF_XPI_WHITELIST_REQUIRED, true);
    if (requireWhitelist && (permission != Ci.nsIPermissionManager.ALLOW_ACTION))
      return false;

    let requireSecureOrigin = Preferences.get(PREF_INSTALL_REQUIRESECUREORIGIN, true);
    let safeSchemes = ["https", "chrome", "file"];
    if (requireSecureOrigin && safeSchemes.indexOf(uri.scheme) == -1)
      return false;

    return true;
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
  getInstallForURL: function(aUrl, aHash, aName, aIcons, aVersion, aBrowser,
                             aCallback) {
    AddonInstall.createDownload(function(aInstall) {
      aCallback(aInstall.wrapper);
    }, aUrl, aHash, aName, aIcons, aVersion, aBrowser);
  },

  /**
   * Called to get an AddonInstall to install an add-on from a local file.
   *
   * @param  aFile
   *         The file to be installed
   * @param  aCallback
   *         A callback to pass the AddonInstall to
   */
  getInstallForFile: function(aFile, aCallback) {
    AddonInstall.createInstall(function(aInstall) {
      if (aInstall)
        aCallback(aInstall.wrapper);
      else
        aCallback(null);
    }, aFile);
  },

  /**
   * Temporarily installs add-on from a local XPI file or directory.
   * As this is intended for development, the signature is not checked and
   * the add-on does not persist on application restart.
   *
   * @param aFile
   *        An nsIFile for the unpacked add-on directory or XPI file.
   *
   * @return a Promise that resolves to an Addon object on success, or rejects
   *         if the add-on is not a valid restartless add-on or if the
   *         same ID is already temporarily installed
   */
  installTemporaryAddon: Task.async(function*(aFile) {
    if (aFile.exists() && aFile.isFile()) {
      flushJarCache(aFile);
    }
    let addon = yield loadManifestFromFile(aFile, TemporaryInstallLocation);

    if (!addon.bootstrap) {
      throw new Error("Only restartless (bootstrap) add-ons"
                    + " can be temporarily installed:", addon.id);
    }
    let installReason = BOOTSTRAP_REASONS.ADDON_INSTALL;
    let oldAddon = yield new Promise(
                   resolve => XPIDatabase.getVisibleAddonForID(addon.id, resolve));
    if (oldAddon) {
      if (!oldAddon.bootstrap) {
        logger.warn("Non-restartless Add-on is already installed", addon.id);
        throw new Error("Non-restartless add-on with ID "
                        + oldAddon.id + " is already installed");
      }
      else {
        logger.warn("Addon with ID " + oldAddon.id + " already installed,"
                    + " older version will be disabled");

        let existingAddonID = oldAddon.id;
        let existingAddon = oldAddon._sourceBundle;

        // We'll be replacing a currently active bootstrapped add-on so
        // call its uninstall method
        let newVersion = addon.version;
        let oldVersion = oldAddon.version;
        if (Services.vc.compare(newVersion, oldVersion) >= 0) {
          installReason = BOOTSTRAP_REASONS.ADDON_UPGRADE;
        } else {
          installReason = BOOTSTRAP_REASONS.ADDON_DOWNGRADE;
        }
        let uninstallReason = installReason;

        if (oldAddon.active) {
          XPIProvider.callBootstrapMethod(oldAddon, existingAddon,
                                          "shutdown", uninstallReason,
                                          { newVersion });
        }
        this.callBootstrapMethod(oldAddon, existingAddon,
                                 "uninstall", uninstallReason, { newVersion });
        this.unloadBootstrapScope(existingAddonID);
        flushChromeCaches();
      }
    }

    let file = addon._sourceBundle;

    XPIProvider._addURIMapping(addon.id, file);
    XPIProvider.callBootstrapMethod(addon, file, "install", installReason);
    addon.state = AddonManager.STATE_INSTALLED;
    logger.debug("Install of temporary addon in " + aFile.path + " completed.");
    addon.visible = true;
    addon.enabled = true;
    addon.active = true;

    addon = XPIDatabase.addAddonMetadata(addon, file.persistentDescriptor);

    XPIStates.addAddon(addon);
    XPIDatabase.saveChanges();

    AddonManagerPrivate.callAddonListeners("onInstalling", addon.wrapper,
                                           false);
    XPIProvider.callBootstrapMethod(addon, file, "startup",
                                    BOOTSTRAP_REASONS.ADDON_ENABLE);
    AddonManagerPrivate.callInstallListeners("onExternalInstall",
                                             null, addon.wrapper,
                                             oldAddon ? oldAddon.wrapper : null,
                                             false);
    AddonManagerPrivate.callAddonListeners("onInstalled", addon.wrapper);

    return addon.wrapper;
  }),

  /**
   * Returns an Addon corresponding to an instance ID.
   * @param aInstanceID
   *        An Addon Instance ID
   * @return {Promise}
   * @resolves The found Addon or null if no such add-on exists.
   * @rejects  Never
   * @throws if the aInstanceID argument is not specified
   */
   getAddonByInstanceID: function(aInstanceID) {
     if (!aInstanceID || typeof aInstanceID != "symbol")
       throw Components.Exception("aInstanceID must be a Symbol()",
                                  Cr.NS_ERROR_INVALID_ARG);

     for (let [id, val] of this.activeAddons) {
       if (aInstanceID == val.instanceID) {
         if (val.safeWrapper) {
           return Promise.resolve(val.safeWrapper);
         }

         return new Promise(resolve => {
           this.getAddonByID(id, function(addon) {
             val.safeWrapper = new PrivateWrapper(addon);
             resolve(val.safeWrapper);
           });
         });
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
  removeActiveInstall: function(aInstall) {
    let where = this.installs.indexOf(aInstall);
    if (where == -1) {
      logger.warn("removeActiveInstall: could not find active install for "
          + aInstall.sourceURI.spec);
      return;
    }
    this.installs.splice(where, 1);
  },

  /**
   * Called to get an Addon with a particular ID.
   *
   * @param  aId
   *         The ID of the add-on to retrieve
   * @param  aCallback
   *         A callback to pass the Addon to
   */
  getAddonByID: function(aId, aCallback) {
    XPIDatabase.getVisibleAddonForID (aId, function(aAddon) {
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
  getAddonsByTypes: function(aTypes, aCallback) {
    let typesToGet = getAllAliasesForTypes(aTypes);

    XPIDatabase.getVisibleAddons(typesToGet, function(aAddons) {
      aCallback(aAddons.map(a => a.wrapper));
    });
  },

  /**
   * Obtain an Addon having the specified Sync GUID.
   *
   * @param  aGUID
   *         String GUID of add-on to retrieve
   * @param  aCallback
   *         A callback to pass the Addon to. Receives null if not found.
   */
  getAddonBySyncGUID: function(aGUID, aCallback) {
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
  getAddonsWithOperationsByTypes: function(aTypes, aCallback) {
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
  getInstallsByTypes: function(aTypes, aCallback) {
    let results = this.installs.slice(0);
    if (aTypes) {
      results = results.filter(install => {
        return aTypes.includes(getExternalType(install.type));
      });
    }

    aCallback(results.map(install => install.wrapper));
  },

  /**
   * Synchronously map a URI to the corresponding Addon ID.
   *
   * Mappable URIs are limited to in-application resources belonging to the
   * add-on, such as Javascript compartments, XUL windows, XBL bindings, etc.
   * but do not include URIs from meta data, such as the add-on homepage.
   *
   * @param  aURI
   *         nsIURI to map or null
   * @return string containing the Addon ID
   * @see    AddonManager.mapURIToAddonID
   * @see    amIAddonManager.mapURIToAddonID
   */
  mapURIToAddonID: function(aURI) {
    // Returns `null` instead of empty string if the URI can't be mapped.
    return AddonPathService.mapURIToAddonId(aURI) || null;
  },

  /**
   * Called when a new add-on has been enabled when only one add-on of that type
   * can be enabled.
   *
   * @param  aId
   *         The ID of the newly enabled add-on
   * @param  aType
   *         The type of the newly enabled add-on
   * @param  aPendingRestart
   *         true if the newly enabled add-on will only become enabled after a
   *         restart
   */
  addonChanged: function(aId, aType, aPendingRestart) {
    // We only care about themes in this provider
    if (aType != "theme")
      return;

    if (!aId) {
      // Fallback to the default theme when no theme was enabled
      this.enableDefaultTheme();
      return;
    }

    // Look for the previously enabled theme and find the internalName of the
    // currently selected theme
    let previousTheme = null;
    let newSkin = this.defaultSkin;
    let addons = XPIDatabase.getAddonsByType("theme");
    for (let theme of addons) {
      if (!theme.visible)
        return;
      if (theme.id == aId)
        newSkin = theme.internalName;
      else if (theme.userDisabled == false && !theme.pendingUninstall)
        previousTheme = theme;
    }

    if (aPendingRestart) {
      Services.prefs.setBoolPref(PREF_DSS_SWITCHPENDING, true);
      Services.prefs.setCharPref(PREF_DSS_SKIN_TO_SELECT, newSkin);
    }
    else if (newSkin == this.currentSkin) {
      try {
        Services.prefs.clearUserPref(PREF_DSS_SWITCHPENDING);
      }
      catch (e) { }
      try {
        Services.prefs.clearUserPref(PREF_DSS_SKIN_TO_SELECT);
      }
      catch (e) { }
    }
    else {
      Services.prefs.setCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN, newSkin);
      this.currentSkin = newSkin;
    }
    this.selectedSkin = newSkin;

    // Flush the preferences to disk so they don't get out of sync with the
    // database
    Services.prefs.savePrefFile(null);

    // Mark the previous theme as disabled. This won't cause recursion since
    // only enabled calls notifyAddonChanged.
    if (previousTheme)
      this.updateAddonDisabledState(previousTheme, true);
  },

  /**
   * Update the appDisabled property for all add-ons.
   */
  updateAddonAppDisabledStates: function() {
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
  updateAddonRepositoryData: function(aCallback) {
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
        AddonRepository.getCachedAddonByID(addon.id, aRepoAddon => {
          if (aRepoAddon) {
            logger.debug("updateAddonRepositoryData got info for " + addon.id);
            addon._repositoryAddon = aRepoAddon;
            addon.compatibilityOverrides = aRepoAddon.compatibilityOverrides;
            this.updateAddonDisabledState(addon);
          }

          notifyComplete();
        });
      }
    });
  },

  /**
   * When the previously selected theme is removed this method will be called
   * to enable the default theme.
   */
  enableDefaultTheme: function() {
    logger.debug("Activating default theme");
    let addon = XPIDatabase.getVisibleAddonForInternalName(this.defaultSkin);
    if (addon) {
      if (addon.userDisabled) {
        this.updateAddonDisabledState(addon, false);
      }
      else if (!this.extensionsActive) {
        // During startup we may end up trying to enable the default theme when
        // the database thinks it is already enabled (see f.e. bug 638847). In
        // this case just force the theme preferences to be correct
        Services.prefs.setCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN,
                                   addon.internalName);
        this.currentSkin = this.selectedSkin = addon.internalName;
        Preferences.reset(PREF_DSS_SKIN_TO_SELECT);
        Preferences.reset(PREF_DSS_SWITCHPENDING);
      }
      else {
        logger.warn("Attempting to activate an already active default theme");
      }
    }
    else {
      logger.warn("Unable to activate the default theme");
    }
  },

  onDebugConnectionChange: function(aEvent, aWhat, aConnection) {
    if (aWhat != "opened")
      return;

    for (let [id, val] of this.activeAddons) {
      aConnection.setAddonOptions(
        id, { global: val.debugGlobal || val.bootstrapScope });
    }
  },

  /**
   * Notified when a preference we're interested in has changed.
   *
   * @see nsIObserver
   */
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == NOTIFICATION_FLUSH_PERMISSIONS) {
      if (!aData || aData == XPI_PERMISSION) {
        this.importPermissions();
      }
      return;
    }
    else if (aTopic == NOTIFICATION_TOOLBOXPROCESS_LOADED) {
      Services.obs.removeObserver(this, NOTIFICATION_TOOLBOXPROCESS_LOADED, false);
      this._toolboxProcessLoaded = true;
      BrowserToolboxProcess.on("connectionchange",
                               this.onDebugConnectionChange.bind(this));
    }

    if (aTopic == "nsPref:changed") {
      switch (aData) {
      case PREF_EM_MIN_COMPAT_APP_VERSION:
        this.minCompatibleAppVersion = Preferences.get(PREF_EM_MIN_COMPAT_APP_VERSION,
                                                       null);
        this.updateAddonAppDisabledStates();
        break;
      case PREF_EM_MIN_COMPAT_PLATFORM_VERSION:
        this.minCompatiblePlatformVersion = Preferences.get(PREF_EM_MIN_COMPAT_PLATFORM_VERSION,
                                                            null);
        this.updateAddonAppDisabledStates();
        break;
      case PREF_XPI_SIGNATURES_REQUIRED:
        this.updateAddonAppDisabledStates();
        break;

      case PREF_E10S_ADDON_BLOCKLIST:
      case PREF_E10S_ADDON_POLICY:
        XPIDatabase.updateAddonsBlockingE10s();
        break;
      }
    }
  },

  /**
   * Determine if an add-on should be blocking e10s if enabled.
   *
   * @param  aAddon
   *         The add-on to test
   * @return true if enabling the add-on should block e10s
   */
  isBlockingE10s: function(aAddon) {
    if (aAddon.type != "extension" &&
        aAddon.type != "webextension" &&
        aAddon.type != "theme")
      return false;

    // The hotfix is exempt
    let hotfixID = Preferences.get(PREF_EM_HOTFIX_ID, undefined);
    if (hotfixID && hotfixID == aAddon.id)
      return false;

    // The default theme is exempt
    if (aAddon.type == "theme" &&
        aAddon.internalName == XPIProvider.defaultSkin)
      return false;

    // System add-ons are exempt
    let locName = aAddon._installLocation ? aAddon._installLocation.name
                                          : undefined;
    if (locName == KEY_APP_SYSTEM_DEFAULTS ||
        locName == KEY_APP_SYSTEM_ADDONS)
      return false;

    if (isAddonPartOfE10SRollout(aAddon)) {
      Preferences.set(PREF_E10S_HAS_NONEXEMPT_ADDON, true);
      return false;
    }

    logger.debug("Add-on " + aAddon.id + " blocks e10s rollout.");
    return true;
  },

  /**
   * In some cases having add-ons active blocks e10s but turning off e10s
   * requires a restart so some add-ons that are normally restartless will
   * require a restart to install or enable.
   *
   * @param  aAddon
   *         The add-on to test
   * @return true if enabling the add-on requires a restart
   */
  e10sBlocksEnabling: function(aAddon) {
    // If the preference isn't set then don't block anything
    if (!Preferences.get(PREF_E10S_BLOCK_ENABLE, false))
      return false;

    // If e10s isn't active then don't block anything
    if (!Services.appinfo.browserTabsRemoteAutostart)
      return false;

    return this.isBlockingE10s(aAddon);
  },

  /**
   * Tests whether enabling an add-on will require a restart.
   *
   * @param  aAddon
   *         The add-on to test
   * @return true if the operation requires a restart
   */
  enableRequiresRestart: function(aAddon) {
    // If the platform couldn't have activated extensions then we can make
    // changes without any restart.
    if (!this.extensionsActive)
      return false;

    // If the application is in safe mode then any change can be made without
    // restarting
    if (Services.appinfo.inSafeMode)
      return false;

    // Anything that is active is already enabled
    if (aAddon.active)
      return false;

    if (aAddon.type == "theme") {
      // If dynamic theme switching is enabled then switching themes does not
      // require a restart
      if (Preferences.get(PREF_EM_DSS_ENABLED))
        return false;

      // If the theme is already the theme in use then no restart is necessary.
      // This covers the case where the default theme is in use but a
      // lightweight theme is considered active.
      return aAddon.internalName != this.currentSkin;
    }

    if (this.e10sBlocksEnabling(aAddon))
      return true;

    return !aAddon.bootstrap;
  },

  /**
   * Tests whether disabling an add-on will require a restart.
   *
   * @param  aAddon
   *         The add-on to test
   * @return true if the operation requires a restart
   */
  disableRequiresRestart: function(aAddon) {
    // If the platform couldn't have activated up extensions then we can make
    // changes without any restart.
    if (!this.extensionsActive)
      return false;

    // If the application is in safe mode then any change can be made without
    // restarting
    if (Services.appinfo.inSafeMode)
      return false;

    // Anything that isn't active is already disabled
    if (!aAddon.active)
      return false;

    if (aAddon.type == "theme") {
      // If dynamic theme switching is enabled then switching themes does not
      // require a restart
      if (Preferences.get(PREF_EM_DSS_ENABLED))
        return false;

      // Non-default themes always require a restart to disable since it will
      // be switching from one theme to another or to the default theme and a
      // lightweight theme.
      if (aAddon.internalName != this.defaultSkin)
        return true;

      // The default theme requires a restart to disable if we are in the
      // process of switching to a different theme. Note that this makes the
      // disabled flag of operationsRequiringRestart incorrect for the default
      // theme (it will be false most of the time). Bug 520124 would be required
      // to fix it. For the UI this isn't a problem since we never try to
      // disable or uninstall the default theme.
      return this.selectedSkin != this.currentSkin;
    }

    return !aAddon.bootstrap;
  },

  /**
   * Tests whether installing an add-on will require a restart.
   *
   * @param  aAddon
   *         The add-on to test
   * @return true if the operation requires a restart
   */
  installRequiresRestart: function(aAddon) {
    // If the platform couldn't have activated up extensions then we can make
    // changes without any restart.
    if (!this.extensionsActive)
      return false;

    // If the application is in safe mode then any change can be made without
    // restarting
    if (Services.appinfo.inSafeMode)
      return false;

    // Add-ons that are already installed don't require a restart to install.
    // This wouldn't normally be called for an already installed add-on (except
    // for forming the operationsRequiringRestart flags) so is really here as
    // a safety measure.
    if (aAddon.inDatabase)
      return false;

    // If we have an AddonInstall for this add-on then we can see if there is
    // an existing installed add-on with the same ID
    if ("_install" in aAddon && aAddon._install) {
      // If there is an existing installed add-on and uninstalling it would
      // require a restart then installing the update will also require a
      // restart
      let existingAddon = aAddon._install.existingAddon;
      if (existingAddon && this.uninstallRequiresRestart(existingAddon))
        return true;
    }

    // If the add-on is not going to be active after installation then it
    // doesn't require a restart to install.
    if (aAddon.disabled)
      return false;

    if (this.e10sBlocksEnabling(aAddon))
      return true;

    // Themes will require a restart (even if dynamic switching is enabled due
    // to some caching issues) and non-bootstrapped add-ons will require a
    // restart
    return aAddon.type == "theme" || !aAddon.bootstrap;
  },

  /**
   * Tests whether uninstalling an add-on will require a restart.
   *
   * @param  aAddon
   *         The add-on to test
   * @return true if the operation requires a restart
   */
  uninstallRequiresRestart: function(aAddon) {
    // If the platform couldn't have activated up extensions then we can make
    // changes without any restart.
    if (!this.extensionsActive)
      return false;

    // If the application is in safe mode then any change can be made without
    // restarting
    if (Services.appinfo.inSafeMode)
      return false;

    // If the add-on can be disabled without a restart then it can also be
    // uninstalled without a restart
    return this.disableRequiresRestart(aAddon);
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
   * @param  aMultiprocessCompatible
   *         Boolean indicating whether the add-on is compatible with electrolysis.
   * @param  aRunInSafeMode
   *         Boolean indicating whether the add-on can run in safe mode.
   * @param  aDependencies
   *         An array of add-on IDs on which this add-on depends.
   * @return a JavaScript scope
   */
  loadBootstrapScope: function(aId, aFile, aVersion, aType,
                               aMultiprocessCompatible, aRunInSafeMode,
                               aDependencies) {
    // Mark the add-on as active for the crash reporter before loading
    this.bootstrappedAddons[aId] = {
      version: aVersion,
      type: aType,
      descriptor: aFile.persistentDescriptor,
      multiprocessCompatible: aMultiprocessCompatible,
      runInSafeMode: aRunInSafeMode,
      dependencies: aDependencies,
    };
    this.persistBootstrappedAddons();
    this.addAddonsToCrashReporter();

    this.activeAddons.set(aId, {
      debugGlobal: null,
      safeWrapper: null,
      bootstrapScope: null,
      // a Symbol passed to this add-on, which it can use to identify itself
      instanceID: Symbol(aId),
    });
    let activeAddon = this.activeAddons.get(aId);

    // Locales only contain chrome and can't have bootstrap scripts
    if (aType == "locale") {
      return;
    }

    logger.debug("Loading bootstrap scope from " + aFile.path);

    let principal = Cc["@mozilla.org/systemprincipal;1"].
                    createInstance(Ci.nsIPrincipal);
    if (!aMultiprocessCompatible && Preferences.get(PREF_INTERPOSITION_ENABLED, false)) {
      let interposition = Cc["@mozilla.org/addons/multiprocess-shims;1"].
        getService(Ci.nsIAddonInterposition);
      Cu.setAddonInterposition(aId, interposition);
      Cu.allowCPOWsInAddon(aId, true);
    }

    if (!aFile.exists()) {
      activeAddon.bootstrapScope =
        new Cu.Sandbox(principal, { sandboxName: aFile.path,
                                    wantGlobalProperties: ["indexedDB"],
                                    addonId: aId,
                                    metadata: { addonID: aId } });
      logger.error("Attempted to load bootstrap scope from missing directory " + aFile.path);
      return;
    }

    let uri = getURIForResourceInFile(aFile, "bootstrap.js").spec;
    if (aType == "dictionary")
      uri = "resource://gre/modules/addons/SpellCheckDictionaryBootstrap.js"
    else if (aType == "webextension")
      uri = "resource://gre/modules/addons/WebExtensionBootstrap.js"
    else if (aType == "apiextension")
      uri = "resource://gre/modules/addons/APIExtensionBootstrap.js"

    activeAddon.bootstrapScope =
      new Cu.Sandbox(principal, { sandboxName: uri,
                                  wantGlobalProperties: ["indexedDB"],
                                  addonId: aId,
                                  metadata: { addonID: aId, URI: uri } });

    let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                 createInstance(Ci.mozIJSSubScriptLoader);

    try {
      // Copy the reason values from the global object into the bootstrap scope.
      for (let name in BOOTSTRAP_REASONS)
        activeAddon.bootstrapScope[name] = BOOTSTRAP_REASONS[name];

      // Add other stuff that extensions want.
      const features = [ "Worker", "ChromeWorker" ];

      for (let feature of features)
        activeAddon.bootstrapScope[feature] = gGlobalScope[feature];

      // Define a console for the add-on
      activeAddon.bootstrapScope["console"] = new ConsoleAPI(
        { consoleID: "addon/" + aId });

      // As we don't want our caller to control the JS version used for the
      // bootstrap file, we run loadSubScript within the context of the
      // sandbox with the latest JS version set explicitly.
      activeAddon.bootstrapScope.__SCRIPT_URI_SPEC__ = uri;
      Components.utils.evalInSandbox(
        "Components.classes['@mozilla.org/moz/jssubscript-loader;1'] \
                   .createInstance(Components.interfaces.mozIJSSubScriptLoader) \
                   .loadSubScript(__SCRIPT_URI_SPEC__);",
                   activeAddon.bootstrapScope, "ECMAv5");
    }
    catch (e) {
      logger.warn("Error loading bootstrap.js for " + aId, e);
    }

    // Only access BrowserToolboxProcess if ToolboxProcess.jsm has been
    // initialized as otherwise, when it will be initialized, all addons'
    // globals will be added anyways
    if (this._toolboxProcessLoaded) {
      BrowserToolboxProcess.setAddonOptions(aId,
        { global: activeAddon.bootstrapScope });
    }
  },

  /**
   * Unloads a bootstrap scope by dropping all references to it and then
   * updating the list of active add-ons with the crash reporter.
   *
   * @param  aId
   *         The add-on's ID
   */
  unloadBootstrapScope: function(aId) {
    // In case the add-on was not multiprocess-compatible, deregister
    // any interpositions for it.
    Cu.setAddonInterposition(aId, null);
    Cu.allowCPOWsInAddon(aId, false);

    this.activeAddons.delete(aId);
    delete this.bootstrappedAddons[aId];
    this.persistBootstrappedAddons();
    this.addAddonsToCrashReporter();

    // Only access BrowserToolboxProcess if ToolboxProcess.jsm has been
    // initialized as otherwise, there won't be any addon globals added to it
    if (this._toolboxProcessLoaded) {
      BrowserToolboxProcess.setAddonOptions(aId, { global: null });
    }
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
  callBootstrapMethod: function(aAddon, aFile, aMethod, aReason, aExtraParams) {
    if (!aAddon.id || !aAddon.version || !aAddon.type) {
      logger.error(new Error("aAddon must include an id, version, and type"));
      return;
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
                                aAddon.multiprocessCompatible || false,
                                runInSafeMode, aAddon.dependencies);
        activeAddon = this.activeAddons.get(aAddon.id);
      }

      if (aMethod == "startup" || aMethod == "shutdown") {
        if (!aExtraParams) {
          aExtraParams = {};
        }
        aExtraParams["instanceID"] = this.activeAddons.get(aAddon.id).instanceID;
      }

      // Nothing to call for locales
      if (aAddon.type == "locale")
        return;

      let method = undefined;
      try {
        method = Components.utils.evalInSandbox(`${aMethod};`,
          activeAddon.bootstrapScope, "ECMAv5");
      }
      catch (e) {
        // An exception will be caught if the expected method is not defined.
        // That will be logged below.
      }

      if (!method) {
        logger.warn("Add-on " + aAddon.id + " is missing bootstrap method " + aMethod);
        return;
      }

      // Extensions are automatically deinitialized in the correct order at shutdown.
      if (aMethod == "shutdown" && aReason != BOOTSTRAP_REASONS.APP_SHUTDOWN) {
        activeAddon.disable = true;
        for (let addon of this.getDependentAddons(aAddon)) {
          if (addon.active)
            this.updateAddonDisabledState(addon);
        }
      }

      let params = {
        id: aAddon.id,
        version: aAddon.version,
        installPath: aFile.clone(),
        resourceURI: getURIForResourceInFile(aFile, "")
      };

      if (aExtraParams) {
        for (let key in aExtraParams) {
          params[key] = aExtraParams[key];
        }
      }

      logger.debug("Calling bootstrap method " + aMethod + " on " + aAddon.id + " version " +
                   aAddon.version);
      try {
        method(params, aReason);
      }
      catch (e) {
        logger.warn("Exception running bootstrap method " + aMethod + " on " + aAddon.id, e);
      }
    }
    finally {
      // Extensions are automatically initialized in the correct order at startup.
      if (aMethod == "startup" && aReason != BOOTSTRAP_REASONS.APP_STARTUP) {
        for (let addon of this.getDependentAddons(aAddon))
          this.updateAddonDisabledState(addon);
      }

      if (CHROME_TYPES.has(aAddon.type) && aMethod == "shutdown" && aReason != BOOTSTRAP_REASONS.APP_SHUTDOWN) {
        logger.debug("Removing manifest for " + aFile.path);
        Components.manager.removeBootstrappedManifestLocation(aFile);

        let manifest = getURIForResourceInFile(aFile, "chrome.manifest");
        for (let line of ChromeManifestParser.parseSync(manifest)) {
          if (line.type == "resource") {
            ResProtocolHandler.setSubstitution(line.args[0], null);
          }
        }
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
  updateAddonDisabledState: function(aAddon, aUserDisabled, aSoftDisabled) {
    if (!(aAddon.inDatabase))
      throw new Error("Can only update addon states for installed addons.");
    if (aUserDisabled !== undefined && aSoftDisabled !== undefined) {
      throw new Error("Cannot change userDisabled and softDisabled at the " +
                      "same time");
    }

    if (aUserDisabled === undefined) {
      aUserDisabled = aAddon.userDisabled;
    }
    else if (!aUserDisabled) {
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
      appDisabled: appDisabled,
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

    // Have we just gone back to the current state?
    if (isDisabled != aAddon.active) {
      AddonManagerPrivate.callAddonListeners("onOperationCancelled", wrapper);
    }
    else {
      if (isDisabled) {
        var needsRestart = this.disableRequiresRestart(aAddon);
        AddonManagerPrivate.callAddonListeners("onDisabling", wrapper,
                                               needsRestart);
      }
      else {
        needsRestart = this.enableRequiresRestart(aAddon);
        AddonManagerPrivate.callAddonListeners("onEnabling", wrapper,
                                               needsRestart);
      }

      if (!needsRestart) {
        XPIDatabase.updateAddonActive(aAddon, !isDisabled);

        if (isDisabled) {
          if (aAddon.bootstrap) {
            this.callBootstrapMethod(aAddon, aAddon._sourceBundle, "shutdown",
                                     BOOTSTRAP_REASONS.ADDON_DISABLE);
            this.unloadBootstrapScope(aAddon.id);
          }
          AddonManagerPrivate.callAddonListeners("onDisabled", wrapper);
        }
        else {
          if (aAddon.bootstrap) {
            this.callBootstrapMethod(aAddon, aAddon._sourceBundle, "startup",
                                     BOOTSTRAP_REASONS.ADDON_ENABLE);
          }
          AddonManagerPrivate.callAddonListeners("onEnabled", wrapper);
        }
      }
      else if (aAddon.bootstrap) {
        // Something blocked the restartless add-on from enabling or disabling
        // make sure it happens on the next startup
        if (isDisabled) {
          this.bootstrappedAddons[aAddon.id].disable = true;
        }
        else {
          this.bootstrappedAddons[aAddon.id] = {
            version: aAddon.version,
            type: aAddon.type,
            descriptor: aAddon._sourceBundle.persistentDescriptor,
            multiprocessCompatible: aAddon.multiprocessCompatible,
            runInSafeMode: canRunInSafeMode(aAddon),
            dependencies: aAddon.dependencies,
          };
          this.persistBootstrappedAddons();
        }
      }
    }

    // Sync with XPIStates.
    let xpiState = XPIStates.getAddon(aAddon.location, aAddon.id);
    if (xpiState) {
      xpiState.syncWithDB(aAddon);
      XPIStates.save();
    } else {
      // There should always be an xpiState
      logger.warn("No XPIState for ${id} in ${location}", aAddon);
    }

    // Notify any other providers that a new theme has been enabled
    if (aAddon.type == "theme" && !isDisabled)
      AddonManagerPrivate.notifyAddonChanged(aAddon.id, aAddon.type, needsRestart);

    return isDisabled;
  },

  /**
   * Uninstalls an add-on, immediately if possible or marks it as pending
   * uninstall if not.
   *
   * @param  aAddon
   *         The DBAddonInternal to uninstall
   * @param  aForcePending
   *         Force this addon into the pending uninstall state, even if
   *         it isn't marked as requiring a restart (used e.g. while the
   *         add-on manager is open and offering an "undo" button)
   * @throws if the addon cannot be uninstalled because it is in an install
   *         location that does not allow it
   */
  uninstallAddon: function(aAddon, aForcePending) {
    if (!(aAddon.inDatabase))
      throw new Error("Cannot uninstall addon " + aAddon.id + " because it is not installed");

    if (aAddon._installLocation.locked)
      throw new Error("Cannot uninstall addon " + aAddon.id
          + " from locked install location " + aAddon._installLocation.name);

    // Inactive add-ons don't require a restart to uninstall
    let requiresRestart = this.uninstallRequiresRestart(aAddon);

    // if makePending is true, we don't actually apply the uninstall,
    // we just mark the addon as having a pending uninstall
    let makePending = aForcePending || requiresRestart;

    if (makePending && aAddon.pendingUninstall)
      throw new Error("Add-on is already marked to be uninstalled");

    aAddon._hasResourceCache.clear();

    if (aAddon._updateCheck) {
      logger.debug("Cancel in-progress update check for " + aAddon.id);
      aAddon._updateCheck.cancel();
    }

    let wasPending = aAddon.pendingUninstall;

    if (makePending) {
      // We create an empty directory in the staging directory to indicate
      // that an uninstall is necessary on next startup. Temporary add-ons are
      // automatically uninstalled on shutdown anyway so there is no need to
      // do this for them.
      if (aAddon._installLocation.name != KEY_APP_TEMPORARY) {
        let stage = aAddon._installLocation.getStagingDir();
        stage.append(aAddon.id);
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
      // Passing makePending as the requiresRestart parameter is a little
      // strange as in some cases this operation can complete without a restart
      // so really this is now saying that the uninstall isn't going to happen
      // immediately but will happen later.
      AddonManagerPrivate.callAddonListeners("onUninstalling", wrapper,
                                             makePending);
    }

    // Reveal the highest priority add-on with the same ID
    function revealAddon(aAddon) {
      XPIDatabase.makeAddonVisible(aAddon);

      let wrappedAddon = aAddon.wrapper;
      AddonManagerPrivate.callAddonListeners("onInstalling", wrappedAddon, false);

      if (!aAddon.disabled && !XPIProvider.enableRequiresRestart(aAddon)) {
        XPIDatabase.updateAddonActive(aAddon, true);
      }

      if (aAddon.bootstrap) {
        XPIProvider.callBootstrapMethod(aAddon, aAddon._sourceBundle,
                                        "install", BOOTSTRAP_REASONS.ADDON_INSTALL);

        if (aAddon.active) {
          XPIProvider.callBootstrapMethod(aAddon, aAddon._sourceBundle,
                                          "startup", BOOTSTRAP_REASONS.ADDON_INSTALL);
        }
        else {
          XPIProvider.unloadBootstrapScope(aAddon.id);
        }
      }

      // We always send onInstalled even if a restart is required to enable
      // the revealed add-on
      AddonManagerPrivate.callAddonListeners("onInstalled", wrappedAddon);
    }

    function findAddonAndReveal(aId) {
      let [locationName, ] = XPIStates.findAddon(aId);
      if (locationName) {
        XPIDatabase.getAddonInLocation(aId, locationName, revealAddon);
      }
    }

    if (!makePending) {
      if (aAddon.bootstrap) {
        if (aAddon.active) {
          this.callBootstrapMethod(aAddon, aAddon._sourceBundle, "shutdown",
                                   BOOTSTRAP_REASONS.ADDON_UNINSTALL);
        }

        this.callBootstrapMethod(aAddon, aAddon._sourceBundle, "uninstall",
                                 BOOTSTRAP_REASONS.ADDON_UNINSTALL);
        this.unloadBootstrapScope(aAddon.id);
        flushChromeCaches();
      }
      aAddon._installLocation.uninstallAddon(aAddon.id);
      XPIDatabase.removeAddonMetadata(aAddon);
      XPIStates.removeAddon(aAddon.location, aAddon.id);
      AddonManagerPrivate.callAddonListeners("onUninstalled", wrapper);

      findAddonAndReveal(aAddon.id);
    }
    else if (aAddon.bootstrap && aAddon.active && !this.disableRequiresRestart(aAddon)) {
      this.callBootstrapMethod(aAddon, aAddon._sourceBundle, "shutdown",
                               BOOTSTRAP_REASONS.ADDON_UNINSTALL);
      this.unloadBootstrapScope(aAddon.id);
      XPIDatabase.updateAddonActive(aAddon, false);
    }

    // Notify any other providers that a new theme has been enabled
    if (aAddon.type == "theme" && aAddon.active)
      AddonManagerPrivate.notifyAddonChanged(null, aAddon.type, requiresRestart);
  },

  /**
   * Cancels the pending uninstall of an add-on.
   *
   * @param  aAddon
   *         The DBAddonInternal to cancel uninstall for
   */
  cancelUninstallAddon: function(aAddon) {
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

    Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);

    // TODO hide hidden add-ons (bug 557710)
    let wrapper = aAddon.wrapper;
    AddonManagerPrivate.callAddonListeners("onOperationCancelled", wrapper);

    if (aAddon.bootstrap && !aAddon.disabled && !this.enableRequiresRestart(aAddon)) {
      this.callBootstrapMethod(aAddon, aAddon._sourceBundle, "startup",
                               BOOTSTRAP_REASONS.ADDON_INSTALL);
      XPIDatabase.updateAddonActive(aAddon, true);
    }

    // Notify any other providers that this theme is now enabled again.
    if (aAddon.type == "theme" && aAddon.active)
      AddonManagerPrivate.notifyAddonChanged(aAddon.id, aAddon.type, false);
  }
};

function getHashStringForCrypto(aCrypto) {
  // return the two-digit hexadecimal code for a byte
  let toHexString = charCode => ("0" + charCode.toString(16)).slice(-2);

  // convert the binary hash data to a hex string.
  let binary = aCrypto.finish(false);
  let hash = Array.from(binary, c => toHexString(c.charCodeAt(0)))
  return hash.join("").toLowerCase();
}

/**
 * Instantiates an AddonInstall.
 *
 * @param  aInstallLocation
 *         The install location the add-on will be installed into
 * @param  aUrl
 *         The nsIURL to get the add-on from. If this is an nsIFileURL then
 *         the add-on will not need to be downloaded
 * @param  aHash
 *         An optional hash for the add-on
 * @param  aReleaseNotesURI
 *         An optional nsIURI of release notes for the add-on
 * @param  aExistingAddon
 *         The add-on this install will update if known
 * @param  aBrowser
 *         The browser performing the install
 * @throws if the url is the url of a local file and the hash does not match
 *         or the add-on does not contain an valid install manifest
 */
function AddonInstall(aInstallLocation, aUrl, aHash, aReleaseNotesURI,
                      aExistingAddon, aBrowser) {
  this.wrapper = new AddonInstallWrapper(this);
  this.installLocation = aInstallLocation;
  this.sourceURI = aUrl;
  this.releaseNotesURI = aReleaseNotesURI;
  if (aHash) {
    let hashSplit = aHash.toLowerCase().split(":");
    this.originalHash = {
      algorithm: hashSplit[0],
      data: hashSplit[1]
    };
  }
  this.hash = this.originalHash;
  this.browser = aBrowser;
  this.listeners = [];
  this.icons = {};
  this.existingAddon = aExistingAddon;
  this.error = 0;
  this.window = aBrowser ? aBrowser.contentWindow : null;

  // Giving each instance of AddonInstall a reference to the logger.
  this.logger = logger;
}

AddonInstall.prototype = {
  installLocation: null,
  wrapper: null,
  stream: null,
  crypto: null,
  originalHash: null,
  hash: null,
  browser: null,
  badCertHandler: null,
  listeners: null,
  restartDownload: false,

  name: null,
  type: null,
  version: null,
  icons: null,
  releaseNotesURI: null,
  sourceURI: null,
  file: null,
  ownsTempFile: false,
  certificate: null,
  certName: null,

  linkedInstalls: null,
  existingAddon: null,
  addon: null,

  state: null,
  error: null,
  progress: null,
  maxProgress: null,

  /**
   * Initialises this install to be a staged install waiting to be applied
   *
   * @param  aManifest
   *         The cached manifest for the staged install
   */
  initStagedInstall: function(aManifest) {
    this.name = aManifest.name;
    this.type = aManifest.type;
    this.version = aManifest.version;
    this.icons = aManifest.icons;
    this.releaseNotesURI = aManifest.releaseNotesURI ?
                           NetUtil.newURI(aManifest.releaseNotesURI) :
                           null
    this.sourceURI = aManifest.sourceURI ?
                     NetUtil.newURI(aManifest.sourceURI) :
                     null;
    this.file = null;
    this.addon = aManifest;

    this.state = AddonManager.STATE_INSTALLED;

    XPIProvider.installs.push(this);
  },

  /**
   * Initialises this install to be an install from a local file.
   *
   * @param  aCallback
   *         The callback to pass the initialised AddonInstall to
   */
  initLocalInstall: function(aCallback) {
    aCallback = makeSafe(aCallback);
    this.file = this.sourceURI.QueryInterface(Ci.nsIFileURL).file;

    if (!this.file.exists()) {
      logger.warn("XPI file " + this.file.path + " does not exist");
      this.state = AddonManager.STATE_DOWNLOAD_FAILED;
      this.error = AddonManager.ERROR_NETWORK_FAILURE;
      aCallback(this);
      return;
    }

    this.state = AddonManager.STATE_DOWNLOADED;
    this.progress = this.file.fileSize;
    this.maxProgress = this.file.fileSize;

    if (this.hash) {
      let crypto = Cc["@mozilla.org/security/hash;1"].
                   createInstance(Ci.nsICryptoHash);
      try {
        crypto.initWithString(this.hash.algorithm);
      }
      catch (e) {
        logger.warn("Unknown hash algorithm '" + this.hash.algorithm + "' for addon " + this.sourceURI.spec, e);
        this.state = AddonManager.STATE_DOWNLOAD_FAILED;
        this.error = AddonManager.ERROR_INCORRECT_HASH;
        aCallback(this);
        return;
      }

      let fis = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
      fis.init(this.file, -1, -1, false);
      crypto.updateFromStream(fis, this.file.fileSize);
      let calculatedHash = getHashStringForCrypto(crypto);
      if (calculatedHash != this.hash.data) {
        logger.warn("File hash (" + calculatedHash + ") did not match provided hash (" +
             this.hash.data + ")");
        this.state = AddonManager.STATE_DOWNLOAD_FAILED;
        this.error = AddonManager.ERROR_INCORRECT_HASH;
        aCallback(this);
        return;
      }
    }

    this.loadManifest(this.file).then(() => {
      XPIDatabase.getVisibleAddonForID(this.addon.id, aAddon => {
        this.existingAddon = aAddon;
        if (aAddon)
          applyBlocklistChanges(aAddon, this.addon);
        this.addon.updateDate = Date.now();
        this.addon.installDate = aAddon ? aAddon.installDate : this.addon.updateDate;

        if (!this.addon.isCompatible) {
          // TODO Should we send some event here?
          this.state = AddonManager.STATE_CHECKING;
          new UpdateChecker(this.addon, {
            onUpdateFinished: aAddon => {
              this.state = AddonManager.STATE_DOWNLOADED;
              XPIProvider.installs.push(this);
              AddonManagerPrivate.callInstallListeners("onNewInstall",
                                                       this.listeners,
                                                       this.wrapper);

              aCallback(this);
            }
          }, AddonManager.UPDATE_WHEN_ADDON_INSTALLED);
        }
        else {
          XPIProvider.installs.push(this);
          AddonManagerPrivate.callInstallListeners("onNewInstall",
                                                   this.listeners,
                                                   this.wrapper);

          aCallback(this);
        }
      });
    }, ([error, message]) => {
      logger.warn("Invalid XPI", message);
      this.state = AddonManager.STATE_DOWNLOAD_FAILED;
      this.error = error;
      AddonManagerPrivate.callInstallListeners("onNewInstall",
                                               this.listeners,
                                               this.wrapper);

      aCallback(this);
    });
  },

  /**
   * Initialises this install to be a download from a remote url.
   *
   * @param  aCallback
   *         The callback to pass the initialised AddonInstall to
   * @param  aName
   *         An optional name for the add-on
   * @param  aType
   *         An optional type for the add-on
   * @param  aIcons
   *         Optional icons for the add-on
   * @param  aVersion
   *         An optional version for the add-on
   */
  initAvailableDownload: function(aName, aType, aIcons, aVersion, aCallback) {
    this.state = AddonManager.STATE_AVAILABLE;
    this.name = aName;
    this.type = aType;
    this.version = aVersion;
    this.icons = aIcons;
    this.progress = 0;
    this.maxProgress = -1;

    XPIProvider.installs.push(this);
    AddonManagerPrivate.callInstallListeners("onNewInstall", this.listeners,
                                             this.wrapper);

    makeSafe(aCallback)(this);
  },

  /**
   * Starts installation of this add-on from whatever state it is currently at
   * if possible.
   *
   * @throws if installation cannot proceed from the current state
   */
  install: function() {
    switch (this.state) {
    case AddonManager.STATE_AVAILABLE:
      this.startDownload();
      break;
    case AddonManager.STATE_DOWNLOADED:
      this.startInstall();
      break;
    case AddonManager.STATE_DOWNLOAD_FAILED:
    case AddonManager.STATE_INSTALL_FAILED:
    case AddonManager.STATE_CANCELLED:
      this.removeTemporaryFile();
      this.state = AddonManager.STATE_AVAILABLE;
      this.error = 0;
      this.progress = 0;
      this.maxProgress = -1;
      this.hash = this.originalHash;
      XPIProvider.installs.push(this);
      this.startDownload();
      break;
    case AddonManager.STATE_POSTPONED:
      logger.debug(`Postponing install of ${this.addon.id}`);
      break;
    case AddonManager.STATE_DOWNLOADING:
    case AddonManager.STATE_CHECKING:
    case AddonManager.STATE_INSTALLING:
      // Installation is already running
      return;
    default:
      throw new Error("Cannot start installing from this state");
    }
  },

  /**
   * Cancels installation of this add-on.
   *
   * @throws if installation cannot be cancelled from the current state
   */
  cancel: function() {
    switch (this.state) {
    case AddonManager.STATE_DOWNLOADING:
      if (this.channel) {
        logger.debug("Cancelling download of " + this.sourceURI.spec);
        this.channel.cancel(Cr.NS_BINDING_ABORTED);
      }
      break;
    case AddonManager.STATE_AVAILABLE:
    case AddonManager.STATE_DOWNLOADED:
      logger.debug("Cancelling download of " + this.sourceURI.spec);
      this.state = AddonManager.STATE_CANCELLED;
      XPIProvider.removeActiveInstall(this);
      AddonManagerPrivate.callInstallListeners("onDownloadCancelled",
                                               this.listeners, this.wrapper);
      this.removeTemporaryFile();
      break;
    case AddonManager.STATE_INSTALLED:
      logger.debug("Cancelling install of " + this.addon.id);
      let xpi = this.installLocation.getStagingDir();
      xpi.append(this.addon.id + ".xpi");
      flushJarCache(xpi);
      this.installLocation.cleanStagingDir([this.addon.id, this.addon.id + ".xpi",
                                            this.addon.id + ".json"]);
      this.state = AddonManager.STATE_CANCELLED;
      XPIProvider.removeActiveInstall(this);

      if (this.existingAddon) {
        delete this.existingAddon.pendingUpgrade;
        this.existingAddon.pendingUpgrade = null;
      }

      AddonManagerPrivate.callAddonListeners("onOperationCancelled", this.addon.wrapper);

      AddonManagerPrivate.callInstallListeners("onInstallCancelled",
                                               this.listeners, this.wrapper);
      break;
    case AddonManager.STATE_POSTPONED:
      logger.debug(`Cancelling postponed install of ${this.addon.id}`);
      this.state = AddonManager.STATE_CANCELLED;
      XPIProvider.removeActiveInstall(this);
      AddonManagerPrivate.callInstallListeners("onInstallCancelled",
                                               this.listeners, this.wrapper);
      this.removeTemporaryFile();

      let stagingDir = this.installLocation.getStagingDir();
      let stagedAddon = stagingDir.clone();

      this.unstageInstall(stagedAddon);
    default:
      throw new Error("Cannot cancel install of " + this.sourceURI.spec +
                      " from this state (" + this.state + ")");
    }
  },

  /**
   * Adds an InstallListener for this instance if the listener is not already
   * registered.
   *
   * @param  aListener
   *         The InstallListener to add
   */
  addListener: function(aListener) {
    if (!this.listeners.some(function(i) { return i == aListener; }))
      this.listeners.push(aListener);
  },

  /**
   * Removes an InstallListener for this instance if it is registered.
   *
   * @param  aListener
   *         The InstallListener to remove
   */
  removeListener: function(aListener) {
    this.listeners = this.listeners.filter(function(i) {
      return i != aListener;
    });
  },

  /**
   * Removes the temporary file owned by this AddonInstall if there is one.
   */
  removeTemporaryFile: function() {
    // Only proceed if this AddonInstall owns its XPI file
    if (!this.ownsTempFile) {
      this.logger.debug("removeTemporaryFile: " + this.sourceURI.spec + " does not own temp file");
      return;
    }

    try {
      this.logger.debug("removeTemporaryFile: " + this.sourceURI.spec + " removing temp file " +
          this.file.path);
      this.file.remove(true);
      this.ownsTempFile = false;
    }
    catch (e) {
      this.logger.warn("Failed to remove temporary file " + this.file.path + " for addon " +
          this.sourceURI.spec,
          e);
    }
  },

  /**
   * Updates the sourceURI and releaseNotesURI values on the Addon being
   * installed by this AddonInstall instance.
   */
  updateAddonURIs: function() {
    this.addon.sourceURI = this.sourceURI.spec;
    if (this.releaseNotesURI)
      this.addon.releaseNotesURI = this.releaseNotesURI.spec;
  },

  /**
   * Fills out linkedInstalls with AddonInstall instances for the other files
   * in a multi-package XPI.
   *
   * @param  aFiles
   *         An array of { entryName, file } for each remaining file from the
   *         multi-package XPI.
   */
  _createLinkedInstalls: Task.async(function*(aFiles) {
    if (aFiles.length == 0)
      return;

    // Create new AddonInstall instances for every remaining file
    if (!this.linkedInstalls)
      this.linkedInstalls = [];

    for (let { entryName, file } of aFiles) {
      logger.debug("Creating linked install from " + entryName);
      let install = yield new Promise(resolve => AddonInstall.createInstall(resolve, file));

      // Make the new install own its temporary file
      install.ownsTempFile = true;

      this.linkedInstalls.push(install);

      // If one of the internal XPIs was multipackage then move its linked
      // installs to the outer install
      if (install.linkedInstalls) {
        this.linkedInstalls.push(...install.linkedInstalls);
        install.linkedInstalls = null;
      }

      install.sourceURI = this.sourceURI;
      install.releaseNotesURI = this.releaseNotesURI;
      if (install.state != AddonManager.STATE_DOWNLOAD_FAILED)
        install.updateAddonURIs();
    }
  }),

  /**
   * Loads add-on manifests from a multi-package XPI file. Each of the
   * XPI and JAR files contained in the XPI will be extracted. Any that
   * do not contain valid add-ons will be ignored. The first valid add-on will
   * be installed by this AddonInstall instance, the rest will have new
   * AddonInstall instances created for them.
   *
   * @param  aZipReader
   *         An open nsIZipReader for the multi-package XPI's files. This will
   *         be closed before this method returns.
   */
  _loadMultipackageManifests: Task.async(function*(aZipReader) {
    let files = [];
    let entries = aZipReader.findEntries("(*.[Xx][Pp][Ii]|*.[Jj][Aa][Rr])");
    while (entries.hasMore()) {
      let entryName = entries.getNext();
      let file = getTemporaryFile();
      try {
        aZipReader.extract(entryName, file);
        files.push({ entryName, file });
      }
      catch (e) {
        logger.warn("Failed to extract " + entryName + " from multi-package " +
             "XPI", e);
        file.remove(false);
      }
    }

    aZipReader.close();

    if (files.length == 0) {
      return Promise.reject([AddonManager.ERROR_CORRUPT_FILE,
                             "Multi-package XPI does not contain any packages to install"]);
    }

    let addon = null;

    // Find the first file that is a valid install and use it for
    // the add-on that this AddonInstall instance will install.
    for (let { entryName, file } of files) {
      this.removeTemporaryFile();
      try {
        yield this.loadManifest(file);
        logger.debug("Base multi-package XPI install came from " + entryName);
        this.file = file;
        this.ownsTempFile = true;

        yield this._createLinkedInstalls(files.filter(f => f.file != file));
        return undefined;
      }
      catch (e) {
        // _createLinkedInstalls will log errors when it tries to process this
        // file
      }
    }

    // No valid add-on was found, delete all the temporary files
    for (let { file } of files) {
      try {
        file.remove(true);
      } catch (e) {
        this.logger.warn("Could not remove temp file " + file.path);
      }
    }

    return Promise.reject([AddonManager.ERROR_CORRUPT_FILE,
                           "Multi-package XPI does not contain any valid packages to install"]);
  }),

  /**
   * Called after the add-on is a local file and the signature and install
   * manifest can be read.
   *
   * @param  aCallback
   *         A function to call when the manifest has been loaded
   * @throws if the add-on does not contain a valid install manifest or the
   *         XPI is incorrectly signed
   */
  loadManifest: Task.async(function*(file) {
    let zipreader = Cc["@mozilla.org/libjar/zip-reader;1"].
                    createInstance(Ci.nsIZipReader);
    try {
      zipreader.open(file);
    }
    catch (e) {
      zipreader.close();
      return Promise.reject([AddonManager.ERROR_CORRUPT_FILE, e]);
    }

    try {
      // loadManifestFromZipReader performs the certificate verification for us
      this.addon = yield loadManifestFromZipReader(zipreader, this.installLocation);
    }
    catch (e) {
      zipreader.close();
      return Promise.reject([AddonManager.ERROR_CORRUPT_FILE, e]);
    }

    // A multi-package XPI is a container, the add-ons it holds each
    // have their own id.  Everything else had better have an id here.
    if (!this.addon.id && this.addon.type != "multipackage") {
      let err = new Error(`Cannot find id for addon ${file.path}`);
      return Promise.reject([AddonManager.ERROR_CORRUPT_FILE, err]);
    }

    if (mustSign(this.addon.type)) {
      if (this.addon.signedState <= AddonManager.SIGNEDSTATE_MISSING) {
        // This add-on isn't properly signed by a signature that chains to the
        // trusted root.
        let state = this.addon.signedState;
        this.addon = null;
        zipreader.close();

        if (state == AddonManager.SIGNEDSTATE_MISSING)
          return Promise.reject([AddonManager.ERROR_SIGNEDSTATE_REQUIRED,
                                 "signature is required but missing"])

        return Promise.reject([AddonManager.ERROR_CORRUPT_FILE,
                               "signature verification failed"])
      }
    }
    else if (this.addon.signedState == AddonManager.SIGNEDSTATE_UNKNOWN ||
             this.addon.signedState == AddonManager.SIGNEDSTATE_NOT_REQUIRED) {
      // Check object signing certificate, if any
      let x509 = zipreader.getSigningCert(null);
      if (x509) {
        logger.debug("Verifying XPI signature");
        if (verifyZipSigning(zipreader, x509)) {
          this.certificate = x509;
          if (this.certificate.commonName.length > 0) {
            this.certName = this.certificate.commonName;
          } else {
            this.certName = this.certificate.organization;
          }
        } else {
          zipreader.close();
          return Promise.reject([AddonManager.ERROR_CORRUPT_FILE,
                                 "XPI is incorrectly signed"]);
        }
      }
    }

    if (this.existingAddon && this.existingAddon.type == "webextension" &&
        this.addon.type != "webextension") {
      zipreader.close();
      return Promise.reject([AddonManager.ERROR_UNEXPECTED_ADDON_TYPE,
                             "WebExtensions may not be upated to other extension types"]);
    }

    if (this.addon.type == "multipackage")
      return this._loadMultipackageManifests(zipreader);

    zipreader.close();

    this.updateAddonURIs();

    this.addon._install = this;
    this.name = this.addon.selectedLocale.name;
    this.type = this.addon.type;
    this.version = this.addon.version;

    // Setting the iconURL to something inside the XPI locks the XPI and
    // makes it impossible to delete on Windows.

    // Try to load from the existing cache first
    let repoAddon = yield new Promise(resolve => AddonRepository.getCachedAddonByID(this.addon.id, resolve));

    // It wasn't there so try to re-download it
    if (!repoAddon) {
      yield new Promise(resolve => AddonRepository.cacheAddons([this.addon.id], resolve));
      repoAddon = yield new Promise(resolve => AddonRepository.getCachedAddonByID(this.addon.id, resolve));
    }

    this.addon._repositoryAddon = repoAddon;
    this.name = this.name || this.addon._repositoryAddon.name;
    this.addon.compatibilityOverrides = repoAddon ?
                                    repoAddon.compatibilityOverrides :
                                    null;
    this.addon.appDisabled = !isUsableAddon(this.addon);
    return undefined;
  }),

  observe: function(aSubject, aTopic, aData) {
    // Network is going offline
    this.cancel();
  },

  /**
   * Starts downloading the add-on's XPI file.
   */
  startDownload: function() {
    this.state = AddonManager.STATE_DOWNLOADING;
    if (!AddonManagerPrivate.callInstallListeners("onDownloadStarted",
                                                  this.listeners, this.wrapper)) {
      logger.debug("onDownloadStarted listeners cancelled installation of addon " + this.sourceURI.spec);
      this.state = AddonManager.STATE_CANCELLED;
      XPIProvider.removeActiveInstall(this);
      AddonManagerPrivate.callInstallListeners("onDownloadCancelled",
                                               this.listeners, this.wrapper)
      return;
    }

    // If a listener changed our state then do not proceed with the download
    if (this.state != AddonManager.STATE_DOWNLOADING)
      return;

    if (this.channel) {
      // A previous download attempt hasn't finished cleaning up yet, signal
      // that it should restart when complete
      logger.debug("Waiting for previous download to complete");
      this.restartDownload = true;
      return;
    }

    this.openChannel();
  },

  openChannel: function() {
    this.restartDownload = false;

    try {
      this.file = getTemporaryFile();
      this.ownsTempFile = true;
      this.stream = Cc["@mozilla.org/network/file-output-stream;1"].
                    createInstance(Ci.nsIFileOutputStream);
      this.stream.init(this.file, FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE |
                       FileUtils.MODE_TRUNCATE, FileUtils.PERMS_FILE, 0);
    }
    catch (e) {
      logger.warn("Failed to start download for addon " + this.sourceURI.spec, e);
      this.state = AddonManager.STATE_DOWNLOAD_FAILED;
      this.error = AddonManager.ERROR_FILE_ACCESS;
      XPIProvider.removeActiveInstall(this);
      AddonManagerPrivate.callInstallListeners("onDownloadFailed",
                                               this.listeners, this.wrapper);
      return;
    }

    let listener = Cc["@mozilla.org/network/stream-listener-tee;1"].
                   createInstance(Ci.nsIStreamListenerTee);
    listener.init(this, this.stream);
    try {
      let requireBuiltIn = Preferences.get(PREF_INSTALL_REQUIREBUILTINCERTS, true);
      this.badCertHandler = new CertUtils.BadCertHandler(!requireBuiltIn);

      this.channel = NetUtil.newChannel({
        uri: this.sourceURI,
        loadUsingSystemPrincipal: true
      });
      this.channel.notificationCallbacks = this;
      if (this.channel instanceof Ci.nsIHttpChannel) {
        this.channel.setRequestHeader("Moz-XPI-Update", "1", true);
        if (this.channel instanceof Ci.nsIHttpChannelInternal)
          this.channel.forceAllowThirdPartyCookie = true;
      }
      this.channel.asyncOpen2(listener);

      Services.obs.addObserver(this, "network:offline-about-to-go-offline", false);
    }
    catch (e) {
      logger.warn("Failed to start download for addon " + this.sourceURI.spec, e);
      this.state = AddonManager.STATE_DOWNLOAD_FAILED;
      this.error = AddonManager.ERROR_NETWORK_FAILURE;
      XPIProvider.removeActiveInstall(this);
      AddonManagerPrivate.callInstallListeners("onDownloadFailed",
                                               this.listeners, this.wrapper);
    }
  },

  /**
   * Update the crypto hasher with the new data and call the progress listeners.
   *
   * @see nsIStreamListener
   */
  onDataAvailable: function(aRequest, aContext, aInputstream,
                                               aOffset, aCount) {
    this.crypto.updateFromStream(aInputstream, aCount);
    this.progress += aCount;
    if (!AddonManagerPrivate.callInstallListeners("onDownloadProgress",
                                                  this.listeners, this.wrapper)) {
      // TODO cancel the download and make it available again (bug 553024)
    }
  },

  /**
   * Check the redirect response for a hash of the target XPI and verify that
   * we don't end up on an insecure channel.
   *
   * @see nsIChannelEventSink
   */
  asyncOnChannelRedirect: function(aOldChannel, aNewChannel, aFlags, aCallback) {
    if (!this.hash && aOldChannel.originalURI.schemeIs("https") &&
        aOldChannel instanceof Ci.nsIHttpChannel) {
      try {
        let hashStr = aOldChannel.getResponseHeader("X-Target-Digest");
        let hashSplit = hashStr.toLowerCase().split(":");
        this.hash = {
          algorithm: hashSplit[0],
          data: hashSplit[1]
        };
      }
      catch (e) {
      }
    }

    // Verify that we don't end up on an insecure channel if we haven't got a
    // hash to verify with (see bug 537761 for discussion)
    if (!this.hash)
      this.badCertHandler.asyncOnChannelRedirect(aOldChannel, aNewChannel, aFlags, aCallback);
    else
      aCallback.onRedirectVerifyCallback(Cr.NS_OK);

    this.channel = aNewChannel;
  },

  /**
   * This is the first chance to get at real headers on the channel.
   *
   * @see nsIStreamListener
   */
  onStartRequest: function(aRequest, aContext) {
    this.crypto = Cc["@mozilla.org/security/hash;1"].
                  createInstance(Ci.nsICryptoHash);
    if (this.hash) {
      try {
        this.crypto.initWithString(this.hash.algorithm);
      }
      catch (e) {
        logger.warn("Unknown hash algorithm '" + this.hash.algorithm + "' for addon " + this.sourceURI.spec, e);
        this.state = AddonManager.STATE_DOWNLOAD_FAILED;
        this.error = AddonManager.ERROR_INCORRECT_HASH;
        XPIProvider.removeActiveInstall(this);
        AddonManagerPrivate.callInstallListeners("onDownloadFailed",
                                                 this.listeners, this.wrapper);
        aRequest.cancel(Cr.NS_BINDING_ABORTED);
        return;
      }
    }
    else {
      // We always need something to consume data from the inputstream passed
      // to onDataAvailable so just create a dummy cryptohasher to do that.
      this.crypto.initWithString("sha1");
    }

    this.progress = 0;
    if (aRequest instanceof Ci.nsIChannel) {
      try {
        this.maxProgress = aRequest.contentLength;
      }
      catch (e) {
      }
      logger.debug("Download started for " + this.sourceURI.spec + " to file " +
          this.file.path);
    }
  },

  /**
   * The download is complete.
   *
   * @see nsIStreamListener
   */
  onStopRequest: function(aRequest, aContext, aStatus) {
    this.stream.close();
    this.channel = null;
    this.badCerthandler = null;
    Services.obs.removeObserver(this, "network:offline-about-to-go-offline");

    // If the download was cancelled then update the state and send events
    if (aStatus == Cr.NS_BINDING_ABORTED) {
      if (this.state == AddonManager.STATE_DOWNLOADING) {
        logger.debug("Cancelled download of " + this.sourceURI.spec);
        this.state = AddonManager.STATE_CANCELLED;
        XPIProvider.removeActiveInstall(this);
        AddonManagerPrivate.callInstallListeners("onDownloadCancelled",
                                                 this.listeners, this.wrapper);
        // If a listener restarted the download then there is no need to
        // remove the temporary file
        if (this.state != AddonManager.STATE_CANCELLED)
          return;
      }

      this.removeTemporaryFile();
      if (this.restartDownload)
        this.openChannel();
      return;
    }

    logger.debug("Download of " + this.sourceURI.spec + " completed.");

    if (Components.isSuccessCode(aStatus)) {
      if (!(aRequest instanceof Ci.nsIHttpChannel) || aRequest.requestSucceeded) {
        if (!this.hash && (aRequest instanceof Ci.nsIChannel)) {
          try {
            CertUtils.checkCert(aRequest,
                                !Preferences.get(PREF_INSTALL_REQUIREBUILTINCERTS, true));
          }
          catch (e) {
            this.downloadFailed(AddonManager.ERROR_NETWORK_FAILURE, e);
            return;
          }
        }

        // convert the binary hash data to a hex string.
        let calculatedHash = getHashStringForCrypto(this.crypto);
        this.crypto = null;
        if (this.hash && calculatedHash != this.hash.data) {
          this.downloadFailed(AddonManager.ERROR_INCORRECT_HASH,
                              "Downloaded file hash (" + calculatedHash +
                              ") did not match provided hash (" + this.hash.data + ")");
          return;
        }

        this.loadManifest(this.file).then(() => {
          if (this.addon.isCompatible) {
            this.downloadCompleted();
          }
          else {
            // TODO Should we send some event here (bug 557716)?
            this.state = AddonManager.STATE_CHECKING;
            new UpdateChecker(this.addon, {
              onUpdateFinished: aAddon => this.downloadCompleted(),
            }, AddonManager.UPDATE_WHEN_ADDON_INSTALLED);
          }
        }, ([error, message]) => {
          this.downloadFailed(error, message);
        });
      }
      else {
        if (aRequest instanceof Ci.nsIHttpChannel)
          this.downloadFailed(AddonManager.ERROR_NETWORK_FAILURE,
                              aRequest.responseStatus + " " +
                              aRequest.responseStatusText);
        else
          this.downloadFailed(AddonManager.ERROR_NETWORK_FAILURE, aStatus);
      }
    }
    else {
      this.downloadFailed(AddonManager.ERROR_NETWORK_FAILURE, aStatus);
    }
  },

  /**
   * Notify listeners that the download failed.
   *
   * @param  aReason
   *         Something to log about the failure
   * @param  error
   *         The error code to pass to the listeners
   */
  downloadFailed: function(aReason, aError) {
    logger.warn("Download of " + this.sourceURI.spec + " failed", aError);
    this.state = AddonManager.STATE_DOWNLOAD_FAILED;
    this.error = aReason;
    XPIProvider.removeActiveInstall(this);
    AddonManagerPrivate.callInstallListeners("onDownloadFailed", this.listeners,
                                             this.wrapper);

    // If the listener hasn't restarted the download then remove any temporary
    // file
    if (this.state == AddonManager.STATE_DOWNLOAD_FAILED) {
      logger.debug("downloadFailed: removing temp file for " + this.sourceURI.spec);
      this.removeTemporaryFile();
    }
    else
      logger.debug("downloadFailed: listener changed AddonInstall state for " +
          this.sourceURI.spec + " to " + this.state);
  },

  /**
   * Notify listeners that the download completed.
   */
  downloadCompleted: function() {
    XPIDatabase.getVisibleAddonForID(this.addon.id, aAddon => {
      if (aAddon)
        this.existingAddon = aAddon;

      this.state = AddonManager.STATE_DOWNLOADED;
      this.addon.updateDate = Date.now();

      if (this.existingAddon) {
        this.addon.existingAddonID = this.existingAddon.id;
        this.addon.installDate = this.existingAddon.installDate;
        applyBlocklistChanges(this.existingAddon, this.addon);
      }
      else {
        this.addon.installDate = this.addon.updateDate;
      }

      if (AddonManagerPrivate.callInstallListeners("onDownloadEnded",
                                                   this.listeners,
                                                   this.wrapper)) {
        // If a listener changed our state then do not proceed with the install
        if (this.state != AddonManager.STATE_DOWNLOADED)
          return;

        // If an upgrade listener is registered for this add-on, pass control
        // over the upgrade to the add-on.
        if (AddonManagerPrivate.hasUpgradeListener(this.addon.id)) {
          logger.info(`${this.addon.id} has an upgrade listener, postponing until restart`);
          this.state = AddonManager.STATE_POSTPONED;

          let stagingDir = this.installLocation.getStagingDir();
          let stagedAddon = stagingDir.clone();

          Task.spawn((function*() {
            yield this.installLocation.requestStagingDir();

            yield this.unstageInstall(stagedAddon);

            stagedAddon.append(this.addon.id);
            stagedAddon.leafName = this.addon.id + ".xpi";

            yield this.stageInstall(true, stagedAddon, true);

            AddonManagerPrivate.callInstallListeners("onInstallPostponed",
                                                     this.listeners, this.wrapper)

            // upgrade has been staged for restart, notify the add-on and give
            // it a way to resume.
            let callback = AddonManagerPrivate.getUpgradeListener(this.addon.id);
            callback({
              install: () => {
                switch (this.state) {
                  case AddonManager.STATE_INSTALLED:
                    // this addon has already been installed, nothing to do
                    logger.warn(`${this.addon.id} tried to resume postponed upgrade, but it's already installed`);
                    break;
                  case AddonManager.STATE_POSTPONED:
                    logger.info(`${this.addon.id} has resumed a previously postponed upgrade`);
                    this.state = AddonManager.STATE_DOWNLOADED;
                    this.install();
                    break;
                  default:
                    logger.warn(`${this.addon.id} cannot resume postponed upgrade from state (${this.state})`);
                    break;
                }
              },
            });
          }).bind(this));
        } else {
          // no upgrade listener present, so proceed with normal install
          this.install();
          if (this.linkedInstalls) {
            for (let install of this.linkedInstalls) {
              if (install.state == AddonManager.STATE_DOWNLOADED)
                install.install();
            }
          }
        }
      }
    });
  },

  // TODO This relies on the assumption that we are always installing into the
  // highest priority install location so the resulting add-on will be visible
  // overriding any existing copy in another install location (bug 557710).
  /**
   * Installs the add-on into the install location.
   */
  startInstall: function() {
    this.state = AddonManager.STATE_INSTALLING;
    if (!AddonManagerPrivate.callInstallListeners("onInstallStarted",
                                                  this.listeners, this.wrapper)) {
      this.state = AddonManager.STATE_DOWNLOADED;
      XPIProvider.removeActiveInstall(this);
      AddonManagerPrivate.callInstallListeners("onInstallCancelled",
                                               this.listeners, this.wrapper)
      return;
    }

    // Find and cancel any pending installs for the same add-on in the same
    // install location
    for (let aInstall of XPIProvider.installs) {
      if (aInstall.state == AddonManager.STATE_INSTALLED &&
          aInstall.installLocation == this.installLocation &&
          aInstall.addon.id == this.addon.id) {
        logger.debug("Cancelling previous pending install of " + aInstall.addon.id);
        aInstall.cancel();
      }
    }

    let isUpgrade = this.existingAddon &&
                    this.existingAddon._installLocation == this.installLocation;
    let requiresRestart = XPIProvider.installRequiresRestart(this.addon);

    logger.debug("Starting install of " + this.addon.id + " from " + this.sourceURI.spec);
    AddonManagerPrivate.callAddonListeners("onInstalling",
                                           this.addon.wrapper,
                                           requiresRestart);

    let stagingDir = this.installLocation.getStagingDir();
    let stagedAddon = stagingDir.clone();

    Task.spawn((function*() {
      let installedUnpacked = 0;

      yield this.installLocation.requestStagingDir();

      // remove any previously staged files
      yield this.unstageInstall(stagedAddon);

      stagedAddon.append(this.addon.id);
      stagedAddon.leafName = this.addon.id + ".xpi";

      installedUnpacked = yield this.stageInstall(requiresRestart, stagedAddon, isUpgrade);

      if (requiresRestart) {
        this.state = AddonManager.STATE_INSTALLED;
        AddonManagerPrivate.callInstallListeners("onInstallEnded",
                                                 this.listeners, this.wrapper,
                                                 this.addon.wrapper);
      }
      else {
        // The install is completed so it should be removed from the active list
        XPIProvider.removeActiveInstall(this);

        // TODO We can probably reduce the number of DB operations going on here
        // We probably also want to support rolling back failed upgrades etc.
        // See bug 553015.

        // Deactivate and remove the old add-on as necessary
        let reason = BOOTSTRAP_REASONS.ADDON_INSTALL;
        if (this.existingAddon) {
          if (Services.vc.compare(this.existingAddon.version, this.addon.version) < 0)
            reason = BOOTSTRAP_REASONS.ADDON_UPGRADE;
          else
            reason = BOOTSTRAP_REASONS.ADDON_DOWNGRADE;

          if (this.existingAddon.bootstrap) {
            let file = this.existingAddon._sourceBundle;
            if (this.existingAddon.active) {
              XPIProvider.callBootstrapMethod(this.existingAddon, file,
                                              "shutdown", reason,
                                              { newVersion: this.addon.version });
            }

            XPIProvider.callBootstrapMethod(this.existingAddon, file,
                                            "uninstall", reason,
                                            { newVersion: this.addon.version });
            XPIProvider.unloadBootstrapScope(this.existingAddon.id);
            flushChromeCaches();
          }

          if (!isUpgrade && this.existingAddon.active) {
            XPIDatabase.updateAddonActive(this.existingAddon, false);
          }
        }

        // Install the new add-on into its final location
        let existingAddonID = this.existingAddon ? this.existingAddon.id : null;
        let file = this.installLocation.installAddon(this.addon.id, stagedAddon,
                                                     existingAddonID);

        // Update the metadata in the database
        this.addon._sourceBundle = file;
        this.addon.visible = true;

        if (isUpgrade) {
          this.addon =  XPIDatabase.updateAddonMetadata(this.existingAddon, this.addon,
                                                        file.persistentDescriptor);
          let state = XPIStates.getAddon(this.installLocation.name, this.addon.id);
          if (state) {
            state.syncWithDB(this.addon, true);
          } else {
            logger.warn("Unexpected missing XPI state for add-on ${id}", this.addon);
          }
        }
        else {
          this.addon.active = (this.addon.visible && !this.addon.disabled);
          this.addon = XPIDatabase.addAddonMetadata(this.addon, file.persistentDescriptor);
          XPIStates.addAddon(this.addon);
          this.addon.installDate = this.addon.updateDate;
          XPIDatabase.saveChanges();
        }
        XPIStates.save();

        let extraParams = {};
        if (this.existingAddon) {
          extraParams.oldVersion = this.existingAddon.version;
        }

        if (this.addon.bootstrap) {
          XPIProvider.callBootstrapMethod(this.addon, file, "install",
                                          reason, extraParams);
        }

        AddonManagerPrivate.callAddonListeners("onInstalled",
                                               this.addon.wrapper);

        logger.debug("Install of " + this.sourceURI.spec + " completed.");
        this.state = AddonManager.STATE_INSTALLED;
        AddonManagerPrivate.callInstallListeners("onInstallEnded",
                                                 this.listeners, this.wrapper,
                                                 this.addon.wrapper);

        if (this.addon.bootstrap) {
          if (this.addon.active) {
            XPIProvider.callBootstrapMethod(this.addon, file, "startup",
                                            reason, extraParams);
          }
          else {
            // XXX this makes it dangerous to do some things in onInstallEnded
            // listeners because important cleanup hasn't been done yet
            XPIProvider.unloadBootstrapScope(this.addon.id);
          }
        }
        XPIProvider.setTelemetry(this.addon.id, "unpacked", installedUnpacked);
        recordAddonTelemetry(this.addon);
      }
    }).bind(this)).then(null, (e) => {
      logger.warn("Failed to install " + this.file.path + " from " + this.sourceURI.spec + " to " + stagedAddon.path, e);
      if (stagedAddon.exists())
        recursiveRemove(stagedAddon);
      this.state = AddonManager.STATE_INSTALL_FAILED;
      this.error = AddonManager.ERROR_FILE_ACCESS;
      XPIProvider.removeActiveInstall(this);
      AddonManagerPrivate.callAddonListeners("onOperationCancelled",
                                             this.addon.wrapper);
      AddonManagerPrivate.callInstallListeners("onInstallFailed",
                                               this.listeners,
                                               this.wrapper);
    }).then(() => {
      this.removeTemporaryFile();
      return this.installLocation.releaseStagingDir();
    });
  },

  /**
   * Stages an upgrade for next application restart.
   */
  stageInstall: function*(restartRequired, stagedAddon, isUpgrade) {
    let stagedJSON = stagedAddon.clone();
    stagedJSON.leafName = this.addon.id + ".json";

    let installedUnpacked = 0;

    // First stage the file regardless of whether restarting is necessary
    if (this.addon.unpack || Preferences.get(PREF_XPI_UNPACK, false)) {
      logger.debug("Addon " + this.addon.id + " will be installed as " +
          "an unpacked directory");
      stagedAddon.leafName = this.addon.id;
      yield OS.File.makeDir(stagedAddon.path);
      yield ZipUtils.extractFilesAsync(this.file, stagedAddon);
      installedUnpacked = 1;
    }
    else {
      logger.debug("Addon " + this.addon.id + " will be installed as " +
          "a packed xpi");
      stagedAddon.leafName = this.addon.id + ".xpi";
      yield OS.File.copy(this.file.path, stagedAddon.path);
    }

    if (restartRequired) {
      // Point the add-on to its extracted files as the xpi may get deleted
      this.addon._sourceBundle = stagedAddon;

      // Cache the AddonInternal as it may have updated compatibility info
      let stream = Cc["@mozilla.org/network/file-output-stream;1"].
                   createInstance(Ci.nsIFileOutputStream);
      let converter = Cc["@mozilla.org/intl/converter-output-stream;1"].
                      createInstance(Ci.nsIConverterOutputStream);

      try {
        stream.init(stagedJSON, FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE |
                                FileUtils.MODE_TRUNCATE, FileUtils.PERMS_FILE,
                               0);
        converter.init(stream, "UTF-8", 0, 0x0000);
        converter.writeString(JSON.stringify(this.addon));
      }
      finally {
        converter.close();
        stream.close();
      }

      logger.debug("Staged install of " + this.addon.id + " from " + this.sourceURI.spec + " ready; waiting for restart.");
      if (isUpgrade) {
        delete this.existingAddon.pendingUpgrade;
        this.existingAddon.pendingUpgrade = this.addon;
      }
    }

    return installedUnpacked;
  },

  /**
   * Removes any previously staged upgrade.
   */
  unstageInstall: function*(stagedAddon) {
    let stagedJSON = stagedAddon.clone();
    let removedAddon = stagedAddon.clone();

    stagedJSON.append(this.addon.id + ".json");

    if (stagedJSON.exists()) {
      stagedJSON.remove(true);
    }

    removedAddon.append(this.addon.id);
    yield removeAsync(removedAddon);
    removedAddon.leafName = this.addon.id + ".xpi";
    yield removeAsync(removedAddon);
  },

  getInterface: function(iid) {
    if (iid.equals(Ci.nsIAuthPrompt2)) {
      let win = this.window;
      if (!win && this.browser)
        win = this.browser.ownerDocument.defaultView;

      let factory = Cc["@mozilla.org/prompter;1"].
                    getService(Ci.nsIPromptFactory);
      let prompt = factory.getPrompt(win, Ci.nsIAuthPrompt2);

      if (this.browser && this.browser.isRemoteBrowser && prompt instanceof Ci.nsILoginManagerPrompter)
        prompt.setE10sData(this.browser, null);

      return prompt;
    }
    else if (iid.equals(Ci.nsIChannelEventSink)) {
      return this;
    }

    return this.badCertHandler.getInterface(iid);
  }
}

/**
 * Creates a new AddonInstall for an already staged install. Used when
 * installing the staged install failed for some reason.
 *
 * @param  aDir
 *         The directory holding the staged install
 * @param  aManifest
 *         The cached manifest for the install
 */
AddonInstall.createStagedInstall = function(aInstallLocation, aDir, aManifest) {
  let url = Services.io.newFileURI(aDir);

  let install = new AddonInstall(aInstallLocation, aDir);
  install.initStagedInstall(aManifest);
};

/**
 * Creates a new AddonInstall to install an add-on from a local file. Installs
 * always go into the profile install location.
 *
 * @param  aCallback
 *         The callback to pass the new AddonInstall to
 * @param  aFile
 *         The file to install
 */
AddonInstall.createInstall = function(aCallback, aFile) {
  let location = XPIProvider.installLocationsByName[KEY_APP_PROFILE];
  let url = Services.io.newFileURI(aFile);

  try {
    let install = new AddonInstall(location, url);
    install.initLocalInstall(aCallback);
  }
  catch (e) {
    logger.error("Error creating install", e);
    makeSafe(aCallback)(null);
  }
};

/**
 * Creates a new AddonInstall to download and install a URL.
 *
 * @param  aCallback
 *         The callback to pass the new AddonInstall to
 * @param  aUri
 *         The URI to download
 * @param  aHash
 *         A hash for the add-on
 * @param  aName
 *         A name for the add-on
 * @param  aIcons
 *         An icon URLs for the add-on
 * @param  aVersion
 *         A version for the add-on
 * @param  aBrowser
 *         The browser performing the install
 */
AddonInstall.createDownload = function(aCallback, aUri, aHash, aName, aIcons,
                                       aVersion, aBrowser) {
  let location = XPIProvider.installLocationsByName[KEY_APP_PROFILE];
  let url = NetUtil.newURI(aUri);

  let install = new AddonInstall(location, url, aHash, null, null, aBrowser);
  if (url instanceof Ci.nsIFileURL)
    install.initLocalInstall(aCallback);
  else
    install.initAvailableDownload(aName, null, aIcons, aVersion, aCallback);
};

/**
 * Creates a new AddonInstall for an update.
 *
 * @param  aCallback
 *         The callback to pass the new AddonInstall to
 * @param  aAddon
 *         The add-on being updated
 * @param  aUpdate
 *         The metadata about the new version from the update manifest
 */
AddonInstall.createUpdate = function(aCallback, aAddon, aUpdate) {
  let url = NetUtil.newURI(aUpdate.updateURL);
  let releaseNotesURI = null;
  try {
    if (aUpdate.updateInfoURL)
      releaseNotesURI = NetUtil.newURI(escapeAddonURI(aAddon, aUpdate.updateInfoURL));
  }
  catch (e) {
    // If the releaseNotesURI cannot be parsed then just ignore it.
  }

  let install = new AddonInstall(aAddon._installLocation, url,
                                 aUpdate.updateHash, releaseNotesURI, aAddon);
  if (url instanceof Ci.nsIFileURL) {
    install.initLocalInstall(aCallback);
  }
  else {
    install.initAvailableDownload(aAddon.selectedLocale.name, aAddon.type,
                                  aAddon.icons, aUpdate.version, aCallback);
  }
};

// This map is shared between AddonInstallWrapper and AddonWrapper
const wrapperMap = new WeakMap();
let installFor = wrapper => wrapperMap.get(wrapper);
let addonFor = installFor;

/**
 * Creates a wrapper for an AddonInstall that only exposes the public API
 *
 * @param  install
 *         The AddonInstall to create a wrapper for
 */
function AddonInstallWrapper(aInstall) {
  wrapperMap.set(this, aInstall);
}

AddonInstallWrapper.prototype = {
  get __AddonInstallInternal__() {
    return AppConstants.DEBUG ? installFor(this) : undefined;
  },

  get type() {
    return getExternalType(installFor(this).type);
  },

  get iconURL() {
    return installFor(this).icons[32];
  },

  get existingAddon() {
    let install = installFor(this);
    return install.existingAddon ? install.existingAddon.wrapper : null;
  },

  get addon() {
    let install = installFor(this);
    return install.addon ? install.addon.wrapper : null;
  },

  get sourceURI() {
    return installFor(this).sourceURI;
  },

  get linkedInstalls() {
    let install = installFor(this);
    if (!install.linkedInstalls)
      return null;
    return install.linkedInstalls.map(i => i.wrapper);
  },

  install: function() {
    installFor(this).install();
  },

  cancel: function() {
    installFor(this).cancel();
  },

  addListener: function(listener) {
    installFor(this).addListener(listener);
  },

  removeListener: function(listener) {
    installFor(this).removeListener(listener);
  },
};

["name", "version", "icons", "releaseNotesURI", "file", "state", "error",
 "progress", "maxProgress", "certificate", "certName"].forEach(function(aProp) {
  Object.defineProperty(AddonInstallWrapper.prototype, aProp, {
    get: function() {
      return installFor(this)[aProp];
    },
    enumerable: true,
  });
});

/**
 * Creates a new update checker.
 *
 * @param  aAddon
 *         The add-on to check for updates
 * @param  aListener
 *         An UpdateListener to notify of updates
 * @param  aReason
 *         The reason for the update check
 * @param  aAppVersion
 *         An optional application version to check for updates for
 * @param  aPlatformVersion
 *         An optional platform version to check for updates for
 * @throws if the aListener or aReason arguments are not valid
 */
function UpdateChecker(aAddon, aListener, aReason, aAppVersion, aPlatformVersion) {
  if (!aListener || !aReason)
    throw Cr.NS_ERROR_INVALID_ARG;

  Components.utils.import("resource://gre/modules/addons/AddonUpdateChecker.jsm");

  this.addon = aAddon;
  aAddon._updateCheck = this;
  XPIProvider.doing(this);
  this.listener = aListener;
  this.appVersion = aAppVersion;
  this.platformVersion = aPlatformVersion;
  this.syncCompatibility = (aReason == AddonManager.UPDATE_WHEN_NEW_APP_INSTALLED);

  let updateURL = aAddon.updateURL;
  if (!updateURL) {
    if (aReason == AddonManager.UPDATE_WHEN_PERIODIC_UPDATE &&
        Services.prefs.getPrefType(PREF_EM_UPDATE_BACKGROUND_URL) == Services.prefs.PREF_STRING) {
      updateURL = Services.prefs.getCharPref(PREF_EM_UPDATE_BACKGROUND_URL);
    } else {
      updateURL = Services.prefs.getCharPref(PREF_EM_UPDATE_URL);
    }
  }

  const UPDATE_TYPE_COMPATIBILITY = 32;
  const UPDATE_TYPE_NEWVERSION = 64;

  aReason |= UPDATE_TYPE_COMPATIBILITY;
  if ("onUpdateAvailable" in this.listener)
    aReason |= UPDATE_TYPE_NEWVERSION;

  let url = escapeAddonURI(aAddon, updateURL, aReason, aAppVersion);
  this._parser = AddonUpdateChecker.checkForUpdates(aAddon.id, aAddon.updateKey,
                                                    url, this);
}

UpdateChecker.prototype = {
  addon: null,
  listener: null,
  appVersion: null,
  platformVersion: null,
  syncCompatibility: null,

  /**
   * Calls a method on the listener passing any number of arguments and
   * consuming any exceptions.
   *
   * @param  aMethod
   *         The method to call on the listener
   */
  callListener: function(aMethod, ...aArgs) {
    if (!(aMethod in this.listener))
      return;

    try {
      this.listener[aMethod].apply(this.listener, aArgs);
    }
    catch (e) {
      logger.warn("Exception calling UpdateListener method " + aMethod, e);
    }
  },

  /**
   * Called when AddonUpdateChecker completes the update check
   *
   * @param  updates
   *         The list of update details for the add-on
   */
  onUpdateCheckComplete: function(aUpdates) {
    XPIProvider.done(this.addon._updateCheck);
    this.addon._updateCheck = null;
    let AUC = AddonUpdateChecker;

    let ignoreMaxVersion = false;
    let ignoreStrictCompat = false;
    if (!AddonManager.checkCompatibility) {
      ignoreMaxVersion = true;
      ignoreStrictCompat = true;
    } else if (this.addon.type in COMPATIBLE_BY_DEFAULT_TYPES &&
               !AddonManager.strictCompatibility &&
               !this.addon.strictCompatibility &&
               !this.addon.hasBinaryComponents) {
      ignoreMaxVersion = true;
    }

    // Always apply any compatibility update for the current version
    let compatUpdate = AUC.getCompatibilityUpdate(aUpdates, this.addon.version,
                                                  this.syncCompatibility,
                                                  null, null,
                                                  ignoreMaxVersion,
                                                  ignoreStrictCompat);
    // Apply the compatibility update to the database
    if (compatUpdate)
      this.addon.applyCompatibilityUpdate(compatUpdate, this.syncCompatibility);

    // If the request is for an application or platform version that is
    // different to the current application or platform version then look for a
    // compatibility update for those versions.
    if ((this.appVersion &&
         Services.vc.compare(this.appVersion, Services.appinfo.version) != 0) ||
        (this.platformVersion &&
         Services.vc.compare(this.platformVersion, Services.appinfo.platformVersion) != 0)) {
      compatUpdate = AUC.getCompatibilityUpdate(aUpdates, this.addon.version,
                                                false, this.appVersion,
                                                this.platformVersion,
                                                ignoreMaxVersion,
                                                ignoreStrictCompat);
    }

    if (compatUpdate)
      this.callListener("onCompatibilityUpdateAvailable", this.addon.wrapper);
    else
      this.callListener("onNoCompatibilityUpdateAvailable", this.addon.wrapper);

    function sendUpdateAvailableMessages(aSelf, aInstall) {
      if (aInstall) {
        aSelf.callListener("onUpdateAvailable", aSelf.addon.wrapper,
                           aInstall.wrapper);
      }
      else {
        aSelf.callListener("onNoUpdateAvailable", aSelf.addon.wrapper);
      }
      aSelf.callListener("onUpdateFinished", aSelf.addon.wrapper,
                         AddonManager.UPDATE_STATUS_NO_ERROR);
    }

    let compatOverrides = AddonManager.strictCompatibility ?
                            null :
                            this.addon.compatibilityOverrides;

    let update = AUC.getNewestCompatibleUpdate(aUpdates,
                                           this.appVersion,
                                           this.platformVersion,
                                           ignoreMaxVersion,
                                           ignoreStrictCompat,
                                           compatOverrides);

    if (update && Services.vc.compare(this.addon.version, update.version) < 0
        && !this.addon._installLocation.locked) {
      for (let currentInstall of XPIProvider.installs) {
        // Skip installs that don't match the available update
        if (currentInstall.existingAddon != this.addon ||
            currentInstall.version != update.version)
          continue;

        // If the existing install has not yet started downloading then send an
        // available update notification. If it is already downloading then
        // don't send any available update notification
        if (currentInstall.state == AddonManager.STATE_AVAILABLE) {
          logger.debug("Found an existing AddonInstall for " + this.addon.id);
          sendUpdateAvailableMessages(this, currentInstall);
        }
        else
          sendUpdateAvailableMessages(this, null);
        return;
      }

      AddonInstall.createUpdate(aInstall => {
        sendUpdateAvailableMessages(this, aInstall);
      }, this.addon, update);
    }
    else {
      sendUpdateAvailableMessages(this, null);
    }
  },

  /**
   * Called when AddonUpdateChecker fails the update check
   *
   * @param  aError
   *         An error status
   */
  onUpdateCheckError: function(aError) {
    XPIProvider.done(this.addon._updateCheck);
    this.addon._updateCheck = null;
    this.callListener("onNoCompatibilityUpdateAvailable", this.addon.wrapper);
    this.callListener("onNoUpdateAvailable", this.addon.wrapper);
    this.callListener("onUpdateFinished", this.addon.wrapper, aError);
  },

  /**
   * Called to cancel an in-progress update check
   */
  cancel: function() {
    let parser = this._parser;
    if (parser) {
      this._parser = null;
      // This will call back to onUpdateCheckError with a CANCELLED error
      parser.cancel();
    }
  }
};

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
  sourceURI: null,
  releaseNotesURI: null,
  foreignInstall: false,
  seen: true,
  skinnable: false,

  /**
   * @property {Array<string>} dependencies
   *   An array of bootstrapped add-on IDs on which this add-on depends.
   *   The add-on will remain appDisabled if any of the dependent
   *   add-ons is not installed and enabled.
   */
  dependencies: Object.freeze([]),

  get selectedLocale() {
    if (this._selectedLocale)
      return this._selectedLocale;
    let locale = Locale.findClosestLocale(this.locales);
    this._selectedLocale = locale ? locale : this.defaultLocale;
    return this._selectedLocale;
  },

  get providesUpdatesSecurely() {
    return !!(this.updateKey || !this.updateURL ||
              this.updateURL.substring(0, 6) == "https:");
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
    }
    catch (e) { }

    // Something is causing errors in here
    try {
      for (let platform of this.targetPlatforms) {
        if (platform.os == Services.appinfo.OS) {
          if (platform.abi) {
            needsABI = true;
            if (platform.abi === abi)
              return true;
          }
          else {
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

  isCompatibleWith: function(aAppVersion, aPlatformVersion) {
    let app = this.matchingTargetApplication;
    if (!app)
      return false;

    if (!aAppVersion)
      aAppVersion = Services.appinfo.version;
    if (!aPlatformVersion)
      aPlatformVersion = Services.appinfo.platformVersion;

    let version;
    if (app.id == Services.appinfo.ID)
      version = aAppVersion;
    else if (app.id == TOOLKIT_ID)
      version = aPlatformVersion

    // Only extensions and dictionaries can be compatible by default; themes
    // and language packs always use strict compatibility checking.
    if (this.type in COMPATIBLE_BY_DEFAULT_TYPES &&
        !AddonManager.strictCompatibility && !this.strictCompatibility &&
        !this.hasBinaryComponents) {

      // The repository can specify compatibility overrides.
      // Note: For now, only blacklisting is supported by overrides.
      if (this._repositoryAddon &&
          this._repositoryAddon.compatibilityOverrides) {
        let overrides = this._repositoryAddon.compatibilityOverrides;
        let override = AddonRepository.findMatchingCompatOverride(this.version,
                                                                  overrides);
        if (override && override.type == "incompatible")
          return false;
      }

      // Extremely old extensions should not be compatible by default.
      let minCompatVersion;
      if (app.id == Services.appinfo.ID)
        minCompatVersion = XPIProvider.minCompatibleAppVersion;
      else if (app.id == TOOLKIT_ID)
        minCompatVersion = XPIProvider.minCompatiblePlatformVersion;

      if (minCompatVersion &&
          Services.vc.compare(minCompatVersion, app.maxVersion) > 0)
        return false;

      return Services.vc.compare(version, app.minVersion) >= 0;
    }

    return (Services.vc.compare(version, app.minVersion) >= 0) &&
           (Services.vc.compare(version, app.maxVersion) <= 0)
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

  get blocklistState() {
    let staticItem = findMatchingStaticBlocklistItem(this);
    if (staticItem)
      return staticItem.level;

    return Blocklist.getAddonBlocklistState(this.wrapper);
  },

  get blocklistURL() {
    let staticItem = findMatchingStaticBlocklistItem(this);
    if (staticItem) {
      let url = Services.urlFormatter.formatURLPref("extensions.blocklist.itemURL");
      return url.replace(/%blockID%/g, staticItem.blockID);
    }

    return Blocklist.getAddonBlocklistURL(this.wrapper);
  },

  applyCompatibilityUpdate: function(aUpdate, aSyncCompatibility) {
    for (let targetApp of this.targetApplications) {
      for (let updateTarget of aUpdate.targetApplications) {
        if (targetApp.id == updateTarget.id && (aSyncCompatibility ||
            Services.vc.compare(targetApp.maxVersion, updateTarget.maxVersion) < 0)) {
          targetApp.minVersion = updateTarget.minVersion;
          targetApp.maxVersion = updateTarget.maxVersion;
        }
      }
    }
    if (aUpdate.multiprocessCompatible !== undefined)
      this.multiprocessCompatible = aUpdate.multiprocessCompatible;
    this.appDisabled = !isUsableAddon(this);
  },

  /**
   * getDataDirectory tries to execute the callback with two arguments:
   * 1) the path of the data directory within the profile,
   * 2) any exception generated from trying to build it.
   */
  getDataDirectory: function(callback) {
    let parentPath = OS.Path.join(OS.Constants.Path.profileDir, "extension-data");
    let dirPath = OS.Path.join(parentPath, this.id);

    Task.spawn(function*() {
      yield OS.File.makeDir(parentPath, {ignoreExisting: true});
      yield OS.File.makeDir(dirPath, {ignoreExisting: true});
    }).then(() => callback(dirPath, null),
            e => callback(dirPath, e));
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
  toJSON: function(aKey) {
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
  importMetadata: function(aObj) {
    for (let prop of PENDING_INSTALL_METADATA) {
      if (!(prop in aObj))
        continue;

      this[prop] = aObj[prop];
    }

    // Compatibility info may have changed so update appDisabled
    this.appDisabled = !isUsableAddon(this);
  },

  permissions: function() {
    let permissions = 0;

    // Add-ons that aren't installed cannot be modified in any way
    if (!(this.inDatabase))
      return permissions;

    if (!this.appDisabled) {
      if (this.userDisabled || this.softDisabled) {
        permissions |= AddonManager.PERM_CAN_ENABLE;
      }
      else if (this.type != "theme") {
        permissions |= AddonManager.PERM_CAN_DISABLE;
      }
    }

    // Add-ons that are in locked install locations, or are pending uninstall
    // cannot be upgraded or uninstalled
    if (!this._installLocation.locked && !this.pendingUninstall) {
      // Experiments cannot be upgraded.
      // Add-ons that are installed by a file link cannot be upgraded.
      if (this.type != "experiment" && !this._installLocation.isLinkedAddon(this.id)) {
        permissions |= AddonManager.PERM_CAN_UPGRADE;
      }

      permissions |= AddonManager.PERM_CAN_UNINSTALL;
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

  markAsSeen: function() {
    addonFor(this).seen = true;
    XPIDatabase.saveChanges();
  },

  get type() {
    return getExternalType(addonFor(this).type);
  },

  get isWebExtension() {
    return addonFor(this).type == "webextension";
  },

  get temporarilyInstalled() {
    return addonFor(this)._installLocation == TemporaryInstallLocation;
  },

  get aboutURL() {
    return this.isActive ? addonFor(this)["aboutURL"] : null;
  },

  get optionsURL() {
    if (!this.isActive) {
      return null;
    }

    let addon = addonFor(this);
    if (addon.optionsURL) {
      if (this.isWebExtension) {
        // The internal object's optionsURL property comes from the addons
        // DB and should be a relative URL.  However, extensions with
        // options pages installed before bug 1293721 was fixed got absolute
        // URLs in the addons db.  This code handles both cases.
        let base = ExtensionManagement.getURLForExtension(addon.id);
        if (!base) {
          return null;
        }
        return new URL(addon.optionsURL, base).href;
      }
      return addon.optionsURL;
    }

    if (this.hasResource("options.xul"))
      return this.getResourceURI("options.xul").spec;

    return null;
  },

  get optionsType() {
    if (!this.isActive)
      return null;

    let addon = addonFor(this);
    let hasOptionsXUL = this.hasResource("options.xul");
    let hasOptionsURL = !!this.optionsURL;

    if (addon.optionsType) {
      switch (parseInt(addon.optionsType, 10)) {
      case AddonManager.OPTIONS_TYPE_DIALOG:
      case AddonManager.OPTIONS_TYPE_TAB:
        return hasOptionsURL ? addon.optionsType : null;
      case AddonManager.OPTIONS_TYPE_INLINE:
      case AddonManager.OPTIONS_TYPE_INLINE_INFO:
      case AddonManager.OPTIONS_TYPE_INLINE_BROWSER:
        return (hasOptionsXUL || hasOptionsURL) ? addon.optionsType : null;
      }
      return null;
    }

    if (hasOptionsXUL)
      return AddonManager.OPTIONS_TYPE_INLINE;

    if (hasOptionsURL)
      return AddonManager.OPTIONS_TYPE_DIALOG;

    return null;
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

    if (this.isActive && addon.iconURL) {
      icons[32] = addon.iconURL;
      icons[48] = addon.iconURL;
    }

    if (this.isActive && addon.icon64URL) {
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

    if (addon.type == "theme" && this.hasResource("preview.png")) {
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
    }
    else if (addon.pendingUninstall) {
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
    let addon = addonFor(this);
    let ops = 0;
    if (XPIProvider.installRequiresRestart(addon))
      ops |= AddonManager.OP_NEEDS_RESTART_INSTALL;
    if (XPIProvider.uninstallRequiresRestart(addon))
      ops |= AddonManager.OP_NEEDS_RESTART_UNINSTALL;
    if (XPIProvider.enableRequiresRestart(addon))
      ops |= AddonManager.OP_NEEDS_RESTART_ENABLE;
    if (XPIProvider.disableRequiresRestart(addon))
      ops |= AddonManager.OP_NEEDS_RESTART_DISABLE;

    return ops;
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
      if (addon.type == "theme" && val) {
        if (addon.internalName == XPIProvider.defaultSkin)
          throw new Error("Cannot disable the default theme");
        XPIProvider.enableDefaultTheme();
      }
      else {
        // hidden and system add-ons should not be user disasbled,
        // as there is no UI to re-enable them.
        if (this.hidden) {
          throw new Error(`Cannot disable hidden add-on ${addon.id}`);
        }
        XPIProvider.updateAddonDisabledState(addon, val);
      }
    }
    else {
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
      if (addon.type == "theme" && val && !addon.userDisabled) {
        if (addon.internalName == XPIProvider.defaultSkin)
          throw new Error("Cannot disable the default theme");
        XPIProvider.enableDefaultTheme();
      }
      else {
        XPIProvider.updateAddonDisabledState(addon, undefined, val);
      }
    }
    else {
      // Only set softDisabled if not already disabled
      if (!addon.userDisabled)
        addon.softDisabled = val;
    }

    return val;
  },

  get hidden() {
    let addon = addonFor(this);
    if (addon._installLocation.name == KEY_APP_TEMPORARY)
      return false;

    return (addon._installLocation.name == KEY_APP_SYSTEM_DEFAULTS ||
            addon._installLocation.name == KEY_APP_SYSTEM_ADDONS);
  },

  get isSystem() {
    let addon = addonFor(this);
    return (addon._installLocation.name == KEY_APP_SYSTEM_DEFAULTS ||
            addon._installLocation.name == KEY_APP_SYSTEM_ADDONS);
  },

  // Returns true if Firefox Sync should sync this addon. Only non-hotfixes
  // directly in the profile are considered syncable.
  get isSyncable() {
    let addon = addonFor(this);
    let hotfixID = Preferences.get(PREF_EM_HOTFIX_ID, undefined);
    if (hotfixID && hotfixID == addon.id) {
      return false;
    }
    return (addon._installLocation.name == KEY_APP_PROFILE);
  },

  isCompatibleWith: function(aAppVersion, aPlatformVersion) {
    return addonFor(this).isCompatibleWith(aAppVersion, aPlatformVersion);
  },

  uninstall: function(alwaysAllowUndo) {
    let addon = addonFor(this);
    XPIProvider.uninstallAddon(addon, alwaysAllowUndo);
  },

  cancelUninstall: function() {
    let addon = addonFor(this);
    XPIProvider.cancelUninstallAddon(addon);
  },

  findUpdates: function(aListener, aReason, aAppVersion, aPlatformVersion) {
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
  cancelUpdate: function() {
    let addon = addonFor(this);
    if (addon._updateCheck) {
      addon._updateCheck.cancel();
      return true;
    }
    return false;
  },

  hasResource: function(aPath) {
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
    }
    catch (e) {
      addon._hasResourceCache.set(aPath, false);
      return false;
    }
    finally {
      zipReader.close();
    }
  },

  /**
   * Reloads the add-on as if one had uninstalled it then reinstalled it.
   *
   * Currently, only temporarily installed add-ons can be reloaded. Attempting
   * to reload other kinds of add-ons will result in a rejected promise.
   *
   * @return Promise
   */
  reload: function() {
    return new Promise((resolve) => {
      const addon = addonFor(this);

      if (!this.temporarilyInstalled) {
        logger.debug(`Cannot reload add-on at ${addon._sourceBundle}`);
        throw new Error("Only temporary add-ons can be reloaded");
      }

      logger.debug(`reloading add-on ${addon.id}`);
      // This function supports re-installing an existing add-on.
      resolve(AddonManager.installTemporaryAddon(addon._sourceBundle));
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
  getResourceURI: function(aPath) {
    let addon = addonFor(this);
    if (!aPath)
      return NetUtil.newURI(addon._sourceBundle);

    return getURIForResourceInFile(addon._sourceBundle, aPath);
  }
};

/**
 * The PrivateWrapper is used to expose certain functionality only when being
 * called with the add-on instanceID, disallowing other add-ons to access it.
 */
function PrivateWrapper(aAddon) {
  AddonWrapper.call(this, aAddon);
}

PrivateWrapper.prototype = Object.create(AddonWrapper.prototype);
Object.assign(PrivateWrapper.prototype, {
  addonId() {
    return this.id;
  },

  /**
   * Retrieves the preferred global context to be used from the
   * add-on debugging window.
   *
   * @returns  global
   *         The object set as global context. Must be a window object.
   */
  getDebugGlobal(global) {
    let activeAddon = XPIProvider.activeAddons.get(this.id);
    if (activeAddon) {
      return activeAddon.debugGlobal;
    }

    return null;
  },

  /**
   * Defines a global context to be used in the console
   * of the add-on debugging window.
   *
   * @param  global
   *         The object to set as global context. Must be a window object.
   */
  setDebugGlobal(global) {
    if (!global) {
      // If the new global is null, notify the listeners regardless
      // from the current state of the addon.
      // NOTE: this happen after the addon has been disabled and
      // the global will never be set to null otherwise.
      AddonManagerPrivate.callAddonListeners("onPropertyChanged",
                                             addonFor(this),
                                             ["debugGlobal"]);
    } else {
      let activeAddon = XPIProvider.activeAddons.get(this.id);
      if (activeAddon) {
        let globalChanged = activeAddon.debugGlobal != global;
        activeAddon.debugGlobal = global;

        if (globalChanged) {
          AddonManagerPrivate.callAddonListeners("onPropertyChanged",
                                                 addonFor(this),
                                                 ["debugGlobal"]);
        }
      }
    }
  }
});

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
 "softDisabled", "skinnable", "size", "foreignInstall", "hasBinaryComponents",
 "strictCompatibility", "compatibilityOverrides", "updateURL", "dependencies",
 "getDataDirectory", "multiprocessCompatible", "signedState"].forEach(function(aProp) {
   defineAddonWrapperProperty(aProp, function() {
     let addon = addonFor(this);
     return (aProp in addon) ? addon[aProp] : undefined;
   });
});

["fullDescription", "developerComments", "eula", "supportURL",
 "contributionURL", "contributionAmount", "averageRating", "reviewCount",
 "reviewURL", "totalDownloads", "weeklyDownloads", "dailyUsers",
 "repositoryStatus"].forEach(function(aProp) {
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
    return NetUtil.newURI(target);
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
        let value = Preferences.get(pref, null, Ci.nsIPrefLocalizedString);
        if (value)
          result = value;
      }
      catch (e) {
      }
    }

    let rest;
    if (result == null)
      [result, ...rest] = chooseValue(addon, addon.selectedLocale, aProp);

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
          let value = Preferences.get(childPref, null, Ci.nsIPrefLocalizedString);
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
function DirectoryInstallLocation(aName, aDirectory, aScope) {
  this._name = aName;
  this.locked = true;
  this._directory = aDirectory;
  this._scope = aScope
  this._IDToFileMap = {};
  this._linkedAddons = [];

  if (!aDirectory || !aDirectory.exists())
    return;
  if (!aDirectory.isDirectory())
    throw new Error("Location must be a directory.");

  this._readAddons();
}

DirectoryInstallLocation.prototype = {
  _name       : "",
  _directory   : null,
  _IDToFileMap : null,  // mapping from add-on ID to nsIFile

  /**
   * Reads a directory linked to in a file.
   *
   * @param   file
   *          The file containing the directory path
   * @return  An nsIFile object representing the linked directory.
   */
  _readDirectoryFromFile: function(aFile) {
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
    }
    else {
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
        }
        catch (e) {
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
  },

  /**
   * Finds all the add-ons installed in this location.
   */
  _readAddons: function() {
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
          }
          catch (e) {
            logger.warn("Failed to remove stale pointer file " + entry.path, e);
            // Failing to remove the stale pointer file is ignorable
          }
          continue;
        }

        entry = newEntry;
        this._linkedAddons.push(id);
      }

      this._IDToFileMap[id] = entry;
      XPIProvider._addURIMapping(id, entry);
    }
  },

  /**
   * Gets the name of this install location.
   */
  get name() {
    return this._name;
  },

  /**
   * Gets the scope of this install location.
   */
  get scope() {
    return this._scope;
  },

  /**
   * Gets an array of nsIFiles for add-ons installed in this location.
   */
  getAddonLocations: function() {
    let locations = new Map();
    for (let id in this._IDToFileMap) {
      locations.set(id, this._IDToFileMap[id].clone());
    }
    return locations;
  },

  /**
   * Gets the directory that the add-on with the given ID is installed in.
   *
   * @param  aId
   *         The ID of the add-on
   * @return The nsIFile
   * @throws if the ID does not match any of the add-ons installed
   */
  getLocationForID: function(aId) {
    if (aId in this._IDToFileMap)
      return this._IDToFileMap[aId].clone();
    throw new Error("Unknown add-on ID " + aId);
  },

  /**
   * Returns true if the given addon was installed in this location by a text
   * file pointing to its real path.
   *
   * @param aId
   *        The ID of the addon
   */
  isLinkedAddon: function(aId) {
    return this._linkedAddons.indexOf(aId) != -1;
  }
};

/**
 * An extension of DirectoryInstallLocation which adds methods to installing
 * and removing add-ons from the directory at runtime.
 *
 * @param  aName
 *         The string identifier for the install location
 * @param  aDirectory
 *         The nsIFile directory for the install location
 * @param  aScope
 *         The scope of add-ons installed in this location
 */
function MutableDirectoryInstallLocation(aName, aDirectory, aScope) {
  DirectoryInstallLocation.call(this, aName, aDirectory, aScope);
  this.locked = false;
  this._stagingDirLock = 0;
}

MutableDirectoryInstallLocation.prototype = Object.create(DirectoryInstallLocation.prototype);
Object.assign(MutableDirectoryInstallLocation.prototype, {
  /**
   * Gets the staging directory to put add-ons that are pending install and
   * uninstall into.
   *
   * @return an nsIFile
   */
  getStagingDir: function() {
    let dir = this._directory.clone();
    dir.append(DIR_STAGE);
    return dir;
  },

  requestStagingDir: function() {
    this._stagingDirLock++;

    if (this._stagingDirPromise)
      return this._stagingDirPromise;

    OS.File.makeDir(this._directory.path);
    let stagepath = OS.Path.join(this._directory.path, DIR_STAGE);
    return this._stagingDirPromise = OS.File.makeDir(stagepath).then(null, (e) => {
      if (e instanceof OS.File.Error && e.becauseExists)
        return;
      logger.error("Failed to create staging directory", e);
      throw e;
    });
  },

  releaseStagingDir: function() {
    this._stagingDirLock--;

    if (this._stagingDirLock == 0) {
      this._stagingDirPromise = null;
      this.cleanStagingDir();
    }

    return Promise.resolve();
  },

  /**
   * Removes the specified files or directories in the staging directory and
   * then if the staging directory is empty attempts to remove it.
   *
   * @param  aLeafNames
   *         An array of file or directory to remove from the directory, the
   *         array may be empty
   */
  cleanStagingDir: function(aLeafNames = []) {
    let dir = this.getStagingDir();

    for (let name of aLeafNames) {
      let file = dir.clone();
      file.append(name);
      recursiveRemove(file);
    }

    if (this._stagingDirLock > 0)
      return;

    let dirEntries = dir.directoryEntries.QueryInterface(Ci.nsIDirectoryEnumerator);
    try {
      if (dirEntries.nextFile)
        return;
    }
    finally {
      dirEntries.close();
    }

    try {
      setFilePermissions(dir, FileUtils.PERMS_DIRECTORY);
      dir.remove(false);
    }
    catch (e) {
      logger.warn("Failed to remove staging dir", e);
      // Failing to remove the staging directory is ignorable
    }
  },

  /**
   * Returns a directory that is normally on the same filesystem as the rest of
   * the install location and can be used for temporarily storing files during
   * safe move operations. Calling this method will delete the existing trash
   * directory and its contents.
   *
   * @return an nsIFile
   */
  getTrashDir: function() {
    let trashDir = this._directory.clone();
    trashDir.append(DIR_TRASH);
    let trashDirExists = trashDir.exists();
    try {
      if (trashDirExists)
        recursiveRemove(trashDir);
      trashDirExists = false;
    } catch (e) {
      logger.warn("Failed to remove trash directory", e);
    }
    if (!trashDirExists)
      trashDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

    return trashDir;
  },

  /**
   * Installs an add-on into the install location.
   *
   * @param  aId
   *         The ID of the add-on to install
   * @param  aSource
   *         The source nsIFile to install from
   * @param  aExistingAddonID
   *         The ID of an existing add-on to uninstall at the same time
   * @param  aCopy
   *         If false the source files will be moved to the new location,
   *         otherwise they will only be copied
   * @return an nsIFile indicating where the add-on was installed to
   */
  installAddon: function(aId, aSource, aExistingAddonID, aCopy) {
    let trashDir = this.getTrashDir();

    let transaction = new SafeInstallOperation();

    let moveOldAddon = aId => {
      let file = this._directory.clone();
      file.append(aId);

      if (file.exists())
        transaction.moveUnder(file, trashDir);

      file = this._directory.clone();
      file.append(aId + ".xpi");
      if (file.exists()) {
        flushJarCache(file);
        transaction.moveUnder(file, trashDir);
      }
    }

    // If any of these operations fails the finally block will clean up the
    // temporary directory
    try {
      moveOldAddon(aId);
      if (aExistingAddonID && aExistingAddonID != aId) {
        moveOldAddon(aExistingAddonID);

        {
          // Move the data directories.
          /* XXX ajvincent We can't use OS.File:  installAddon isn't compatible
           * with Promises, nor is SafeInstallOperation.  Bug 945540 has been filed
           * for porting to OS.File.
           */
          let oldDataDir = FileUtils.getDir(
            KEY_PROFILEDIR, ["extension-data", aExistingAddonID], false, true
          );

          if (oldDataDir.exists()) {
            let newDataDir = FileUtils.getDir(
              KEY_PROFILEDIR, ["extension-data", aId], false, true
            );
            if (newDataDir.exists()) {
              let trashData = trashDir.clone();
              trashData.append("data-directory");
              transaction.moveUnder(newDataDir, trashData);
            }

            transaction.moveTo(oldDataDir, newDataDir);
          }
        }
      }

      if (aCopy) {
        transaction.copy(aSource, this._directory);
      }
      else {
        if (aSource.isFile())
          flushJarCache(aSource);

        transaction.moveUnder(aSource, this._directory);
      }
    }
    finally {
      // It isn't ideal if this cleanup fails but it isn't worth rolling back
      // the install because of it.
      try {
        recursiveRemove(trashDir);
      }
      catch (e) {
        logger.warn("Failed to remove trash directory when installing " + aId, e);
      }
    }

    let newFile = this._directory.clone();
    newFile.append(aSource.leafName);
    try {
      newFile.lastModifiedTime = Date.now();
    } catch (e)  {
      logger.warn("failed to set lastModifiedTime on " + newFile.path, e);
    }
    this._IDToFileMap[aId] = newFile;
    XPIProvider._addURIMapping(aId, newFile);

    if (aExistingAddonID && aExistingAddonID != aId &&
        aExistingAddonID in this._IDToFileMap) {
      delete this._IDToFileMap[aExistingAddonID];
    }

    return newFile;
  },

  /**
   * Uninstalls an add-on from this location.
   *
   * @param  aId
   *         The ID of the add-on to uninstall
   * @throws if the ID does not match any of the add-ons installed
   */
  uninstallAddon: function(aId) {
    let file = this._IDToFileMap[aId];
    if (!file) {
      logger.warn("Attempted to remove " + aId + " from " +
           this._name + " but it was already gone");
      return;
    }

    file = this._directory.clone();
    file.append(aId);
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
      flushJarCache(file);
    }

    let transaction = new SafeInstallOperation();

    try {
      transaction.moveUnder(file, trashDir);
    }
    finally {
      // It isn't ideal if this cleanup fails, but it is probably better than
      // rolling back the uninstall at this point
      try {
        recursiveRemove(trashDir);
      }
      catch (e) {
        logger.warn("Failed to remove trash directory when uninstalling " + aId, e);
      }
    }

    delete this._IDToFileMap[aId];
  },
});

/**
 * An object which identifies a directory install location for system add-ons.
 * The location consists of a directory which contains the add-ons installed in
 * the location.
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
function SystemAddonInstallLocation(aName, aDirectory, aScope, aResetSet) {
  this._baseDir = aDirectory;
  this._nextDir = null;

  if (aResetSet)
    this.resetAddonSet();

  this._addonSet = this._loadAddonSet();

  this._directory = null;
  if (this._addonSet.directory) {
    this._directory = aDirectory.clone();
    this._directory.append(this._addonSet.directory);
    logger.info("SystemAddonInstallLocation scanning directory " + this._directory.path);
  }
  else {
    logger.info("SystemAddonInstallLocation directory is missing");
  }

  DirectoryInstallLocation.call(this, aName, this._directory, aScope);
  this.locked = true;
}

SystemAddonInstallLocation.prototype = Object.create(DirectoryInstallLocation.prototype);
Object.assign(SystemAddonInstallLocation.prototype, {
  /**
   * Reads the current set of system add-ons
   */
  _loadAddonSet: function() {
    try {
      let setStr = Preferences.get(PREF_SYSTEM_ADDON_SET, null);
      if (setStr) {
        let addonSet = JSON.parse(setStr);
        if ((typeof addonSet == "object") && addonSet.schema == 1)
          return addonSet;
      }
    }
    catch (e) {
      logger.error("Malformed system add-on set, resetting.");
    }

    return { schema: 1, addons: {} };
  },

  /**
   * Saves the current set of system add-ons
   */
  _saveAddonSet: function(aAddonSet) {
    Preferences.set(PREF_SYSTEM_ADDON_SET, JSON.stringify(aAddonSet));
  },

  getAddonLocations: function() {
    // Updated system add-ons are ignored in safe mode
    if (Services.appinfo.inSafeMode)
      return new Map();

    let addons = DirectoryInstallLocation.prototype.getAddonLocations.call(this);

    // Strip out any unexpected add-ons from the list
    for (let id of addons.keys()) {
      if (!(id in this._addonSet.addons))
        addons.delete(id);
    }

    return addons;
  },

  /**
   * Tests whether updated system add-ons are expected.
   */
  isActive: function() {
    return this._directory != null;
  },

  isValidAddon: function(aAddon) {
    if (aAddon.appDisabled) {
      logger.warn(`System add-on ${aAddon.id} isn't compatible with the application.`);
      return false;
    }

    if (aAddon.unpack) {
      logger.warn(`System add-on ${aAddon.id} isn't a packed add-on.`);
      return false;
    }

    if (!aAddon.bootstrap) {
      logger.warn(`System add-on ${aAddon.id} isn't restartless.`);
      return false;
    }

    return true;
  },

  /**
   * Tests whether the loaded add-on information matches what is expected.
   */
  isValid: function(aAddons) {
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
  },

  /**
   * Resets the add-on set so on the next startup the default set will be used.
   */
  resetAddonSet: function() {
    this._saveAddonSet({ schema: 1, addons: {} });
  },

  /**
   * Removes any directories not currently in use or pending use after a
   * restart. Any errors that happen here don't really matter as we'll attempt
   * to cleanup again next time.
   */
  cleanDirectories: Task.async(function*() {

    // System add-ons directory does not exist
    if (!(yield OS.File.exists(this._baseDir.path))) {
      return;
    }

    let iterator;
    try {
      iterator = new OS.File.DirectoryIterator(this._baseDir.path);
    }
    catch (e) {
      logger.error("Failed to clean updated system add-ons directories.", e);
      return;
    }

    try {
      let entries = [];

      yield iterator.forEach(entry => {
        // Skip the directory currently in use
        if (this._directory && this._directory.path == entry.path)
          return;

        // Skip the next directory
        if (this._nextDir && this._nextDir.path == entry.path)
          return;

        entries.push(entry);
      });

      for (let entry of entries) {
        if (entry.isDir) {
          yield OS.File.removeDir(entry.path, {
            ignoreAbsent: true,
            ignorePermissions: true,
          });
        }
        else {
          yield OS.File.remove(entry.path, {
            ignoreAbsent: true,
          });
        }
      }
    }
    catch (e) {
      logger.error("Failed to clean updated system add-ons directories.", e);
    }
    finally {
      iterator.close();
    }
  }),

  /**
   * Installs a new set of system add-ons into the location and updates the
   * add-on set in prefs. We wait to switch state until a restart.
   */
  installAddonSet: Task.async(function*(aAddons) {
    // Make sure the base dir exists
    yield OS.File.makeDir(this._baseDir.path, { ignoreExisting: true });

    let newDir = this._baseDir.clone();

    let uuidGen = Cc["@mozilla.org/uuid-generator;1"].
                  getService(Ci.nsIUUIDGenerator);
    newDir.append("blank");

    while (true) {
      newDir.leafName = uuidGen.generateUUID().toString();

      try {
        yield OS.File.makeDir(newDir.path, { ignoreExisting: false });
        break;
      }
      catch (e) {
        // Directory already exists, pick another
      }
    }

    let copyAddon = Task.async(function*(addon) {
      let target = OS.Path.join(newDir.path, addon.id + ".xpi");
      logger.info(`Copying ${addon.id} from ${addon._sourceBundle.path} to ${target}.`);
      try {
        yield OS.File.copy(addon._sourceBundle.path, target);
      }
      catch (e) {
        logger.error(`Failed to copy ${addon.id} from ${addon._sourceBundle.path} to ${target}.`, e);
        throw e;
      }
      addon._sourceBundle = new nsIFile(target);
    });

    try {
      yield waitForAllPromises(aAddons.map(copyAddon));
    }
    catch (e) {
      try {
        yield OS.File.removeDir(newDir.path, { ignorePermissions: true });
      }
      catch (e) {
        logger.warn(`Failed to remove new system add-on directory ${newDir.path}.`, e);
      }
      throw e;
    }

    // All add-ons in position, create the new state and store it in prefs
    let state = { schema: 1, directory: newDir.leafName, addons: {} };
    for (let addon of aAddons) {
      state.addons[addon.id] = {
        version: addon.version
      }
    }

    this._saveAddonSet(state);
    this._nextDir = newDir;
  }),
});

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
}

/**
 * An object that identifies a registry install location for add-ons. The location
 * consists of a registry key which contains string values mapping ID to the
 * path where an add-on is installed
 *
 * @param  aName
 *         The string identifier of this Install Location.
 * @param  aRootKey
 *         The root key (one of the ROOT_KEY_ values from nsIWindowsRegKey).
 * @param  scope
 *         The scope of add-ons installed in this location
 */
function WinRegInstallLocation(aName, aRootKey, aScope) {
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
  }
  catch (e) {
    return;
  }

  this._readAddons(key);
  key.close();
}

WinRegInstallLocation.prototype = {
  _name       : "",
  _rootKey    : null,
  _scope      : null,
  _IDToFileMap : null,  // mapping from ID to nsIFile

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
  },

  /**
   * Read the registry and build a mapping between ID and path for each
   * installed add-on.
   *
   * @param  key
   *         The key that contains the ID to path mapping
   */
  _readAddons: function(aKey) {
    let count = aKey.valueCount;
    for (let i = 0; i < count; ++i) {
      let id = aKey.getValueName(i);

      let file = Cc["@mozilla.org/file/local;1"].
                createInstance(Ci.nsIFile);
      file.initWithPath(aKey.readStringValue(id));

      if (!file.exists()) {
        logger.warn("Ignoring missing add-on in " + file.path);
        continue;
      }

      this._IDToFileMap[id] = file;
      XPIProvider._addURIMapping(id, file);
    }
  },

  /**
   * Gets the name of this install location.
   */
  get name() {
    return this._name;
  },

  /**
   * Gets the scope of this install location.
   */
  get scope() {
    return this._scope;
  },

  /**
   * Gets an array of nsIFiles for add-ons installed in this location.
   */
  getAddonLocations: function() {
    let locations = new Map();
    for (let id in this._IDToFileMap) {
      locations.set(id, this._IDToFileMap[id].clone());
    }
    return locations;
  },

  /**
   * @see DirectoryInstallLocation
   */
  isLinkedAddon: function(aId) {
    return true;
  }
};

var addonTypes = [
  new AddonManagerPrivate.AddonType("extension", URI_EXTENSION_STRINGS,
                                    STRING_TYPE_NAME,
                                    AddonManager.VIEW_TYPE_LIST, 4000,
                                    AddonManager.TYPE_SUPPORTS_UNDO_RESTARTLESS_UNINSTALL),
  new AddonManagerPrivate.AddonType("theme", URI_EXTENSION_STRINGS,
                                    STRING_TYPE_NAME,
                                    AddonManager.VIEW_TYPE_LIST, 5000),
  new AddonManagerPrivate.AddonType("dictionary", URI_EXTENSION_STRINGS,
                                    STRING_TYPE_NAME,
                                    AddonManager.VIEW_TYPE_LIST, 7000,
                                    AddonManager.TYPE_UI_HIDE_EMPTY | AddonManager.TYPE_SUPPORTS_UNDO_RESTARTLESS_UNINSTALL),
  new AddonManagerPrivate.AddonType("locale", URI_EXTENSION_STRINGS,
                                    STRING_TYPE_NAME,
                                    AddonManager.VIEW_TYPE_LIST, 8000,
                                    AddonManager.TYPE_UI_HIDE_EMPTY | AddonManager.TYPE_SUPPORTS_UNDO_RESTARTLESS_UNINSTALL),
];

// We only register experiments support if the application supports them.
// Ideally, we would install an observer to watch the pref. Installing
// an observer for this pref is not necessary here and may be buggy with
// regards to registering this XPIProvider twice.
if (Preferences.get("experiments.supported", false)) {
  addonTypes.push(
    new AddonManagerPrivate.AddonType("experiment",
                                      URI_EXTENSION_STRINGS,
                                      STRING_TYPE_NAME,
                                      AddonManager.VIEW_TYPE_LIST, 11000,
                                      AddonManager.TYPE_UI_HIDE_EMPTY | AddonManager.TYPE_SUPPORTS_UNDO_RESTARTLESS_UNINSTALL));
}

AddonManagerPrivate.registerProvider(XPIProvider, addonTypes);
