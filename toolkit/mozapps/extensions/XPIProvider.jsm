/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

this.EXPORTED_SYMBOLS = [];

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonRepository",
                                  "resource://gre/modules/AddonRepository.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ChromeManifestParser",
                                  "resource://gre/modules/ChromeManifestParser.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeManager",
                                  "resource://gre/modules/LightweightThemeManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

const PREF_DB_SCHEMA                  = "extensions.databaseSchema";
const PREF_INSTALL_CACHE              = "extensions.installCache";
const PREF_BOOTSTRAP_ADDONS           = "extensions.bootstrappedAddons";
const PREF_PENDING_OPERATIONS         = "extensions.pendingOperations";
const PREF_MATCH_OS_LOCALE            = "intl.locale.matchOS";
const PREF_SELECTED_LOCALE            = "general.useragent.locale";
const PREF_EM_DSS_ENABLED             = "extensions.dss.enabled";
const PREF_DSS_SWITCHPENDING          = "extensions.dss.switchPending";
const PREF_DSS_SKIN_TO_SELECT         = "extensions.lastSelectedSkin";
const PREF_GENERAL_SKINS_SELECTEDSKIN = "general.skins.selectedSkin";
const PREF_EM_UPDATE_URL              = "extensions.update.url";
const PREF_EM_UPDATE_BACKGROUND_URL   = "extensions.update.background.url";
const PREF_EM_ENABLED_ADDONS          = "extensions.enabledAddons";
const PREF_EM_EXTENSION_FORMAT        = "extensions.";
const PREF_EM_ENABLED_SCOPES          = "extensions.enabledScopes";
const PREF_EM_AUTO_DISABLED_SCOPES    = "extensions.autoDisableScopes";
const PREF_EM_SHOW_MISMATCH_UI        = "extensions.showMismatchUI";
const PREF_XPI_ENABLED                = "xpinstall.enabled";
const PREF_XPI_WHITELIST_REQUIRED     = "xpinstall.whitelist.required";
const PREF_XPI_WHITELIST_PERMISSIONS  = "xpinstall.whitelist.add";
const PREF_XPI_BLACKLIST_PERMISSIONS  = "xpinstall.blacklist.add";
const PREF_XPI_UNPACK                 = "extensions.alwaysUnpack";
const PREF_INSTALL_REQUIREBUILTINCERTS = "extensions.install.requireBuiltInCerts";
const PREF_INSTALL_DISTRO_ADDONS      = "extensions.installDistroAddons";
const PREF_BRANCH_INSTALLED_ADDON     = "extensions.installedDistroAddon.";
const PREF_SHOWN_SELECTION_UI         = "extensions.shownSelectionUI";

const PREF_EM_MIN_COMPAT_APP_VERSION      = "extensions.minCompatibleAppVersion";
const PREF_EM_MIN_COMPAT_PLATFORM_VERSION = "extensions.minCompatiblePlatformVersion";

const URI_EXTENSION_SELECT_DIALOG     = "chrome://mozapps/content/extensions/selectAddons.xul";
const URI_EXTENSION_UPDATE_DIALOG     = "chrome://mozapps/content/extensions/update.xul";
const URI_EXTENSION_STRINGS           = "chrome://mozapps/locale/extensions/extensions.properties";

const STRING_TYPE_NAME                = "type.%ID%.name";

const DIR_EXTENSIONS                  = "extensions";
const DIR_STAGE                       = "staged";
const DIR_XPI_STAGE                   = "staged-xpis";
const DIR_TRASH                       = "trash";

const FILE_DATABASE                   = "extensions.sqlite";
const FILE_OLD_CACHE                  = "extensions.cache";
const FILE_INSTALL_MANIFEST           = "install.rdf";
const FILE_XPI_ADDONS_LIST            = "extensions.ini";

const KEY_PROFILEDIR                  = "ProfD";
const KEY_APPDIR                      = "XCurProcD";
const KEY_TEMPDIR                     = "TmpD";
const KEY_APP_DISTRIBUTION            = "XREAppDist";

const KEY_APP_PROFILE                 = "app-profile";
const KEY_APP_GLOBAL                  = "app-global";
const KEY_APP_SYSTEM_LOCAL            = "app-system-local";
const KEY_APP_SYSTEM_SHARE            = "app-system-share";
const KEY_APP_SYSTEM_USER             = "app-system-user";

const UNKNOWN_XPCOM_ABI               = "unknownABI";
const XPI_PERMISSION                  = "install";

const PREFIX_ITEM_URI                 = "urn:mozilla:item:";
const RDFURI_ITEM_ROOT                = "urn:mozilla:item:root"
const RDFURI_INSTALL_MANIFEST_ROOT    = "urn:mozilla:install-manifest";
const PREFIX_NS_EM                    = "http://www.mozilla.org/2004/em-rdf#";

const TOOLKIT_ID                      = "toolkit@mozilla.org";

// The value for this is in Makefile.in
#expand const DB_SCHEMA                       = __MOZ_EXTENSIONS_DB_SCHEMA__;

// Properties that exist in the install manifest
const PROP_METADATA      = ["id", "version", "type", "internalName", "updateURL",
                            "updateKey", "optionsURL", "optionsType", "aboutURL",
                            "iconURL", "icon64URL"];
const PROP_LOCALE_SINGLE = ["name", "description", "creator", "homepageURL"];
const PROP_LOCALE_MULTI  = ["developers", "translators", "contributors"];
const PROP_TARGETAPP     = ["id", "minVersion", "maxVersion"];

// Properties that should be migrated where possible from an old database. These
// shouldn't include properties that can be read directly from install.rdf files
// or calculated
const DB_MIGRATE_METADATA= ["installDate", "userDisabled", "softDisabled",
                            "sourceURI", "applyBackgroundUpdates",
                            "releaseNotesURI", "isForeignInstall", "syncGUID"];

// Note: When adding/changing/removing items here, remember to change the
// DB schema version to ensure changes are picked up ASAP.
const STATIC_BLOCKLIST_PATTERNS = [
  { creator: "Mozilla Corp.",
    level: Ci.nsIBlocklistService.STATE_BLOCKED,
    blockID: "i162" },
  { creator: "Mozilla.org",
    level: Ci.nsIBlocklistService.STATE_BLOCKED,
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
  dictionary: 64
};

const COMPATIBLE_BY_DEFAULT_TYPES = {
  extension: true,
  dictionary: true
};

const MSG_JAR_FLUSH = "AddonJarFlush";

var gGlobalScope = this;

/**
 * Valid IDs fit this pattern.
 */
var gIDTest = /^(\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}|[a-z0-9-\._]*\@[a-z0-9-\._]+)$/i;

["LOG", "WARN", "ERROR"].forEach(function(aName) {
  this.__defineGetter__(aName, function logFuncGetter() {
    Components.utils.import("resource://gre/modules/AddonLogging.jsm");

    LogManager.getLogger("addons.xpi", this);
    return this[aName];
  })
}, this);


const LAZY_OBJECTS = ["XPIDatabase"];

var gLazyObjectsLoaded = false;

function loadLazyObjects() {
  let scope = {};
  Services.scriptloader.loadSubScript("resource://gre/modules/XPIProviderUtils.js",
                                      scope);
  scope.XPIProvider = XPIProvider;

  for (let name of LAZY_OBJECTS) {
    delete gGlobalScope[name];
    gGlobalScope[name] = scope[name];
  }
  gLazyObjectsLoaded = true;
  return scope;
}

for (let name of LAZY_OBJECTS) {
  gGlobalScope.__defineGetter__(name, function lazyObjectGetter() {
    let objs = loadLazyObjects();
    return objs[name];
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
    WARN("Failed to set permissions " + aPermissions.toString(8) + " on " +
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

  _installFile: function SIO_installFile(aFile, aTargetDirectory, aCopy) {
    let oldFile = aCopy ? null : aFile.clone();
    let newFile = aFile.clone();
    try {
      if (aCopy)
        newFile.copyTo(aTargetDirectory, null);
      else
        newFile.moveTo(aTargetDirectory, null);
    }
    catch (e) {
      ERROR("Failed to " + (aCopy ? "copy" : "move") + " file " + aFile.path +
            " to " + aTargetDirectory.path, e);
      throw e;
    }
    this._installedFiles.push({ oldFile: oldFile, newFile: newFile });
  },

  _installDirectory: function SIO_installDirectory(aDirectory, aTargetDirectory, aCopy) {
    let newDir = aTargetDirectory.clone();
    newDir.append(aDirectory.leafName);
    try {
      newDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    }
    catch (e) {
      ERROR("Failed to create directory " + newDir.path, e);
      throw e;
    }
    this._createdDirs.push(newDir);

    // Use a snapshot of the directory contents to avoid possible issues with
    // iterating over a directory while removing files from it (the YAFFS2
    // embedded filesystem has this issue, see bug 772238), and to remove
    // normal files before their resource forks on OSX (see bug 733436).
    let entries = getDirectoryEntries(aDirectory, true);
    entries.forEach(function(aEntry) {
      try {
        this._installDirEntry(aEntry, newDir, aCopy);
      }
      catch (e) {
        ERROR("Failed to " + (aCopy ? "copy" : "move") + " entry " +
              aEntry.path, e);
        throw e;
      }
    }, this);

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
      ERROR("Failed to remove directory " + aDirectory.path, e);
      throw e;
    }

    // Note we put the directory move in after all the file moves so the
    // directory is recreated before all the files are moved back
    this._installedFiles.push({ oldFile: aDirectory, newFile: newDir });
  },

  _installDirEntry: function SIO_installDirEntry(aDirEntry, aTargetDirectory, aCopy) {
    let isDir = null;

    try {
      isDir = aDirEntry.isDirectory();
    }
    catch (e) {
      // If the file has already gone away then don't worry about it, this can
      // happen on OSX where the resource fork is automatically moved with the
      // data fork for the file. See bug 733436.
      if (e.result == Cr.NS_ERROR_FILE_TARGET_DOES_NOT_EXIST)
        return;

      ERROR("Failure " + (aCopy ? "copying" : "moving") + " " + aDirEntry.path +
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
      ERROR("Failure " + (aCopy ? "copying" : "moving") + " " + aDirEntry.path +
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
  move: function SIO_move(aFile, aTargetDirectory) {
    try {
      this._installDirEntry(aFile, aTargetDirectory, false);
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
  copy: function SIO_copy(aFile, aTargetDirectory) {
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
  rollback: function SIO_rollback() {
    while (this._installedFiles.length > 0) {
      let move = this._installedFiles.pop();
      if (move.newFile.isDirectory()) {
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
 * Gets the currently selected locale for display.
 * @return  the selected locale or "en-US" if none is selected
 */
function getLocale() {
  if (Prefs.getBoolPref(PREF_MATCH_OS_LOCALE, false))
    return Services.locale.getLocaleComponentForUserAgent();
  let locale = Prefs.getComplexValue(PREF_SELECTED_LOCALE, Ci.nsIPrefLocalizedString);
  if (locale)
    return locale;
  return Prefs.getCharPref(PREF_SELECTED_LOCALE, "en-US");
}

/**
 * Selects the closest matching locale from a list of locales.
 *
 * @param  aLocales
 *         An array of locales
 * @return the best match for the currently selected locale
 */
function findClosestLocale(aLocales) {
  let appLocale = getLocale();

  // Holds the best matching localized resource
  var bestmatch = null;
  // The number of locale parts it matched with
  var bestmatchcount = 0;
  // The number of locale parts in the match
  var bestpartcount = 0;

  var matchLocales = [appLocale.toLowerCase()];
  /* If the current locale is English then it will find a match if there is
     a valid match for en-US so no point searching that locale too. */
  if (matchLocales[0].substring(0, 3) != "en-")
    matchLocales.push("en-us");

  for each (var locale in matchLocales) {
    var lparts = locale.split("-");
    for each (var localized in aLocales) {
      for each (let found in localized.locales) {
        found = found.toLowerCase();
        // Exact match is returned immediately
        if (locale == found)
          return localized;

        var fparts = found.split("-");
        /* If we have found a possible match and this one isn't any longer
           then we dont need to check further. */
        if (bestmatch && fparts.length < bestmatchcount)
          continue;

        // Count the number of parts that match
        var maxmatchcount = Math.min(fparts.length, lparts.length);
        var matchcount = 0;
        while (matchcount < maxmatchcount &&
               fparts[matchcount] == lparts[matchcount])
          matchcount++;

        /* If we matched more than the last best match or matched the same and
           this locale is less specific than the last best match. */
        if (matchcount > bestmatchcount ||
           (matchcount == bestmatchcount && fparts.length < bestpartcount)) {
          bestmatch = localized;
          bestmatchcount = matchcount;
          bestpartcount = fparts.length;
        }
      }
    }
    // If we found a valid match for this locale return it
    if (bestmatch)
      return bestmatch;
  }
  return null;
}

/**
 * Sets the userDisabled and softDisabled properties of an add-on based on what
 * values those properties had for a previous instance of the add-on. The
 * previous instance may be a previous install or in the case of an application
 * version change the same add-on.
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

  let bs = Cc["@mozilla.org/extensions/blocklist;1"].
           getService(Ci.nsIBlocklistService);

  let oldBlocklistState = bs.getAddonBlocklistState(aOldAddon.id,
                                                    aOldAddon.version,
                                                    aOldAppVersion,
                                                    aOldPlatformVersion);
  let newBlocklistState = bs.getAddonBlocklistState(aNewAddon.id,
                                                    aNewAddon.version);

  // If the blocklist state hasn't changed then the properties don't need to
  // change
  if (newBlocklistState == oldBlocklistState)
    return;

  if (newBlocklistState == Ci.nsIBlocklistService.STATE_SOFTBLOCKED) {
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

  if (aAddon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED)
    return false;

  if (AddonManager.checkUpdateSecurity && !aAddon.providesUpdatesSecurely)
    return false;

  if (!aAddon.isPlatformCompatible)
    return false;

  if (AddonManager.checkCompatibility) {
    if (!aAddon.isCompatible)
      return false;
  }
  else {
    if (!aAddon.matchingTargetApplication)
      return false;
  }

  return true;
}

function isAddonDisabled(aAddon) {
  return aAddon.appDisabled || aAddon.softDisabled || aAddon.userDisabled;
}

this.__defineGetter__("gRDF", function gRDFGetter() {
  delete this.gRDF;
  return this.gRDF = Cc["@mozilla.org/rdf/rdf-service;1"].
                     getService(Ci.nsIRDFService);
});

function EM_R(aProperty) {
  return gRDF.GetResource(PREFIX_NS_EM + aProperty);
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
          WARN("Ignoring empty locale in localized properties");
          continue;
        }
        if (aSeenLocales.indexOf(localeName) != -1) {
          WARN("Ignoring duplicate locale in localized properties");
          continue;
        }
        aSeenLocales.push(localeName);
        locale.locales.push(localeName);
      }

      if (locale.locales.length == 0) {
        WARN("Ignoring localized properties with no listed locales");
        return null;
      }
    }

    PROP_LOCALE_SINGLE.forEach(function(aProp) {
      locale[aProp] = getRDFProperty(aDs, aSource, aProp);
    });

    PROP_LOCALE_MULTI.forEach(function(aProp) {
      locale[aProp] = getPropertyArray(aDs, aSource,
                                       aProp.substring(0, aProp.length - 1));
    });

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
  PROP_METADATA.forEach(function(aProp) {
    addon[aProp] = getRDFProperty(ds, root, aProp);
  });
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

  // Only read the bootstrap property for extensions.
  if (addon.type == "extension") {
    addon.bootstrap = getRDFProperty(ds, root, "bootstrap") == "true";
    if (addon.optionsType &&
        addon.optionsType != AddonManager.OPTIONS_TYPE_DIALOG &&
        addon.optionsType != AddonManager.OPTIONS_TYPE_INLINE &&
        addon.optionsType != AddonManager.OPTIONS_TYPE_TAB &&
        addon.optionsType != AddonManager.OPTIONS_TYPE_INLINE_INFO) {
      throw new Error("Install manifest specifies unknown type: " + addon.optionsType);
    }
  }
  else {
    // spell check dictionaries and language packs never require a restart
    if (addon.type == "dictionary" || addon.type == "locale")
      addon.bootstrap = true;

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

  let seenApplications = [];
  addon.targetApplications = [];
  targets = ds.GetTargets(root, EM_R("targetApplication"), true);
  while (targets.hasMoreElements()) {
    let target = targets.getNext().QueryInterface(Ci.nsIRDFResource);
    let targetAppInfo = {};
    PROP_TARGETAPP.forEach(function(aProp) {
      targetAppInfo[aProp] = getRDFProperty(ds, target, aProp);
    });
    if (!targetAppInfo.id || !targetAppInfo.minVersion ||
        !targetAppInfo.maxVersion) {
      WARN("Ignoring invalid targetApplication entry in install manifest");
      continue;
    }
    if (seenApplications.indexOf(targetAppInfo.id) != -1) {
      WARN("Ignoring duplicate targetApplication entry for " + targetAppInfo.id +
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
  targetPlatforms.forEach(function(aPlatform) {
    let platform = {
      os: null,
      abi: null
    };

    let pos = aPlatform.indexOf("_");
    if (pos != -1) {
      platform.os = aPlatform.substring(0, pos);
      platform.abi = aPlatform.substring(pos + 1);
    }
    else {
      platform.os = aPlatform;
    }

    addon.targetPlatforms.push(platform);
  });

  // A theme's userDisabled value is true if the theme is not the selected skin
  // or if there is an active lightweight theme. We ignore whether softblocking
  // is in effect since it would change the active theme.
  if (addon.type == "theme") {
    addon.userDisabled = !!LightweightThemeManager.currentTheme ||
                         addon.internalName != XPIProvider.selectedSkin;
  }
  else {
    addon.userDisabled = false;
    addon.softDisabled = addon.blocklistState == Ci.nsIBlocklistService.STATE_SOFTBLOCKED;
  }

  addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DEFAULT;

  // Load the storage service before NSS (nsIRandomGenerator),
  // to avoid a SQLite initialization error (bug 717904).
  let storage = Services.storage;

  // Generate random GUID used for Sync.
  // This was lifted from util.js:makeGUID() from services-sync.
  let rng = Cc["@mozilla.org/security/random-generator;1"].
            createInstance(Ci.nsIRandomGenerator);
  let bytes = rng.generateRandomBytes(9);
  let byte_string = [String.fromCharCode(byte) for each (byte in bytes)]
                    .join("");
  // Base64 encode
  addon.syncGUID = btoa(byte_string).replace(/\+/g, '-')
                                    .replace(/\//g, '_');

  return addon;
}

/**
 * Loads an AddonInternal object from an add-on extracted in a directory.
 *
 * @param  aDir
 *         The nsIFile directory holding the add-on
 * @return an AddonInternal object
 * @throws if the directory does not contain a valid install manifest
 */
function loadManifestFromDir(aDir) {
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

  let file = aDir.clone();
  file.append(FILE_INSTALL_MANIFEST);
  if (!file.exists() || !file.isFile())
    throw new Error("Directory " + aDir.path + " does not contain a valid " +
                    "install manifest");

  let fis = Cc["@mozilla.org/network/file-input-stream;1"].
            createInstance(Ci.nsIFileInputStream);
  fis.init(file, -1, -1, false);
  let bis = Cc["@mozilla.org/network/buffered-input-stream;1"].
            createInstance(Ci.nsIBufferedInputStream);
  bis.init(fis, 4096);

  try {
    let addon = loadManifestFromRDF(Services.io.newFileURI(file), bis);
    addon._sourceBundle = aDir.clone();
    addon.size = getFileSize(aDir);

    file = aDir.clone();
    file.append("chrome.manifest");
    let chromeManifest = ChromeManifestParser.parseSync(Services.io.newFileURI(file));
    addon.hasBinaryComponents = ChromeManifestParser.hasType(chromeManifest,
                                                             "binary-component");

    addon.appDisabled = !isUsableAddon(addon);
    return addon;
  }
  finally {
    bis.close();
    fis.close();
  }
}

/**
 * Loads an AddonInternal object from an nsIZipReader for an add-on.
 *
 * @param  aZipReader
 *         An open nsIZipReader for the add-on's files
 * @return an AddonInternal object
 * @throws if the XPI file does not contain a valid install manifest
 */
function loadManifestFromZipReader(aZipReader) {
  let zis = aZipReader.getInputStream(FILE_INSTALL_MANIFEST);
  let bis = Cc["@mozilla.org/network/buffered-input-stream;1"].
            createInstance(Ci.nsIBufferedInputStream);
  bis.init(zis, 4096);

  try {
    let uri = buildJarURI(aZipReader.file, FILE_INSTALL_MANIFEST);
    let addon = loadManifestFromRDF(uri, bis);
    addon._sourceBundle = aZipReader.file;

    addon.size = 0;
    let entries = aZipReader.findEntries(null);
    while (entries.hasMore())
      addon.size += aZipReader.getEntry(entries.getNext()).realSize;

    // Binary components can only be loaded from unpacked addons.
    if (addon.unpack) {
      uri = buildJarURI(aZipReader.file, "chrome.manifest");
      let chromeManifest = ChromeManifestParser.parseSync(uri);
      addon.hasBinaryComponents = ChromeManifestParser.hasType(chromeManifest,
                                                               "binary-component");
    } else {
      addon.hasBinaryComponents = false;
    }

    addon.appDisabled = !isUsableAddon(addon);
    return addon;
  }
  finally {
    bis.close();
    zis.close();
  }
}

/**
 * Loads an AddonInternal object from an add-on in an XPI file.
 *
 * @param  aXPIFile
 *         An nsIFile pointing to the add-on's XPI file
 * @return an AddonInternal object
 * @throws if the XPI file does not contain a valid install manifest
 */
function loadManifestFromZipFile(aXPIFile) {
  let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                  createInstance(Ci.nsIZipReader);
  try {
    zipReader.open(aXPIFile);

    return loadManifestFromZipReader(zipReader);
  }
  finally {
    zipReader.close();
  }
}

function loadManifestFromFile(aFile) {
  if (aFile.isFile())
    return loadManifestFromZipFile(aFile);
  else
    return loadManifestFromDir(aFile);
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
    if (aPath) {
      aPath.split("/").forEach(function(aPart) {
        resource.append(aPart);
      });
    }
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
  Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageBroadcaster)
    .broadcastAsyncMessage(MSG_JAR_FLUSH, aJarFile.path);
}

function flushStartupCache() {
  // Init this, so it will get the notification.
  Services.obs.notifyObservers(null, "startupcache-invalidate", null);
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
 * Extracts files from a ZIP file into a directory.
 *
 * @param  aZipFile
 *         The source ZIP file that contains the add-on.
 * @param  aDir
 *         The nsIFile to extract to.
 */
function extractFiles(aZipFile, aDir) {
  function getTargetFile(aDir, entry) {
    let target = aDir.clone();
    entry.split("/").forEach(function(aPart) {
      target.append(aPart);
    });
    return target;
  }

  let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                  createInstance(Ci.nsIZipReader);
  zipReader.open(aZipFile);

  try {
    // create directories first
    let entries = zipReader.findEntries("*/");
    while (entries.hasMore()) {
      var entryName = entries.getNext();
      let target = getTargetFile(aDir, entryName);
      if (!target.exists()) {
        try {
          target.create(Ci.nsIFile.DIRECTORY_TYPE,
                        FileUtils.PERMS_DIRECTORY);
        }
        catch (e) {
          ERROR("extractFiles: failed to create target directory for " +
                "extraction file = " + target.path, e);
        }
      }
    }

    entries = zipReader.findEntries(null);
    while (entries.hasMore()) {
      let entryName = entries.getNext();
      let target = getTargetFile(aDir, entryName);
      if (target.exists())
        continue;

      zipReader.extract(entryName, target);
      try {
        target.permissions |= FileUtils.PERMS_FILE;
      }
      catch (e) {
        WARN("Failed to set permissions " + aPermissions.toString(8) + " on " +
             target.path, e);
      }
    }
  }
  finally {
    zipReader.close();
  }
}

/**
 * Verifies that a zip file's contents are all signed by the same principal.
 * Directory entries and anything in the META-INF directory are not checked.
 *
 * @param  aZip
 *         A nsIZipReader to check
 * @param  aPrincipal
 *         The nsIPrincipal to compare against
 * @return true if all the contents that should be signed were signed by the
 *         principal
 */
function verifyZipSigning(aZip, aPrincipal) {
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
    var entryPrincipal = aZip.getCertificatePrincipal(entry);
    if (!entryPrincipal || !aPrincipal.equals(entryPrincipal))
      return false;
  }
  return aZip.manifestEntriesCount == count;
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


/**
 * Removes the specified files or directories in a staging directory and then if
 * the staging directory is empty attempts to remove it.
 *
 * @param  aDir
 *         nsIFile for the staging directory to clean up
 * @param  aLeafNames
 *         An array of file or directory to remove from the directory, the
 *         array may be empty
 */
function cleanStagingDir(aDir, aLeafNames) {
  aLeafNames.forEach(function(aName) {
    let file = aDir.clone();
    file.append(aName);
    if (file.exists())
      recursiveRemove(file);
  });

  let dirEntries = aDir.directoryEntries.QueryInterface(Ci.nsIDirectoryEnumerator);
  try {
    if (dirEntries.nextFile)
      return;
  }
  finally {
    dirEntries.close();
  }

  try {
    setFilePermissions(aDir, FileUtils.PERMS_DIRECTORY);
    aDir.remove(false);
  }
  catch (e) {
    WARN("Failed to remove staging dir", e);
    // Failing to remove the staging directory is ignorable
  }
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

    throw e;
  }

  setFilePermissions(aFile, isDir ? FileUtils.PERMS_DIRECTORY
                                  : FileUtils.PERMS_FILE);

  try {
    aFile.remove(true);
    return;
  }
  catch (e) {
    if (!aFile.isDirectory()) {
      ERROR("Failed to remove file " + aFile.path, e);
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
    ERROR("Failed to remove empty directory " + aFile.path, e);
    throw e;
  }
}

/**
 * Returns the timestamp of the most recently modified file in a directory,
 * or simply the file's own timestamp if it is not a directory.
 *
 * @param  aFile
 *         A non-null nsIFile object
 * @return Epoch time, as described above. 0 for an empty directory.
 */
function recursiveLastModifiedTime(aFile) {
  if (aFile.isFile())
    return aFile.lastModifiedTime;

  if (aFile.isDirectory()) {
    let entries = aFile.directoryEntries.QueryInterface(Ci.nsIDirectoryEnumerator);
    let entry, time;
    let maxTime = aFile.lastModifiedTime;
    while ((entry = entries.nextFile)) {
      time = recursiveLastModifiedTime(entry);
      maxTime = Math.max(time, maxTime);
    }
    entries.close();
    return maxTime;
  }

  // If the file is something else, just ignore it.
  return 0;
}

/**
 * Gets a snapshot of directory entries.
 *
 * @param  aDir
 *         Directory to look at
 * @param  aSortEntries
 *         True to sort entries by filename
 * @return An array of nsIFile, or null if aDir is not a readable directory
 */
function getDirectoryEntries(aDir, aSortEntries) {
  let dirEnum;
  try {
    dirEnum = aDir.directoryEntries.QueryInterface(Ci.nsIDirectoryEnumerator);
    let entries = [];
    while (dirEnum.hasMoreElements())
      entries.push(dirEnum.nextFile);

    if (aSortEntries) {
      entries.sort(function sortDirEntries(a, b) {
        return a.path > b.path ? -1 : 1;
      });
    }

    return entries
  }
  catch (e) {
    return null;
  }
  finally {
    dirEnum.close();
  }
}

/**
 * A helpful wrapper around the prefs service that allows for default values
 * when requested values aren't set.
 */
var Prefs = {
  /**
   * Gets a preference from the default branch ignoring user-set values.
   *
   * @param  aName
   *         The name of the preference
   * @param  aDefaultValue
   *         A value to return if the preference does not exist
   * @return the default value of the preference or aDefaultValue if there is
   *         none
   */
  getDefaultCharPref: function Prefs_getDefaultCharPref(aName, aDefaultValue) {
    try {
      return Services.prefs.getDefaultBranch("").getCharPref(aName);
    }
    catch (e) {
    }
    return aDefaultValue;
  },

  /**
   * Gets a string preference.
   *
   * @param  aName
   *         The name of the preference
   * @param  aDefaultValue
   *         A value to return if the preference does not exist
   * @return the value of the preference or aDefaultValue if there is none
   */
  getCharPref: function Prefs_getCharPref(aName, aDefaultValue) {
    try {
      return Services.prefs.getCharPref(aName);
    }
    catch (e) {
    }
    return aDefaultValue;
  },

  /**
   * Gets a complex preference.
   *
   * @param  aName
   *         The name of the preference
   * @param  aType
   *         The interface type of the preference
   * @param  aDefaultValue
   *         A value to return if the preference does not exist
   * @return the value of the preference or aDefaultValue if there is none
   */
  getComplexValue: function Prefs_getComplexValue(aName, aType, aDefaultValue) {
    try {
      return Services.prefs.getComplexValue(aName, aType).data;
    }
    catch (e) {
    }
    return aDefaultValue;
  },

  /**
   * Gets a boolean preference.
   *
   * @param  aName
   *         The name of the preference
   * @param  aDefaultValue
   *         A value to return if the preference does not exist
   * @return the value of the preference or aDefaultValue if there is none
   */
  getBoolPref: function Prefs_getBoolPref(aName, aDefaultValue) {
    try {
      return Services.prefs.getBoolPref(aName);
    }
    catch (e) {
    }
    return aDefaultValue;
  },

  /**
   * Gets an integer preference.
   *
   * @param  aName
   *         The name of the preference
   * @param  defaultValue
   *         A value to return if the preference does not exist
   * @return the value of the preference or defaultValue if there is none
   */
  getIntPref: function Prefs_getIntPref(aName, defaultValue) {
    try {
      return Services.prefs.getIntPref(aName);
    }
    catch (e) {
    }
    return defaultValue;
  },

  /**
   * Clears a preference if it has a user value
   *
   * @param  aName
   *         The name of the preference
   */
  clearUserPref: function Prefs_clearUserPref(aName) {
    if (Services.prefs.prefHasUserValue(aName))
      Services.prefs.clearUserPref(aName);
  }
}

var XPIProvider = {
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
  // A dictionary of JS scopes of loaded bootstrappable add-ons by ID
  bootstrapScopes: {},
  // True if the platform could have activated extensions
  extensionsActive: false,

  // True if all of the add-ons found during startup were installed in the
  // application install location
  allAppGlobal: true,
  // A string listing the enabled add-ons for annotating crash reports
  enabledAddons: null,
  // An array of add-on IDs of add-ons that were inactive during startup
  inactiveAddonIDs: [],
  // Count of unpacked add-ons
  unpackedAddons: 0,

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
  startup: function XPI_startup(aAppChanged, aOldAppVersion, aOldPlatformVersion) {
    LOG("startup");
    this.installs = [];
    this.installLocations = [];
    this.installLocationsByName = {};

    AddonManagerPrivate.recordTimestamp("XPI_startup_begin");

    function addDirectoryInstallLocation(aName, aKey, aPaths, aScope, aLocked) {
      try {
        var dir = FileUtils.getDir(aKey, aPaths);
      }
      catch (e) {
        // Some directories aren't defined on some platforms, ignore them
        LOG("Skipping unavailable install location " + aName);
        return;
      }

      try {
        var location = new DirectoryInstallLocation(aName, dir, aScope, aLocked);
      }
      catch (e) {
        WARN("Failed to add directory install location " + aName, e);
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
        WARN("Failed to add registry install location " + aName, e);
        return;
      }

      XPIProvider.installLocations.push(location);
      XPIProvider.installLocationsByName[location.name] = location;
    }

    let hasRegistry = ("nsIWindowsRegKey" in Ci);

    let enabledScopes = Prefs.getIntPref(PREF_EM_ENABLED_SCOPES,
                                         AddonManager.SCOPE_ALL);

    // These must be in order of priority for processFileChanges etc. to work
    if (enabledScopes & AddonManager.SCOPE_SYSTEM) {
      if (hasRegistry) {
        addRegistryInstallLocation("winreg-app-global",
                                   Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                                   AddonManager.SCOPE_SYSTEM);
      }
      addDirectoryInstallLocation(KEY_APP_SYSTEM_LOCAL, "XRESysLExtPD",
                                  [Services.appinfo.ID],
                                  AddonManager.SCOPE_SYSTEM, true);
      addDirectoryInstallLocation(KEY_APP_SYSTEM_SHARE, "XRESysSExtPD",
                                  [Services.appinfo.ID],
                                  AddonManager.SCOPE_SYSTEM, true);
    }

    if (enabledScopes & AddonManager.SCOPE_APPLICATION) {
      addDirectoryInstallLocation(KEY_APP_GLOBAL, KEY_APPDIR,
                                  [DIR_EXTENSIONS],
                                  AddonManager.SCOPE_APPLICATION, true);
    }

    if (enabledScopes & AddonManager.SCOPE_USER) {
      if (hasRegistry) {
        addRegistryInstallLocation("winreg-app-user",
                                   Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                                   AddonManager.SCOPE_USER);
      }
      addDirectoryInstallLocation(KEY_APP_SYSTEM_USER, "XREUSysExt",
                                  [Services.appinfo.ID],
                                  AddonManager.SCOPE_USER, true);
    }

    // The profile location is always enabled
    addDirectoryInstallLocation(KEY_APP_PROFILE, KEY_PROFILEDIR,
                                [DIR_EXTENSIONS],
                                AddonManager.SCOPE_PROFILE, false);

    this.defaultSkin = Prefs.getDefaultCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN,
                                                "classic/1.0");
    this.currentSkin = Prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN,
                                         this.defaultSkin);
    this.selectedSkin = this.currentSkin;
    this.applyThemeChange();

    this.minCompatibleAppVersion = Prefs.getCharPref(PREF_EM_MIN_COMPAT_APP_VERSION,
                                                     null);
    this.minCompatiblePlatformVersion = Prefs.getCharPref(PREF_EM_MIN_COMPAT_PLATFORM_VERSION,
                                                          null);
    this.enabledAddons = [];

    Services.prefs.addObserver(PREF_EM_MIN_COMPAT_APP_VERSION, this, false);
    Services.prefs.addObserver(PREF_EM_MIN_COMPAT_PLATFORM_VERSION, this, false);

    let flushCaches = this.checkForChanges(aAppChanged, aOldAppVersion,
                                           aOldPlatformVersion);

    // Changes to installed extensions may have changed which theme is selected
    this.applyThemeChange();

    // If the application has been upgraded and there are add-ons outside the
    // application directory then we may need to synchronize compatibility
    // information but only if the mismatch UI isn't disabled
    if (aAppChanged && !this.allAppGlobal &&
        Prefs.getBoolPref(PREF_EM_SHOW_MISMATCH_UI, true)) {
      this.showUpgradeUI();
      flushCaches = true;
    }
    else if (aAppChanged === undefined) {
      // For new profiles we will never need to show the add-on selection UI
      Services.prefs.setBoolPref(PREF_SHOWN_SELECTION_UI, true);
    }

    if (flushCaches) {
      flushStartupCache();

      // UI displayed early in startup (like the compatibility UI) may have
      // caused us to cache parts of the skin or locale in memory. These must
      // be flushed to allow extension provided skins and locales to take full
      // effect
      Services.obs.notifyObservers(null, "chrome-flush-skin-caches", null);
      Services.obs.notifyObservers(null, "chrome-flush-caches", null);
    }

    this.enabledAddons = Prefs.getCharPref(PREF_EM_ENABLED_ADDONS, "");
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

    AddonManagerPrivate.recordTimestamp("XPI_bootstrap_addons_begin");
    for (let id in this.bootstrappedAddons) {
      let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
      file.persistentDescriptor = this.bootstrappedAddons[id].descriptor;
      let reason = BOOTSTRAP_REASONS.APP_STARTUP;
      // Eventually set INSTALLED reason when a bootstrap addon
      // is dropped in profile folder and automatically installed
      if (AddonManager.getStartupChanges(AddonManager.STARTUP_CHANGE_INSTALLED)
                      .indexOf(id) !== -1)
        reason = BOOTSTRAP_REASONS.ADDON_INSTALL;
      this.callBootstrapMethod(id, this.bootstrappedAddons[id].version,
                               this.bootstrappedAddons[id].type, file,
                               "startup", reason);
    }
    AddonManagerPrivate.recordTimestamp("XPI_bootstrap_addons_end");

    // Let these shutdown a little earlier when they still have access to most
    // of XPCOM
    Services.obs.addObserver({
      observe: function shutdownObserver(aSubject, aTopic, aData) {
        for (let id in XPIProvider.bootstrappedAddons) {
          let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
          file.persistentDescriptor = XPIProvider.bootstrappedAddons[id].descriptor;
          XPIProvider.callBootstrapMethod(id, XPIProvider.bootstrappedAddons[id].version,
                                          XPIProvider.bootstrappedAddons[id].type, file, "shutdown",
                                          BOOTSTRAP_REASONS.APP_SHUTDOWN);
        }
        Services.obs.removeObserver(this, "quit-application-granted");
      }
    }, "quit-application-granted", false);

    AddonManagerPrivate.recordTimestamp("XPI_startup_end");

    this.extensionsActive = true;
  },

  /**
   * Shuts down the database and releases all references.
   */
  shutdown: function XPI_shutdown() {
    LOG("shutdown");

    this.bootstrappedAddons = {};
    this.bootstrapScopes = {};
    this.enabledAddons = null;
    this.allAppGlobal = true;

    this.inactiveAddonIDs = [];

    // If there are pending operations then we must update the list of active
    // add-ons
    if (Prefs.getBoolPref(PREF_PENDING_OPERATIONS, false)) {
      XPIDatabase.updateActiveAddons();
      XPIDatabase.writeAddonsList();
      Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, false);
    }

    this.installs = null;
    this.installLocations = null;
    this.installLocationsByName = null;

    // This is needed to allow xpcshell tests to simulate a restart
    this.extensionsActive = false;

    if (gLazyObjectsLoaded) {
      XPIDatabase.shutdown(function shutdownCallback() {
        Services.obs.notifyObservers(null, "xpi-provider-shutdown", null);
      });
    }
    else {
      Services.obs.notifyObservers(null, "xpi-provider-shutdown", null);
    }
  },

  /**
   * Applies any pending theme change to the preferences.
   */
  applyThemeChange: function XPI_applyThemeChange() {
    if (!Prefs.getBoolPref(PREF_DSS_SWITCHPENDING, false))
      return;

    // Tell the Chrome Registry which Skin to select
    try {
      this.selectedSkin = Prefs.getCharPref(PREF_DSS_SKIN_TO_SELECT);
      Services.prefs.setCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN,
                                 this.selectedSkin);
      Services.prefs.clearUserPref(PREF_DSS_SKIN_TO_SELECT);
      LOG("Changed skin to " + this.selectedSkin);
      this.currentSkin = this.selectedSkin;
    }
    catch (e) {
      ERROR("Error applying theme change", e);
    }
    Services.prefs.clearUserPref(PREF_DSS_SWITCHPENDING);
  },

  /**
   * Shows the "Compatibility Updates" UI
   */
  showUpgradeUI: function XPI_showUpgradeUI() {
    // Flip a flag to indicate that we interrupted startup with an interactive prompt
    Services.startup.interrupted = true;

    if (!Prefs.getBoolPref(PREF_SHOWN_SELECTION_UI, false)) {
      // This *must* be modal as it has to block startup.
      var features = "chrome,centerscreen,dialog,titlebar,modal";
      Services.ww.openWindow(null, URI_EXTENSION_SELECT_DIALOG, "", features, null);
      Services.prefs.setBoolPref(PREF_SHOWN_SELECTION_UI, true);
    }
    else {
      var variant = Cc["@mozilla.org/variant;1"].
                    createInstance(Ci.nsIWritableVariant);
      variant.setFromVariant(this.inactiveAddonIDs);
  
      // This *must* be modal as it has to block startup.
      var features = "chrome,centerscreen,dialog,titlebar,modal";
      var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
               getService(Ci.nsIWindowWatcher);
      ww.openWindow(null, URI_EXTENSION_UPDATE_DIALOG, "", features, variant);
    }

    // Ensure any changes to the add-ons list are flushed to disk
    XPIDatabase.writeAddonsList();
    Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, false);
  },

  /**
   * Persists changes to XPIProvider.bootstrappedAddons to it's store (a pref).
   */
  persistBootstrappedAddons: function XPI_persistBootstrappedAddons() {
    Services.prefs.setCharPref(PREF_BOOTSTRAP_ADDONS,
                               JSON.stringify(this.bootstrappedAddons));
  },

  /**
   * Adds a list of currently active add-ons to the next crash report.
   */
  addAddonsToCrashReporter: function XPI_addAddonsToCrashReporter() {
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
    
    const TelemetryPing = Cc["@mozilla.org/base/telemetry-ping;1"].getService(Ci.nsITelemetryPing);
    TelemetryPing.setAddOns(data);
  },

  /**
   * Gets the add-on states for an install location.
   * This function may be expensive because of the recursiveLastModifiedTime call.
   *
   * @param  location
   *         The install location to retrieve the add-on states for
   * @return a dictionary mapping add-on IDs to objects with a descriptor
   *         property which contains the add-ons dir/file descriptor and an
   *         mtime property which contains the add-on's last modified time as
   *         the number of milliseconds since the epoch.
   */
  getAddonStates: function XPI_getAddonStates(aLocation) {
    let addonStates = {};
    aLocation.addonLocations.forEach(function(file) {
      let id = aLocation.getIDForLocation(file);
      addonStates[id] = {
        descriptor: file.persistentDescriptor,
        mtime: recursiveLastModifiedTime(file)
      };
      try {
        // get the install.rdf update time, if any
        file.append(FILE_INSTALL_MANIFEST);
        let rdfTime = file.lastModifiedTime;
        addonStates[id].rdfTime = rdfTime;
        this.unpackedAddons += 1;
      }
      catch (e) { }
    }, this);

    return addonStates;
  },

  /**
   * Gets an array of install location states which uniquely describes all
   * installed add-ons with the add-on's InstallLocation name and last modified
   * time. This function may be expensive because of the getAddonStates() call.
   *
   * @return an array of add-on states for each install location. Each state
   *         is an object with a name property holding the location's name and
   *         an addons property holding the add-on states for the location
   */
  getInstallLocationStates: function XPI_getInstallLocationStates() {
    let states = [];
    this.unpackedAddons = 0;
    this.installLocations.forEach(function(aLocation) {
      let addons = aLocation.addonLocations;
      if (addons.length == 0)
        return;

      let locationState = {
        name: aLocation.name,
        addons: this.getAddonStates(aLocation)
      };

      states.push(locationState);
    }, this);
    return states;
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
  processPendingFileChanges: function XPI_processPendingFileChanges(aManifests) {
    let changed = false;
    this.installLocations.forEach(function(aLocation) {
      aManifests[aLocation.name] = {};
      // We can't install or uninstall anything in locked locations
      if (aLocation.locked)
        return;

      let stagedXPIDir = aLocation.getXPIStagingDir();
      let stagingDir = aLocation.getStagingDir();

      if (stagedXPIDir.exists() && stagedXPIDir.isDirectory()) {
        let entries = stagedXPIDir.directoryEntries
                                  .QueryInterface(Ci.nsIDirectoryEnumerator);
        while (entries.hasMoreElements()) {
          let stageDirEntry = entries.nextFile;

          if (!stageDirEntry.isDirectory()) {
            WARN("Ignoring file in XPI staging directory: " + stageDirEntry.path);
            continue;
          }

          // Find the last added XPI file in the directory
          let stagedXPI = null;
          var xpiEntries = stageDirEntry.directoryEntries
                                        .QueryInterface(Ci.nsIDirectoryEnumerator);
          while (xpiEntries.hasMoreElements()) {
            let file = xpiEntries.nextFile;
            if (file.isDirectory())
              continue;

            let extension = file.leafName;
            extension = extension.substring(extension.length - 4);

            if (extension != ".xpi" && extension != ".jar")
              continue;

            stagedXPI = file;
          }
          xpiEntries.close();

          if (!stagedXPI)
            continue;

          let addon = null;
          try {
            addon = loadManifestFromZipFile(stagedXPI);
          }
          catch (e) {
            ERROR("Unable to read add-on manifest from " + stagedXPI.path, e);
            continue;
          }

          LOG("Migrating staged install of " + addon.id + " in " + aLocation.name);

          if (addon.unpack || Prefs.getBoolPref(PREF_XPI_UNPACK, false)) {
            let targetDir = stagingDir.clone();
            targetDir.append(addon.id);
            try {
              targetDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
            }
            catch (e) {
              ERROR("Failed to create staging directory for add-on " + id, e);
              continue;
            }

            try {
              extractFiles(stagedXPI, targetDir);
            }
            catch (e) {
              ERROR("Failed to extract staged XPI for add-on " + id + " in " +
                    aLocation.name, e);
            }
          }
          else {
            try {
              stagedXPI.moveTo(stagingDir, addon.id + ".xpi");
            }
            catch (e) {
              ERROR("Failed to move staged XPI for add-on " + id + " in " +
                    aLocation.name, e);
            }
          }
        }
        entries.close();
      }

      if (stagedXPIDir.exists()) {
        try {
          recursiveRemove(stagedXPIDir);
        }
        catch (e) {
          // Non-critical, just saves some perf on startup if we clean this up.
          LOG("Error removing XPI staging dir " + stagedXPIDir.path, e);
        }
      }

      if (!stagingDir || !stagingDir.exists() || !stagingDir.isDirectory())
        return;

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
        catch (e if e.result == Cr.NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
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
              WARN("Ignoring file: " + stageDirEntry.path);
              seenFiles.push(stageDirEntry.leafName);
            }
            continue;
          }
        }

        // Check that the directory's name is a valid ID.
        if (!gIDTest.test(id)) {
          WARN("Ignoring directory whose name is not a valid add-on ID: " +
               stageDirEntry.path);
          seenFiles.push(stageDirEntry.leafName);
          continue;
        }

        changed = true;

        if (isDir) {
          // Check if the directory contains an install manifest.
          let manifest = stageDirEntry.clone();
          manifest.append(FILE_INSTALL_MANIFEST);

          // If the install manifest doesn't exist uninstall this add-on in this
          // install location.
          if (!manifest.exists()) {
            LOG("Processing uninstall of " + id + " in " + aLocation.name);
            try {
              aLocation.uninstallAddon(id);
              seenFiles.push(stageDirEntry.leafName);
            }
            catch (e) {
              ERROR("Failed to uninstall add-on " + id + " in " + aLocation.name, e);
            }
            // The file check later will spot the removal and cleanup the database
            continue;
          }
        }

        aManifests[aLocation.name][id] = null;
        let existingAddonID = id;

        let jsonfile = stagingDir.clone();
        jsonfile.append(id + ".json");

        try {
          aManifests[aLocation.name][id] = loadManifestFromFile(stageDirEntry);
        }
        catch (e) {
          ERROR("Unable to read add-on manifest from " + stageDirEntry.path, e);
          // This add-on can't be installed so just remove it now
          seenFiles.push(stageDirEntry.leafName);
          seenFiles.push(jsonfile.leafName);
          continue;
        }

        // Check for a cached metadata for this add-on, it may contain updated
        // compatibility information
        if (jsonfile.exists()) {
          LOG("Found updated metadata for " + id + " in " + aLocation.name);
          let fis = Cc["@mozilla.org/network/file-input-stream;1"].
                       createInstance(Ci.nsIFileInputStream);
          let json = Cc["@mozilla.org/dom/json;1"].
                     createInstance(Ci.nsIJSON);

          try {
            fis.init(jsonfile, -1, 0, 0);
            let metadata = json.decodeFromStream(fis, jsonfile.fileSize);
            aManifests[aLocation.name][id].importMetadata(metadata);
          }
          catch (e) {
            // If some data can't be recovered from the cached metadata then it
            // is unlikely to be a problem big enough to justify throwing away
            // the install, just log and error and continue
            ERROR("Unable to read metadata from " + jsonfile.path, e);
          }
          finally {
            fis.close();
          }
        }
        seenFiles.push(jsonfile.leafName);

        existingAddonID = aManifests[aLocation.name][id].existingAddonID || id;

        var oldBootstrap = null;
        LOG("Processing install of " + id + " in " + aLocation.name);
        if (existingAddonID in this.bootstrappedAddons) {
          try {
            var existingAddon = aLocation.getLocationForID(existingAddonID);
            if (this.bootstrappedAddons[existingAddonID].descriptor ==
                existingAddon.persistentDescriptor) {
              oldBootstrap = this.bootstrappedAddons[existingAddonID];

              // We'll be replacing a currently active bootstrapped add-on so
              // call its uninstall method
              let newVersion = aManifests[aLocation.name][id].version;
              let oldVersion = oldBootstrap.version;
              let uninstallReason = Services.vc.compare(oldVersion, newVersion) < 0 ?
                                    BOOTSTRAP_REASONS.ADDON_UPGRADE :
                                    BOOTSTRAP_REASONS.ADDON_DOWNGRADE;

              this.callBootstrapMethod(existingAddonID, oldBootstrap.version,
                                       oldBootstrap.type, existingAddon, "uninstall", uninstallReason,
                                       { newVersion: newVersion });
              this.unloadBootstrapScope(existingAddonID);
              flushStartupCache();
            }
          }
          catch (e) {
          }
        }

        try {
          var addonInstallLocation = aLocation.installAddon(id, stageDirEntry,
                                                            existingAddonID);
          if (aManifests[aLocation.name][id])
            aManifests[aLocation.name][id]._sourceBundle = addonInstallLocation;
        }
        catch (e) {
          ERROR("Failed to install staged add-on " + id + " in " + aLocation.name,
                e);
          // Re-create the staged install
          AddonInstall.createStagedInstall(aLocation, stageDirEntry,
                                           aManifests[aLocation.name][id]);
          // Make sure not to delete the cached manifest json file
          seenFiles.pop();

          delete aManifests[aLocation.name][id];

          if (oldBootstrap) {
            // Re-install the old add-on
            this.callBootstrapMethod(existingAddonID, oldBootstrap.version,
                                     oldBootstrap.type, existingAddon, "install",
                                     BOOTSTRAP_REASONS.ADDON_INSTALL);
          }
          continue;
        }
      }

      try {
        cleanStagingDir(stagingDir, seenFiles);
      }
      catch (e) {
        // Non-critical, just saves some perf on startup if we clean this up.
        LOG("Error cleaning staging dir " + stagingDir.path, e);
      }
    }, this);
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
   * @return true if any new add-ons were installed
   */
  installDistributionAddons: function XPI_installDistributionAddons(aManifests) {
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
          LOG("Ignoring distribution add-on that isn't an XPI: " + entry.path);
          continue;
        }
      }
      else if (!entry.isDirectory()) {
        LOG("Ignoring distribution add-on that isn't a file or directory: " +
            entry.path);
        continue;
      }

      if (!gIDTest.test(id)) {
        LOG("Ignoring distribution add-on whose name is not a valid add-on ID: " +
            entry.path);
        continue;
      }

      let addon;
      try {
        addon = loadManifestFromFile(entry);
      }
      catch (e) {
        WARN("File entry " + entry.path + " contains an invalid add-on", e);
        continue;
      }

      if (addon.id != id) {
        WARN("File entry " + entry.path + " contains an add-on with an " +
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
          existingAddon = loadManifestFromFile(existingEntry);

          if (Services.vc.compare(addon.version, existingAddon.version) <= 0)
            continue;
        }
        catch (e) {
          // Bad add-on in the profile so just proceed and install over the top
          WARN("Profile contains an add-on with a bad or missing install " +
               "manifest at " + existingEntry.path + ", overwriting", e);
        }
      }
      else if (Prefs.getBoolPref(PREF_BRANCH_INSTALLED_ADDON + id, false)) {
        continue;
      }

      // Install the add-on
      try {
        profileLocation.installAddon(id, entry, null, true);
        LOG("Installed distribution add-on " + id);

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
        ERROR("Failed to install distribution add-on " + entry.path, e);
      }
    }

    entries.close();

    return changed;
  },

  /**
   * Compares the add-ons that are currently installed to those that were
   * known to be installed when the application last ran and applies any
   * changes found to the database. Also sends "startupcache-invalidate" signal to
   * observerservice if it detects that data may have changed.
   *
   * @param  aState
   *         The array of current install location states
   * @param  aManifests
   *         A dictionary of cached AddonInstalls for add-ons that have been
   *         installed
   * @param  aUpdateCompatibility
   *         true to update add-ons appDisabled property when the application
   *         version has changed
   * @param  aOldAppVersion
   *         The version of the application last run with this profile or null
   *         if it is a new profile or the version is unknown
   * @param  aOldPlatformVersion
   *         The version of the platform last run with this profile or null
   *         if it is a new profile or the version is unknown
   * @return a boolean indicating if a change requiring flushing the caches was
   *         detected
   */
  processFileChanges: function XPI_processFileChanges(aState, aManifests,
                                                      aUpdateCompatibility,
                                                      aOldAppVersion,
                                                      aOldPlatformVersion) {
    let visibleAddons = {};
    let oldBootstrappedAddons = this.bootstrappedAddons;
    this.bootstrappedAddons = {};

    /**
     * Updates an add-on's metadata and determines if a restart of the
     * application is necessary. This is called when either the add-on's
     * install directory path or last modified time has changed.
     *
     * @param  aInstallLocation
     *         The install location containing the add-on
     * @param  aOldAddon
     *         The AddonInternal as it appeared the last time the application
     *         ran
     * @param  aAddonState
     *         The new state of the add-on
     * @return a boolean indicating if flushing caches is required to complete
     *         changing this add-on
     */
    function updateMetadata(aInstallLocation, aOldAddon, aAddonState) {
      LOG("Add-on " + aOldAddon.id + " modified in " + aInstallLocation.name);

      // Check if there is an updated install manifest for this add-on
      let newAddon = aManifests[aInstallLocation.name][aOldAddon.id];

      try {
        // If not load it
        if (!newAddon) {
          let file = aInstallLocation.getLocationForID(aOldAddon.id);
          newAddon = loadManifestFromFile(file);
          applyBlocklistChanges(aOldAddon, newAddon);

          // Carry over any pendingUninstall state to add-ons modified directly
          // in the profile. This is impoprtant when the attempt to remove the
          // add-on in processPendingFileChanges failed and caused an mtime
          // change to the add-ons files.
          newAddon.pendingUninstall = aOldAddon.pendingUninstall;
        }

        // The ID in the manifest that was loaded must match the ID of the old
        // add-on.
        if (newAddon.id != aOldAddon.id)
          throw new Error("Incorrect id in install manifest");
      }
      catch (e) {
        WARN("Add-on is invalid", e);
        XPIDatabase.removeAddonMetadata(aOldAddon);
        if (!aInstallLocation.locked)
          aInstallLocation.uninstallAddon(aOldAddon.id);
        else
          WARN("Could not uninstall invalid item from locked install location");
        // If this was an active add-on then we must force a restart
        if (aOldAddon.active)
          return true;

        return false;
      }

      // Set the additional properties on the new AddonInternal
      newAddon._installLocation = aInstallLocation;
      newAddon.updateDate = aAddonState.mtime;
      newAddon.visible = !(newAddon.id in visibleAddons);

      // Update the database
      XPIDatabase.updateAddonMetadata(aOldAddon, newAddon, aAddonState.descriptor);
      if (newAddon.visible) {
        visibleAddons[newAddon.id] = newAddon;
        // Remember add-ons that were changed during startup
        AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_CHANGED,
                                             newAddon.id);

        // If this was the active theme and it is now disabled then enable the
        // default theme
        if (aOldAddon.active && isAddonDisabled(newAddon))
          XPIProvider.enableDefaultTheme();

        // If the new add-on is bootstrapped and active then call its install method
        if (newAddon.active && newAddon.bootstrap) {
          // Startup cache must be flushed before calling the bootstrap script
          flushStartupCache();

          let installReason = Services.vc.compare(aOldAddon.version, newAddon.version) < 0 ?
                              BOOTSTRAP_REASONS.ADDON_UPGRADE :
                              BOOTSTRAP_REASONS.ADDON_DOWNGRADE;

          let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
          file.persistentDescriptor = aAddonState.descriptor;
          XPIProvider.callBootstrapMethod(newAddon.id, newAddon.version, newAddon.type, file,
                                          "install", installReason, { oldVersion: aOldAddon.version });
          return false;
        }

        return true;
      }

      return false;
    }

    /**
     * Updates an add-on's descriptor for when the add-on has moved in the
     * filesystem but hasn't changed in any other way.
     *
     * @param  aInstallLocation
     *         The install location containing the add-on
     * @param  aOldAddon
     *         The AddonInternal as it appeared the last time the application
     *         ran
     * @param  aAddonState
     *         The new state of the add-on
     * @return a boolean indicating if flushing caches is required to complete
     *         changing this add-on
     */
    function updateDescriptor(aInstallLocation, aOldAddon, aAddonState) {
      LOG("Add-on " + aOldAddon.id + " moved to " + aAddonState.descriptor);

      aOldAddon._descriptor = aAddonState.descriptor;
      aOldAddon.visible = !(aOldAddon.id in visibleAddons);

      // Update the database
      XPIDatabase.setAddonDescriptor(aOldAddon, aAddonState.descriptor);
      if (aOldAddon.visible) {
        visibleAddons[aOldAddon.id] = aOldAddon;

        if (aOldAddon.bootstrap && aOldAddon.active) {
          let bootstrap = oldBootstrappedAddons[aOldAddon.id];
          bootstrap.descriptor = aAddonState.descriptor;
          XPIProvider.bootstrappedAddons[aOldAddon.id] = bootstrap;
        }

        return true;
      }

      return false;
    }

    /**
     * Called when no change has been detected for an add-on's metadata. The
     * add-on may have become visible due to other add-ons being removed or
     * the add-on may need to be updated when the application version has
     * changed.
     *
     * @param  aInstallLocation
     *         The install location containing the add-on
     * @param  aOldAddon
     *         The AddonInternal as it appeared the last time the application
     *         ran
     * @param  aAddonState
     *         The new state of the add-on
     * @return a boolean indicating if flushing caches is required to complete
     *         changing this add-on
     */
    function updateVisibilityAndCompatibility(aInstallLocation, aOldAddon,
                                              aAddonState) {
      let changed = false;

      // This add-ons metadata has not changed but it may have become visible
      if (!(aOldAddon.id in visibleAddons)) {
        visibleAddons[aOldAddon.id] = aOldAddon;

        if (!aOldAddon.visible) {
          // Remember add-ons that were changed during startup.
          AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_CHANGED,
                                               aOldAddon.id);
          XPIDatabase.makeAddonVisible(aOldAddon);

          if (aOldAddon.bootstrap) {
            // The add-on is bootstrappable so call its install script
            let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
            file.persistentDescriptor = aAddonState.descriptor;
            XPIProvider.callBootstrapMethod(aOldAddon.id, aOldAddon.version, aOldAddon.type, file,
                                            "install",
                                            BOOTSTRAP_REASONS.ADDON_INSTALL);

            // If it should be active then mark it as active otherwise unload
            // its scope
            if (!isAddonDisabled(aOldAddon)) {
              aOldAddon.active = true;
              XPIDatabase.updateAddonActive(aOldAddon);
            }
            else {
              XPIProvider.unloadBootstrapScope(newAddon.id);
            }
          }
          else {
            // Otherwise a restart is necessary
            changed = true;
          }
        }
      }

      // App version changed, we may need to update the appDisabled property.
      if (aUpdateCompatibility) {
        // Create a basic add-on object for the new state to save reproducing
        // the applyBlocklistChanges code
        let newAddon = new AddonInternal();
        newAddon.id = aOldAddon.id;
        newAddon.syncGUID = aOldAddon.syncGUID;
        newAddon.version = aOldAddon.version;
        newAddon.type = aOldAddon.type;
        newAddon.appDisabled = !isUsableAddon(aOldAddon);

        // Sync the userDisabled flag to the selectedSkin
        if (aOldAddon.type == "theme")
          newAddon.userDisabled = aOldAddon.internalName != XPIProvider.selectedSkin;

        applyBlocklistChanges(aOldAddon, newAddon, aOldAppVersion,
                              aOldPlatformVersion);

        let wasDisabled = isAddonDisabled(aOldAddon);
        let isDisabled = isAddonDisabled(newAddon);

        // If either property has changed update the database.
        if (newAddon.appDisabled != aOldAddon.appDisabled ||
            newAddon.userDisabled != aOldAddon.userDisabled ||
            newAddon.softDisabled != aOldAddon.softDisabled) {
          LOG("Add-on " + aOldAddon.id + " changed appDisabled state to " +
              newAddon.appDisabled + ", userDisabled state to " +
              newAddon.userDisabled + " and softDisabled state to " +
              newAddon.softDisabled);
          XPIDatabase.setAddonProperties(aOldAddon, {
            appDisabled: newAddon.appDisabled,
            userDisabled: newAddon.userDisabled,
            softDisabled: newAddon.softDisabled
          });
        }

        // If this is a visible add-on and it has changed disabled state then we
        // may need a restart or to update the bootstrap list.
        if (aOldAddon.visible && wasDisabled != isDisabled) {
          // Remember add-ons that became disabled or enabled by the application
          // change
          let change = isDisabled ? AddonManager.STARTUP_CHANGE_DISABLED
                                  : AddonManager.STARTUP_CHANGE_ENABLED;
          AddonManagerPrivate.addStartupChange(change, aOldAddon.id);
          if (aOldAddon.bootstrap) {
            // Update the add-ons active state
            aOldAddon.active = !isDisabled;
            XPIDatabase.updateAddonActive(aOldAddon);
          }
          else {
            changed = true;
          }
        }
      }

      if (aOldAddon.visible && aOldAddon.active && aOldAddon.bootstrap) {
        XPIProvider.bootstrappedAddons[aOldAddon.id] = {
          version: aOldAddon.version,
          type: aOldAddon.type,
          descriptor: aAddonState.descriptor
        };
      }

      return changed;
    }

    /**
     * Called when an add-on has been removed.
     *
     * @param  aInstallLocation
     *         The install location containing the add-on
     * @param  aOldAddon
     *         The AddonInternal as it appeared the last time the application
     *         ran
     * @return a boolean indicating if flushing caches is required to complete
     *         changing this add-on
     */
    function removeMetadata(aInstallLocation, aOldAddon) {
      // This add-on has disappeared
      LOG("Add-on " + aOldAddon.id + " removed from " + aInstallLocation.name);
      XPIDatabase.removeAddonMetadata(aOldAddon);

      // Remember add-ons that were uninstalled during startup
      if (aOldAddon.visible) {
        AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_UNINSTALLED,
                                             aOldAddon.id);
      }
      else if (AddonManager.getStartupChanges(AddonManager.STARTUP_CHANGE_INSTALLED)
                           .indexOf(aOldAddon.id) != -1) {
        AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_CHANGED,
                                             aOldAddon.id);
      }

      if (aOldAddon.active) {
        // Enable the default theme if the previously active theme has been
        // removed
        if (aOldAddon.type == "theme")
          XPIProvider.enableDefaultTheme();

        return true;
      }

      return false;
    }

    /**
     * Called to add the metadata for an add-on in one of the install locations
     * to the database. This can be called in three different cases. Either an
     * add-on has been dropped into the location from outside of Firefox, or
     * an add-on has been installed through the application, or the database
     * has been upgraded or become corrupt and add-on data has to be reloaded
     * into it.
     *
     * @param  aInstallLocation
     *         The install location containing the add-on
     * @param  aId
     *         The ID of the add-on
     * @param  aAddonState
     *         The new state of the add-on
     * @param  aMigrateData
     *         If during startup the database had to be upgraded this will
     *         contain data that used to be held about this add-on
     * @return a boolean indicating if flushing caches is required to complete
     *         changing this add-on
     */
    function addMetadata(aInstallLocation, aId, aAddonState, aMigrateData) {
      LOG("New add-on " + aId + " installed in " + aInstallLocation.name);

      let newAddon = null;
      let sameVersion = false;
      // Check the updated manifests lists for the install location, If there
      // is no manifest for the add-on ID then newAddon will be undefined
      if (aInstallLocation.name in aManifests)
        newAddon = aManifests[aInstallLocation.name][aId];

      // If we had staged data for this add-on or we aren't recovering from a
      // corrupt database and we don't have migration data for this add-on then
      // this must be a new install.
      let isNewInstall = (!!newAddon) || (!XPIDatabase.activeBundles && !aMigrateData);

      // If it's a new install and we haven't yet loaded the manifest then it
      // must be something dropped directly into the install location
      let isDetectedInstall = isNewInstall && !newAddon;

      // Load the manifest if necessary and sanity check the add-on ID
      try {
        if (!newAddon) {
          // Load the manifest from the add-on.
          let file = aInstallLocation.getLocationForID(aId);
          newAddon = loadManifestFromFile(file);
        }
        // The add-on in the manifest should match the add-on ID.
        if (newAddon.id != aId)
          throw new Error("Incorrect id in install manifest");
      }
      catch (e) {
        WARN("Add-on is invalid", e);

        // Remove the invalid add-on from the install location if the install
        // location isn't locked, no restart will be necessary
        if (!aInstallLocation.locked)
          aInstallLocation.uninstallAddon(aId);
        else
          WARN("Could not uninstall invalid item from locked install location");
        return false;
      }

      // Update the AddonInternal properties.
      newAddon._installLocation = aInstallLocation;
      newAddon.visible = !(newAddon.id in visibleAddons);
      newAddon.installDate = aAddonState.mtime;
      newAddon.updateDate = aAddonState.mtime;
      newAddon.foreignInstall = isDetectedInstall;

      if (aMigrateData) {
        // If there is migration data then apply it.
        LOG("Migrating data from old database");

        DB_MIGRATE_METADATA.forEach(function(aProp) {
          // A theme's disabled state is determined by the selected theme
          // preference which is read in loadManifestFromRDF
          if (aProp == "userDisabled" && newAddon.type == "theme")
            return;

          if (aProp in aMigrateData)
            newAddon[aProp] = aMigrateData[aProp];
        });

        // Force all non-profile add-ons to be foreignInstalls since they can't
        // have been installed through the API
        newAddon.foreignInstall |= aInstallLocation.name != KEY_APP_PROFILE;

        // Some properties should only be migrated if the add-on hasn't changed.
        // The version property isn't a perfect check for this but covers the
        // vast majority of cases.
        if (aMigrateData.version == newAddon.version) {
          LOG("Migrating compatibility info");
          sameVersion = true;
          if ("targetApplications" in aMigrateData)
            newAddon.applyCompatibilityUpdate(aMigrateData, true);
        }

        // Since the DB schema has changed make sure softDisabled is correct
        applyBlocklistChanges(newAddon, newAddon, aOldAppVersion,
                              aOldPlatformVersion);
      }

      // The default theme is never a foreign install
      if (newAddon.type == "theme" && newAddon.internalName == XPIProvider.defaultSkin)
        newAddon.foreignInstall = false;

      if (isDetectedInstall && newAddon.foreignInstall) {
        // If the add-on is a foreign install and is in a scope where add-ons
        // that were dropped in should default to disabled then disable it
        let disablingScopes = Prefs.getIntPref(PREF_EM_AUTO_DISABLED_SCOPES, 0);
        if (aInstallLocation.scope & disablingScopes)
          newAddon.userDisabled = true;
      }

      // If we have a list of what add-ons should be marked as active then use
      // it to guess at migration data.
      if (!isNewInstall && XPIDatabase.activeBundles) {
        // For themes we know which is active by the current skin setting
        if (newAddon.type == "theme")
          newAddon.active = newAddon.internalName == XPIProvider.currentSkin;
        else
          newAddon.active = XPIDatabase.activeBundles.indexOf(aAddonState.descriptor) != -1;

        // If the add-on wasn't active and it isn't already disabled in some way
        // then it was probably either softDisabled or userDisabled
        if (!newAddon.active && newAddon.visible && !isAddonDisabled(newAddon)) {
          // If the add-on is softblocked then assume it is softDisabled
          if (newAddon.blocklistState == Ci.nsIBlocklistService.STATE_SOFTBLOCKED)
            newAddon.softDisabled = true;
          else
            newAddon.userDisabled = true;
        }
      }
      else {
        newAddon.active = (newAddon.visible && !isAddonDisabled(newAddon))
      }

      try {
        // Update the database.
        XPIDatabase.addAddonMetadata(newAddon, aAddonState.descriptor);
      }
      catch (e) {
        // Failing to write the add-on into the database is non-fatal, the
        // add-on will just be unavailable until we try again in a subsequent
        // startup
        ERROR("Failed to add add-on " + aId + " in " + aInstallLocation.name +
              " to database", e);
        return false;
      }

      if (newAddon.visible) {
        // Remember add-ons that were first detected during startup.
        if (isDetectedInstall) {
          // If a copy from a higher priority location was removed then this
          // add-on has changed
          if (AddonManager.getStartupChanges(AddonManager.STARTUP_CHANGE_UNINSTALLED)
                          .indexOf(newAddon.id) != -1) {
            AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_CHANGED,
                                                 newAddon.id);
          }
          else {
            AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_INSTALLED,
                                                 newAddon.id);
          }
        }

        // Note if any visible add-on is not in the application install location
        if (newAddon._installLocation.name != KEY_APP_GLOBAL)
          XPIProvider.allAppGlobal = false;

        visibleAddons[newAddon.id] = newAddon;

        let installReason = BOOTSTRAP_REASONS.ADDON_INSTALL;
        let extraParams = {};

        // If we're hiding a bootstrapped add-on then call its uninstall method
        if (newAddon.id in oldBootstrappedAddons) {
          let oldBootstrap = oldBootstrappedAddons[newAddon.id];
          extraParams.oldVersion = oldBootstrap.version;
          XPIProvider.bootstrappedAddons[newAddon.id] = oldBootstrap;

          // If the old version is the same as the new version, or we're
          // recovering from a corrupt DB, don't call uninstall and install
          // methods.
          if (sameVersion || !isNewInstall)
            return false;

          installReason = Services.vc.compare(oldBootstrap.version, newAddon.version) < 0 ?
                          BOOTSTRAP_REASONS.ADDON_UPGRADE :
                          BOOTSTRAP_REASONS.ADDON_DOWNGRADE;

          let oldAddonFile = Cc["@mozilla.org/file/local;1"].
                             createInstance(Ci.nsIFile);
          oldAddonFile.persistentDescriptor = oldBootstrap.descriptor;

          XPIProvider.callBootstrapMethod(newAddon.id, oldBootstrap.version,
                                          oldBootstrap.type, oldAddonFile, "uninstall",
                                          installReason, { newVersion: newAddon.version });
          XPIProvider.unloadBootstrapScope(newAddon.id);

          // If the new add-on is bootstrapped then we must flush the caches
          // before calling the new bootstrap script
          if (newAddon.bootstrap)
            flushStartupCache();
        }

        if (!newAddon.bootstrap)
          return true;

        // Visible bootstrapped add-ons need to have their install method called
        let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
        file.persistentDescriptor = aAddonState.descriptor;
        XPIProvider.callBootstrapMethod(newAddon.id, newAddon.version, newAddon.type, file,
                                        "install", installReason, extraParams);
        if (!newAddon.active)
          XPIProvider.unloadBootstrapScope(newAddon.id);
      }

      return false;
    }

    let changed = false;
    let knownLocations = XPIDatabase.getInstallLocations();

    // Gather stats for addon telemetry
    let modifiedUnpacked = 0;
    let modifiedExManifest = 0;
    let modifiedXPI = 0;

    // The install locations are iterated in reverse order of priority so when
    // there are multiple add-ons installed with the same ID the one that
    // should be visible is the first one encountered.
    aState.reverse().forEach(function(aSt) {

      // We can't include the install location directly in the state as it has
      // to be cached as JSON.
      let installLocation = this.installLocationsByName[aSt.name];
      let addonStates = aSt.addons;

      // Check if the database knows about any add-ons in this install location.
      let pos = knownLocations.indexOf(installLocation.name);
      if (pos >= 0) {
        knownLocations.splice(pos, 1);
        let addons = XPIDatabase.getAddonsInLocation(installLocation.name);
        // Iterate through the add-ons installed the last time the application
        // ran
        addons.forEach(function(aOldAddon) {
          // If a version of this add-on has been installed in an higher
          // priority install location then count it as changed
          if (AddonManager.getStartupChanges(AddonManager.STARTUP_CHANGE_INSTALLED)
                          .indexOf(aOldAddon.id) != -1) {
            AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_CHANGED,
                                                 aOldAddon.id);
          }

          // Check if the add-on is still installed
          if (aOldAddon.id in addonStates) {
            let addonState = addonStates[aOldAddon.id];
            delete addonStates[aOldAddon.id];

            // Remember add-ons that were inactive during startup
            if (aOldAddon.visible && !aOldAddon.active)
              XPIProvider.inactiveAddonIDs.push(aOldAddon.id);

            // Check if the add-on is unpacked, and has had other files changed
            // on disk without the install.rdf manifest being changed
            if ((addonState.rdfTime) && (aOldAddon.updateDate != addonState.mtime)) {
              modifiedUnpacked += 1;
              if (aOldAddon.updateDate >= addonState.rdfTime)
                modifiedExManifest += 1;
            }
            else if (aOldAddon.updateDate != addonState.mtime) {
              modifiedXPI += 1;
            }

            // The add-on has changed if the modification time has changed, or
            // we have an updated manifest for it. Also reload the metadata for
            // add-ons in the application directory when the application version
            // has changed
            if (aOldAddon.id in aManifests[installLocation.name] ||
                aOldAddon.updateDate != addonState.mtime ||
                (aUpdateCompatibility && installLocation.name == KEY_APP_GLOBAL)) {
              changed = updateMetadata(installLocation, aOldAddon, addonState) ||
                        changed;
            }
            else if (aOldAddon._descriptor != addonState.descriptor) {
              changed = updateDescriptor(installLocation, aOldAddon, addonState) ||
                        changed;
            }
            else {
              changed = updateVisibilityAndCompatibility(installLocation,
                                                         aOldAddon, addonState) ||
                        changed;
            }
            if (aOldAddon.visible && aOldAddon._installLocation.name != KEY_APP_GLOBAL)
              XPIProvider.allAppGlobal = false;
          }
          else {
            changed = removeMetadata(installLocation, aOldAddon) || changed;
          }
        }, this);
      }

      // All the remaining add-ons in this install location must be new.

      // Get the migration data for this install location.
      let locMigrateData = {};
      if (XPIDatabase.migrateData && installLocation.name in XPIDatabase.migrateData)
        locMigrateData = XPIDatabase.migrateData[installLocation.name];
      for (let id in addonStates) {
        changed = addMetadata(installLocation, id, addonStates[id],
                              locMigrateData[id]) || changed;
      }
    }, this);

    // The remaining locations that had add-ons installed in them no longer
    // have any add-ons installed in them, or the locations no longer exist.
    // The metadata for the add-ons that were in them must be removed from the
    // database.
    knownLocations.forEach(function(aLocation) {
      let addons = XPIDatabase.getAddonsInLocation(aLocation);
      addons.forEach(function(aOldAddon) {
        changed = removeMetadata(aLocation, aOldAddon) || changed;
      }, this);
    }, this);

    // Tell Telemetry what we found
    AddonManagerPrivate.recordSimpleMeasure("modifiedUnpacked", modifiedUnpacked);
    if (modifiedUnpacked > 0)
      AddonManagerPrivate.recordSimpleMeasure("modifiedExceptInstallRDF", modifiedExManifest);
    AddonManagerPrivate.recordSimpleMeasure("modifiedXPI", modifiedXPI);

    // Cache the new install location states
    let cache = JSON.stringify(this.getInstallLocationStates());
    Services.prefs.setCharPref(PREF_INSTALL_CACHE, cache);
    this.persistBootstrappedAddons();

    // Clear out any cached migration data.
    XPIDatabase.migrateData = null;

    return changed;
  },

  /**
   * Imports the xpinstall permissions from preferences into the permissions
   * manager for the user to change later.
   */
  importPermissions: function XPI_importPermissions() {
    function importList(aPrefBranch, aAction) {
      let list = Services.prefs.getChildList(aPrefBranch, {});
      list.forEach(function(aPref) {
        let hosts = Prefs.getCharPref(aPref, "");
        if (!hosts)
          return;

        hosts.split(",").forEach(function(aHost) {
          Services.perms.add(NetUtil.newURI("http://" + aHost), XPI_PERMISSION,
                             aAction);
        });

        Services.prefs.setCharPref(aPref, "");
      });
    }

    importList(PREF_XPI_WHITELIST_PERMISSIONS,
               Ci.nsIPermissionManager.ALLOW_ACTION);
    importList(PREF_XPI_BLACKLIST_PERMISSIONS,
               Ci.nsIPermissionManager.DENY_ACTION);
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
  checkForChanges: function XPI_checkForChanges(aAppChanged, aOldAppVersion,
                                                aOldPlatformVersion) {
    LOG("checkForChanges");

    // Import the website installation permissions if the application has changed
    if (aAppChanged !== false)
      this.importPermissions();

    // If the application version has changed then the database information
    // needs to be updated
    let updateDatabase = aAppChanged;

    // Load the list of bootstrapped add-ons first so processFileChanges can
    // modify it
    try {
      this.bootstrappedAddons = JSON.parse(Prefs.getCharPref(PREF_BOOTSTRAP_ADDONS,
                                           "{}"));
    } catch (e) {
      WARN("Error parsing enabled bootstrapped extensions cache", e);
    }

    // First install any new add-ons into the locations, if there are any
    // changes then we must update the database with the information in the
    // install locations
    let manifests = {};
    updateDatabase = this.processPendingFileChanges(manifests) || updateDatabase;

    // This will be true if the previous session made changes that affect the
    // active state of add-ons but didn't commit them properly (normally due
    // to the application crashing)
    let hasPendingChanges = Prefs.getBoolPref(PREF_PENDING_OPERATIONS);

    // If the schema appears to have changed then we should update the database
    updateDatabase = updateDatabase || DB_SCHEMA != Prefs.getIntPref(PREF_DB_SCHEMA, 0);

    // If the application has changed then check for new distribution add-ons
    if (aAppChanged !== false &&
        Prefs.getBoolPref(PREF_INSTALL_DISTRO_ADDONS, true))
      updateDatabase = this.installDistributionAddons(manifests) || updateDatabase;

    // Telemetry probe added around getInstallLocationStates() to check perf
    let telemetryCaptureTime = new Date();
    let state = this.getInstallLocationStates();
    let telemetry = Services.telemetry;
    telemetry.getHistogramById("CHECK_ADDONS_MODIFIED_MS").add(new Date() - telemetryCaptureTime);

    if (!updateDatabase) {
      // If the state has changed then we must update the database
      let cache = Prefs.getCharPref(PREF_INSTALL_CACHE, null);
      updateDatabase = cache != JSON.stringify(state);
    }

    // If the database doesn't exist and there are add-ons installed then we
    // must update the database however if there are no add-ons then there is
    // no need to update the database.
    // Avoid using XPIDatabase.dbFileExists, as that code is lazy-loaded,
    // and we want to avoid loading it until absolutely necessary.
    let dbFile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_DATABASE], true);
    if (!dbFile.exists())
      updateDatabase = state.length > 0;

    if (!updateDatabase) {
      let bootstrapDescriptors = [this.bootstrappedAddons[b].descriptor
                                  for (b in this.bootstrappedAddons)];

      state.forEach(function(aInstallLocationState) {
        for (let id in aInstallLocationState.addons) {
          let pos = bootstrapDescriptors.indexOf(aInstallLocationState.addons[id].descriptor);
          if (pos != -1)
            bootstrapDescriptors.splice(pos, 1);
        }
      });

      if (bootstrapDescriptors.length > 0) {
        WARN("Bootstrap state is invalid (missing add-ons: " + bootstrapDescriptors.toSource() + ")");
        updateDatabase = true;
      }
    }

    // Catch any errors during the main startup and rollback the database changes
    let transationBegun = false;
    try {
      let extensionListChanged = false;
      // If the database needs to be updated then open it and then update it
      // from the filesystem
      if (updateDatabase || hasPendingChanges) {
        XPIDatabase.beginTransaction();
        transationBegun = true;
        XPIDatabase.openConnection(false, true);

        try {
          extensionListChanged = this.processFileChanges(state, manifests,
                                                         aAppChanged,
                                                         aOldAppVersion,
                                                         aOldPlatformVersion);
        }
        catch (e) {
          ERROR("Error processing file changes", e);
        }
      }
      AddonManagerPrivate.recordSimpleMeasure("installedUnpacked", this.unpackedAddons);

      if (aAppChanged) {
        // When upgrading the app and using a custom skin make sure it is still
        // compatible otherwise switch back the default
        if (this.currentSkin != this.defaultSkin) {
          if (!transationBegun) {
            XPIDatabase.beginTransaction();
            transationBegun = true;
          }
          let oldSkin = XPIDatabase.getVisibleAddonForInternalName(this.currentSkin);
          if (!oldSkin || isAddonDisabled(oldSkin))
            this.enableDefaultTheme();
        }

        // When upgrading remove the old extensions cache to force older
        // versions to rescan the entire list of extensions
        let oldCache = FileUtils.getFile(KEY_PROFILEDIR, [FILE_OLD_CACHE], true);
        if (oldCache.exists())
          oldCache.remove(true);
      }

      // If the application crashed before completing any pending operations then
      // we should perform them now.
      if (extensionListChanged || hasPendingChanges) {
        LOG("Updating database with changes to installed add-ons");
        if (!transationBegun) {
          XPIDatabase.beginTransaction();
          transationBegun = true;
        }
        XPIDatabase.updateActiveAddons();
        XPIDatabase.commitTransaction();
        XPIDatabase.writeAddonsList();
        Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, false);
        Services.prefs.setCharPref(PREF_BOOTSTRAP_ADDONS,
                                   JSON.stringify(this.bootstrappedAddons));
        return true;
      }

      LOG("No changes found");
      if (transationBegun)
        XPIDatabase.commitTransaction();
    }
    catch (e) {
      ERROR("Error during startup file checks, rolling back any database " +
            "changes", e);
      if (transationBegun)
        XPIDatabase.rollbackTransaction();
    }

    // Check that the add-ons list still exists
    let addonsList = FileUtils.getFile(KEY_PROFILEDIR, [FILE_XPI_ADDONS_LIST],
                                       true);
    if (addonsList.exists() == (state.length == 0)) {
      LOG("Add-ons list is invalid, rebuilding");
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
  supportsMimetype: function XPI_supportsMimetype(aMimetype) {
    return aMimetype == "application/x-xpinstall";
  },

  /**
   * Called to test whether installing XPI add-ons is enabled.
   *
   * @return true if installing is enabled
   */
  isInstallEnabled: function XPI_isInstallEnabled() {
    // Default to enabled if the preference does not exist
    return Prefs.getBoolPref(PREF_XPI_ENABLED, true);
  },

  /**
   * Called to test whether installing XPI add-ons from a URI is allowed.
   *
   * @param  aUri
   *         The URI being installed from
   * @return true if installing is allowed
   */
  isInstallAllowed: function XPI_isInstallAllowed(aUri) {
    if (!this.isInstallEnabled())
      return false;

    if (!aUri)
      return true;

    // file: and chrome: don't need whitelisted hosts
    if (aUri.schemeIs("chrome") || aUri.schemeIs("file"))
      return true;


    let permission = Services.perms.testPermission(aUri, XPI_PERMISSION);
    if (permission == Ci.nsIPermissionManager.DENY_ACTION)
      return false;

    let requireWhitelist = Prefs.getBoolPref(PREF_XPI_WHITELIST_REQUIRED, true);
    if (requireWhitelist && (permission != Ci.nsIPermissionManager.ALLOW_ACTION))
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
   * @param  aLoadGroup
   *         An nsILoadGroup to associate requests with
   * @param  aCallback
   *         A callback to pass the AddonInstall to
   */
  getInstallForURL: function XPI_getInstallForURL(aUrl, aHash, aName, aIcons,
                                                  aVersion, aLoadGroup, aCallback) {
    AddonInstall.createDownload(function getInstallForURL_createDownload(aInstall) {
      aCallback(aInstall.wrapper);
    }, aUrl, aHash, aName, aIcons, aVersion, aLoadGroup);
  },

  /**
   * Called to get an AddonInstall to install an add-on from a local file.
   *
   * @param  aFile
   *         The file to be installed
   * @param  aCallback
   *         A callback to pass the AddonInstall to
   */
  getInstallForFile: function XPI_getInstallForFile(aFile, aCallback) {
    AddonInstall.createInstall(function getInstallForFile_createInstall(aInstall) {
      if (aInstall)
        aCallback(aInstall.wrapper);
      else
        aCallback(null);
    }, aFile);
  },

  /**
   * Removes an AddonInstall from the list of active installs.
   *
   * @param  install
   *         The AddonInstall to remove
   */
  removeActiveInstall: function XPI_removeActiveInstall(aInstall) {
    this.installs = this.installs.filter(function installFilter(i) i != aInstall);
  },

  /**
   * Called to get an Addon with a particular ID.
   *
   * @param  aId
   *         The ID of the add-on to retrieve
   * @param  aCallback
   *         A callback to pass the Addon to
   */
  getAddonByID: function XPI_getAddonByID(aId, aCallback) {
    XPIDatabase.getVisibleAddonForID (aId, function getAddonByID_getVisibleAddonForID(aAddon) {
      if (aAddon)
        aCallback(createWrapper(aAddon));
      else
        aCallback(null);
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
  getAddonsByTypes: function XPI_getAddonsByTypes(aTypes, aCallback) {
    XPIDatabase.getVisibleAddons(aTypes, function getAddonsByTypes_getVisibleAddons(aAddons) {
      aCallback([createWrapper(a) for each (a in aAddons)]);
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
  getAddonBySyncGUID: function XPI_getAddonBySyncGUID(aGUID, aCallback) {
    XPIDatabase.getAddonBySyncGUID(aGUID, function getAddonBySyncGUID_getAddonBySyncGUID(aAddon) {
      if (aAddon)
        aCallback(createWrapper(aAddon));
      else
        aCallback(null);
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
  getAddonsWithOperationsByTypes:
  function XPI_getAddonsWithOperationsByTypes(aTypes, aCallback) {
    XPIDatabase.getVisibleAddonsWithPendingOperations(aTypes,
      function getAddonsWithOpsByTypes_getVisibleAddonsWithPendingOps(aAddons) {
      let results = [createWrapper(a) for each (a in aAddons)];
      XPIProvider.installs.forEach(function(aInstall) {
        if (aInstall.state == AddonManager.STATE_INSTALLED &&
            !(aInstall.addon instanceof DBAddonInternal))
          results.push(createWrapper(aInstall.addon));
      });
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
  getInstallsByTypes: function XPI_getInstallsByTypes(aTypes, aCallback) {
    let results = [];
    this.installs.forEach(function(aInstall) {
      if (!aTypes || aTypes.indexOf(aInstall.type) >= 0)
        results.push(aInstall.wrapper);
    });
    aCallback(results);
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
  addonChanged: function XPI_addonChanged(aId, aType, aPendingRestart) {
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
    addons.forEach(function(aTheme) {
      if (!aTheme.visible)
        return;
      if (aTheme.id == aId)
        newSkin = aTheme.internalName;
      else if (aTheme.userDisabled == false && !aTheme.pendingUninstall)
        previousTheme = aTheme;
    }, this);

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
  updateAddonAppDisabledStates: function XPI_updateAddonAppDisabledStates() {
    let addons = XPIDatabase.getAddons();
    addons.forEach(function(aAddon) {
      this.updateAddonDisabledState(aAddon);
    }, this);
  },

  /**
   * Update the repositoryAddon property for all add-ons.
   *
   * @param  aCallback
   *         Function to call when operation is complete.
   */
  updateAddonRepositoryData: function XPI_updateAddonRepositoryData(aCallback) {
    let self = this;
    XPIDatabase.getVisibleAddons(null, function UARD_getVisibleAddonsCallback(aAddons) {
      let pending = aAddons.length;
      if (pending == 0) {
        aCallback();
        return;
      }

      function notifyComplete() {
        if (--pending == 0)
          aCallback();
      }

      aAddons.forEach(function UARD_forEachCallback(aAddon) {
        AddonRepository.getCachedAddonByID(aAddon.id,
                                           function UARD_getCachedAddonCallback(aRepoAddon) {
          if (aRepoAddon) {
            aAddon._repositoryAddon = aRepoAddon;
            aAddon.compatibilityOverrides = aRepoAddon.compatibilityOverrides;
            self.updateAddonDisabledState(aAddon);
          }

          notifyComplete();
        });
      });
    });
  },

  /**
   * When the previously selected theme is removed this method will be called
   * to enable the default theme.
   */
  enableDefaultTheme: function XPI_enableDefaultTheme() {
    LOG("Activating default theme");
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
        Prefs.clearUserPref(PREF_DSS_SKIN_TO_SELECT);
        Prefs.clearUserPref(PREF_DSS_SWITCHPENDING);
      }
      else {
        WARN("Attempting to activate an already active default theme");
      }
    }
    else {
      WARN("Unable to activate the default theme");
    }
  },

  /**
   * Notified when a preference we're interested in has changed.
   *
   * @see nsIObserver
   */
  observe: function XPI_observe(aSubject, aTopic, aData) {
    switch (aData) {
    case PREF_EM_MIN_COMPAT_APP_VERSION:
    case PREF_EM_MIN_COMPAT_PLATFORM_VERSION:
      this.minCompatibleAppVersion = Prefs.getCharPref(PREF_EM_MIN_COMPAT_APP_VERSION,
                                                       null);
      this.minCompatiblePlatformVersion = Prefs.getCharPref(PREF_EM_MIN_COMPAT_PLATFORM_VERSION,
                                                            null);
      this.updateAllAddonDisabledStates();
      break;
    }
  },

  /**
   * Tests whether enabling an add-on will require a restart.
   *
   * @param  aAddon
   *         The add-on to test
   * @return true if the operation requires a restart
   */
  enableRequiresRestart: function XPI_enableRequiresRestart(aAddon) {
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
      if (Prefs.getBoolPref(PREF_EM_DSS_ENABLED))
        return false;

      // If the theme is already the theme in use then no restart is necessary.
      // This covers the case where the default theme is in use but a
      // lightweight theme is considered active.
      return aAddon.internalName != this.currentSkin;
    }

    return !aAddon.bootstrap;
  },

  /**
   * Tests whether disabling an add-on will require a restart.
   *
   * @param  aAddon
   *         The add-on to test
   * @return true if the operation requires a restart
   */
  disableRequiresRestart: function XPI_disableRequiresRestart(aAddon) {
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
      if (Prefs.getBoolPref(PREF_EM_DSS_ENABLED))
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
  installRequiresRestart: function XPI_installRequiresRestart(aAddon) {
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
    if (aAddon instanceof DBAddonInternal)
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
    if (isAddonDisabled(aAddon))
      return false;

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
  uninstallRequiresRestart: function XPI_uninstallRequiresRestart(aAddon) {
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
   * @return a JavaScript scope
   */
  loadBootstrapScope: function XPI_loadBootstrapScope(aId, aFile, aVersion, aType) {
    // Mark the add-on as active for the crash reporter before loading
    this.bootstrappedAddons[aId] = {
      version: aVersion,
      type: aType,
      descriptor: aFile.persistentDescriptor
    };
    this.persistBootstrappedAddons();
    this.addAddonsToCrashReporter();

    // Locales only contain chrome and can't have bootstrap scripts
    if (aType == "locale") {
      this.bootstrapScopes[aId] = null;
      return;
    }

    LOG("Loading bootstrap scope from " + aFile.path);

    let principal = Cc["@mozilla.org/systemprincipal;1"].
                    createInstance(Ci.nsIPrincipal);

    if (!aFile.exists()) {
      this.bootstrapScopes[aId] = new Components.utils.Sandbox(principal,
                                                               {sandboxName: aFile.path});
      ERROR("Attempted to load bootstrap scope from missing directory " + aFile.path);
      return;
    }

    let uri = getURIForResourceInFile(aFile, "bootstrap.js").spec;
    this.bootstrapScopes[aId] = new Components.utils.Sandbox(principal,
                                                             {sandboxName: uri});

    let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                 createInstance(Ci.mozIJSSubScriptLoader);

    try {
      // Copy the reason values from the global object into the bootstrap scope.
      for (let name in BOOTSTRAP_REASONS)
        this.bootstrapScopes[aId][name] = BOOTSTRAP_REASONS[name];

      // Add other stuff that extensions want.
      const features = [ "Worker", "ChromeWorker" ];

      for (let feature of features)
        this.bootstrapScopes[aId][feature] = gGlobalScope[feature];

      // As we don't want our caller to control the JS version used for the
      // bootstrap file, we run loadSubScript within the context of the
      // sandbox with the latest JS version set explicitly.
      if (aType == "dictionary") {
        this.bootstrapScopes[aId].__SCRIPT_URI_SPEC__ =
            "resource://gre/modules/SpellCheckDictionaryBootstrap.js"
      } else {
        this.bootstrapScopes[aId].__SCRIPT_URI_SPEC__ = uri;
      }
      Components.utils.evalInSandbox(
        "Components.classes['@mozilla.org/moz/jssubscript-loader;1'] \
                   .createInstance(Components.interfaces.mozIJSSubScriptLoader) \
                   .loadSubScript(__SCRIPT_URI_SPEC__);", this.bootstrapScopes[aId], "ECMAv5");
    }
    catch (e) {
      WARN("Error loading bootstrap.js for " + aId, e);
    }
  },

  /**
   * Unloads a bootstrap scope by dropping all references to it and then
   * updating the list of active add-ons with the crash reporter.
   *
   * @param  aId
   *         The add-on's ID
   */
  unloadBootstrapScope: function XPI_unloadBootstrapScope(aId) {
    delete this.bootstrapScopes[aId];
    delete this.bootstrappedAddons[aId];
    this.persistBootstrappedAddons();
    this.addAddonsToCrashReporter();
  },

  /**
   * Calls a bootstrap method for an add-on.
   *
   * @param  aId
   *         The ID of the add-on
   * @param  aVersion
   *         The version of the add-on
   * @param  aType
   *         The type for the add-on
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
  callBootstrapMethod: function XPI_callBootstrapMethod(aId, aVersion, aType, aFile,
                                                        aMethod, aReason, aExtraParams) {
    // Never call any bootstrap methods in safe mode
    if (Services.appinfo.inSafeMode)
      return;

    if (aMethod == "startup")
      Components.manager.addBootstrappedManifestLocation(aFile);

    try {
      // Load the scope if it hasn't already been loaded
      if (!(aId in this.bootstrapScopes))
        this.loadBootstrapScope(aId, aFile, aVersion, aType);

      // Nothing to call for locales
      if (aType == "locale")
        return;

      if (!(aMethod in this.bootstrapScopes[aId])) {
        WARN("Add-on " + aId + " is missing bootstrap method " + aMethod);
        return;
      }

      let params = {
        id: aId,
        version: aVersion,
        installPath: aFile.clone(),
        resourceURI: getURIForResourceInFile(aFile, "")
      };

      if (aExtraParams) {
        for (let key in aExtraParams) {
          params[key] = aExtraParams[key];
        }
      }

      LOG("Calling bootstrap method " + aMethod + " on " + aId + " version " +
          aVersion);
      try {
        this.bootstrapScopes[aId][aMethod](params, aReason);
      }
      catch (e) {
        WARN("Exception running bootstrap method " + aMethod + " on " +
             aId, e);
      }
    }
    finally {
      if (aMethod == "shutdown")
        Components.manager.removeBootstrappedManifestLocation(aFile);
    }
  },

  /**
   * Updates the appDisabled property for all add-ons.
   */
  updateAllAddonDisabledStates: function XPI_updateAllAddonDisabledStates() {
    let addons = XPIDatabase.getAddons();
    addons.forEach(function(aAddon) {
      this.updateAddonDisabledState(aAddon);
    }, this);
  },

  /**
   * Updates the disabled state for an add-on. Its appDisabled property will be
   * calculated and if the add-on is changed appropriate notifications will be
   * sent out to the registered AddonListeners.
   *
   * @param  aAddon
   *         The DBAddonInternal to update
   * @param  aUserDisabled
   *         Value for the userDisabled property. If undefined the value will
   *         not change
   * @param  aSoftDisabled
   *         Value for the softDisabled property. If undefined the value will
   *         not change. If true this will force userDisabled to be true
   * @throws if addon is not a DBAddonInternal
   */
  updateAddonDisabledState: function XPI_updateAddonDisabledState(aAddon,
                                                                  aUserDisabled,
                                                                  aSoftDisabled) {
    if (!(aAddon instanceof DBAddonInternal))
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
      return;

    let wasDisabled = isAddonDisabled(aAddon);
    let isDisabled = aUserDisabled || aSoftDisabled || appDisabled;

    // If appDisabled changes but the result of isAddonDisabled() doesn't,
    // no onDisabling/onEnabling is sent - so send a onPropertyChanged.
    let appDisabledChanged = aAddon.appDisabled != appDisabled;

    // Update the properties in the database
    XPIDatabase.setAddonProperties(aAddon, {
      userDisabled: aUserDisabled,
      appDisabled: appDisabled,
      softDisabled: aSoftDisabled
    });

    if (appDisabledChanged) {
      AddonManagerPrivate.callAddonListeners("onPropertyChanged",
                                            aAddon,
                                            ["appDisabled"]);
    }

    // If the add-on is not visible or the add-on is not changing state then
    // there is no need to do anything else
    if (!aAddon.visible || (wasDisabled == isDisabled))
      return;

    // Flag that active states in the database need to be updated on shutdown
    Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);

    let wrapper = createWrapper(aAddon);
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
        aAddon.active = !isDisabled;
        XPIDatabase.updateAddonActive(aAddon);
        if (isDisabled) {
          if (aAddon.bootstrap) {
            let file = aAddon._installLocation.getLocationForID(aAddon.id);
            this.callBootstrapMethod(aAddon.id, aAddon.version, aAddon.type, file, "shutdown",
                                     BOOTSTRAP_REASONS.ADDON_DISABLE);
            this.unloadBootstrapScope(aAddon.id);
          }
          AddonManagerPrivate.callAddonListeners("onDisabled", wrapper);
        }
        else {
          if (aAddon.bootstrap) {
            let file = aAddon._installLocation.getLocationForID(aAddon.id);
            this.callBootstrapMethod(aAddon.id, aAddon.version, aAddon.type, file, "startup",
                                     BOOTSTRAP_REASONS.ADDON_ENABLE);
          }
          AddonManagerPrivate.callAddonListeners("onEnabled", wrapper);
        }
      }
    }

    // Notify any other providers that a new theme has been enabled
    if (aAddon.type == "theme" && !isDisabled)
      AddonManagerPrivate.notifyAddonChanged(aAddon.id, aAddon.type, needsRestart);
  },

  /**
   * Uninstalls an add-on, immediately if possible or marks it as pending
   * uninstall if not.
   *
   * @param  aAddon
   *         The DBAddonInternal to uninstall
   * @throws if the addon cannot be uninstalled because it is in an install
   *         location that does not allow it
   */
  uninstallAddon: function XPI_uninstallAddon(aAddon) {
    if (!(aAddon instanceof DBAddonInternal))
      throw new Error("Can only uninstall installed addons.");

    if (aAddon._installLocation.locked)
      throw new Error("Cannot uninstall addons from locked install locations");

    if ("_hasResourceCache" in aAddon)
      aAddon._hasResourceCache = new Map();

    // Inactive add-ons don't require a restart to uninstall
    let requiresRestart = this.uninstallRequiresRestart(aAddon);

    if (requiresRestart) {
      // We create an empty directory in the staging directory to indicate that
      // an uninstall is necessary on next startup.
      let stage = aAddon._installLocation.getStagingDir();
      stage.append(aAddon.id);
      if (!stage.exists())
        stage.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

      XPIDatabase.setAddonProperties(aAddon, {
        pendingUninstall: true
      });
      Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);
    }

    // If the add-on is not visible then there is no need to notify listeners.
    if (!aAddon.visible)
      return;

    let wrapper = createWrapper(aAddon);
    AddonManagerPrivate.callAddonListeners("onUninstalling", wrapper,
                                           requiresRestart);

    // Reveal the highest priority add-on with the same ID
    function revealAddon(aAddon) {
      XPIDatabase.makeAddonVisible(aAddon);

      let wrappedAddon = createWrapper(aAddon);
      AddonManagerPrivate.callAddonListeners("onInstalling", wrappedAddon, false);

      if (!isAddonDisabled(aAddon) && !XPIProvider.enableRequiresRestart(aAddon)) {
        aAddon.active = true;
        XPIDatabase.updateAddonActive(aAddon);
      }

      if (aAddon.bootstrap) {
        let file = aAddon._installLocation.getLocationForID(aAddon.id);
        XPIProvider.callBootstrapMethod(aAddon.id, aAddon.version, aAddon.type, file,
                                        "install", BOOTSTRAP_REASONS.ADDON_INSTALL);

        if (aAddon.active) {
          XPIProvider.callBootstrapMethod(aAddon.id, aAddon.version, aAddon.type, file,
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

    function checkInstallLocation(aPos) {
      if (aPos < 0)
        return;

      let location = XPIProvider.installLocations[aPos];
      XPIDatabase.getAddonInLocation(aAddon.id, location.name,
        function checkInstallLocation_getAddonInLocation(aNewAddon) {
        if (aNewAddon)
          revealAddon(aNewAddon);
        else
          checkInstallLocation(aPos - 1);
      })
    }

    if (!requiresRestart) {
      if (aAddon.bootstrap) {
        let file = aAddon._installLocation.getLocationForID(aAddon.id);
        if (aAddon.active) {
          this.callBootstrapMethod(aAddon.id, aAddon.version, aAddon.type, file,
                                   "shutdown",
                                   BOOTSTRAP_REASONS.ADDON_UNINSTALL);
        }

        this.callBootstrapMethod(aAddon.id, aAddon.version, aAddon.type, file,
                                 "uninstall",
                                 BOOTSTRAP_REASONS.ADDON_UNINSTALL);
        this.unloadBootstrapScope(aAddon.id);
        flushStartupCache();
      }
      aAddon._installLocation.uninstallAddon(aAddon.id);
      XPIDatabase.removeAddonMetadata(aAddon);
      AddonManagerPrivate.callAddonListeners("onUninstalled", wrapper);

      checkInstallLocation(this.installLocations.length - 1);
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
  cancelUninstallAddon: function XPI_cancelUninstallAddon(aAddon) {
    if (!(aAddon instanceof DBAddonInternal))
      throw new Error("Can only cancel uninstall for installed addons.");

    cleanStagingDir(aAddon._installLocation.getStagingDir(), [aAddon.id]);

    XPIDatabase.setAddonProperties(aAddon, {
      pendingUninstall: false
    });

    if (!aAddon.visible)
      return;

    Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);

    // TODO hide hidden add-ons (bug 557710)
    let wrapper = createWrapper(aAddon);
    AddonManagerPrivate.callAddonListeners("onOperationCancelled", wrapper);

    // Notify any other providers that this theme is now enabled again.
    if (aAddon.type == "theme" && aAddon.active)
      AddonManagerPrivate.notifyAddonChanged(aAddon.id, aAddon.type, false);
  }
};


function getHashStringForCrypto(aCrypto) {
  // return the two-digit hexadecimal code for a byte
  function toHexString(charCode)
    ("0" + charCode.toString(16)).slice(-2);

  // convert the binary hash data to a hex string.
  let binary = aCrypto.finish(false);
  return [toHexString(binary.charCodeAt(i)) for (i in binary)].join("").toLowerCase()
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
 * @param  aLoadGroup
 *         The nsILoadGroup to associate any requests with
 * @throws if the url is the url of a local file and the hash does not match
 *         or the add-on does not contain an valid install manifest
 */
function AddonInstall(aInstallLocation, aUrl, aHash, aReleaseNotesURI,
                      aExistingAddon, aLoadGroup) {
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
  this.loadGroup = aLoadGroup;
  this.listeners = [];
  this.icons = {};
  this.existingAddon = aExistingAddon;
  this.error = 0;
  if (aLoadGroup)
    this.window = aLoadGroup.notificationCallbacks
                            .getInterface(Ci.nsIDOMWindow);
  else
    this.window = null;
}

AddonInstall.prototype = {
  installLocation: null,
  wrapper: null,
  stream: null,
  crypto: null,
  originalHash: null,
  hash: null,
  loadGroup: null,
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
  initStagedInstall: function AI_initStagedInstall(aManifest) {
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
  initLocalInstall: function AI_initLocalInstall(aCallback) {
    this.file = this.sourceURI.QueryInterface(Ci.nsIFileURL).file;

    if (!this.file.exists()) {
      WARN("XPI file " + this.file.path + " does not exist");
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
        WARN("Unknown hash algorithm " + this.hash.algorithm);
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
        WARN("File hash (" + calculatedHash + ") did not match provided hash (" +
             this.hash.data + ")");
        this.state = AddonManager.STATE_DOWNLOAD_FAILED;
        this.error = AddonManager.ERROR_INCORRECT_HASH;
        aCallback(this);
        return;
      }
    }

    try {
      let self = this;
      this.loadManifest(function  initLocalInstall_loadManifest() {
        XPIDatabase.getVisibleAddonForID(self.addon.id, function initLocalInstall_getVisibleAddon(aAddon) {
          self.existingAddon = aAddon;
          if (aAddon)
            applyBlocklistChanges(aAddon, self.addon);
          self.addon.updateDate = Date.now();
          self.addon.installDate = aAddon ? aAddon.installDate : self.addon.updateDate;

          if (!self.addon.isCompatible) {
            // TODO Should we send some event here?
            self.state = AddonManager.STATE_CHECKING;
            new UpdateChecker(self.addon, {
              onUpdateFinished: function updateChecker_onUpdateFinished(aAddon) {
                self.state = AddonManager.STATE_DOWNLOADED;
                XPIProvider.installs.push(self);
                AddonManagerPrivate.callInstallListeners("onNewInstall",
                                                         self.listeners,
                                                         self.wrapper);

                aCallback(self);
              }
            }, AddonManager.UPDATE_WHEN_ADDON_INSTALLED);
          }
          else {
            XPIProvider.installs.push(self);
            AddonManagerPrivate.callInstallListeners("onNewInstall",
                                                     self.listeners,
                                                     self.wrapper);

            aCallback(self);
          }
        });
      });
    }
    catch (e) {
      WARN("Invalid XPI", e);
      this.state = AddonManager.STATE_DOWNLOAD_FAILED;
      this.error = AddonManager.ERROR_CORRUPT_FILE;
      aCallback(this);
      return;
    }
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
  initAvailableDownload: function AI_initAvailableDownload(aName, aType, aIcons, aVersion, aCallback) {
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

    aCallback(this);
  },

  /**
   * Starts installation of this add-on from whatever state it is currently at
   * if possible.
   *
   * @throws if installation cannot proceed from the current state
   */
  install: function AI_install() {
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
  cancel: function AI_cancel() {
    switch (this.state) {
    case AddonManager.STATE_DOWNLOADING:
      if (this.channel)
        this.channel.cancel(Cr.NS_BINDING_ABORTED);
    case AddonManager.STATE_AVAILABLE:
    case AddonManager.STATE_DOWNLOADED:
      LOG("Cancelling download of " + this.sourceURI.spec);
      this.state = AddonManager.STATE_CANCELLED;
      XPIProvider.removeActiveInstall(this);
      AddonManagerPrivate.callInstallListeners("onDownloadCancelled",
                                               this.listeners, this.wrapper);
      this.removeTemporaryFile();
      break;
    case AddonManager.STATE_INSTALLED:
      LOG("Cancelling install of " + this.addon.id);
      let xpi = this.installLocation.getStagingDir();
      xpi.append(this.addon.id + ".xpi");
      flushJarCache(xpi);
      cleanStagingDir(this.installLocation.getStagingDir(),
                      [this.addon.id, this.addon.id + ".xpi",
                       this.addon.id + ".json"]);
      this.state = AddonManager.STATE_CANCELLED;
      XPIProvider.removeActiveInstall(this);

      if (this.existingAddon) {
        delete this.existingAddon.pendingUpgrade;
        this.existingAddon.pendingUpgrade = null;
      }

      AddonManagerPrivate.callAddonListeners("onOperationCancelled", createWrapper(this.addon));

      AddonManagerPrivate.callInstallListeners("onInstallCancelled",
                                               this.listeners, this.wrapper);
      break;
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
  addListener: function AI_addListener(aListener) {
    if (!this.listeners.some(function addListener_matchListener(i) { return i == aListener; }))
      this.listeners.push(aListener);
  },

  /**
   * Removes an InstallListener for this instance if it is registered.
   *
   * @param  aListener
   *         The InstallListener to remove
   */
  removeListener: function AI_removeListener(aListener) {
    this.listeners = this.listeners.filter(function removeListener_filterListener(i) {
      return i != aListener;
    });
  },

  /**
   * Removes the temporary file owned by this AddonInstall if there is one.
   */
  removeTemporaryFile: function AI_removeTemporaryFile() {
    // Only proceed if this AddonInstall owns its XPI file
    if (!this.ownsTempFile)
      return;

    try {
      this.file.remove(true);
      this.ownsTempFile = false;
    }
    catch (e) {
      WARN("Failed to remove temporary file " + this.file.path, e);
    }
  },

  /**
   * Updates the sourceURI and releaseNotesURI values on the Addon being
   * installed by this AddonInstall instance.
   */
  updateAddonURIs: function AI_updateAddonURIs() {
    this.addon.sourceURI = this.sourceURI.spec;
    if (this.releaseNotesURI)
      this.addon.releaseNotesURI = this.releaseNotesURI.spec;
  },

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
   * @param  aCallback
   *         A function to call when all of the add-on manifests have been
   *         loaded.
   */
  loadMultipackageManifests: function AI_loadMultipackageManifests(aZipReader,
                                                                   aCallback) {
    let files = [];
    let entries = aZipReader.findEntries("(*.[Xx][Pp][Ii]|*.[Jj][Aa][Rr])");
    while (entries.hasMore()) {
      let entryName = entries.getNext();
      var target = getTemporaryFile();
      try {
        aZipReader.extract(entryName, target);
        files.push(target);
      }
      catch (e) {
        WARN("Failed to extract " + entryName + " from multi-package " +
             "XPI", e);
        target.remove(false);
      }
    }

    aZipReader.close();

    if (files.length == 0) {
      throw new Error("Multi-package XPI does not contain any packages " +
                      "to install");
    }

    let addon = null;

    // Find the first file that has a valid install manifest and use it for
    // the add-on that this AddonInstall instance will install.
    while (files.length > 0) {
      this.removeTemporaryFile();
      this.file = files.shift();
      this.ownsTempFile = true;
      try {
        addon = loadManifestFromZipFile(this.file);
        break;
      }
      catch (e) {
        WARN(this.file.leafName + " cannot be installed from multi-package " +
             "XPI", e);
      }
    }

    if (!addon) {
      // No valid add-on was found
      aCallback();
      return;
    }

    this.addon = addon;

    this.updateAddonURIs();

    this.addon._install = this;
    this.name = this.addon.selectedLocale.name;
    this.type = this.addon.type;
    this.version = this.addon.version;

    // Setting the iconURL to something inside the XPI locks the XPI and
    // makes it impossible to delete on Windows.
    //let newIcon = createWrapper(this.addon).iconURL;
    //if (newIcon)
    //  this.iconURL = newIcon;

    // Create new AddonInstall instances for every remaining file
    if (files.length > 0) {
      this.linkedInstalls = [];
      let count = 0;
      let self = this;
      files.forEach(function(file) {
        AddonInstall.createInstall(function loadMultipackageManifests_createInstall(aInstall) {
          // Ignore bad add-ons (createInstall will have logged the error)
          if (aInstall.state == AddonManager.STATE_DOWNLOAD_FAILED) {
            // Manually remove the temporary file
            file.remove(true);
          }
          else {
            // Make the new install own its temporary file
            aInstall.ownsTempFile = true;

            self.linkedInstalls.push(aInstall)

            aInstall.sourceURI = self.sourceURI;
            aInstall.releaseNotesURI = self.releaseNotesURI;
            aInstall.updateAddonURIs();
          }

          count++;
          if (count == files.length)
            aCallback();
        }, file);
      }, this);
    }
    else {
      aCallback();
    }
  },

  /**
   * Called after the add-on is a local file and the signature and install
   * manifest can be read.
   *
   * @param  aCallback
   *         A function to call when the manifest has been loaded
   * @throws if the add-on does not contain a valid install manifest or the
   *         XPI is incorrectly signed
   */
  loadManifest: function AI_loadManifest(aCallback) {
    function addRepositoryData(aAddon) {
      // Try to load from the existing cache first
      AddonRepository.getCachedAddonByID(aAddon.id, function loadManifest_getCachedAddonByID(aRepoAddon) {
        if (aRepoAddon) {
          aAddon._repositoryAddon = aRepoAddon;
          aAddon.compatibilityOverrides = aRepoAddon.compatibilityOverrides;
          aAddon.appDisabled = !isUsableAddon(aAddon);
          aCallback();
          return;
        }

        // It wasn't there so try to re-download it
        AddonRepository.cacheAddons([aAddon.id], function loadManifest_cacheAddons() {
          AddonRepository.getCachedAddonByID(aAddon.id, function loadManifest_getCachedAddonByID(aRepoAddon) {
            aAddon._repositoryAddon = aRepoAddon;
            aAddon.compatibilityOverrides = aRepoAddon ?
                                              aRepoAddon.compatibilityOverrides :
                                              null;
            aAddon.appDisabled = !isUsableAddon(aAddon);
            aCallback();
          });
        });
      });
    }

    let zipreader = Cc["@mozilla.org/libjar/zip-reader;1"].
                    createInstance(Ci.nsIZipReader);
    try {
      zipreader.open(this.file);
    }
    catch (e) {
      zipreader.close();
      throw e;
    }

    let principal = zipreader.getCertificatePrincipal(null);
    if (principal && principal.hasCertificate) {
      LOG("Verifying XPI signature");
      if (verifyZipSigning(zipreader, principal)) {
        let x509 = principal.certificate;
        if (x509 instanceof Ci.nsIX509Cert)
          this.certificate = x509;
        if (this.certificate && this.certificate.commonName.length > 0)
          this.certName = this.certificate.commonName;
        else
          this.certName = principal.prettyName;
      }
      else {
        zipreader.close();
        throw new Error("XPI is incorrectly signed");
      }
    }

    try {
      this.addon = loadManifestFromZipReader(zipreader);
    }
    catch (e) {
      zipreader.close();
      throw e;
    }

    if (this.addon.type == "multipackage") {
      let self = this;
      this.loadMultipackageManifests(zipreader, function loadManifest_loadMultipackageManifests() {
        addRepositoryData(self.addon);
      });
      return;
    }

    zipreader.close();

    this.updateAddonURIs();

    this.addon._install = this;
    this.name = this.addon.selectedLocale.name;
    this.type = this.addon.type;
    this.version = this.addon.version;

    // Setting the iconURL to something inside the XPI locks the XPI and
    // makes it impossible to delete on Windows.
    //let newIcon = createWrapper(this.addon).iconURL;
    //if (newIcon)
    //  this.iconURL = newIcon;

    addRepositoryData(this.addon);
  },

  observe: function AI_observe(aSubject, aTopic, aData) {
    // Network is going offline
    this.cancel();
  },

  /**
   * Starts downloading the add-on's XPI file.
   */
  startDownload: function AI_startDownload() {
    this.state = AddonManager.STATE_DOWNLOADING;
    if (!AddonManagerPrivate.callInstallListeners("onDownloadStarted",
                                                  this.listeners, this.wrapper)) {
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
      LOG("Waiting for previous download to complete");
      this.restartDownload = true;
      return;
    }

    this.openChannel();
  },

  openChannel: function AI_openChannel() {
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
      WARN("Failed to start download", e);
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
      Components.utils.import("resource://gre/modules/CertUtils.jsm");
      let requireBuiltIn = Prefs.getBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, true);
      this.badCertHandler = new BadCertHandler(!requireBuiltIn);

      this.channel = NetUtil.newChannel(this.sourceURI);
      this.channel.notificationCallbacks = this;
      if (this.channel instanceof Ci.nsIHttpChannelInternal)
        this.channel.forceAllowThirdPartyCookie = true;
      this.channel.asyncOpen(listener, null);

      Services.obs.addObserver(this, "network:offline-about-to-go-offline", false);
    }
    catch (e) {
      WARN("Failed to start download", e);
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
  onDataAvailable: function AI_onDataAvailable(aRequest, aContext, aInputstream,
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
  asyncOnChannelRedirect: function AI_asyncOnChannelRedirect(aOldChannel, aNewChannel, aFlags, aCallback) {
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
  onStartRequest: function AI_onStartRequest(aRequest, aContext) {
    this.crypto = Cc["@mozilla.org/security/hash;1"].
                  createInstance(Ci.nsICryptoHash);
    if (this.hash) {
      try {
        this.crypto.initWithString(this.hash.algorithm);
      }
      catch (e) {
        WARN("Unknown hash algorithm " + this.hash.algorithm);
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
      LOG("Download started for " + this.sourceURI.spec + " to file " +
          this.file.path);
    }
  },

  /**
   * The download is complete.
   *
   * @see nsIStreamListener
   */
  onStopRequest: function AI_onStopRequest(aRequest, aContext, aStatus) {
    this.stream.close();
    this.channel = null;
    this.badCerthandler = null;
    Services.obs.removeObserver(this, "network:offline-about-to-go-offline");

    // If the download was cancelled then all events will have already been sent
    if (aStatus == Cr.NS_BINDING_ABORTED) {
      this.removeTemporaryFile();
      if (this.restartDownload)
        this.openChannel();
      return;
    }

    LOG("Download of " + this.sourceURI.spec + " completed.");

    if (Components.isSuccessCode(aStatus)) {
      if (!(aRequest instanceof Ci.nsIHttpChannel) || aRequest.requestSucceeded) {
        if (!this.hash && (aRequest instanceof Ci.nsIChannel)) {
          try {
            checkCert(aRequest,
                      !Prefs.getBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, true));
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
        try {
          let self = this;
          this.loadManifest(function onStopRequest_loadManifest() {
            if (self.addon.isCompatible) {
              self.downloadCompleted();
            }
            else {
              // TODO Should we send some event here (bug 557716)?
              self.state = AddonManager.STATE_CHECKING;
              new UpdateChecker(self.addon, {
                onUpdateFinished: function onStopRequest_onUpdateFinished(aAddon) {
                  self.downloadCompleted();
                }
              }, AddonManager.UPDATE_WHEN_ADDON_INSTALLED);
            }
          });
        }
        catch (e) {
          this.downloadFailed(AddonManager.ERROR_CORRUPT_FILE, e);
        }
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
  downloadFailed: function AI_downloadFailed(aReason, aError) {
    WARN("Download failed", aError);
    this.state = AddonManager.STATE_DOWNLOAD_FAILED;
    this.error = aReason;
    XPIProvider.removeActiveInstall(this);
    AddonManagerPrivate.callInstallListeners("onDownloadFailed", this.listeners,
                                             this.wrapper);

    // If the listener hasn't restarted the download then remove any temporary
    // file
    if (this.state == AddonManager.STATE_DOWNLOAD_FAILED)
      this.removeTemporaryFile();
  },

  /**
   * Notify listeners that the download completed.
   */
  downloadCompleted: function AI_downloadCompleted() {
    let self = this;
    XPIDatabase.getVisibleAddonForID(this.addon.id, function downloadCompleted_getVisibleAddonForID(aAddon) {
      if (aAddon)
        self.existingAddon = aAddon;

      self.state = AddonManager.STATE_DOWNLOADED;
      self.addon.updateDate = Date.now();

      if (self.existingAddon) {
        self.addon.existingAddonID = self.existingAddon.id;
        self.addon.installDate = self.existingAddon.installDate;
        applyBlocklistChanges(self.existingAddon, self.addon);
      }
      else {
        self.addon.installDate = self.addon.updateDate;
      }

      if (AddonManagerPrivate.callInstallListeners("onDownloadEnded",
                                                   self.listeners,
                                                   self.wrapper)) {
        // If a listener changed our state then do not proceed with the install
        if (self.state != AddonManager.STATE_DOWNLOADED)
          return;

        self.install();

        if (self.linkedInstalls) {
          self.linkedInstalls.forEach(function(aInstall) {
            aInstall.install();
          });
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
  startInstall: function AI_startInstall() {
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
    XPIProvider.installs.forEach(function(aInstall) {
      if (aInstall.state == AddonManager.STATE_INSTALLED &&
          aInstall.installLocation == this.installLocation &&
          aInstall.addon.id == this.addon.id)
        aInstall.cancel();
    }, this);

    let isUpgrade = this.existingAddon &&
                    this.existingAddon._installLocation == this.installLocation;
    let requiresRestart = XPIProvider.installRequiresRestart(this.addon);

    LOG("Starting install of " + this.sourceURI.spec);
    AddonManagerPrivate.callAddonListeners("onInstalling",
                                           createWrapper(this.addon),
                                           requiresRestart);
    let stagedAddon = this.installLocation.getStagingDir();

    try {
      // First stage the file regardless of whether restarting is necessary
      if (this.addon.unpack || Prefs.getBoolPref(PREF_XPI_UNPACK, false)) {
        LOG("Addon " + this.addon.id + " will be installed as " +
            "an unpacked directory");
        stagedAddon.append(this.addon.id);
        if (stagedAddon.exists())
          recursiveRemove(stagedAddon);
        stagedAddon.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
        extractFiles(this.file, stagedAddon);
      }
      else {
        LOG("Addon " + this.addon.id + " will be installed as " +
            "a packed xpi");
        stagedAddon.append(this.addon.id + ".xpi");
        if (stagedAddon.exists())
          stagedAddon.remove(true);
        this.file.copyTo(this.installLocation.getStagingDir(),
                         this.addon.id + ".xpi");
      }

      if (requiresRestart) {
        // Point the add-on to its extracted files as the xpi may get deleted
        this.addon._sourceBundle = stagedAddon;

        // Cache the AddonInternal as it may have updated compatibiltiy info
        let stagedJSON = stagedAddon.clone();
        stagedJSON.leafName = this.addon.id + ".json";
        if (stagedJSON.exists())
          stagedJSON.remove(true);
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

        LOG("Install of " + this.sourceURI.spec + " completed.");
        this.state = AddonManager.STATE_INSTALLED;
        if (isUpgrade) {
          delete this.existingAddon.pendingUpgrade;
          this.existingAddon.pendingUpgrade = this.addon;
        }
        AddonManagerPrivate.callInstallListeners("onInstallEnded",
                                                 this.listeners, this.wrapper,
                                                 createWrapper(this.addon));
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
            let file = this.existingAddon._installLocation
                           .getLocationForID(this.existingAddon.id);
            if (this.existingAddon.active) {
              XPIProvider.callBootstrapMethod(this.existingAddon.id,
                                              this.existingAddon.version,
                                              this.existingAddon.type, file,
                                              "shutdown", reason,
                                              { newVersion: this.addon.version });
            }

            XPIProvider.callBootstrapMethod(this.existingAddon.id,
                                            this.existingAddon.version,
                                            this.existingAddon.type, file,
                                            "uninstall", reason,
                                            { newVersion: this.addon.version });
            XPIProvider.unloadBootstrapScope(this.existingAddon.id);
            flushStartupCache();
          }

          if (!isUpgrade && this.existingAddon.active) {
            this.existingAddon.active = false;
            XPIDatabase.updateAddonActive(this.existingAddon);
          }
        }

        // Install the new add-on into its final location
        let existingAddonID = this.existingAddon ? this.existingAddon.id : null;
        let file = this.installLocation.installAddon(this.addon.id, stagedAddon,
                                                     existingAddonID);
        cleanStagingDir(stagedAddon.parent, []);

        // Update the metadata in the database
        this.addon._sourceBundle = file;
        this.addon._installLocation = this.installLocation;
        this.addon.updateDate = recursiveLastModifiedTime(file);
        this.addon.visible = true;
        if (isUpgrade) {
          XPIDatabase.updateAddonMetadata(this.existingAddon, this.addon,
                                          file.persistentDescriptor);
        }
        else {
          this.addon.installDate = this.addon.updateDate;
          this.addon.active = (this.addon.visible && !isAddonDisabled(this.addon))
          XPIDatabase.addAddonMetadata(this.addon, file.persistentDescriptor);
        }

        // Retrieve the new DBAddonInternal for the add-on we just added
        let self = this;
        XPIDatabase.getAddonInLocation(this.addon.id, this.installLocation.name,
                                       function startInstall_getAddonInLocation(a) {
          self.addon = a;
          let extraParams = {};
          if (self.existingAddon) {
            extraParams.oldVersion = self.existingAddon.version;
          }

          if (self.addon.bootstrap) {
            XPIProvider.callBootstrapMethod(self.addon.id, self.addon.version,
                                            self.addon.type, file, "install",
                                            reason, extraParams);
          }

          AddonManagerPrivate.callAddonListeners("onInstalled",
                                                 createWrapper(self.addon));

          LOG("Install of " + self.sourceURI.spec + " completed.");
          self.state = AddonManager.STATE_INSTALLED;
          AddonManagerPrivate.callInstallListeners("onInstallEnded",
                                                   self.listeners, self.wrapper,
                                                   createWrapper(self.addon));

          if (self.addon.bootstrap) {
            if (self.addon.active) {
              XPIProvider.callBootstrapMethod(self.addon.id, self.addon.version,
                                              self.addon.type, file, "startup",
                                              reason, extraParams);
            }
            else {
              XPIProvider.unloadBootstrapScope(self.addon.id);
            }
          }
        });
      }
    }
    catch (e) {
      WARN("Failed to install", e);
      if (stagedAddon.exists())
        recursiveRemove(stagedAddon);
      this.state = AddonManager.STATE_INSTALL_FAILED;
      this.error = AddonManager.ERROR_FILE_ACCESS;
      XPIProvider.removeActiveInstall(this);
      AddonManagerPrivate.callInstallListeners("onInstallFailed",
                                               this.listeners,
                                               this.wrapper);
    }
    finally {
      this.removeTemporaryFile();
    }
  },

  getInterface: function AI_getInterface(iid) {
    if (iid.equals(Ci.nsIAuthPrompt2)) {
      var factory = Cc["@mozilla.org/prompter;1"].
                    getService(Ci.nsIPromptFactory);
      return factory.getPrompt(this.window, Ci.nsIAuthPrompt);
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
AddonInstall.createStagedInstall = function AI_createStagedInstall(aInstallLocation, aDir, aManifest) {
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
AddonInstall.createInstall = function AI_createInstall(aCallback, aFile) {
  let location = XPIProvider.installLocationsByName[KEY_APP_PROFILE];
  let url = Services.io.newFileURI(aFile);

  try {
    let install = new AddonInstall(location, url);
    install.initLocalInstall(aCallback);
  }
  catch(e) {
    ERROR("Error creating install", e);
    aCallback(null);
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
 * @param  aLoadGroup
 *         An nsILoadGroup to associate the download with
 */
AddonInstall.createDownload = function AI_createDownload(aCallback, aUri, aHash, aName, aIcons,
                                       aVersion, aLoadGroup) {
  let location = XPIProvider.installLocationsByName[KEY_APP_PROFILE];
  let url = NetUtil.newURI(aUri);

  let install = new AddonInstall(location, url, aHash, null, null, aLoadGroup);
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
AddonInstall.createUpdate = function AI_createUpdate(aCallback, aAddon, aUpdate) {
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

/**
 * Creates a wrapper for an AddonInstall that only exposes the public API
 *
 * @param  install
 *         The AddonInstall to create a wrapper for
 */
function AddonInstallWrapper(aInstall) {
#ifdef MOZ_EM_DEBUG
  this.__defineGetter__("__AddonInstallInternal__", function AIW_debugGetter() {
    return aInstall;
  });
#endif

  ["name", "type", "version", "icons", "releaseNotesURI", "file", "state", "error",
   "progress", "maxProgress", "certificate", "certName"].forEach(function(aProp) {
    this.__defineGetter__(aProp, function AIW_propertyGetter() aInstall[aProp]);
  }, this);

  this.__defineGetter__("iconURL", function AIW_iconURL() aInstall.icons[32]);

  this.__defineGetter__("existingAddon", function AIW_existingAddonGetter() {
    return createWrapper(aInstall.existingAddon);
  });
  this.__defineGetter__("addon", function AIW_addonGetter() createWrapper(aInstall.addon));
  this.__defineGetter__("sourceURI", function AIW_sourceURIGetter() aInstall.sourceURI);

  this.__defineGetter__("linkedInstalls", function AIW_linkedInstallsGetter() {
    if (!aInstall.linkedInstalls)
      return null;
    return [i.wrapper for each (i in aInstall.linkedInstalls)];
  });

  this.install = function AIW_install() {
    aInstall.install();
  }

  this.cancel = function AIW_cancel() {
    aInstall.cancel();
  }

  this.addListener = function AIW_addListener(listener) {
    aInstall.addListener(listener);
  }

  this.removeListener = function AIW_removeListener(listener) {
    aInstall.removeListener(listener);
  }
}

AddonInstallWrapper.prototype = {};

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

  Components.utils.import("resource://gre/modules/AddonUpdateChecker.jsm");

  this.addon = aAddon;
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
  AddonUpdateChecker.checkForUpdates(aAddon.id, aAddon.updateKey,
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
  callListener: function UC_callListener(aMethod, ...aArgs) {
    if (!(aMethod in this.listener))
      return;

    try {
      this.listener[aMethod].apply(this.listener, aArgs);
    }
    catch (e) {
      LOG("Exception calling UpdateListener method " + aMethod + ": " + e);
    }
  },

  /**
   * Called when AddonUpdateChecker completes the update check
   *
   * @param  updates
   *         The list of update details for the add-on
   */
  onUpdateCheckComplete: function UC_onUpdateCheckComplete(aUpdates) {
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
      this.callListener("onCompatibilityUpdateAvailable", createWrapper(this.addon));
    else
      this.callListener("onNoCompatibilityUpdateAvailable", createWrapper(this.addon));

    function sendUpdateAvailableMessages(aSelf, aInstall) {
      if (aInstall) {
        aSelf.callListener("onUpdateAvailable", createWrapper(aSelf.addon),
                           aInstall.wrapper);
      }
      else {
        aSelf.callListener("onNoUpdateAvailable", createWrapper(aSelf.addon));
      }
      aSelf.callListener("onUpdateFinished", createWrapper(aSelf.addon),
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

    if (update && Services.vc.compare(this.addon.version, update.version) < 0) {
      for (let currentInstall of XPIProvider.installs) {
        // Skip installs that don't match the available update
        if (currentInstall.existingAddon != this.addon ||
            currentInstall.version != update.version)
          continue;

        // If the existing install has not yet started downloading then send an
        // available update notification. If it is already downloading then
        // don't send any available update notification
        if (currentInstall.state == AddonManager.STATE_AVAILABLE)
          sendUpdateAvailableMessages(this, currentInstall);
        else
          sendUpdateAvailableMessages(this, null);
        return;
      }

      let self = this;
      AddonInstall.createUpdate(function onUpdateCheckComplete_createUpdate(aInstall) {
        sendUpdateAvailableMessages(self, aInstall);
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
  onUpdateCheckError: function UC_onUpdateCheckError(aError) {
    this.callListener("onNoCompatibilityUpdateAvailable", createWrapper(this.addon));
    this.callListener("onNoUpdateAvailable", createWrapper(this.addon));
    this.callListener("onUpdateFinished", createWrapper(this.addon), aError);
  }
};

/**
 * The AddonInternal is an internal only representation of add-ons. It may
 * have come from the database (see DBAddonInternal below) or an install
 * manifest.
 */
function AddonInternal() {
}

AddonInternal.prototype = {
  _selectedLocale: null,
  active: false,
  visible: false,
  userDisabled: false,
  appDisabled: false,
  softDisabled: false,
  sourceURI: null,
  releaseNotesURI: null,
  foreignInstall: false,

  get isForeignInstall() {
    return this.foreignInstall;
  },
  set isForeignInstall(aVal) {
    this.foreignInstall = aVal;
  },

  get selectedLocale() {
    if (this._selectedLocale)
      return this._selectedLocale;
    let locale = findClosestLocale(this.locales);
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

    return matchedOS && !needsABI;
  },

  isCompatibleWith: function AddonInternal_isCompatibleWith(aAppVersion, aPlatformVersion) {
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

    let bs = Cc["@mozilla.org/extensions/blocklist;1"].
             getService(Ci.nsIBlocklistService);
    return bs.getAddonBlocklistState(this.id, this.version);
  },

  get blocklistURL() {
    let staticItem = findMatchingStaticBlocklistItem(this);
    if (staticItem) {
      let url = Services.urlFormatter.formatURLPref("extensions.blocklist.itemURL");
      return url.replace(/%blockID%/g, staticItem.blockID);
    }

    let bs = Cc["@mozilla.org/extensions/blocklist;1"].
             getService(Ci.nsIBlocklistService);
    return bs.getAddonBlocklistURL(this.id, this.version);
  },

  applyCompatibilityUpdate: function AddonInternal_applyCompatibilityUpdate(aUpdate, aSyncCompatibility) {
    this.targetApplications.forEach(function(aTargetApp) {
      aUpdate.targetApplications.forEach(function(aUpdateTarget) {
        if (aTargetApp.id == aUpdateTarget.id && (aSyncCompatibility ||
            Services.vc.compare(aTargetApp.maxVersion, aUpdateTarget.maxVersion) < 0)) {
          aTargetApp.minVersion = aUpdateTarget.minVersion;
          aTargetApp.maxVersion = aUpdateTarget.maxVersion;
        }
      });
    });
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
  toJSON: function AddonInternal_toJSON(aKey) {
    let obj = {};
    for (let prop in this) {
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
  importMetadata: function AddonInternal_importMetaData(aObj) {
    ["syncGUID", "targetApplications", "userDisabled", "softDisabled",
     "existingAddonID", "sourceURI", "releaseNotesURI", "installDate",
     "updateDate", "applyBackgroundUpdates", "compatibilityOverrides"]
    .forEach(function(aProp) {
      if (!(aProp in aObj))
        return;

      this[aProp] = aObj[aProp];
    }, this);

    // Compatibility info may have changed so update appDisabled
    this.appDisabled = !isUsableAddon(this);
  }
};

/**
 * The DBAddonInternal is a special AddonInternal that has been retrieved from
 * the database. Add-ons retrieved synchronously only have the basic metadata
 * the rest is filled out synchronously when needed. Asynchronously read add-ons
 * have all data available.
 */
function DBAddonInternal() {
  this.__defineGetter__("targetApplications", function DBA_targetApplicationsGetter() {
    delete this.targetApplications;
    return this.targetApplications = XPIDatabase._getTargetApplications(this);
  });

  this.__defineGetter__("targetPlatforms", function DBA_targetPlatformsGetter() {
    delete this.targetPlatforms;
    return this.targetPlatforms = XPIDatabase._getTargetPlatforms(this);
  });

  this.__defineGetter__("locales", function DBA_localesGetter() {
    delete this.locales;
    return this.locales = XPIDatabase._getLocales(this);
  });

  this.__defineGetter__("defaultLocale", function DBA_defaultLocaleGetter() {
    delete this.defaultLocale;
    return this.defaultLocale = XPIDatabase._getDefaultLocale(this);
  });

  this.__defineGetter__("pendingUpgrade", function DBA_pendingUpgradeGetter() {
    delete this.pendingUpgrade;
    for (let install of XPIProvider.installs) {
      if (install.state == AddonManager.STATE_INSTALLED &&
          !(install.addon instanceof DBAddonInternal) &&
          install.addon.id == this.id &&
          install.installLocation == this._installLocation) {
        return this.pendingUpgrade = install.addon;
      }
    };
  });
}

DBAddonInternal.prototype = {
  applyCompatibilityUpdate: function DBA_applyCompatibilityUpdate(aUpdate, aSyncCompatibility) {
    let changes = [];
    this.targetApplications.forEach(function(aTargetApp) {
      aUpdate.targetApplications.forEach(function(aUpdateTarget) {
        if (aTargetApp.id == aUpdateTarget.id && (aSyncCompatibility ||
            Services.vc.compare(aTargetApp.maxVersion, aUpdateTarget.maxVersion) < 0)) {
          aTargetApp.minVersion = aUpdateTarget.minVersion;
          aTargetApp.maxVersion = aUpdateTarget.maxVersion;
          changes.push(aUpdateTarget);
        }
      });
    });
    try {
      XPIDatabase.updateTargetApplications(this, changes);
    }
    catch (e) {
      // A failure just means that we discard the compatibility update
      ERROR("Failed to update target application info in the database for " +
            "add-on " + this.id, e);
      return;
    }
    XPIProvider.updateAddonDisabledState(this);
  }
}

DBAddonInternal.prototype.__proto__ = AddonInternal.prototype;
// Make it accessible to XPIDatabase.
XPIProvider.DBAddonInternal = DBAddonInternal;


/**
 * Creates an AddonWrapper for an AddonInternal.
 *
 * @param   addon
 *          The AddonInternal to wrap
 * @return  an AddonWrapper or null if addon was null
 */
function createWrapper(aAddon) {
  if (!aAddon)
    return null;
  if (!aAddon._wrapper) {
    aAddon._hasResourceCache = new Map();
    aAddon._wrapper = new AddonWrapper(aAddon);
  }
  return aAddon._wrapper;
}

/**
 * The AddonWrapper wraps an Addon to provide the data visible to consumers of
 * the public API.
 */
function AddonWrapper(aAddon) {
#ifdef MOZ_EM_DEBUG
  this.__defineGetter__("__AddonInternal__", function AW_debugGetter() {
    return aAddon;
  });
#endif

  function chooseValue(aObj, aProp) {
    let repositoryAddon = aAddon._repositoryAddon;
    let objValue = aObj[aProp];

    if (repositoryAddon && (aProp in repositoryAddon) &&
        (objValue === undefined || objValue === null)) {
      return [repositoryAddon[aProp], true];
    }

    return [objValue, false];
  }

  ["id", "syncGUID", "version", "type", "isCompatible", "isPlatformCompatible",
   "providesUpdatesSecurely", "blocklistState", "blocklistURL", "appDisabled",
   "softDisabled", "skinnable", "size", "foreignInstall", "hasBinaryComponents",
   "strictCompatibility", "compatibilityOverrides"].forEach(function(aProp) {
     this.__defineGetter__(aProp, function AddonWrapper_propertyGetter() aAddon[aProp]);
  }, this);

  ["fullDescription", "developerComments", "eula", "supportURL",
   "contributionURL", "contributionAmount", "averageRating", "reviewCount",
   "reviewURL", "totalDownloads", "weeklyDownloads", "dailyUsers",
   "repositoryStatus"].forEach(function(aProp) {
    this.__defineGetter__(aProp, function AddonWrapper_repoPropertyGetter() {
      if (aAddon._repositoryAddon)
        return aAddon._repositoryAddon[aProp];

      return null;
    });
  }, this);

  this.__defineGetter__("aboutURL", function AddonWrapper_aboutURLGetter() {
    return this.isActive ? aAddon["aboutURL"] : null;
  });

  ["installDate", "updateDate"].forEach(function(aProp) {
    this.__defineGetter__(aProp, function AddonWrapper_datePropertyGetter() new Date(aAddon[aProp]));
  }, this);

  ["sourceURI", "releaseNotesURI"].forEach(function(aProp) {
    this.__defineGetter__(aProp, function AddonWrapper_URIPropertyGetter() {
      let target = chooseValue(aAddon, aProp)[0];
      if (!target)
        return null;
      return NetUtil.newURI(target);
    });
  }, this);

  this.__defineGetter__("optionsURL", function AddonWrapper_optionsURLGetter() {
    if (this.isActive && aAddon.optionsURL)
      return aAddon.optionsURL;

    if (this.isActive && this.hasResource("options.xul"))
      return this.getResourceURI("options.xul").spec;

    return null;
  }, this);

  this.__defineGetter__("optionsType", function AddonWrapper_optionsTypeGetter() {
    if (!this.isActive)
      return null;

    let hasOptionsXUL = this.hasResource("options.xul");
    let hasOptionsURL = !!this.optionsURL;

    if (aAddon.optionsType) {
      switch (parseInt(aAddon.optionsType, 10)) {
      case AddonManager.OPTIONS_TYPE_DIALOG:
      case AddonManager.OPTIONS_TYPE_TAB:
        return hasOptionsURL ? aAddon.optionsType : null;
      case AddonManager.OPTIONS_TYPE_INLINE:
      case AddonManager.OPTIONS_TYPE_INLINE_INFO:
        return (hasOptionsXUL || hasOptionsURL) ? aAddon.optionsType : null;
      }
      return null;
    }

    if (hasOptionsXUL)
      return AddonManager.OPTIONS_TYPE_INLINE;

    if (hasOptionsURL)
      return AddonManager.OPTIONS_TYPE_DIALOG;

    return null;
  }, this);

  this.__defineGetter__("iconURL", function AddonWrapper_iconURLGetter() {
    return this.icons[32];
  }, this);

  this.__defineGetter__("icon64URL", function AddonWrapper_icon64URLGetter() {
    return this.icons[64];
  }, this);

  this.__defineGetter__("icons", function AddonWrapper_iconsGetter() {
    let icons = {};
    if (aAddon._repositoryAddon) {
      for (let size in aAddon._repositoryAddon.icons) {
        icons[size] = aAddon._repositoryAddon.icons[size];
      }
    }
    if (this.isActive && aAddon.iconURL) {
      icons[32] = aAddon.iconURL;
    } else if (this.hasResource("icon.png")) {
      icons[32] = this.getResourceURI("icon.png").spec;
    }
    if (this.isActive && aAddon.icon64URL) {
      icons[64] = aAddon.icon64URL;
    } else if (this.hasResource("icon64.png")) {
      icons[64] = this.getResourceURI("icon64.png").spec;
    }
    Object.freeze(icons);
    return icons;
  }, this);

  PROP_LOCALE_SINGLE.forEach(function(aProp) {
    this.__defineGetter__(aProp, function AddonWrapper_singleLocaleGetter() {
      // Override XPI creator if repository creator is defined
      if (aProp == "creator" &&
          aAddon._repositoryAddon && aAddon._repositoryAddon.creator) {
        return aAddon._repositoryAddon.creator;
      }

      let result = null;

      if (aAddon.active) {
        try {
          let pref = PREF_EM_EXTENSION_FORMAT + aAddon.id + "." + aProp;
          let value = Services.prefs.getComplexValue(pref,
                                                     Ci.nsIPrefLocalizedString);
          if (value.data)
            result = value.data;
        }
        catch (e) {
        }
      }

      if (result == null)
        [result, ] = chooseValue(aAddon.selectedLocale, aProp);

      if (aProp == "creator")
        return result ? new AddonManagerPrivate.AddonAuthor(result) : null;

      return result;
    });
  }, this);

  PROP_LOCALE_MULTI.forEach(function(aProp) {
    this.__defineGetter__(aProp, function AddonWrapper_multiLocaleGetter() {
      let results = null;
      let usedRepository = false;

      if (aAddon.active) {
        let pref = PREF_EM_EXTENSION_FORMAT + aAddon.id + "." +
                   aProp.substring(0, aProp.length - 1);
        let list = Services.prefs.getChildList(pref, {});
        if (list.length > 0) {
          list.sort();
          results = [];
          list.forEach(function(aPref) {
            let value = Services.prefs.getComplexValue(aPref,
                                                       Ci.nsIPrefLocalizedString);
            if (value.data)
              results.push(value.data);
          });
        }
      }

      if (results == null)
        [results, usedRepository] = chooseValue(aAddon.selectedLocale, aProp);

      if (results && !usedRepository) {
        results = results.map(function mapResult(aResult) {
          return new AddonManagerPrivate.AddonAuthor(aResult);
        });
      }

      return results;
    });
  }, this);

  this.__defineGetter__("screenshots", function AddonWrapper_screenshotsGetter() {
    let repositoryAddon = aAddon._repositoryAddon;
    if (repositoryAddon && ("screenshots" in repositoryAddon)) {
      let repositoryScreenshots = repositoryAddon.screenshots;
      if (repositoryScreenshots && repositoryScreenshots.length > 0)
        return repositoryScreenshots;
    }

    if (aAddon.type == "theme" && this.hasResource("preview.png")) {
      let url = this.getResourceURI("preview.png").spec;
      return [new AddonManagerPrivate.AddonScreenshot(url)];
    }

    return null;
  });

  this.__defineGetter__("applyBackgroundUpdates", function AddonWrapper_applyBackgroundUpdatesGetter() {
    return aAddon.applyBackgroundUpdates;
  });
  this.__defineSetter__("applyBackgroundUpdates", function AddonWrapper_applyBackgroundUpdatesSetter(val) {
    if (val != AddonManager.AUTOUPDATE_DEFAULT &&
        val != AddonManager.AUTOUPDATE_DISABLE &&
        val != AddonManager.AUTOUPDATE_ENABLE) {
      val = val ? AddonManager.AUTOUPDATE_DEFAULT :
                  AddonManager.AUTOUPDATE_DISABLE;
    }

    if (val == aAddon.applyBackgroundUpdates)
      return val;

    XPIDatabase.setAddonProperties(aAddon, {
      applyBackgroundUpdates: val
    });
    AddonManagerPrivate.callAddonListeners("onPropertyChanged", this, ["applyBackgroundUpdates"]);

    return val;
  });

  this.__defineSetter__("syncGUID", function AddonWrapper_syncGUIDGetter(val) {
    if (aAddon.syncGUID == val)
      return val;

    if (aAddon instanceof DBAddonInternal)
      XPIDatabase.setAddonSyncGUID(aAddon, val);

    aAddon.syncGUID = val;

    return val;
  });

  this.__defineGetter__("install", function AddonWrapper_installGetter() {
    if (!("_install" in aAddon) || !aAddon._install)
      return null;
    return aAddon._install.wrapper;
  });

  this.__defineGetter__("pendingUpgrade", function AddonWrapper_pendingUpgradeGetter() {
    return createWrapper(aAddon.pendingUpgrade);
  });

  this.__defineGetter__("scope", function AddonWrapper_scopeGetter() {
    if (aAddon._installLocation)
      return aAddon._installLocation.scope;

    return AddonManager.SCOPE_PROFILE;
  });

  this.__defineGetter__("pendingOperations", function AddonWrapper_pendingOperationsGetter() {
    let pending = 0;
    if (!(aAddon instanceof DBAddonInternal)) {
      // Add-on is pending install if there is no associated install (shouldn't
      // happen here) or if the install is in the process of or has successfully
      // completed the install. If an add-on is pending install then we ignore
      // any other pending operations.
      if (!aAddon._install || aAddon._install.state == AddonManager.STATE_INSTALLING ||
          aAddon._install.state == AddonManager.STATE_INSTALLED)
        return AddonManager.PENDING_INSTALL;
    }
    else if (aAddon.pendingUninstall) {
      // If an add-on is pending uninstall then we ignore any other pending
      // operations
      return AddonManager.PENDING_UNINSTALL;
    }

    if (aAddon.active && isAddonDisabled(aAddon))
      pending |= AddonManager.PENDING_DISABLE;
    else if (!aAddon.active && !isAddonDisabled(aAddon))
      pending |= AddonManager.PENDING_ENABLE;

    if (aAddon.pendingUpgrade)
      pending |= AddonManager.PENDING_UPGRADE;

    return pending;
  });

  this.__defineGetter__("operationsRequiringRestart", function AddonWrapper_operationsRequiringRestartGetter() {
    let ops = 0;
    if (XPIProvider.installRequiresRestart(aAddon))
      ops |= AddonManager.OP_NEEDS_RESTART_INSTALL;
    if (XPIProvider.uninstallRequiresRestart(aAddon))
      ops |= AddonManager.OP_NEEDS_RESTART_UNINSTALL;
    if (XPIProvider.enableRequiresRestart(aAddon))
      ops |= AddonManager.OP_NEEDS_RESTART_ENABLE;
    if (XPIProvider.disableRequiresRestart(aAddon))
      ops |= AddonManager.OP_NEEDS_RESTART_DISABLE;

    return ops;
  });

  this.__defineGetter__("permissions", function AddonWrapper_permisionsGetter() {
    let permissions = 0;

    // Add-ons that aren't installed cannot be modified in any way
    if (!(aAddon instanceof DBAddonInternal))
      return permissions;

    if (!aAddon.appDisabled) {
      if (this.userDisabled)
        permissions |= AddonManager.PERM_CAN_ENABLE;
      else if (aAddon.type != "theme")
        permissions |= AddonManager.PERM_CAN_DISABLE;
    }

    // Add-ons that are in locked install locations, or are pending uninstall
    // cannot be upgraded or uninstalled
    if (!aAddon._installLocation.locked && !aAddon.pendingUninstall) {
      // Add-ons that are installed by a file link cannot be upgraded
      if (!aAddon._installLocation.isLinkedAddon(aAddon.id))
        permissions |= AddonManager.PERM_CAN_UPGRADE;

      permissions |= AddonManager.PERM_CAN_UNINSTALL;
    }
    return permissions;
  });

  this.__defineGetter__("isActive", function AddonWrapper_isActiveGetter() {
    if (Services.appinfo.inSafeMode)
      return false;
    return aAddon.active;
  });

  this.__defineGetter__("userDisabled", function AddonWrapper_userDisabledGetter() {
    return aAddon.softDisabled || aAddon.userDisabled;
  });
  this.__defineSetter__("userDisabled", function AddonWrapper_userDisabledSetter(val) {
    if (val == this.userDisabled)
      return val;

    if (aAddon instanceof DBAddonInternal) {
      if (aAddon.type == "theme" && val) {
        if (aAddon.internalName == XPIProvider.defaultSkin)
          throw new Error("Cannot disable the default theme");
        XPIProvider.enableDefaultTheme();
      }
      else {
        XPIProvider.updateAddonDisabledState(aAddon, val);
      }
    }
    else {
      aAddon.userDisabled = val;
      // When enabling remove the softDisabled flag
      if (!val)
        aAddon.softDisabled = false;
    }

    return val;
  });

  this.__defineSetter__("softDisabled", function AddonWrapper_softDisabledSetter(val) {
    if (val == aAddon.softDisabled)
      return val;

    if (aAddon instanceof DBAddonInternal) {
      // When softDisabling a theme just enable the active theme
      if (aAddon.type == "theme" && val && !aAddon.userDisabled) {
        if (aAddon.internalName == XPIProvider.defaultSkin)
          throw new Error("Cannot disable the default theme");
        XPIProvider.enableDefaultTheme();
      }
      else {
        XPIProvider.updateAddonDisabledState(aAddon, undefined, val);
      }
    }
    else {
      // Only set softDisabled if not already disabled
      if (!aAddon.userDisabled)
        aAddon.softDisabled = val;
    }

    return val;
  });

  this.isCompatibleWith = function AddonWrapper_isCompatiblewith(aAppVersion, aPlatformVersion) {
    return aAddon.isCompatibleWith(aAppVersion, aPlatformVersion);
  };

  this.uninstall = function AddonWrapper_uninstall() {
    if (!(aAddon instanceof DBAddonInternal))
      throw new Error("Cannot uninstall an add-on that isn't installed");
    if (aAddon.pendingUninstall)
      throw new Error("Add-on is already marked to be uninstalled");
    XPIProvider.uninstallAddon(aAddon);
  };

  this.cancelUninstall = function AddonWrapper_cancelUninstall() {
    if (!(aAddon instanceof DBAddonInternal))
      throw new Error("Cannot cancel uninstall for an add-on that isn't installed");
    if (!aAddon.pendingUninstall)
      throw new Error("Add-on is not marked to be uninstalled");
    XPIProvider.cancelUninstallAddon(aAddon);
  };

  this.findUpdates = function AddonWrapper_findUpdates(aListener, aReason, aAppVersion, aPlatformVersion) {
    new UpdateChecker(aAddon, aListener, aReason, aAppVersion, aPlatformVersion);
  };

  this.hasResource = function AddonWrapper_hasResource(aPath) {
    if (aAddon._hasResourceCache.has(aPath))
      return aAddon._hasResourceCache.get(aPath);

    let bundle = aAddon._sourceBundle.clone();

    // Bundle may not exist any more if the addon has just been uninstalled,
    // but explicitly first checking .exists() results in unneeded file I/O.
    try {
      var isDir = bundle.isDirectory();
    } catch (e) {
      aAddon._hasResourceCache.set(aPath, false);
      return false;
    }

    if (isDir) {
      if (aPath) {
        aPath.split("/").forEach(function(aPart) {
          bundle.append(aPart);
        });
      }
      let result = bundle.exists();
      aAddon._hasResourceCache.set(aPath, result);
      return result;
    }

    let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                    createInstance(Ci.nsIZipReader);
    try {
      zipReader.open(bundle);
      let result = zipReader.hasEntry(aPath);
      aAddon._hasResourceCache.set(aPath, result);
      return result;
    }
    catch (e) {
      aAddon._hasResourceCache.set(aPath, false);
      return false;
    }
    finally {
      zipReader.close();
    }
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
  this.getResourceURI = function AddonWrapper_getResourceURI(aPath) {
    if (!aPath)
      return NetUtil.newURI(aAddon._sourceBundle);

    return getURIForResourceInFile(aAddon._sourceBundle, aPath);
  }
}

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
 * There may also a special directory named "staged" which can contain
 * directories with the same name as an add-on ID. If the directory is empty
 * then it means the add-on will be uninstalled from this location during the
 * next startup. If the directory contains the add-on's files then they will be
 * installed during the next startup.
 *
 * @param  aName
 *         The string identifier for the install location
 * @param  aDirectory
 *         The nsIFile directory for the install location
 * @param  aScope
 *         The scope of add-ons installed in this location
 * @param  aLocked
 *         true if add-ons cannot be installed, uninstalled or upgraded in the
 *         install location
 */
function DirectoryInstallLocation(aName, aDirectory, aScope, aLocked) {
  this._name = aName;
  this.locked = aLocked;
  this._directory = aDirectory;
  this._scope = aScope
  this._IDToFileMap = {};
  this._FileToIDMap = {};
  this._linkedAddons = [];

  if (!aDirectory.exists())
    return;
  if (!aDirectory.isDirectory())
    throw new Error("Location must be a directory.");

  this._readAddons();
}

DirectoryInstallLocation.prototype = {
  _name       : "",
  _directory   : null,
  _IDToFileMap : null,  // mapping from add-on ID to nsIFile
  _FileToIDMap : null,  // mapping from add-on path to add-on ID

  /**
   * Reads a directory linked to in a file.
   *
   * @param   file
   *          The file containing the directory path
   * @return  An nsIFile object representing the linked directory.
   */
  _readDirectoryFromFile: function DirInstallLocation__readDirectoryFromFile(aFile) {
    let fis = Cc["@mozilla.org/network/file-input-stream;1"].
              createInstance(Ci.nsIFileInputStream);
    fis.init(aFile, -1, -1, false);
    let line = { value: "" };
    if (fis instanceof Ci.nsILineInputStream)
      fis.readLine(line);
    fis.close();
    if (line.value) {
      let linkedDirectory = Cc["@mozilla.org/file/local;1"].
                            createInstance(Ci.nsIFile);

      try {
        linkedDirectory.initWithPath(line.value);
      }
      catch (e) {
        linkedDirectory.setRelativeDescriptor(aFile.parent, line.value);
      }

      if (!linkedDirectory.exists()) {
        WARN("File pointer " + aFile.path + " points to " + linkedDirectory.path +
             " which does not exist");
        return null;
      }

      if (!linkedDirectory.isDirectory()) {
        WARN("File pointer " + aFile.path + " points to " + linkedDirectory.path +
             " which is not a directory");
        return null;
      }

      return linkedDirectory;
    }

    WARN("File pointer " + aFile.path + " does not contain a path");
    return null;
  },

  /**
   * Finds all the add-ons installed in this location.
   */
  _readAddons: function DirInstallLocation__readAddons() {
    // Use a snapshot of the directory contents to avoid possible issues with
    // iterating over a directory while removing files from it (the YAFFS2
    // embedded filesystem has this issue, see bug 772238).
    let entries = getDirectoryEntries(this._directory);
    for (let entry of entries) {
      let id = entry.leafName;

      if (id == DIR_STAGE || id == DIR_XPI_STAGE || id == DIR_TRASH)
        continue;

      let directLoad = false;
      if (entry.isFile() &&
          id.substring(id.length - 4).toLowerCase() == ".xpi") {
        directLoad = true;
        id = id.substring(0, id.length - 4);
      }

      if (!gIDTest.test(id)) {
        LOG("Ignoring file entry whose name is not a valid add-on ID: " +
             entry.path);
        continue;
      }

      if (entry.isFile() && !directLoad) {
        let newEntry = this._readDirectoryFromFile(entry);
        if (!newEntry) {
          LOG("Deleting stale pointer file " + entry.path);
          try {
            entry.remove(true);
          }
          catch (e) {
            WARN("Failed to remove stale pointer file " + entry.path, e);
            // Failing to remove the stale pointer file is ignorable
          }
          continue;
        }

        entry = newEntry;
        this._linkedAddons.push(id);
      }

      this._IDToFileMap[id] = entry;
      this._FileToIDMap[entry.path] = id;
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
  get addonLocations() {
    let locations = [];
    for (let id in this._IDToFileMap) {
      locations.push(this._IDToFileMap[id].clone());
    }
    return locations;
  },

  /**
   * Gets the staging directory to put add-ons that are pending install and
   * uninstall into.
   *
   * @return an nsIFile
   */
  getStagingDir: function DirInstallLocation_getStagingDir() {
    let dir = this._directory.clone();
    dir.append(DIR_STAGE);
    return dir;
  },

  /**
   * Gets the directory used by old versions for staging XPI and JAR files ready
   * to be installed.
   *
   * @return an nsIFile
   */
  getXPIStagingDir: function DirInstallLocation_getXPIStagingDir() {
    let dir = this._directory.clone();
    dir.append(DIR_XPI_STAGE);
    return dir;
  },

  /**
   * Returns a directory that is normally on the same filesystem as the rest of
   * the install location and can be used for temporarily storing files during
   * safe move operations. Calling this method will delete the existing trash
   * directory and its contents.
   *
   * @return an nsIFile
   */
  getTrashDir: function DirInstallLocation_getTrashDir() {
    let trashDir = this._directory.clone();
    trashDir.append(DIR_TRASH);
    if (trashDir.exists())
      recursiveRemove(trashDir);
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
  installAddon: function DirInstallLocation_installAddon(aId, aSource,
                                                         aExistingAddonID,
                                                         aCopy) {
    let trashDir = this.getTrashDir();

    let transaction = new SafeInstallOperation();

    let self = this;
    function moveOldAddon(aId) {
      let file = self._directory.clone();
      file.append(aId);

      if (file.exists())
        transaction.move(file, trashDir);

      file = self._directory.clone();
      file.append(aId + ".xpi");
      if (file.exists()) {
        flushJarCache(file);
        transaction.move(file, trashDir);
      }
    }

    // If any of these operations fails the finally block will clean up the
    // temporary directory
    try {
      moveOldAddon(aId);
      if (aExistingAddonID && aExistingAddonID != aId)
        moveOldAddon(aExistingAddonID);

      if (aCopy) {
        transaction.copy(aSource, this._directory);
      }
      else {
        if (aSource.isFile())
          flushJarCache(aSource);

        transaction.move(aSource, this._directory);
      }
    }
    finally {
      // It isn't ideal if this cleanup fails but it isn't worth rolling back
      // the install because of it.
      try {
        recursiveRemove(trashDir);
      }
      catch (e) {
        WARN("Failed to remove trash directory when installing " + aId, e);
      }
    }

    let newFile = this._directory.clone();
    newFile.append(aSource.leafName);
    try {
      newFile.lastModifiedTime = Date.now();
    } catch (e)  {
      WARN("failed to set lastModifiedTime on " + newFile.path, e);
    }
    this._FileToIDMap[newFile.path] = aId;
    this._IDToFileMap[aId] = newFile;

    if (aExistingAddonID && aExistingAddonID != aId &&
        aExistingAddonID in this._IDToFileMap) {
      delete this._FileToIDMap[this._IDToFileMap[aExistingAddonID]];
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
  uninstallAddon: function DirInstallLocation_uninstallAddon(aId) {
    let file = this._IDToFileMap[aId];
    if (!file) {
      WARN("Attempted to remove " + aId + " from " +
           this._name + " but it was already gone");
      return;
    }

    file = this._directory.clone();
    file.append(aId);
    if (!file.exists())
      file.leafName += ".xpi";

    if (!file.exists()) {
      WARN("Attempted to remove " + aId + " from " +
           this._name + " but it was already gone");

      delete this._FileToIDMap[file.path];
      delete this._IDToFileMap[aId];
      return;
    }

    let trashDir = this.getTrashDir();

    if (file.leafName != aId)
      flushJarCache(file);

    let transaction = new SafeInstallOperation();

    try {
      transaction.move(file, trashDir);
    }
    finally {
      // It isn't ideal if this cleanup fails, but it is probably better than
      // rolling back the uninstall at this point
      try {
        recursiveRemove(trashDir);
      }
      catch (e) {
        WARN("Failed to remove trash directory when uninstalling " + aId, e);
      }
    }

    delete this._FileToIDMap[file.path];
    delete this._IDToFileMap[aId];
  },

  /**
   * Gets the ID of the add-on installed in the given nsIFile.
   *
   * @param  aFile
   *         The nsIFile to look in
   * @return the ID
   * @throws if the file does not represent an installed add-on
   */
  getIDForLocation: function DirInstallLocation_getIDForLocation(aFile) {
    if (aFile.path in this._FileToIDMap)
      return this._FileToIDMap[aFile.path];
    throw new Error("Unknown add-on location " + aFile.path);
  },

  /**
   * Gets the directory that the add-on with the given ID is installed in.
   *
   * @param  aId
   *         The ID of the add-on
   * @return The nsIFile
   * @throws if the ID does not match any of the add-ons installed
   */
  getLocationForID: function DirInstallLocation_getLocationForID(aId) {
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
  isLinkedAddon: function DirInstallLocation__isLinkedAddon(aId) {
    return this._linkedAddons.indexOf(aId) != -1;
  }
};

#ifdef XP_WIN
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
  this._FileToIDMap = {};

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
  _FileToIDMap : null,  // mapping from path to ID

  /**
   * Retrieves the path of this Application's data key in the registry.
   */
  get _appKeyPath() {
    let appVendor = Services.appinfo.vendor;
    let appName = Services.appinfo.name;

#ifdef MOZ_THUNDERBIRD
    // XXX Thunderbird doesn't specify a vendor string
    if (appVendor == "")
      appVendor = "Mozilla";
#endif

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
  _readAddons: function RegInstallLocation__readAddons(aKey) {
    let count = aKey.valueCount;
    for (let i = 0; i < count; ++i) {
      let id = aKey.getValueName(i);

      let file = Cc["@mozilla.org/file/local;1"].
                createInstance(Ci.nsIFile);
      file.initWithPath(aKey.readStringValue(id));

      if (!file.exists()) {
        WARN("Ignoring missing add-on in " + file.path);
        continue;
      }

      this._IDToFileMap[id] = file;
      this._FileToIDMap[file.path] = id;
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
  get addonLocations() {
    let locations = [];
    for (let id in this._IDToFileMap) {
      locations.push(this._IDToFileMap[id].clone());
    }
    return locations;
  },

  /**
   * Gets the ID of the add-on installed in the given nsIFile.
   *
   * @param  aFile
   *         The nsIFile to look in
   * @return the ID
   * @throws if the file does not represent an installed add-on
   */
  getIDForLocation: function RegInstallLocation_getIDForLocation(aFile) {
    if (aFile.path in this._FileToIDMap)
      return this._FileToIDMap[aFile.path];
    throw new Error("Unknown add-on location");
  },

  /**
   * Gets the nsIFile that the add-on with the given ID is installed in.
   *
   * @param  aId
   *         The ID of the add-on
   * @return the nsIFile
   */
  getLocationForID: function RegInstallLocation_getLocationForID(aId) {
    if (aId in this._IDToFileMap)
      return this._IDToFileMap[aId].clone();
    throw new Error("Unknown add-on ID");
  },

  /**
   * @see DirectoryInstallLocation
   */
  isLinkedAddon: function RegInstallLocation_isLinkedAddon(aId) {
    return true;
  }
};
#endif

AddonManagerPrivate.registerProvider(XPIProvider, [
  new AddonManagerPrivate.AddonType("extension", URI_EXTENSION_STRINGS,
                                    STRING_TYPE_NAME,
                                    AddonManager.VIEW_TYPE_LIST, 4000),
  new AddonManagerPrivate.AddonType("theme", URI_EXTENSION_STRINGS,
                                    STRING_TYPE_NAME,
                                    AddonManager.VIEW_TYPE_LIST, 5000),
  new AddonManagerPrivate.AddonType("dictionary", URI_EXTENSION_STRINGS,
                                    STRING_TYPE_NAME,
                                    AddonManager.VIEW_TYPE_LIST, 7000,
                                    AddonManager.TYPE_UI_HIDE_EMPTY),
  new AddonManagerPrivate.AddonType("locale", URI_EXTENSION_STRINGS,
                                    STRING_TYPE_NAME,
                                    AddonManager.VIEW_TYPE_LIST, 8000,
                                    AddonManager.TYPE_UI_HIDE_EMPTY)
]);
