/*
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Extension Manager.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Dave Townsend <dtownsend@oxymoronical.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
*/

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

var EXPORTED_SYMBOLS = [];

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/AddonRepository.jsm");
Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

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
const PREF_EM_CHECK_COMPATIBILITY_BASE = "extensions.checkCompatibility";
const PREF_EM_CHECK_UPDATE_SECURITY   = "extensions.checkUpdateSecurity";
const PREF_EM_UPDATE_URL              = "extensions.update.url";
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

const URI_EXTENSION_SELECT_DIALOG     = "chrome://mozapps/content/extensions/selectAddons.xul";
const URI_EXTENSION_UPDATE_DIALOG     = "chrome://mozapps/content/extensions/update.xul";
const URI_EXTENSION_STRINGS           = "chrome://mozapps/locale/extensions/extensions.properties";

const STRING_TYPE_NAME                = "type.%ID%.name";

const DIR_EXTENSIONS                  = "extensions";
const DIR_STAGE                       = "staged";
const DIR_XPI_STAGE                   = "staged-xpis";
const DIR_TRASH                       = "trash";

const FILE_OLD_DATABASE               = "extensions.rdf";
const FILE_OLD_CACHE                  = "extensions.cache";
const FILE_DATABASE                   = "extensions.sqlite";
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

const CATEGORY_UPDATE_PARAMS          = "extension-update-params";

const UNKNOWN_XPCOM_ABI               = "unknownABI";
const XPI_PERMISSION                  = "install";

const PREFIX_ITEM_URI                 = "urn:mozilla:item:";
const RDFURI_ITEM_ROOT                = "urn:mozilla:item:root"
const RDFURI_INSTALL_MANIFEST_ROOT    = "urn:mozilla:install-manifest";
const PREFIX_NS_EM                    = "http://www.mozilla.org/2004/em-rdf#";

const TOOLKIT_ID                      = "toolkit@mozilla.org";

const BRANCH_REGEXP                   = /^([^\.]+\.[0-9]+[a-z]*).*/gi;

const DB_SCHEMA                       = 5;
const REQ_VERSION                     = 2;

#ifdef MOZ_COMPATIBILITY_NIGHTLY
const PREF_EM_CHECK_COMPATIBILITY = PREF_EM_CHECK_COMPATIBILITY_BASE +
                                    ".nightly";
#else
const PREF_EM_CHECK_COMPATIBILITY = PREF_EM_CHECK_COMPATIBILITY_BASE + "." +
                                    Services.appinfo.version.replace(BRANCH_REGEXP, "$1");
#endif

// Properties that exist in the install manifest
const PROP_METADATA      = ["id", "version", "type", "internalName", "updateURL",
                            "updateKey", "optionsURL", "optionsType", "aboutURL",
                            "iconURL", "icon64URL"];
const PROP_LOCALE_SINGLE = ["name", "description", "creator", "homepageURL"];
const PROP_LOCALE_MULTI  = ["developers", "translators", "contributors"];
const PROP_TARGETAPP     = ["id", "minVersion", "maxVersion"];

// Properties that only exist in the database
const DB_METADATA        = ["installDate", "updateDate", "size", "sourceURI",
                            "releaseNotesURI", "applyBackgroundUpdates"];
const DB_BOOL_METADATA   = ["visible", "active", "userDisabled", "appDisabled",
                            "pendingUninstall", "bootstrap", "skinnable",
                            "softDisabled"];

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
  multipackage: 32
};

const MSG_JAR_FLUSH = "AddonJarFlush";

/**
 * Valid IDs fit this pattern.
 */
var gIDTest = /^(\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}|[a-z0-9-\._]*\@[a-z0-9-\._]+)$/i;

["LOG", "WARN", "ERROR"].forEach(function(aName) {
  this.__defineGetter__(aName, function() {
    Components.utils.import("resource://gre/modules/AddonLogging.jsm");

    LogManager.getLogger("addons.xpi", this);
    return this[aName];
  })
}, this);

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

  _installDirectory: function(aDirectory, aTargetDirectory, aCopy) {
    let newDir = aTargetDirectory.clone();
    newDir.append(aDirectory.leafName);
    try {
      newDir.create(Ci.nsILocalFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    }
    catch (e) {
      ERROR("Failed to create directory " + newDir.path, e);
      throw e;
    }
    this._createdDirs.push(newDir);

    let entries = aDirectory.directoryEntries
                            .QueryInterface(Ci.nsIDirectoryEnumerator);
    let cacheEntries = [];
    try {
      let entry;
      while (entry = entries.nextFile)
        cacheEntries.push(entry);
    }
    finally {
      entries.close();
    }

    cacheEntries.forEach(function(aEntry) {
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
      aDirectory.permissions = FileUtils.PERMS_DIRECTORY;
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

  _installDirEntry: function(aDirEntry, aTargetDirectory, aCopy) {
    try {
      if (aDirEntry.isDirectory())
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
  move: function(aFile, aTargetDirectory) {
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
      if (move.newFile.isDirectory()) {
        let oldDir = move.oldFile.parent.clone();
        oldDir.append(move.oldFile.leafName);
        oldDir.create(Ci.nsILocalFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
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

  if (XPIProvider.checkUpdateSecurity && !aAddon.providesUpdatesSecurely)
    return false;

  if (!aAddon.isPlatformCompatible)
    return false;

  if (XPIProvider.checkCompatibility) {
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

this.__defineGetter__("gRDF", function() {
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

  // Only read the bootstrapped property for extensions
  if (addon.type == "extension") {
    addon.bootstrap = getRDFProperty(ds, root, "bootstrap") == "true";
    if (addon.optionsType &&
        addon.optionsType != AddonManager.OPTIONS_TYPE_DIALOG &&
        addon.optionsType != AddonManager.OPTIONS_TYPE_INLINE &&
        addon.optionsType != AddonManager.OPTIONS_TYPE_TAB) {
      throw new Error("Install manifest specifies unknown type: " + addon.optionsType);
    }
  }
  else {
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

  addon.appDisabled = !isUsableAddon(addon);

  addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DEFAULT;

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
    while (entry = entries.nextFile)
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
    addon._sourceBundle = aDir.clone().QueryInterface(Ci.nsILocalFile);
    addon.size = getFileSize(aDir);
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
  Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIChromeFrameMessageManager)
    .sendAsyncMessage(MSG_JAR_FLUSH, aJarFile.path);
}

function flushStartupCache() {
  // Init this, so it will get the notification.
  Cc["@mozilla.org/xul/xul-prototype-cache;1"].getService(Ci.nsISupports);
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
          target.create(Ci.nsILocalFile.DIRECTORY_TYPE,
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
      target.permissions |= FileUtils.PERMS_FILE;
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
  var addonStatus = aAddon.userDisabled || aAddon.softDisabled ? "userDisabled"
                                                               : "userEnabled";

  if (!aAddon.isCompatible)
    addonStatus += ",incompatible";
  if (aAddon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED)
    addonStatus += ",blocklisted";
  if (aAddon.blocklistState == Ci.nsIBlocklistService.STATE_SOFTBLOCKED)
    addonStatus += ",softblocked";

  try {
    var xpcomABI = Services.appinfo.XPCOMABI;
  } catch (ex) {
    xpcomABI = UNKNOWN_XPCOM_ABI;
  }

  let uri = aUri.replace(/%ITEM_ID%/g, aAddon.id);
  uri = uri.replace(/%ITEM_VERSION%/g, aAddon.version);
  uri = uri.replace(/%ITEM_STATUS%/g, addonStatus);
  uri = uri.replace(/%APP_ID%/g, Services.appinfo.ID);
  uri = uri.replace(/%APP_VERSION%/g, aAppVersion ? aAppVersion :
                                                    Services.appinfo.version);
  uri = uri.replace(/%REQ_VERSION%/g, REQ_VERSION);
  uri = uri.replace(/%APP_OS%/g, Services.appinfo.OS);
  uri = uri.replace(/%APP_ABI%/g, xpcomABI);
  uri = uri.replace(/%APP_LOCALE%/g, getLocale());
  uri = uri.replace(/%CURRENT_APP_VERSION%/g, Services.appinfo.version);

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

  // Replace custom parameters (names of custom parameters must have at
  // least 3 characters to prevent lookups for something like %D0%C8)
  var catMan = null;
  uri = uri.replace(/%(\w{3,})%/g, function(aMatch, aParam) {
    if (!catMan) {
      catMan = Cc["@mozilla.org/categorymanager;1"].
               getService(Ci.nsICategoryManager);
    }

    try {
      var contractID = catMan.getCategoryEntry(CATEGORY_UPDATE_PARAMS, aParam);
      var paramHandler = Cc[contractID].getService(Ci.nsIPropertyBag2);
      return paramHandler.getPropertyAsAString(aParam);
    }
    catch(e) {
      return aMatch;
    }
  });

  // escape() does not properly encode + symbols in any embedded FVF strings.
  return uri.replace(/\+/g, "%2B");
}

/**
 * Copies properties from one object to another. If no target object is passed
 * a new object will be created and returned.
 *
 * @param  aObject
 *         An object to copy from
 * @param  aProperties
 *         An array of properties to be copied
 * @param  aTarget
 *         An optional target object to copy the properties to
 * @return the object that the properties were copied onto
 */
function copyProperties(aObject, aProperties, aTarget) {
  if (!aTarget)
    aTarget = {};
  aProperties.forEach(function(aProp) {
    aTarget[aProp] = aObject[aProp];
  });
  return aTarget;
}

/**
 * Copies properties from a mozIStorageRow to an object. If no target object is
 * passed a new object will be created and returned.
 *
 * @param  aRow
 *         A mozIStorageRow to copy from
 * @param  aProperties
 *         An array of properties to be copied
 * @param  aTarget
 *         An optional target object to copy the properties to
 * @return the object that the properties were copied onto
 */
function copyRowProperties(aRow, aProperties, aTarget) {
  if (!aTarget)
    aTarget = {};
  aProperties.forEach(function(aProp) {
    aTarget[aProp] = aRow.getResultByName(aProp);
  });
  return aTarget;
}

/**
 * A generator to synchronously return result rows from an mozIStorageStatement.
 *
 * @param  aStatement
 *         The statement to execute
 */
function resultRows(aStatement) {
  try {
    while (stepStatement(aStatement))
      yield aStatement.row;
  }
  finally {
    aStatement.reset();
  }
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
    aDir.permissions = FileUtils.PERMS_DIRECTORY;
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
  aFile.permissions = aFile.isDirectory() ? FileUtils.PERMS_DIRECTORY
                                          : FileUtils.PERMS_FILE;

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

  let entry;
  let dirEntries = aFile.directoryEntries.QueryInterface(Ci.nsIDirectoryEnumerator);
  try {
    while (entry = dirEntries.nextFile)
      recursiveRemove(entry);
    try {
      aFile.remove(true);
    }
    catch (e) {
      ERROR("Failed to remove empty directory " + aFile.path, e);
      throw e;
    }
  }
  finally {
    dirEntries.close();
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
    while (entry = entries.nextFile) {
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
  getDefaultCharPref: function(aName, aDefaultValue) {
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
  getCharPref: function(aName, aDefaultValue) {
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
  getComplexValue: function(aName, aType, aDefaultValue) {
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
  getBoolPref: function(aName, aDefaultValue) {
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
  getIntPref: function(aName, defaultValue) {
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
  clearUserPref: function(aName) {
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
  // The value of the checkCompatibility preference
  checkCompatibility: true,
  // The value of the checkUpdateSecurity preference
  checkUpdateSecurity: true,
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

    this.checkCompatibility = Prefs.getBoolPref(PREF_EM_CHECK_COMPATIBILITY,
                                                true)
    this.checkUpdateSecurity = Prefs.getBoolPref(PREF_EM_CHECK_UPDATE_SECURITY,
                                                 true)
    this.enabledAddons = [];

    Services.prefs.addObserver(PREF_EM_CHECK_COMPATIBILITY, this, false);
    Services.prefs.addObserver(PREF_EM_CHECK_UPDATE_SECURITY, this, false);

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
                                             this.checkCompatibility);
      } catch (e) { }
      this.addAddonsToCrashReporter();
    }

    for (let id in this.bootstrappedAddons) {
      let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
      file.persistentDescriptor = this.bootstrappedAddons[id].descriptor;
      this.callBootstrapMethod(id, this.bootstrappedAddons[id].version, file,
                               "startup", BOOTSTRAP_REASONS.APP_STARTUP);
    }

    // Let these shutdown a little earlier when they still have access to most
    // of XPCOM
    Services.obs.addObserver({
      observe: function(aSubject, aTopic, aData) {
        Services.prefs.setCharPref(PREF_BOOTSTRAP_ADDONS,
                                   JSON.stringify(XPIProvider.bootstrappedAddons));
        for (let id in XPIProvider.bootstrappedAddons) {
          let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
          file.persistentDescriptor = XPIProvider.bootstrappedAddons[id].descriptor;
          XPIProvider.callBootstrapMethod(id, XPIProvider.bootstrappedAddons[id].version,
                                          file, "shutdown",
                                          BOOTSTRAP_REASONS.APP_SHUTDOWN);
        }
        Services.obs.removeObserver(this, "quit-application-granted");
      }
    }, "quit-application-granted", false);

    this.extensionsActive = true;
  },

  /**
   * Shuts down the database and releases all references.
   */
  shutdown: function XPI_shutdown() {
    LOG("shutdown");

    Services.prefs.removeObserver(PREF_EM_CHECK_COMPATIBILITY, this);
    Services.prefs.removeObserver(PREF_EM_CHECK_UPDATE_SECURITY, this);

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

    XPIDatabase.shutdown(function() {
      Services.obs.notifyObservers(null, "xpi-provider-shutdown", null);
    });
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
    XPIDatabase.writeAddonsList([]);
    Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, false);
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
    for (let id in this.bootstrappedAddons)
      data += (data ? "," : "") + id + ":" + this.bootstrappedAddons[id].version;

    try {
      Services.appinfo.annotateCrashReport("Add-ons", data);
    }
    catch (e) { }
    
    const TelemetryPing = Cc["@mozilla.org/base/telemetry-ping;1"].getService(Ci.nsIObserver);
    TelemetryPing.observe(null, "Add-ons", data);
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
    });

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
            if (!(file instanceof Ci.nsILocalFile))
              continue;
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
      let entries = stagingDir.directoryEntries
                              .QueryInterface(Ci.nsIDirectoryEnumerator);
      while (entries.hasMoreElements()) {
        let stageDirEntry = entries.getNext().QueryInterface(Ci.nsILocalFile);

        let id = stageDirEntry.leafName;
        if (!stageDirEntry.isDirectory()) {
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

        if (stageDirEntry.isDirectory()) {
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

        // Check for a cached AddonInternal for this add-on, it may contain
        // updated compatibility information
        let jsonfile = stagingDir.clone();
        jsonfile.append(id + ".json");
        if (jsonfile.exists()) {
          LOG("Found updated manifest for " + id + " in " + aLocation.name);
          let fis = Cc["@mozilla.org/network/file-input-stream;1"].
                       createInstance(Ci.nsIFileInputStream);
          let json = Cc["@mozilla.org/dom/json;1"].
                     createInstance(Ci.nsIJSON);

          try {
            fis.init(jsonfile, -1, 0, 0);
            let addonObj = json.decodeFromStream(fis, jsonfile.fileSize);
            aManifests[aLocation.name][id] = new AddonInternal();
            aManifests[aLocation.name][id].fromJSON(addonObj);
            existingAddonID = aManifests[aLocation.name][id].existingAddonID || id;
          }
          catch (e) {
            ERROR("Unable to read add-on manifest from " + jsonfile.path, e);
          }
          finally {
            fis.close();
          }
        }
        seenFiles.push(jsonfile.leafName);

        // If there was no cached AddonInternal then load it directly
        if (!aManifests[aLocation.name][id]) {
          try {
            aManifests[aLocation.name][id] = loadManifestFromFile(stageDirEntry);
            existingAddonID = aManifests[aLocation.name][id].existingAddonID || id;
          }
          catch (e) {
            ERROR("Unable to read add-on manifest from " + stageDirEntry.path, e);
            // This add-on can't be installed so just remove it now
            seenFiles.push(stageDirEntry.leafName);
            continue;
          }
        }

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
              let oldVersion = aManifests[aLocation.name][id].version;
              let newVersion = oldBootstrap.version;
              let uninstallReason = Services.vc.compare(newVersion, oldVersion) < 0 ?
                                    BOOTSTRAP_REASONS.ADDON_UPGRADE :
                                    BOOTSTRAP_REASONS.ADDON_DOWNGRADE;

              this.callBootstrapMethod(existingAddonID, oldBootstrap.version,
                                       existingAddon, "uninstall", uninstallReason);
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
                                     existingAddon, "install",
                                     BOOTSTRAP_REASONS.ADDON_INSTALL);
          }
          continue;
        }
      }
      entries.close();

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
    while (entry = entries.nextFile) {
      // Should never happen really
      if (!(entry instanceof Ci.nsILocalFile))
        continue;

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
   * @param  aMigrateData
   *         an object generated from a previous version of the database
   *         holding information about what add-ons were previously userDisabled
   *         and updated compatibility information if present
   * @param  aActiveBundles
   *         When performing recovery after startup this will be an array of
   *         persistent descriptors of add-ons that are known to be active,
   *         otherwise it will be null
   * @return a boolean indicating if a change requiring flushing the caches was
   *         detected
   */
  processFileChanges: function XPI_processFileChanges(aState, aManifests,
                                                      aUpdateCompatibility,
                                                      aOldAppVersion,
                                                      aOldPlatformVersion,
                                                      aMigrateData,
                                                      aActiveBundles) {
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

          let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
          file.persistentDescriptor = aAddonState.descriptor;
          XPIProvider.callBootstrapMethod(newAddon.id, newAddon.version, file,
                                          "install", installReason);
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
            let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
            file.persistentDescriptor = aAddonState.descriptor;
            XPIProvider.callBootstrapMethod(aOldAddon.id, aOldAddon.version, file,
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
     * Called when a new add-on has been detected.
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
      // Check the updated manifests lists for the install location, If there
      // is no manifest for the add-on ID then newAddon will be undefined
      if (aInstallLocation.name in aManifests)
        newAddon = aManifests[aInstallLocation.name][aId];

      try {
        // Otherwise load the manifest from the add-on
        if (!newAddon) {
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

      // Check if the add-on is in a scope where add-ons should install disabled
      let disablingScopes = Prefs.getIntPref(PREF_EM_AUTO_DISABLED_SCOPES, 0);
      if (aInstallLocation.scope & disablingScopes)
        newAddon.userDisabled = true;

      // If there is migration data then apply it.
      if (aMigrateData) {
        LOG("Migrating data from old database");
        // A theme's disabled state is determined by the selected theme
        // preference which is read in loadManifestFromRDF
        if (newAddon.type != "theme")
          newAddon.userDisabled = aMigrateData.userDisabled;
        if ("installDate" in aMigrateData)
          newAddon.installDate = aMigrateData.installDate;
        if ("softDisabled" in aMigrateData)
          newAddon.softDisabled = aMigrateData.softDisabled;

        // Some properties should only be migrated if the add-on hasn't changed.
        // The version property isn't a perfect check for this but covers the
        // vast majority of cases.
        if (aMigrateData.version == newAddon.version) {
          LOG("Migrating compatibility info");
          if ("targetApplications" in aMigrateData)
            newAddon.applyCompatibilityUpdate(aMigrateData, true);
        }

        // Since the DB schema has changed make sure softDisabled is correct
        applyBlocklistChanges(newAddon, newAddon, aOldAppVersion,
                              aOldPlatformVersion);
      }

      if (aActiveBundles) {
        // If we have a list of what add-ons should be marked as active then use
        // it to guess at migration data
        // For themes we know which is active by the current skin setting
        if (newAddon.type == "theme")
          newAddon.active = newAddon.internalName == XPIProvider.currentSkin;
        else
          newAddon.active = aActiveBundles.indexOf(aAddonState.descriptor) != -1;

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
        // Remember add-ons that were installed during startup. If there was a
        // cached manifest or migration data then this install is already
        // expected
        if (!aMigrateData && (!(aInstallLocation.name in aManifests) ||
                              !(aId in aManifests[aInstallLocation.name]))) {
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

        // If we're hiding a bootstrapped add-on then call its uninstall method
        if (newAddon.id in oldBootstrappedAddons) {
          let oldBootstrap = oldBootstrappedAddons[newAddon.id];
          XPIProvider.bootstrappedAddons[newAddon.id] = oldBootstrap;

          installReason = Services.vc.compare(oldBootstrap.version, newAddon.version) < 0 ?
                          BOOTSTRAP_REASONS.ADDON_UPGRADE :
                          BOOTSTRAP_REASONS.ADDON_DOWNGRADE;

          let oldAddonFile = Cc["@mozilla.org/file/local;1"].
                             createInstance(Ci.nsILocalFile);
          oldAddonFile.persistentDescriptor = oldBootstrap.descriptor;

          XPIProvider.callBootstrapMethod(newAddon.id, oldBootstrap.version,
                                          oldAddonFile, "uninstall",
                                          installReason);
          XPIProvider.unloadBootstrapScope(newAddon.id);

          // If the new add-on is bootstrapped then we must flush the caches
          // before calling the new bootstrap script
          if (newAddon.bootstrap)
            flushStartupCache();
        }

        if (!newAddon.bootstrap)
          return true;

        // Visible bootstrapped add-ons need to have their install method called
        let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
        file.persistentDescriptor = aAddonState.descriptor;
        XPIProvider.callBootstrapMethod(newAddon.id, newAddon.version, file,
                                        "install", installReason);
        if (!newAddon.active)
          XPIProvider.unloadBootstrapScope(newAddon.id);
      }

      return false;
    }

    let changed = false;
    let knownLocations = XPIDatabase.getInstallLocations();

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
      if (aMigrateData && installLocation.name in aMigrateData)
        locMigrateData = aMigrateData[installLocation.name];
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

    // Cache the new install location states
    let cache = JSON.stringify(this.getInstallLocationStates());
    Services.prefs.setCharPref(PREF_INSTALL_CACHE, cache);

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
    this.bootstrappedAddons = JSON.parse(Prefs.getCharPref(PREF_BOOTSTRAP_ADDONS,
                                         "{}"));

    // First install any new add-ons into the locations, if there are any
    // changes then we must update the database with the information in the
    // install locations
    let manifests = {};
    updateDatabase = this.processPendingFileChanges(manifests) | updateDatabase;

    // This will be true if the previous session made changes that affect the
    // active state of add-ons but didn't commit them properly (normally due
    // to the application crashing)
    let hasPendingChanges = Prefs.getBoolPref(PREF_PENDING_OPERATIONS);

    // If the schema appears to have changed then we should update the database
    updateDatabase |= DB_SCHEMA != Prefs.getIntPref(PREF_DB_SCHEMA, 0);

    // If the application has changed then check for new distribution add-ons
    if (aAppChanged !== false &&
        Prefs.getBoolPref(PREF_INSTALL_DISTRO_ADDONS, true))
      updateDatabase = this.installDistributionAddons(manifests) | updateDatabase;

    let state = this.getInstallLocationStates();

    // If the database exists then the previous file cache can be trusted
    // otherwise the database needs to be recreated
    let dbFile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_DATABASE], true);
    updateDatabase |= !dbFile.exists();
    if (!updateDatabase) {
      // If the state has changed then we must update the database
      let cache = Prefs.getCharPref(PREF_INSTALL_CACHE, null);
      updateDatabase |= cache != JSON.stringify(state);
    }

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
    XPIDatabase.beginTransaction();
    try {
      let extensionListChanged = false;
      // If the database needs to be updated then open it and then update it
      // from the filesystem
      if (updateDatabase || hasPendingChanges) {
        let migrateData = XPIDatabase.openConnection(false);

        try {
          extensionListChanged = this.processFileChanges(state, manifests,
                                                         aAppChanged,
                                                         aOldAppVersion,
                                                         aOldPlatformVersion,
                                                         migrateData, null);
        }
        catch (e) {
          ERROR("Error processing file changes", e);
        }
      }

      if (aAppChanged) {
        // When upgrading the app and using a custom skin make sure it is still
        // compatible otherwise switch back the default
        if (this.currentSkin != this.defaultSkin) {
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
        XPIDatabase.updateActiveAddons();
        XPIDatabase.commitTransaction();
        XPIDatabase.writeAddonsList();
        Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, false);
        Services.prefs.setCharPref(PREF_BOOTSTRAP_ADDONS,
                                   JSON.stringify(this.bootstrappedAddons));
        return true;
      }

      LOG("No changes found");
      XPIDatabase.commitTransaction();
    }
    catch (e) {
      ERROR("Error during startup file checks, rolling back any database " +
            "changes", e);
      XPIDatabase.rollbackTransaction();
    }

    // Check that the add-ons list still exists
    let addonsList = FileUtils.getFile(KEY_PROFILEDIR, [FILE_XPI_ADDONS_LIST],
                                       true);
    if (!addonsList.exists()) {
      LOG("Add-ons list is missing, recreating");
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
   * @param  aIconURL
   *         An icon URL for the install
   * @param  aVersion
   *         A version for the install
   * @param  aLoadGroup
   *         An nsILoadGroup to associate requests with
   * @param  aCallback
   *         A callback to pass the AddonInstall to
   */
  getInstallForURL: function XPI_getInstallForURL(aUrl, aHash, aName, aIconURL,
                                                  aVersion, aLoadGroup, aCallback) {
    AddonInstall.createDownload(function(aInstall) {
      aCallback(aInstall.wrapper);
    }, aUrl, aHash, aName, aIconURL, aVersion, aLoadGroup);
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
    AddonInstall.createInstall(function(aInstall) {
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
    this.installs = this.installs.filter(function(i) i != aInstall);
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
    XPIDatabase.getVisibleAddonForID(aId, function(aAddon) {
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
    XPIDatabase.getVisibleAddons(aTypes, function(aAddons) {
      aCallback([createWrapper(a) for each (a in aAddons)]);
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
    XPIDatabase.getVisibleAddonsWithPendingOperations(aTypes, function(aAddons) {
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
    case PREF_EM_CHECK_COMPATIBILITY:
    case PREF_EM_CHECK_UPDATE_SECURITY:
      this.checkCompatibility = Prefs.getBoolPref(PREF_EM_CHECK_COMPATIBILITY,
                                                  true);
      this.checkUpdateSecurity = Prefs.getBoolPref(PREF_EM_CHECK_UPDATE_SECURITY,
                                                   true);
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
   *         The nsILocalFile for the add-on
   * @param  aVersion
   *         The add-on's version
   * @return a JavaScript scope
   */
  loadBootstrapScope: function XPI_loadBootstrapScope(aId, aFile, aVersion) {
    LOG("Loading bootstrap scope from " + aFile.path);
    // Mark the add-on as active for the crash reporter before loading
    this.bootstrappedAddons[aId] = {
      version: aVersion,
      descriptor: aFile.persistentDescriptor
    };
    this.addAddonsToCrashReporter();

    let principal = Cc["@mozilla.org/systemprincipal;1"].
                    createInstance(Ci.nsIPrincipal);

    if (!aFile.exists()) {
      this.bootstrapScopes[aId] = new Components.utils.Sandbox(principal,
                                                               {sandboxName: aFile.path});
      ERROR("Attempted to load bootstrap scope from missing directory " + bootstrap.path);
      return;
    }

    let uri = getURIForResourceInFile(aFile, "bootstrap.js").spec;
    this.bootstrapScopes[aId] = new Components.utils.Sandbox(principal,
                                                             {sandboxName: uri});

    let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                 createInstance(Ci.mozIJSSubScriptLoader);

    try {
      // As we don't want our caller to control the JS version used for the
      // bootstrap file, we run loadSubScript within the context of the
      // sandbox with the latest JS version set explicitly.
      this.bootstrapScopes[aId].__SCRIPT_URI_SPEC__ = uri;
      Components.utils.evalInSandbox(
        "Components.classes['@mozilla.org/moz/jssubscript-loader;1'] \
                   .createInstance(Components.interfaces.mozIJSSubScriptLoader) \
                   .loadSubScript(__SCRIPT_URI_SPEC__);", this.bootstrapScopes[aId], "ECMAv5");
    }
    catch (e) {
      WARN("Error loading bootstrap.js for " + aId, e);
    }

    // Copy the reason values from the global object into the bootstrap scope.
    for (let name in BOOTSTRAP_REASONS)
      this.bootstrapScopes[aId][name] = BOOTSTRAP_REASONS[name];
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
    this.addAddonsToCrashReporter();
  },

  /**
   * Calls a bootstrap method for an add-on.
   *
   * @param  aId
   *         The ID of the add-on
   * @param  aVersion
   *         The version of the add-on
   * @param  aFile
   *         The nsILocalFile for the add-on
   * @param  aMethod
   *         The name of the bootstrap method to call
   * @param  aReason
   *         The reason flag to pass to the bootstrap's startup method
   */
  callBootstrapMethod: function XPI_callBootstrapMethod(aId, aVersion, aFile,
                                                        aMethod, aReason) {
    // Never call any bootstrap methods in safe mode
    if (Services.appinfo.inSafeMode)
      return;

    // Load the scope if it hasn't already been loaded
    if (!(aId in this.bootstrapScopes))
      this.loadBootstrapScope(aId, aFile, aVersion);

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

    LOG("Calling bootstrap method " + aMethod + " on " + aId + " version " +
        aVersion);
    try {
      this.bootstrapScopes[aId][aMethod](params, aReason);
    }
    catch (e) {
      WARN("Exception running bootstrap method " + aMethod + " on " +
           aId, e);
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

    // Update the properties in the database
    XPIDatabase.setAddonProperties(aAddon, {
      userDisabled: aUserDisabled,
      appDisabled: appDisabled,
      softDisabled: aSoftDisabled
    });

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
            this.callBootstrapMethod(aAddon.id, aAddon.version, file, "shutdown",
                                     BOOTSTRAP_REASONS.ADDON_DISABLE);
            this.unloadBootstrapScope(aAddon.id);
          }
          AddonManagerPrivate.callAddonListeners("onDisabled", wrapper);
        }
        else {
          if (aAddon.bootstrap) {
            let file = aAddon._installLocation.getLocationForID(aAddon.id);
            this.callBootstrapMethod(aAddon.id, aAddon.version, file, "startup",
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
        XPIProvider.callBootstrapMethod(aAddon.id, aAddon.version, file,
                                        "install", BOOTSTRAP_REASONS.ADDON_INSTALL);

        if (aAddon.active) {
          XPIProvider.callBootstrapMethod(aAddon.id, aAddon.version, file,
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
      XPIDatabase.getAddonInLocation(aAddon.id, location.name, function(aNewAddon) {
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
          this.callBootstrapMethod(aAddon.id, aAddon.version, file, "shutdown",
                                   BOOTSTRAP_REASONS.ADDON_UNINSTALL);
        }

        this.callBootstrapMethod(aAddon.id, aAddon.version, file, "uninstall",
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

const FIELDS_ADDON = "internal_id, id, location, version, type, internalName, " +
                     "updateURL, updateKey, optionsURL, optionsType, aboutURL, " +
                     "iconURL, icon64URL, defaultLocale, visible, active, " +
                     "userDisabled, appDisabled, pendingUninstall, descriptor, " +
                     "installDate, updateDate, applyBackgroundUpdates, bootstrap, " +
                     "skinnable, size, sourceURI, releaseNotesURI, softDisabled";

/**
 * A helper function to log an SQL error.
 *
 * @param  aError
 *         The storage error code associated with the error
 * @param  aErrorString
 *         An error message
 */
function logSQLError(aError, aErrorString) {
  ERROR("SQL error " + aError + ": " + aErrorString);
}

/**
 * A helper function to log any errors that occur during async statements.
 *
 * @param  aError
 *         A mozIStorageError to log
 */
function asyncErrorLogger(aError) {
  logSQLError(aError.result, aError.message);
}

/**
 * A helper function to execute a statement synchronously and log any error
 * that occurs.
 *
 * @param  aStatement
 *         A mozIStorageStatement to execute
 */
function executeStatement(aStatement) {
  try {
    aStatement.execute();
  }
  catch (e) {
    logSQLError(XPIDatabase.connection.lastError,
                XPIDatabase.connection.lastErrorString);
    throw e;
  }
}

/**
 * A helper function to step a statement synchronously and log any error that
 * occurs.
 *
 * @param  aStatement
 *         A mozIStorageStatement to execute
 */
function stepStatement(aStatement) {
  try {
    return aStatement.executeStep();
  }
  catch (e) {
    logSQLError(XPIDatabase.connection.lastError,
                XPIDatabase.connection.lastErrorString);
    throw e;
  }
}

/**
 * A mozIStorageStatementCallback that will asynchronously build DBAddonInternal
 * instances from the results it receives. Once the statement has completed
 * executing and all of the metadata for all of the add-ons has been retrieved
 * they will be passed as an array to aCallback.
 *
 * @param  aCallback
 *         A callback function to pass the array of DBAddonInternals to
 */
function AsyncAddonListCallback(aCallback) {
  this.callback = aCallback;
  this.addons = [];
}

AsyncAddonListCallback.prototype = {
  callback: null,
  complete: false,
  count: 0,
  addons: null,

  handleResult: function(aResults) {
    let row = null;
    while (row = aResults.getNextRow()) {
      this.count++;
      let self = this;
      XPIDatabase.makeAddonFromRowAsync(row, function(aAddon) {
        function completeAddon(aRepositoryAddon) {
          aAddon._repositoryAddon = aRepositoryAddon;
          self.addons.push(aAddon);
          if (self.complete && self.addons.length == self.count)
           self.callback(self.addons);
        }

        if ("getCachedAddonByID" in AddonRepository)
          AddonRepository.getCachedAddonByID(aAddon.id, completeAddon);
        else
          completeAddon(null);
      });
    }
  },

  handleError: asyncErrorLogger,

  handleCompletion: function(aReason) {
    this.complete = true;
    if (this.addons.length == this.count)
      this.callback(this.addons);
  }
};

var XPIDatabase = {
  // true if the database connection has been opened
  initialized: false,
  // A cache of statements that are used and need to be finalized on shutdown
  statementCache: {},
  // A cache of weak referenced DBAddonInternals so we can reuse objects where
  // possible
  addonCache: [],
  // The nested transaction count
  transactionCount: 0,

  // The statements used by the database
  statements: {
    _getDefaultLocale: "SELECT id, name, description, creator, homepageURL " +
                       "FROM locale WHERE id=:id",
    _getLocales: "SELECT addon_locale.locale, locale.id, locale.name, " +
                 "locale.description, locale.creator, locale.homepageURL " +
                 "FROM addon_locale JOIN locale ON " +
                 "addon_locale.locale_id=locale.id WHERE " +
                 "addon_internal_id=:internal_id",
    _getTargetApplications: "SELECT addon_internal_id, id, minVersion, " +
                            "maxVersion FROM targetApplication WHERE " +
                            "addon_internal_id=:internal_id",
    _getTargetPlatforms: "SELECT os, abi FROM targetPlatform WHERE " +
                         "addon_internal_id=:internal_id",
    _readLocaleStrings: "SELECT locale_id, type, value FROM locale_strings " +
                        "WHERE locale_id=:id",

    addAddonMetadata_addon: "INSERT INTO addon VALUES (NULL, :id, :location, " +
                            ":version, :type, :internalName, :updateURL, " +
                            ":updateKey, :optionsURL, :optionsType, :aboutURL, " +
                            ":iconURL, :icon64URL, :locale, :visible, :active, " +
                            ":userDisabled, :appDisabled, :pendingUninstall, " +
                            ":descriptor, :installDate, :updateDate, " +
                            ":applyBackgroundUpdates, :bootstrap, :skinnable, " +
                            ":size, :sourceURI, :releaseNotesURI, :softDisabled)",
    addAddonMetadata_addon_locale: "INSERT INTO addon_locale VALUES " +
                                   "(:internal_id, :name, :locale)",
    addAddonMetadata_locale: "INSERT INTO locale (name, description, creator, " +
                             "homepageURL) VALUES (:name, :description, " +
                             ":creator, :homepageURL)",
    addAddonMetadata_strings: "INSERT INTO locale_strings VALUES (:locale, " +
                              ":type, :value)",
    addAddonMetadata_targetApplication: "INSERT INTO targetApplication VALUES " +
                                        "(:internal_id, :id, :minVersion, " +
                                        ":maxVersion)",
    addAddonMetadata_targetPlatform: "INSERT INTO targetPlatform VALUES " +
                                     "(:internal_id, :os, :abi)",

    clearVisibleAddons: "UPDATE addon SET visible=0 WHERE id=:id",
    updateAddonActive: "UPDATE addon SET active=:active WHERE " +
                       "internal_id=:internal_id",

    getActiveAddons: "SELECT " + FIELDS_ADDON + " FROM addon WHERE active=1 AND " +
                     "type<>'theme' AND bootstrap=0",
    getActiveTheme: "SELECT " + FIELDS_ADDON + " FROM addon WHERE " +
                    "internalName=:internalName AND type='theme'",
    getThemes: "SELECT " + FIELDS_ADDON + " FROM addon WHERE type='theme'",

    getAddonInLocation: "SELECT " + FIELDS_ADDON + " FROM addon WHERE id=:id " +
                        "AND location=:location",
    getAddons: "SELECT " + FIELDS_ADDON + " FROM addon",
    getAddonsByType: "SELECT " + FIELDS_ADDON + " FROM addon WHERE type=:type",
    getAddonsInLocation: "SELECT " + FIELDS_ADDON + " FROM addon WHERE " +
                         "location=:location",
    getInstallLocations: "SELECT DISTINCT location FROM addon",
    getVisibleAddonForID: "SELECT " + FIELDS_ADDON + " FROM addon WHERE " +
                          "visible=1 AND id=:id",
    getVisibleAddoForInternalName: "SELECT " + FIELDS_ADDON + " FROM addon " +
                                   "WHERE visible=1 AND internalName=:internalName",
    getVisibleAddons: "SELECT " + FIELDS_ADDON + " FROM addon WHERE visible=1",
    getVisibleAddonsWithPendingOperations: "SELECT " + FIELDS_ADDON + " FROM " +
                                           "addon WHERE visible=1 " +
                                           "AND (pendingUninstall=1 OR " +
                                           "MAX(userDisabled,appDisabled)=active)",

    makeAddonVisible: "UPDATE addon SET visible=1 WHERE internal_id=:internal_id",
    removeAddonMetadata: "DELETE FROM addon WHERE internal_id=:internal_id",
    // Equates to active = visible && !userDisabled && !softDisabled &&
    //                     !appDisabled && !pendingUninstall
    setActiveAddons: "UPDATE addon SET active=MIN(visible, 1 - userDisabled, " +
                     "1 - softDisabled, 1 - appDisabled, 1 - pendingUninstall)",
    setAddonProperties: "UPDATE addon SET userDisabled=:userDisabled, " +
                        "appDisabled=:appDisabled, " +
                        "softDisabled=:softDisabled, " +
                        "pendingUninstall=:pendingUninstall, " +
                        "applyBackgroundUpdates=:applyBackgroundUpdates WHERE " +
                        "internal_id=:internal_id",
    setAddonDescriptor: "UPDATE addon SET descriptor=:descriptor WHERE " +
                        "internal_id=:internal_id",
    updateTargetApplications: "UPDATE targetApplication SET " +
                              "minVersion=:minVersion, maxVersion=:maxVersion " +
                              "WHERE addon_internal_id=:internal_id AND id=:id",

    createSavepoint: "SAVEPOINT 'default'",
    releaseSavepoint: "RELEASE SAVEPOINT 'default'",
    rollbackSavepoint: "ROLLBACK TO SAVEPOINT 'default'"
  },

  /**
   * Begins a new transaction in the database. Transactions may be nested. Data
   * written by an inner transaction may be rolled back on its own. Rolling back
   * an outer transaction will rollback all the changes made by inner
   * transactions even if they were committed. No data is written to the disk
   * until the outermost transaction is committed. Transactions can be started
   * even when the database is not yet open in which case they will be started
   * when the database is first opened.
   */
  beginTransaction: function XPIDB_beginTransaction() {
    if (this.initialized)
      this.getStatement("createSavepoint").execute();
    this.transactionCount++;
  },

  /**
   * Commits the most recent transaction. The data may still be rolled back if
   * an outer transaction is rolled back.
   */
  commitTransaction: function XPIDB_commitTransaction() {
    if (this.transactionCount == 0) {
      ERROR("Attempt to commit one transaction too many.");
      return;
    }

    if (this.initialized)
      this.getStatement("releaseSavepoint").execute();
    this.transactionCount--;
  },

  /**
   * Rolls back the most recent transaction. The database will return to its
   * state when the transaction was started.
   */
  rollbackTransaction: function XPIDB_rollbackTransaction() {
    if (this.transactionCount == 0) {
      ERROR("Attempt to rollback one transaction too many.");
      return;
    }

    if (this.initialized) {
      this.getStatement("rollbackSavepoint").execute();
      this.getStatement("releaseSavepoint").execute();
    }
    this.transactionCount--;
  },

  /**
   * Attempts to open the database file. If it fails it will try to delete the
   * existing file and create an empty database. If that fails then it will
   * open an in-memory database that can be used during this session.
   *
   * @param  aDBFile
   *         The nsIFile to open
   * @return the mozIStorageConnection for the database
   */
  openDatabaseFile: function XPIDB_openDatabaseFile(aDBFile) {
    LOG("Opening database");
    let connection = null;

    // Attempt to open the database
    try {
      connection = Services.storage.openUnsharedDatabase(aDBFile);
    }
    catch (e) {
      ERROR("Failed to open database (1st attempt)", e);
      try {
        aDBFile.remove(true);
      }
      catch (e) {
        ERROR("Failed to remove database that could not be opened", e);
      }
      try {
        connection = Services.storage.openUnsharedDatabase(aDBFile);
      }
      catch (e) {
        ERROR("Failed to open database (2nd attempt)", e);

        // If we have got here there seems to be no way to open the real
        // database, instead open a temporary memory database so things will
        // work for this session
        return Services.storage.openSpecialDatabase("memory");
      }
    }

    connection.executeSimpleSQL("PRAGMA synchronous = FULL");
    connection.executeSimpleSQL("PRAGMA locking_mode = EXCLUSIVE");

    return connection;
  },

  /**
   * Opens a new connection to the database file.
   *
   * @param  aRebuildOnError
   *         A boolean indicating whether add-on information should be loaded
   *         from the install locations if the database needs to be rebuilt.
   * @return the migration data from the database if it was an old schema or
   *         null otherwise.
   */
  openConnection: function XPIDB_openConnection(aRebuildOnError) {
    this.initialized = true;
    let dbfile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_DATABASE], true);
    delete this.connection;

    this.connection = this.openDatabaseFile(dbfile);

    let migrateData = null;
    // If the database was corrupt or missing then the new blank database will
    // have a schema version of 0.
    let schemaVersion = this.connection.schemaVersion;
    if (schemaVersion != DB_SCHEMA) {
      // A non-zero schema version means that a schema has been successfully
      // created in the database in the past so we might be able to get useful
      // information from it
      if (schemaVersion != 0) {
        LOG("Migrating data from schema " + schemaVersion);
        migrateData = this.getMigrateDataFromDatabase();

        // Delete the existing database
        this.connection.close();
        try {
          if (dbfile.exists())
            dbfile.remove(true);

          // Reopen an empty database
          this.connection = this.openDatabaseFile(dbfile);
        }
        catch (e) {
          ERROR("Failed to remove old database", e);
          // If the file couldn't be deleted then fall back to an in-memory
          // database
          this.connection = Services.storage.openSpecialDatabase("memory");
        }
      }
      else if (Prefs.getIntPref(PREF_DB_SCHEMA, 0) == 0) {
        // Only migrate data from the RDF if we haven't done it before
        LOG("Migrating data from extensions.rdf");
        migrateData = this.getMigrateDataFromRDF();
      }

      // At this point the database should be completely empty
      this.createSchema();

      if (aRebuildOnError) {
        let activeBundles = this.getActiveBundles();
        WARN("Rebuilding add-ons database from installed extensions.");
        this.beginTransaction();
        try {
          let state = XPIProvider.getInstallLocationStates();
          XPIProvider.processFileChanges(state, {}, false, undefined, undefined,
                                         migrateData, activeBundles)
          // Make sure to update the active add-ons and add-ons list on shutdown
          Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);
          this.commitTransaction();
        }
        catch (e) {
          ERROR("Error processing file changes", e);
          this.rollbackTransaction();
        }
      }
    }

    // If the database connection has a file open then it has the right schema
    // by now so make sure the preferences reflect that. If not then there is
    // an in-memory database open which means a problem opening and deleting the
    // real database, clear the schema preference to force trying to load the
    // database on the next startup
    if (this.connection.databaseFile) {
      Services.prefs.setIntPref(PREF_DB_SCHEMA, DB_SCHEMA);
    }
    else {
      try {
        Services.prefs.clearUserPref(PREF_DB_SCHEMA);
      }
      catch (e) {
        // The preference may not be defined
      }
    }
    Services.prefs.savePrefFile(null);

    // Begin any pending transactions
    for (let i = 0; i < this.transactionCount; i++)
      this.connection.executeSimpleSQL("SAVEPOINT 'default'");
    return migrateData;
  },

  /**
   * A lazy getter for the database connection.
   */
  get connection() {
    this.openConnection(true);
    return this.connection;
  },

  /**
   * Gets the list of file descriptors of active extension directories or XPI
   * files from the add-ons list. This must be loaded from disk since the
   * directory service gives no easy way to get both directly. This list doesn't
   * include themes as preferences already say which theme is currently active
   *
   * @return an array of persisitent descriptors for the directories
   */
  getActiveBundles: function XPIDB_getActiveBundles() {
    let bundles = [];

    let addonsList = FileUtils.getFile(KEY_PROFILEDIR, [FILE_XPI_ADDONS_LIST],
                                       true);

    let iniFactory = Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
                     getService(Ci.nsIINIParserFactory);
    let parser = iniFactory.createINIParser(addonsList);

    let keys = parser.getKeys("ExtensionDirs");

    while (keys.hasMore())
      bundles.push(parser.getString("ExtensionDirs", keys.getNext()));

    // Also include the list of active bootstrapped extensions
    for (let id in XPIProvider.bootstrappedAddons)
      bundles.push(XPIProvider.bootstrappedAddons[id].descriptor);

    return bundles;
  },

  /**
   * Retrieves migration data from the old extensions.rdf database.
   *
   * @return an object holding information about what add-ons were previously
   *         userDisabled and any updated compatibility information
   */
  getMigrateDataFromRDF: function XPIDB_getMigrateDataFromRDF(aDbWasMissing) {
    let migrateData = {};

    // Migrate data from extensions.rdf
    let rdffile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_OLD_DATABASE], true);
    if (rdffile.exists()) {
      let ds = gRDF.GetDataSourceBlocking(Services.io.newFileURI(rdffile).spec);
      let root = Cc["@mozilla.org/rdf/container;1"].
                 createInstance(Ci.nsIRDFContainer);
      root.Init(ds, gRDF.GetResource(RDFURI_ITEM_ROOT));
      let elements = root.GetElements();
      while (elements.hasMoreElements()) {
        let source = elements.getNext().QueryInterface(Ci.nsIRDFResource);

        let location = getRDFProperty(ds, source, "installLocation");
        if (location) {
          if (!(location in migrateData))
            migrateData[location] = {};
          let id = source.ValueUTF8.substring(PREFIX_ITEM_URI.length);
          migrateData[location][id] = {
            version: getRDFProperty(ds, source, "version"),
            userDisabled: false,
            targetApplications: []
          }

          let disabled = getRDFProperty(ds, source, "userDisabled");
          if (disabled == "true" || disabled == "needs-disable")
            migrateData[location][id].userDisabled = true;

          let targetApps = ds.GetTargets(source, EM_R("targetApplication"),
                                         true);
          while (targetApps.hasMoreElements()) {
            let targetApp = targetApps.getNext()
                                      .QueryInterface(Ci.nsIRDFResource);
            let appInfo = {
              id: getRDFProperty(ds, targetApp, "id")
            };

            let minVersion = getRDFProperty(ds, targetApp, "updatedMinVersion");
            if (minVersion) {
              appInfo.minVersion = minVersion;
              appInfo.maxVersion = getRDFProperty(ds, targetApp, "updatedMaxVersion");
            }
            else {
              appInfo.minVersion = getRDFProperty(ds, targetApp, "minVersion");
              appInfo.maxVersion = getRDFProperty(ds, targetApp, "maxVersion");
            }
            migrateData[location][id].targetApplications.push(appInfo);
          }
        }
      }
    }

    return migrateData;
  },

  /**
   * Retrieves migration data from a database that has an older or newer schema.
   *
   * @return an object holding information about what add-ons were previously
   *         userDisabled and any updated compatibility information
   */
  getMigrateDataFromDatabase: function XPIDB_getMigrateDataFromDatabase() {
    let migrateData = {};

    // Attempt to migrate data from a different (even future!) version of the
    // database
    try {
      // Build a list of sql statements that might recover useful data from this
      // and future versions of the schema
      var sql = [];
      sql.push("SELECT internal_id, id, location, userDisabled, " +
               "softDisabled, installDate, version FROM addon");
      sql.push("SELECT internal_id, id, location, userDisabled, installDate, " +
               "version FROM addon");

      var stmt = null;
      if (!sql.some(function(aSql) {
        try {
          stmt = this.connection.createStatement(aSql);
          return true;
        }
        catch (e) {
          return false;
        }
      }, this)) {
        ERROR("Unable to read anything useful from the database");
        return migrateData;
      }

      for (let row in resultRows(stmt)) {
        if (!(row.location in migrateData))
          migrateData[row.location] = {};
        migrateData[row.location][row.id] = {
          internal_id: row.internal_id,
          version: row.version,
          installDate: row.installDate,
          userDisabled: row.userDisabled == 1,
          targetApplications: []
        };

        if ("softDisabled" in row)
          migrateData[row.location][row.id].softDisabled = row.softDisabled == 1;
      }

      var taStmt = this.connection.createStatement("SELECT id, minVersion, " +
                                                   "maxVersion FROM " +
                                                   "targetApplication WHERE " +
                                                   "addon_internal_id=:internal_id");

      for (let location in migrateData) {
        for (let id in migrateData[location]) {
          taStmt.params.internal_id = migrateData[location][id].internal_id;
          delete migrateData[location][id].internal_id;
          for (let row in resultRows(taStmt)) {
            migrateData[location][id].targetApplications.push({
              id: row.id,
              minVersion: row.minVersion,
              maxVersion: row.maxVersion
            });
          }
        }
      }
    }
    catch (e) {
      // An error here means the schema is too different to read
      ERROR("Error migrating data", e);
    }
    finally {
      if (taStmt)
        taStmt.finalize();
      if (stmt)
        stmt.finalize();
    }

    return migrateData;
  },

  /**
   * Shuts down the database connection and releases all cached objects.
   */
  shutdown: function XPIDB_shutdown(aCallback) {
    if (this.initialized) {
      for each (let stmt in this.statementCache)
        stmt.finalize();
      this.statementCache = {};
      this.addonCache = [];

      if (this.transactionCount > 0) {
        ERROR(this.transactionCount + " outstanding transactions, rolling back.");
        while (this.transactionCount > 0)
          this.rollbackTransaction();
      }

      this.initialized = false;
      let connection = this.connection;
      delete this.connection;

      // Re-create the connection smart getter to allow the database to be
      // re-loaded during testing.
      this.__defineGetter__("connection", function() {
        this.openConnection(true);
        return this.connection;
      });

      connection.asyncClose(aCallback);
    }
    else {
      if (aCallback)
        aCallback();
    }
  },

  /**
   * Gets a cached statement or creates a new statement if it doesn't already
   * exist.
   *
   * @param  key
   *         A unique key to reference the statement
   * @param  aSql
   *         An optional SQL string to use for the query, otherwise a
   *         predefined sql string for the key will be used.
   * @return a mozIStorageStatement for the passed SQL
   */
  getStatement: function XPIDB_getStatement(aKey, aSql) {
    if (aKey in this.statementCache)
      return this.statementCache[aKey];
    if (!aSql)
      aSql = this.statements[aKey];

    try {
      return this.statementCache[aKey] = this.connection.createStatement(aSql);
    }
    catch (e) {
      ERROR("Error creating statement " + aKey + " (" + aSql + ")");
      throw e;
    }
  },

  /**
   * Creates the schema in the database.
   */
  createSchema: function XPIDB_createSchema() {
    LOG("Creating database schema");
    this.beginTransaction();

    // Any errors in here should rollback the transaction
    try {
      this.connection.createTable("addon",
                                  "internal_id INTEGER PRIMARY KEY AUTOINCREMENT, " +
                                  "id TEXT, location TEXT, version TEXT, " +
                                  "type TEXT, internalName TEXT, updateURL TEXT, " +
                                  "updateKey TEXT, optionsURL TEXT, " +
                                  "optionsType TEXT, aboutURL TEXT, iconURL TEXT, " +
                                  "icon64URL TEXT, defaultLocale INTEGER, " +
                                  "visible INTEGER, active INTEGER, " +
                                  "userDisabled INTEGER, appDisabled INTEGER, " +
                                  "pendingUninstall INTEGER, descriptor TEXT, " +
                                  "installDate INTEGER, updateDate INTEGER, " +
                                  "applyBackgroundUpdates INTEGER, " +
                                  "bootstrap INTEGER, skinnable INTEGER, " +
                                  "size INTEGER, sourceURI TEXT, " +
                                  "releaseNotesURI TEXT, softDisabled INTEGER, " +
                                  "UNIQUE (id, location)");
      this.connection.createTable("targetApplication",
                                  "addon_internal_id INTEGER, " +
                                  "id TEXT, minVersion TEXT, maxVersion TEXT, " +
                                  "UNIQUE (addon_internal_id, id)");
      this.connection.createTable("targetPlatform",
                                  "addon_internal_id INTEGER, " +
                                  "os, abi TEXT, " +
                                  "UNIQUE (addon_internal_id, os, abi)");
      this.connection.createTable("addon_locale",
                                  "addon_internal_id INTEGER, "+
                                  "locale TEXT, locale_id INTEGER, " +
                                  "UNIQUE (addon_internal_id, locale)");
      this.connection.createTable("locale",
                                  "id INTEGER PRIMARY KEY AUTOINCREMENT, " +
                                  "name TEXT, description TEXT, creator TEXT, " +
                                  "homepageURL TEXT");
      this.connection.createTable("locale_strings",
                                  "locale_id INTEGER, type TEXT, value TEXT");
      this.connection.executeSimpleSQL("CREATE TRIGGER delete_addon AFTER DELETE " +
        "ON addon BEGIN " +
        "DELETE FROM targetApplication WHERE addon_internal_id=old.internal_id; " +
        "DELETE FROM targetPlatform WHERE addon_internal_id=old.internal_id; " +
        "DELETE FROM addon_locale WHERE addon_internal_id=old.internal_id; " +
        "DELETE FROM locale WHERE id=old.defaultLocale; " +
        "END");
      this.connection.executeSimpleSQL("CREATE TRIGGER delete_addon_locale AFTER " +
        "DELETE ON addon_locale WHEN NOT EXISTS " +
        "(SELECT * FROM addon_locale WHERE locale_id=old.locale_id) BEGIN " +
        "DELETE FROM locale WHERE id=old.locale_id; " +
        "END");
      this.connection.executeSimpleSQL("CREATE TRIGGER delete_locale AFTER " +
        "DELETE ON locale BEGIN " +
        "DELETE FROM locale_strings WHERE locale_id=old.id; " +
        "END");
      this.connection.schemaVersion = DB_SCHEMA;
      this.commitTransaction();
    }
    catch (e) {
      ERROR("Failed to create database schema", e);
      logSQLError(this.connection.lastError, this.connection.lastErrorString);
      this.rollbackTransaction();
      this.connection.close();
      this.connection = null;
      throw e;
    }
  },

  /**
   * Synchronously reads the multi-value locale strings for a locale
   *
   * @param  aLocale
   *         The locale object to read into
   */
  _readLocaleStrings: function XPIDB__readLocaleStrings(aLocale) {
    let stmt = this.getStatement("_readLocaleStrings");

    stmt.params.id = aLocale.id;
    for (let row in resultRows(stmt)) {
      if (!(row.type in aLocale))
        aLocale[row.type] = [];
      aLocale[row.type].push(row.value);
    }
  },

  /**
   * Synchronously reads the locales for an add-on
   *
   * @param  aAddon
   *         The DBAddonInternal to read the locales for
   * @return the array of locales
   */
  _getLocales: function XPIDB__getLocales(aAddon) {
    let stmt = this.getStatement("_getLocales");

    let locales = [];
    stmt.params.internal_id = aAddon._internal_id;
    for (let row in resultRows(stmt)) {
      let locale = {
        id: row.id,
        locales: [row.locale]
      };
      copyProperties(row, PROP_LOCALE_SINGLE, locale);
      locales.push(locale);
    }
    locales.forEach(function(aLocale) {
      this._readLocaleStrings(aLocale);
    }, this);
    return locales;
  },

  /**
   * Synchronously reads the default locale for an add-on
   *
   * @param  aAddon
   *         The DBAddonInternal to read the default locale for
   * @return the default locale for the add-on
   * @throws if the database does not contain the default locale information
   */
  _getDefaultLocale: function XPIDB__getDefaultLocale(aAddon) {
    let stmt = this.getStatement("_getDefaultLocale");

    stmt.params.id = aAddon._defaultLocale;
    if (!stepStatement(stmt))
      throw new Error("Missing default locale for " + aAddon.id);
    let locale = copyProperties(stmt.row, PROP_LOCALE_SINGLE);
    locale.id = aAddon._defaultLocale;
    stmt.reset();
    this._readLocaleStrings(locale);
    return locale;
  },

  /**
   * Synchronously reads the target application entries for an add-on
   *
   * @param  aAddon
   *         The DBAddonInternal to read the target applications for
   * @return an array of target applications
   */
  _getTargetApplications: function XPIDB__getTargetApplications(aAddon) {
    let stmt = this.getStatement("_getTargetApplications");

    stmt.params.internal_id = aAddon._internal_id;
    return [copyProperties(row, PROP_TARGETAPP) for each (row in resultRows(stmt))];
  },

  /**
   * Synchronously reads the target platform entries for an add-on
   *
   * @param  aAddon
   *         The DBAddonInternal to read the target platforms for
   * @return an array of target platforms
   */
  _getTargetPlatforms: function XPIDB__getTargetPlatforms(aAddon) {
    let stmt = this.getStatement("_getTargetPlatforms");

    stmt.params.internal_id = aAddon._internal_id;
    return [copyProperties(row, ["os", "abi"]) for each (row in resultRows(stmt))];
  },

  /**
   * Synchronously makes a DBAddonInternal from a storage row or returns one
   * from the cache.
   *
   * @param  aRow
   *         The storage row to make the DBAddonInternal from
   * @return a DBAddonInternal
   */
  makeAddonFromRow: function XPIDB_makeAddonFromRow(aRow) {
    if (this.addonCache[aRow.internal_id]) {
      let addon = this.addonCache[aRow.internal_id].get();
      if (addon)
        return addon;
    }

    let addon = new DBAddonInternal();
    addon._internal_id = aRow.internal_id;
    addon._installLocation = XPIProvider.installLocationsByName[aRow.location];
    addon._descriptor = aRow.descriptor;
    addon._defaultLocale = aRow.defaultLocale;
    copyProperties(aRow, PROP_METADATA, addon);
    copyProperties(aRow, DB_METADATA, addon);
    DB_BOOL_METADATA.forEach(function(aProp) {
      addon[aProp] = aRow[aProp] != 0;
    });
    try {
      addon._sourceBundle = addon._installLocation.getLocationForID(addon.id);
    }
    catch (e) {
      // An exception will be thrown if the add-on appears in the database but
      // not on disk. In general this should only happen during startup as
      // this change is being detected.
    }

    this.addonCache[aRow.internal_id] = Components.utils.getWeakReference(addon);
    return addon;
  },

  /**
   * Asynchronously fetches additional metadata for a DBAddonInternal.
   *
   * @param  aAddon
   *         The DBAddonInternal
   * @param  aCallback
   *         The callback to call when the metadata is completely retrieved
   */
  fetchAddonMetadata: function XPIDB_fetchAddonMetadata(aAddon) {
    function readLocaleStrings(aLocale, aCallback) {
      let stmt = XPIDatabase.getStatement("_readLocaleStrings");

      stmt.params.id = aLocale.id;
      stmt.executeAsync({
        handleResult: function(aResults) {
          let row = null;
          while (row = aResults.getNextRow()) {
            let type = row.getResultByName("type");
            if (!(type in aLocale))
              aLocale[type] = [];
            aLocale[type].push(row.getResultByName("value"));
          }
        },

        handleError: asyncErrorLogger,

        handleCompletion: function(aReason) {
          aCallback();
        }
      });
    }

    function readDefaultLocale() {
      delete aAddon.defaultLocale;
      let stmt = XPIDatabase.getStatement("_getDefaultLocale");

      stmt.params.id = aAddon._defaultLocale;
      stmt.executeAsync({
        handleResult: function(aResults) {
          aAddon.defaultLocale = copyRowProperties(aResults.getNextRow(),
                                                   PROP_LOCALE_SINGLE);
          aAddon.defaultLocale.id = aAddon._defaultLocale;
        },

        handleError: asyncErrorLogger,

        handleCompletion: function(aReason) {
          if (aAddon.defaultLocale) {
            readLocaleStrings(aAddon.defaultLocale, readLocales);
          }
          else {
            ERROR("Missing default locale for " + aAddon.id);
            readLocales();
          }
        }
      });
    }

    function readLocales() {
      delete aAddon.locales;
      aAddon.locales = [];
      let stmt = XPIDatabase.getStatement("_getLocales");

      stmt.params.internal_id = aAddon._internal_id;
      stmt.executeAsync({
        handleResult: function(aResults) {
          let row = null;
          while (row = aResults.getNextRow()) {
            let locale = {
              id: row.getResultByName("id"),
              locales: [row.getResultByName("locale")]
            };
            copyRowProperties(row, PROP_LOCALE_SINGLE, locale);
            aAddon.locales.push(locale);
          }
        },

        handleError: asyncErrorLogger,

        handleCompletion: function(aReason) {
          let pos = 0;
          function readNextLocale() {
            if (pos < aAddon.locales.length)
              readLocaleStrings(aAddon.locales[pos++], readNextLocale);
            else
              readTargetApplications();
          }

          readNextLocale();
        }
      });
    }

    function readTargetApplications() {
      delete aAddon.targetApplications;
      aAddon.targetApplications = [];
      let stmt = XPIDatabase.getStatement("_getTargetApplications");

      stmt.params.internal_id = aAddon._internal_id;
      stmt.executeAsync({
        handleResult: function(aResults) {
          let row = null;
          while (row = aResults.getNextRow())
            aAddon.targetApplications.push(copyRowProperties(row, PROP_TARGETAPP));
        },

        handleError: asyncErrorLogger,

        handleCompletion: function(aReason) {
          readTargetPlatforms();
        }
      });
    }

    function readTargetPlatforms() {
      delete aAddon.targetPlatforms;
      aAddon.targetPlatforms = [];
      let stmt = XPIDatabase.getStatement("_getTargetPlatforms");

      stmt.params.internal_id = aAddon._internal_id;
      stmt.executeAsync({
        handleResult: function(aResults) {
          let row = null;
          while (row = aResults.getNextRow())
            aAddon.targetPlatforms.push(copyRowProperties(row, ["os", "abi"]));
        },

        handleError: asyncErrorLogger,

        handleCompletion: function(aReason) {
          let callbacks = aAddon._pendingCallbacks;
          delete aAddon._pendingCallbacks;
          callbacks.forEach(function(aCallback) {
            aCallback(aAddon);
          });
        }
      });
    }

    readDefaultLocale();
  },

  /**
   * Synchronously makes a DBAddonInternal from a mozIStorageRow or returns one
   * from the cache.
   *
   * @param  aRow
   *         The mozIStorageRow to make the DBAddonInternal from
   * @return a DBAddonInternal
   */
  makeAddonFromRowAsync: function XPIDB_makeAddonFromRowAsync(aRow, aCallback) {
    let internal_id = aRow.getResultByName("internal_id");
    if (this.addonCache[internal_id]) {
      let addon = this.addonCache[internal_id].get();
      if (addon) {
        // If metadata is still pending for this instance add our callback to
        // the list to be called when complete, otherwise pass the addon to
        // our callback
        if ("_pendingCallbacks" in addon)
          addon._pendingCallbacks.push(aCallback);
        else
          aCallback(addon);
        return;
      }
    }

    let addon = new DBAddonInternal();
    addon._internal_id = internal_id;
    let location = aRow.getResultByName("location");
    addon._installLocation = XPIProvider.installLocationsByName[location];
    addon._descriptor = aRow.getResultByName("descriptor");
    copyRowProperties(aRow, PROP_METADATA, addon);
    addon._defaultLocale = aRow.getResultByName("defaultLocale");
    copyRowProperties(aRow, DB_METADATA, addon);
    DB_BOOL_METADATA.forEach(function(aProp) {
      addon[aProp] = aRow.getResultByName(aProp) != 0;
    });
    try {
      addon._sourceBundle = addon._installLocation.getLocationForID(addon.id);
    }
    catch (e) {
      // An exception will be thrown if the add-on appears in the database but
      // not on disk. In general this should only happen during startup as
      // this change is being detected.
    }

    this.addonCache[internal_id] = Components.utils.getWeakReference(addon);
    addon._pendingCallbacks = [aCallback];
    this.fetchAddonMetadata(addon);
  },

  /**
   * Synchronously reads all install locations known about by the database. This
   * is often a a subset of the total install locations when not all have
   * installed add-ons, occasionally a superset when an install location no
   * longer exists.
   *
   * @return  an array of names of install locations
   */
  getInstallLocations: function XPIDB_getInstallLocations() {
    let stmt = this.getStatement("getInstallLocations");

    return [row.location for each (row in resultRows(stmt))];
  },

  /**
   * Synchronously reads all the add-ons in a particular install location.
   *
   * @param  location
   *         The name of the install location
   * @return an array of DBAddonInternals
   */
  getAddonsInLocation: function XPIDB_getAddonsInLocation(aLocation) {
    let stmt = this.getStatement("getAddonsInLocation");

    stmt.params.location = aLocation;
    return [this.makeAddonFromRow(row) for each (row in resultRows(stmt))];
  },

  /**
   * Asynchronously gets an add-on with a particular ID in a particular
   * install location.
   *
   * @param  aId
   *         The ID of the add-on to retrieve
   * @param  aLocation
   *         The name of the install location
   * @param  aCallback
   *         A callback to pass the DBAddonInternal to
   */
  getAddonInLocation: function XPIDB_getAddonInLocation(aId, aLocation, aCallback) {
    let stmt = this.getStatement("getAddonInLocation");

    stmt.params.id = aId;
    stmt.params.location = aLocation;
    stmt.executeAsync(new AsyncAddonListCallback(function(aAddons) {
      if (aAddons.length == 0) {
        aCallback(null);
        return;
      }
      // This should never happen but indicates invalid data in the database if
      // it does
      if (aAddons.length > 1)
        ERROR("Multiple addons with ID " + aId + " found in location " + aLocation);
      aCallback(aAddons[0]);
    }));
  },

  /**
   * Asynchronously gets the add-on with an ID that is visible.
   *
   * @param  aId
   *         The ID of the add-on to retrieve
   * @param  aCallback
   *         A callback to pass the DBAddonInternal to
   */
  getVisibleAddonForID: function XPIDB_getVisibleAddonForID(aId, aCallback) {
    let stmt = this.getStatement("getVisibleAddonForID");

    stmt.params.id = aId;
    stmt.executeAsync(new AsyncAddonListCallback(function(aAddons) {
      if (aAddons.length == 0) {
        aCallback(null);
        return;
      }
      // This should never happen but indicates invalid data in the database if
      // it does
      if (aAddons.length > 1)
        ERROR("Multiple visible addons with ID " + aId + " found");
      aCallback(aAddons[0]);
    }));
  },

  /**
   * Asynchronously gets the visible add-ons, optionally restricting by type.
   *
   * @param  aTypes
   *         An array of types to include or null to include all types
   * @param  aCallback
   *         A callback to pass the array of DBAddonInternals to
   */
  getVisibleAddons: function XPIDB_getVisibleAddons(aTypes, aCallback) {
    let stmt = null;
    if (!aTypes || aTypes.length == 0) {
      stmt = this.getStatement("getVisibleAddons");
    }
    else {
      let sql = "SELECT " + FIELDS_ADDON + " FROM addon WHERE visible=1 AND " +
                "type IN (";
      for (let i = 1; i <= aTypes.length; i++) {
        sql += "?" + i;
        if (i < aTypes.length)
          sql += ",";
      }
      sql += ")";

      // Note that binding to index 0 sets the value for the ?1 parameter
      stmt = this.getStatement("getVisibleAddons_" + aTypes.length, sql);
      for (let i = 0; i < aTypes.length; i++)
        stmt.bindByIndex(i, aTypes[i]);
    }

    stmt.executeAsync(new AsyncAddonListCallback(aCallback));
  },

  /**
   * Synchronously gets all add-ons of a particular type.
   *
   * @param  aType
   *         The type of add-on to retrieve
   * @return an array of DBAddonInternals
   */
  getAddonsByType: function XPIDB_getAddonsByType(aType) {
    let stmt = this.getStatement("getAddonsByType");

    stmt.params.type = aType;
    return [this.makeAddonFromRow(row) for each (row in resultRows(stmt))];;
  },

  /**
   * Synchronously gets an add-on with a particular internalName.
   *
   * @param  aInternalName
   *         The internalName of the add-on to retrieve
   * @return a DBAddonInternal
   */
  getVisibleAddonForInternalName: function XPIDB_getVisibleAddonForInternalName(aInternalName) {
    let stmt = this.getStatement("getVisibleAddoForInternalName");

    let addon = null;
    stmt.params.internalName = aInternalName;

    if (stepStatement(stmt))
      addon = this.makeAddonFromRow(stmt.row);

    stmt.reset();
    return addon;
  },

  /**
   * Asynchronously gets all add-ons with pending operations.
   *
   * @param  aTypes
   *         The types of add-ons to retrieve or null to get all types
   * @param  aCallback
   *         A callback to pass the array of DBAddonInternal to
   */
  getVisibleAddonsWithPendingOperations:
    function XPIDB_getVisibleAddonsWithPendingOperations(aTypes, aCallback) {
    let stmt = null;
    if (!aTypes || aTypes.length == 0) {
      stmt = this.getStatement("getVisibleAddonsWithPendingOperations");
    }
    else {
      let sql = "SELECT * FROM addon WHERE visible=1 AND " +
                "(pendingUninstall=1 OR MAX(userDisabled,appDisabled)=active) " +
                "AND type IN (";
      for (let i = 1; i <= aTypes.length; i++) {
        sql += "?" + i;
        if (i < aTypes.length)
          sql += ",";
      }
      sql += ")";

      // Note that binding to index 0 sets the value for the ?1 parameter
      stmt = this.getStatement("getVisibleAddonsWithPendingOperations_" +
                               aTypes.length, sql);
      for (let i = 0; i < aTypes.length; i++)
        stmt.bindByIndex(i, aTypes[i]);
    }

    stmt.executeAsync(new AsyncAddonListCallback(aCallback));
  },

  /**
   * Synchronously gets all add-ons in the database.
   *
   * @return  an array of DBAddonInternals
   */
  getAddons: function XPIDB_getAddons() {
    let stmt = this.getStatement("getAddons");

    return [this.makeAddonFromRow(row) for each (row in resultRows(stmt))];;
  },

  /**
   * Synchronously adds an AddonInternal's metadata to the database.
   *
   * @param  aAddon
   *         AddonInternal to add
   * @param  aDescriptor
   *         The file descriptor of the add-on
   */
  addAddonMetadata: function XPIDB_addAddonMetadata(aAddon, aDescriptor) {
    this.beginTransaction();

    var self = this;
    function insertLocale(aLocale) {
      let localestmt = self.getStatement("addAddonMetadata_locale");
      let stringstmt = self.getStatement("addAddonMetadata_strings");

      copyProperties(aLocale, PROP_LOCALE_SINGLE, localestmt.params);
      executeStatement(localestmt);
      let row = XPIDatabase.connection.lastInsertRowID;

      PROP_LOCALE_MULTI.forEach(function(aProp) {
        aLocale[aProp].forEach(function(aStr) {
          stringstmt.params.locale = row;
          stringstmt.params.type = aProp;
          stringstmt.params.value = aStr;
          executeStatement(stringstmt);
        });
      });
      return row;
    }

    // Any errors in here should rollback the transaction
    try {

      if (aAddon.visible) {
        let stmt = this.getStatement("clearVisibleAddons");
        stmt.params.id = aAddon.id;
        executeStatement(stmt);
      }

      let stmt = this.getStatement("addAddonMetadata_addon");

      stmt.params.locale = insertLocale(aAddon.defaultLocale);
      stmt.params.location = aAddon._installLocation.name;
      stmt.params.descriptor = aDescriptor;
      copyProperties(aAddon, PROP_METADATA, stmt.params);
      copyProperties(aAddon, DB_METADATA, stmt.params);
      DB_BOOL_METADATA.forEach(function(aProp) {
        stmt.params[aProp] = aAddon[aProp] ? 1 : 0;
      });
      executeStatement(stmt);
      let internal_id = this.connection.lastInsertRowID;

      stmt = this.getStatement("addAddonMetadata_addon_locale");
      aAddon.locales.forEach(function(aLocale) {
        let id = insertLocale(aLocale);
        aLocale.locales.forEach(function(aName) {
          stmt.params.internal_id = internal_id;
          stmt.params.name = aName;
          stmt.params.locale = insertLocale(aLocale);
          executeStatement(stmt);
        });
      });

      stmt = this.getStatement("addAddonMetadata_targetApplication");

      aAddon.targetApplications.forEach(function(aApp) {
        stmt.params.internal_id = internal_id;
        stmt.params.id = aApp.id;
        stmt.params.minVersion = aApp.minVersion;
        stmt.params.maxVersion = aApp.maxVersion;
        executeStatement(stmt);
      });

      stmt = this.getStatement("addAddonMetadata_targetPlatform");

      aAddon.targetPlatforms.forEach(function(aPlatform) {
        stmt.params.internal_id = internal_id;
        stmt.params.os = aPlatform.os;
        stmt.params.abi = aPlatform.abi;
        executeStatement(stmt);
      });

      this.commitTransaction();
    }
    catch (e) {
      this.rollbackTransaction();
      throw e;
    }
  },

  /**
   * Synchronously updates an add-ons metadata in the database. Currently just
   * removes and recreates.
   *
   * @param  aOldAddon
   *         The DBAddonInternal to be replaced
   * @param  aNewAddon
   *         The new AddonInternal to add
   * @param  aDescriptor
   *         The file descriptor of the add-on
   */
  updateAddonMetadata: function XPIDB_updateAddonMetadata(aOldAddon, aNewAddon,
                                                          aDescriptor) {
    this.beginTransaction();

    // Any errors in here should rollback the transaction
    try {
      this.removeAddonMetadata(aOldAddon);
      aNewAddon.installDate = aOldAddon.installDate;
      aNewAddon.applyBackgroundUpdates = aOldAddon.applyBackgroundUpdates;
      aNewAddon.active = (aNewAddon.visible && !aNewAddon.userDisabled &&
                          !aNewAddon.appDisabled)
      this.addAddonMetadata(aNewAddon, aDescriptor);
      this.commitTransaction();
    }
    catch (e) {
      this.rollbackTransaction();
      throw e;
    }
  },

  /**
   * Synchronously updates the target application entries for an add-on.
   *
   * @param  aAddon
   *         The DBAddonInternal being updated
   * @param  aTargets
   *         The array of target applications to update
   */
  updateTargetApplications: function XPIDB_updateTargetApplications(aAddon,
                                                                    aTargets) {
    this.beginTransaction();

    // Any errors in here should rollback the transaction
    try {
      let stmt = this.getStatement("updateTargetApplications");
      aTargets.forEach(function(aTarget) {
        stmt.params.internal_id = aAddon._internal_id;
        stmt.params.id = aTarget.id;
        stmt.params.minVersion = aTarget.minVersion;
        stmt.params.maxVersion = aTarget.maxVersion;
        executeStatement(stmt);
      });
      this.commitTransaction();
    }
    catch (e) {
      this.rollbackTransaction();
      throw e;
    }
  },

  /**
   * Synchronously removes an add-on from the database.
   *
   * @param  aAddon
   *         The DBAddonInternal being removed
   */
  removeAddonMetadata: function XPIDB_removeAddonMetadata(aAddon) {
    let stmt = this.getStatement("removeAddonMetadata");
    stmt.params.internal_id = aAddon._internal_id;
    executeStatement(stmt);
  },

  /**
   * Synchronously marks a DBAddonInternal as visible marking all other
   * instances with the same ID as not visible.
   *
   * @param  aAddon
   *         The DBAddonInternal to make visible
   * @param  callback
   *         A callback to pass the DBAddonInternal to
   */
  makeAddonVisible: function XPIDB_makeAddonVisible(aAddon) {
    let stmt = this.getStatement("clearVisibleAddons");
    stmt.params.id = aAddon.id;
    executeStatement(stmt);

    stmt = this.getStatement("makeAddonVisible");
    stmt.params.internal_id = aAddon._internal_id;
    executeStatement(stmt);

    aAddon.visible = true;
  },

  /**
   * Synchronously sets properties for an add-on.
   *
   * @param  aAddon
   *         The DBAddonInternal being updated
   * @param  aProperties
   *         A dictionary of properties to set
   */
  setAddonProperties: function XPIDB_setAddonProperties(aAddon, aProperties) {
    function convertBoolean(value) {
      return value ? 1 : 0;
    }

    let stmt = this.getStatement("setAddonProperties");
    stmt.params.internal_id = aAddon._internal_id;

    ["userDisabled", "appDisabled", "softDisabled",
     "pendingUninstall"].forEach(function(aProp) {
      if (aProp in aProperties) {
        stmt.params[aProp] = convertBoolean(aProperties[aProp]);
        aAddon[aProp] = aProperties[aProp];
      }
      else {
        stmt.params[aProp] = convertBoolean(aAddon[aProp]);
      }
    });

    if ("applyBackgroundUpdates" in aProperties) {
      stmt.params.applyBackgroundUpdates = aProperties.applyBackgroundUpdates;
      aAddon.applyBackgroundUpdates = aProperties.applyBackgroundUpdates;
    }
    else {
      stmt.params.applyBackgroundUpdates = aAddon.applyBackgroundUpdates;
    }

    executeStatement(stmt);
  },

  /**
   * Synchronously sets the file descriptor for an add-on.
   *
   * @param  aAddon
   *         The DBAddonInternal being updated
   * @param  aProperties
   *         A dictionary of properties to set
   */
  setAddonDescriptor: function XPIDB_setAddonDescriptor(aAddon, aDescriptor) {
    let stmt = this.getStatement("setAddonDescriptor");
    stmt.params.internal_id = aAddon._internal_id;
    stmt.params.descriptor = aDescriptor;

    executeStatement(stmt);
  },

  /**
   * Synchronously updates an add-on's active flag in the database.
   *
   * @param  aAddon
   *         The DBAddonInternal to update
   */
  updateAddonActive: function XPIDB_updateAddonActive(aAddon) {
    LOG("Updating add-on state");

    let stmt = this.getStatement("updateAddonActive");
    stmt.params.internal_id = aAddon._internal_id;
    stmt.params.active = aAddon.active ? 1 : 0;
    executeStatement(stmt);
  },

  /**
   * Synchronously calculates and updates all the active flags in the database.
   */
  updateActiveAddons: function XPIDB_updateActiveAddons() {
    LOG("Updating add-on states");
    let stmt = this.getStatement("setActiveAddons");
    executeStatement(stmt);

    // Note that this does not update the active property on cached
    // DBAddonInternal instances so we throw away the cache. This should only
    // happen during shutdown when everything is going away anyway or during
    // startup when the only references are internal.
    this.addonCache = [];
  },

  /**
   * Writes out the XPI add-ons list for the platform to read.
   */
  writeAddonsList: function XPIDB_writeAddonsList() {
    LOG("Writing add-ons list");
    Services.appinfo.invalidateCachesOnRestart();
    let addonsList = FileUtils.getFile(KEY_PROFILEDIR, [FILE_XPI_ADDONS_LIST],
                                       true);

    let enabledAddons = [];
    let text = "[ExtensionDirs]\r\n";
    let count = 0;

    let stmt = this.getStatement("getActiveAddons");

    for (let row in resultRows(stmt)) {
      text += "Extension" + (count++) + "=" + row.descriptor + "\r\n";
      enabledAddons.push(row.id + ":" + row.version);
    }

    // The selected skin may come from an inactive theme (the default theme
    // when a lightweight theme is applied for example)
    text += "\r\n[ThemeDirs]\r\n";
    if (Prefs.getBoolPref(PREF_EM_DSS_ENABLED)) {
      stmt = this.getStatement("getThemes");
    }
    else {
      stmt = this.getStatement("getActiveTheme");
      stmt.params.internalName = XPIProvider.selectedSkin;
    }
    count = 0;
    for (let row in resultRows(stmt)) {
      text += "Extension" + (count++) + "=" + row.descriptor + "\r\n";
      enabledAddons.push(row.id + ":" + row.version);
    }

    var fos = FileUtils.openSafeFileOutputStream(addonsList);
    fos.write(text, text.length);
    FileUtils.closeSafeFileOutputStream(fos);

    Services.prefs.setCharPref(PREF_EM_ENABLED_ADDONS, enabledAddons.join(","));
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
  iconURL: null,
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
    this.iconURL = aManifest.iconURL;
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
    this.file = this.sourceURI.QueryInterface(Ci.nsIFileURL)
                    .file.QueryInterface(Ci.nsILocalFile);

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
      this.loadManifest(function() {
        XPIDatabase.getVisibleAddonForID(self.addon.id, function(aAddon) {
          self.existingAddon = aAddon;
          if (aAddon)
            applyBlocklistChanges(aAddon, self.addon);
          self.addon.updateDate = Date.now();
          self.addon.installDate = aAddon ? aAddon.installDate : self.addon.updateDate;

          if (!self.addon.isCompatible) {
            // TODO Should we send some event here?
            self.state = AddonManager.STATE_CHECKING;
            new UpdateChecker(self.addon, {
              onUpdateFinished: function(aAddon) {
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
   * @param  aIconURL
   *         An optional icon for the add-on
   * @param  aVersion
   *         An optional version for the add-on
   */
  initAvailableDownload: function(aName, aType, aIconURL, aVersion, aCallback) {
    this.state = AddonManager.STATE_AVAILABLE;
    this.name = aName;
    this.type = aType;
    this.version = aVersion;
    this.iconURL = aIconURL;
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
    if (!this.listeners.some(function(i) { return i == aListener; }))
      this.listeners.push(aListener);
  },

  /**
   * Removes an InstallListener for this instance if it is registered.
   *
   * @param  aListener
   *         The InstallListener to remove
   */
  removeListener: function AI_removeListener(aListener) {
    this.listeners = this.listeners.filter(function(i) {
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
        AddonInstall.createInstall(function(aInstall) {
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
      AddonRepository.getCachedAddonByID(aAddon.id, function(aRepoAddon) {
        if (aRepoAddon) {
          aAddon._repositoryAddon = aRepoAddon;
          aCallback();
          return;
        }

        // It wasn't there so try to re-download it
        AddonRepository.cacheAddons([aAddon.id], function() {
          AddonRepository.getCachedAddonByID(aAddon.id, function(aRepoAddon) {
            aAddon._repositoryAddon = aRepoAddon;
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
      this.loadMultipackageManifests(zipreader, function() {
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
          this.loadManifest(function() {
            if (self.addon.isCompatible) {
              self.downloadCompleted();
            }
            else {
              // TODO Should we send some event here (bug 557716)?
              self.state = AddonManager.STATE_CHECKING;
              new UpdateChecker(self.addon, {
                onUpdateFinished: function(aAddon) {
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
  downloadFailed: function(aReason, aError) {
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
  downloadCompleted: function() {
    let self = this;
    XPIDatabase.getVisibleAddonForID(this.addon.id, function(aAddon) {
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
                                              file, "shutdown", reason);
            }

            XPIProvider.callBootstrapMethod(this.existingAddon.id,
                                            this.existingAddon.version,
                                            file, "uninstall", reason);
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
                                       function(a) {
          self.addon = a;
          if (self.addon.bootstrap) {
            XPIProvider.callBootstrapMethod(self.addon.id, self.addon.version,
                                            file, "install", reason);
            if (self.addon.active) {
              XPIProvider.callBootstrapMethod(self.addon.id, self.addon.version,
                                              file, "startup", reason);
            }
            else {
              XPIProvider.unloadBootstrapScope(self.addon.id);
            }
          }
          AddonManagerPrivate.callAddonListeners("onInstalled",
                                                 createWrapper(self.addon));

          LOG("Install of " + self.sourceURI.spec + " completed.");
          self.state = AddonManager.STATE_INSTALLED;
          AddonManagerPrivate.callInstallListeners("onInstallEnded",
                                                   self.listeners, self.wrapper,
                                                   createWrapper(self.addon));
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

  getInterface: function(iid) {
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
 * @param  aIconURL
 *         An icon URL for the add-on
 * @param  aVersion
 *         A version for the add-on
 * @param  aLoadGroup
 *         An nsILoadGroup to associate the download with
 */
AddonInstall.createDownload = function(aCallback, aUri, aHash, aName, aIconURL,
                                       aVersion, aLoadGroup) {
  let location = XPIProvider.installLocationsByName[KEY_APP_PROFILE];
  let url = NetUtil.newURI(aUri);

  let install = new AddonInstall(location, url, aHash, null, null, aLoadGroup);
  if (url instanceof Ci.nsIFileURL)
    install.initLocalInstall(aCallback);
  else
    install.initAvailableDownload(aName, null, aIconURL, aVersion, aCallback);
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
                                  aAddon.iconURL, aUpdate.version, aCallback);
  }
};

/**
 * Creates a wrapper for an AddonInstall that only exposes the public API
 *
 * @param  install
 *         The AddonInstall to create a wrapper for
 */
function AddonInstallWrapper(aInstall) {
  ["name", "type", "version", "iconURL", "releaseNotesURI", "file", "state", "error",
   "progress", "maxProgress", "certificate", "certName"].forEach(function(aProp) {
    this.__defineGetter__(aProp, function() aInstall[aProp]);
  }, this);

  this.__defineGetter__("existingAddon", function() {
    return createWrapper(aInstall.existingAddon);
  });
  this.__defineGetter__("addon", function() createWrapper(aInstall.addon));
  this.__defineGetter__("sourceURI", function() aInstall.sourceURI);

  this.__defineGetter__("linkedInstalls", function() {
    if (!aInstall.linkedInstalls)
      return null;
    return [i.wrapper for each (i in aInstall.linkedInstalls)];
  });

  this.install = function() {
    aInstall.install();
  }

  this.cancel = function() {
    aInstall.cancel();
  }

  this.addListener = function(listener) {
    aInstall.addListener(listener);
  }

  this.removeListener = function(listener) {
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

  let updateURL = aAddon.updateURL ? aAddon.updateURL :
                                     Services.prefs.getCharPref(PREF_EM_UPDATE_URL);

  const UPDATE_TYPE_COMPATIBILITY = 32;
  const UPDATE_TYPE_NEWVERSION = 64;

  aReason |= UPDATE_TYPE_COMPATIBILITY;
  if ("onUpdateAvailable" in this.listener)
    aReason |= UPDATE_TYPE_NEWVERSION;

  let url = escapeAddonURI(aAddon, updateURL, aReason, aAppVersion);
  AddonUpdateChecker.checkForUpdates(aAddon.id, aAddon.type, aAddon.updateKey,
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
  callListener: function(aMethod) {
    if (!(aMethod in this.listener))
      return;

    let args = Array.slice(arguments, 1);
    try {
      this.listener[aMethod].apply(this.listener, args);
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

    // Always apply any compatibility update for the current version
    let compatUpdate = AUC.getCompatibilityUpdate(aUpdates, this.addon.version,
                                                  this.syncCompatibility);

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
                                                this.platformVersion);
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

    let update = AUC.getNewestCompatibleUpdate(aUpdates,
                                               this.appVersion,
                                               this.platformVersion);

    if (update && Services.vc.compare(this.addon.version, update.version) < 0) {
      for (let i = 0; i < XPIProvider.installs.length; i++) {
        // Skip installs that don't match the available update
        if (XPIProvider.installs[i].existingAddon != this.addon ||
            XPIProvider.installs[i].version != update.version)
          continue;

        // If the existing install has not yet started downloading then send an
        // available update notification. If it is already downloading then
        // don't send any available update notification
        if (XPIProvider.installs[i].state == AddonManager.STATE_AVAILABLE)
          sendUpdateAvailableMessages(this, XPIProvider.installs[i]);
        else
          sendUpdateAvailableMessages(this, null);
        return;
      }

      let self = this;
      AddonInstall.createUpdate(function(aInstall) {
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

    for (let i = 0; i < this.targetPlatforms.length; i++) {
      let platform = this.targetPlatforms[i];
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

    return (Services.vc.compare(version, app.minVersion) >= 0) &&
           (Services.vc.compare(version, app.maxVersion) <= 0)
  },

  get matchingTargetApplication() {
    let app = null;
    for (let i = 0; i < this.targetApplications.length; i++) {
      if (this.targetApplications[i].id == Services.appinfo.ID)
        return this.targetApplications[i];
      if (this.targetApplications[i].id == TOOLKIT_ID)
        app = this.targetApplications[i];
    }
    return app;
  },

  get blocklistState() {
    let bs = Cc["@mozilla.org/extensions/blocklist;1"].
             getService(Ci.nsIBlocklistService);
    return bs.getAddonBlocklistState(this.id, this.version);
  },

  get blocklistURL() {
    let bs = Cc["@mozilla.org/extensions/blocklist;1"].
             getService(Ci.nsIBlocklistService);
    return bs.getAddonBlocklistURL(this.id, this.version);
  },

  applyCompatibilityUpdate: function(aUpdate, aSyncCompatibility) {
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
  toJSON: function(aKey) {
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
   * fromJSON should be called to set the properties of this AddonInternal to
   * those from the passed in object. It is essentially the inverse of toJSON.
   *
   * @param  aObj
   *         A JS object containing properties to be set on this AddonInternal
   */
  fromJSON: function(aObj) {
    for (let prop in aObj)
      this[prop] = aObj[prop];
  }
};

/**
 * The DBAddonInternal is a special AddonInternal that has been retrieved from
 * the database. Add-ons retrieved synchronously only have the basic metadata
 * the rest is filled out synchronously when needed. Asynchronously read add-ons
 * have all data available.
 */
function DBAddonInternal() {
  this.__defineGetter__("targetApplications", function() {
    delete this.targetApplications;
    return this.targetApplications = XPIDatabase._getTargetApplications(this);
  });

  this.__defineGetter__("targetPlatforms", function() {
    delete this.targetPlatforms;
    return this.targetPlatforms = XPIDatabase._getTargetPlatforms(this);
  });

  this.__defineGetter__("locales", function() {
    delete this.locales;
    return this.locales = XPIDatabase._getLocales(this);
  });

  this.__defineGetter__("defaultLocale", function() {
    delete this.defaultLocale;
    return this.defaultLocale = XPIDatabase._getDefaultLocale(this);
  });

  this.__defineGetter__("pendingUpgrade", function() {
    delete this.pendingUpgrade;
    for (let i = 0; i < XPIProvider.installs.length; i++) {
      let install = XPIProvider.installs[i];
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
  applyCompatibilityUpdate: function(aUpdate, aSyncCompatibility) {
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
  if (!aAddon._wrapper)
    aAddon._wrapper = new AddonWrapper(aAddon);
  return aAddon._wrapper;
}

/**
 * The AddonWrapper wraps an Addon to provide the data visible to consumers of
 * the public API.
 */
function AddonWrapper(aAddon) {
  function chooseValue(aObj, aProp) {
    let repositoryAddon = aAddon._repositoryAddon;
    let objValue = aObj[aProp];

    if (repositoryAddon && (aProp in repositoryAddon) &&
        (objValue === undefined || objValue === null)) {
      return [repositoryAddon[aProp], true];
    }

    return [objValue, false];
  }

  ["id", "version", "type", "isCompatible", "isPlatformCompatible",
   "providesUpdatesSecurely", "blocklistState", "blocklistURL", "appDisabled",
   "softDisabled", "skinnable", "size"].forEach(function(aProp) {
     this.__defineGetter__(aProp, function() aAddon[aProp]);
  }, this);

  ["fullDescription", "developerComments", "eula", "supportURL",
   "contributionURL", "contributionAmount", "averageRating", "reviewCount",
   "reviewURL", "totalDownloads", "weeklyDownloads", "dailyUsers",
   "repositoryStatus"].forEach(function(aProp) {
    this.__defineGetter__(aProp, function() {
      if (aAddon._repositoryAddon)
        return aAddon._repositoryAddon[aProp];

      return null;
    });
  }, this);

  this.__defineGetter__("aboutURL", function() {
    return this.isActive ? aAddon["aboutURL"] : null;
  });

  ["installDate", "updateDate"].forEach(function(aProp) {
    this.__defineGetter__(aProp, function() new Date(aAddon[aProp]));
  }, this);

  ["sourceURI", "releaseNotesURI"].forEach(function(aProp) {
    this.__defineGetter__(aProp, function() {
      let target = chooseValue(aAddon, aProp)[0];
      if (!target)
        return null;
      return NetUtil.newURI(target);
    });
  }, this);

  // Maps iconURL, icon64URL and optionsURL to the properties of the same name
  // or icon.png, icon64.png and options.xul in the add-on's files.
  ["icon", "icon64", "options"].forEach(function(aProp) {
    this.__defineGetter__(aProp + "URL", function() {
      if (this.isActive && aAddon[aProp + "URL"])
        return aAddon[aProp + "URL"];

      switch (aProp) {
        case "icon":
        case "icon64":
          if (this.hasResource(aProp + ".png"))
            return this.getResourceURI(aProp + ".png").spec;
          break;
        case "options":
          if (this.isActive && this.hasResource(aProp + ".xul"))
            return this.getResourceURI(aProp + ".xul").spec;
          break;
      }

      if (aAddon._repositoryAddon)
        return aAddon._repositoryAddon[aProp + "URL"];

      return null;
    }, this);
  }, this);

  this.__defineGetter__("optionsType", function() {
    if (!this.isActive)
      return null;

    if (aAddon.optionsType)
      return aAddon.optionsType;

    if (this.hasResource("options.xul"))
      return AddonManager.OPTIONS_TYPE_INLINE;

    if (this.optionsURL)
      return AddonManager.OPTIONS_TYPE_DIALOG;

    return null;
  }, this);

  PROP_LOCALE_SINGLE.forEach(function(aProp) {
    this.__defineGetter__(aProp, function() {
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
    this.__defineGetter__(aProp, function() {
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
        results = results.map(function(aResult) {
          return new AddonManagerPrivate.AddonAuthor(aResult);
        });
      }

      return results;
    });
  }, this);

  this.__defineGetter__("screenshots", function() {
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

  this.__defineGetter__("applyBackgroundUpdates", function() {
    return aAddon.applyBackgroundUpdates;
  });
  this.__defineSetter__("applyBackgroundUpdates", function(val) {
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

  this.__defineGetter__("install", function() {
    if (!("_install" in aAddon) || !aAddon._install)
      return null;
    return aAddon._install.wrapper;
  });

  this.__defineGetter__("pendingUpgrade", function() {
    return createWrapper(aAddon.pendingUpgrade);
  });

  this.__defineGetter__("scope", function() {
    if (aAddon._installLocation)
      return aAddon._installLocation.scope;

    return AddonManager.SCOPE_PROFILE;
  });

  this.__defineGetter__("pendingOperations", function() {
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

  this.__defineGetter__("operationsRequiringRestart", function() {
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

  this.__defineGetter__("permissions", function() {
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

  this.__defineGetter__("isActive", function() {
    if (Services.appinfo.inSafeMode)
      return false;
    return aAddon.active;
  });

  this.__defineGetter__("userDisabled", function() {
    return aAddon.softDisabled || aAddon.userDisabled;
  });
  this.__defineSetter__("userDisabled", function(val) {
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

  this.__defineSetter__("softDisabled", function(val) {
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

  this.isCompatibleWith = function(aAppVersion, aPlatformVersion) {
    return aAddon.isCompatibleWith(aAppVersion, aPlatformVersion);
  };

  this.uninstall = function() {
    if (!(aAddon instanceof DBAddonInternal))
      throw new Error("Cannot uninstall an add-on that isn't installed");
    if (aAddon.pendingUninstall)
      throw new Error("Add-on is already marked to be uninstalled");
    XPIProvider.uninstallAddon(aAddon);
  };

  this.cancelUninstall = function() {
    if (!(aAddon instanceof DBAddonInternal))
      throw new Error("Cannot cancel uninstall for an add-on that isn't installed");
    if (!aAddon.pendingUninstall)
      throw new Error("Add-on is not marked to be uninstalled");
    XPIProvider.cancelUninstallAddon(aAddon);
  };

  this.findUpdates = function(aListener, aReason, aAppVersion, aPlatformVersion) {
    new UpdateChecker(aAddon, aListener, aReason, aAppVersion, aPlatformVersion);
  };

  this.hasResource = function(aPath) {
    let bundle = aAddon._sourceBundle.clone();

    if (bundle.isDirectory()) {
      if (aPath) {
        aPath.split("/").forEach(function(aPart) {
          bundle.append(aPart);
        });
      }
      return bundle.exists();
    }

    let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                    createInstance(Ci.nsIZipReader);
    zipReader.open(bundle);
    let result = zipReader.hasEntry(aPath);
    zipReader.close();
    return result;
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
  this.getResourceURI = function(aPath) {
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
   * @return  a nsILocalFile object representing the linked directory.
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
                            createInstance(Ci.nsILocalFile);

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
    let entries = this._directory.directoryEntries
                                 .QueryInterface(Ci.nsIDirectoryEnumerator);
    let entry;
    while (entry = entries.nextFile) {
      // Should never happen really
      if (!(entry instanceof Ci.nsILocalFile))
        continue;

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
          entry.remove(true);
          continue;
        }

        entry = newEntry;
        this._linkedAddons.push(id);
      }

      this._IDToFileMap[id] = entry;
      this._FileToIDMap[entry.path] = id;
    }
    entries.close();
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
      locations.push(this._IDToFileMap[id].clone()
                         .QueryInterface(Ci.nsILocalFile));
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
      let file = self._directory.clone().QueryInterface(Ci.nsILocalFile);
      file.append(aId);

      if (file.exists())
        transaction.move(file, trashDir);

      file = self._directory.clone().QueryInterface(Ci.nsILocalFile);
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

    let newFile = this._directory.clone().QueryInterface(Ci.nsILocalFile);
    newFile.append(aSource.leafName);
    newFile.lastModifiedTime = Date.now();
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
   * @return the nsILocalFile
   * @throws if the ID does not match any of the add-ons installed
   */
  getLocationForID: function DirInstallLocation_getLocationForID(aId) {
    if (aId in this._IDToFileMap)
      return this._IDToFileMap[aId].clone().QueryInterface(Ci.nsILocalFile);
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
                createInstance(Ci.nsILocalFile);
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
      locations.push(this._IDToFileMap[id].clone()
                         .QueryInterface(Ci.nsILocalFile));
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
      return this._IDToFileMap[aId].clone().QueryInterface(Ci.nsILocalFile);
    throw new Error("Unknown add-on ID");
  },

  /**
   * @see DirectoryInstallLocation
   */
  isLinkedAddon: function(aId) {
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
  new AddonManagerPrivate.AddonType("locale", URI_EXTENSION_STRINGS,
                                    STRING_TYPE_NAME,
                                    AddonManager.VIEW_TYPE_LIST, 2000,
                                    AddonManager.TYPE_UI_HIDE_EMPTY)
]);
