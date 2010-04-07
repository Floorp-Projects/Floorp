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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

var EXPORTED_SYMBOLS = [];

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");
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
const PREF_EM_CHECK_COMPATIBILITY     = "extensions.checkCompatibility";
const PREF_EM_CHECK_UPDATE_SECURITY   = "extensions.checkUpdateSecurity";
const PREF_EM_UPDATE_URL              = "extensions.update.url";
const PREF_XPI_ENABLED                = "xpinstall.enabled";
const PREF_XPI_WHITELIST_REQUIRED     = "xpinstall.whitelist.required";

const DIR_EXTENSIONS                  = "extensions";
const DIR_STAGE                       = "staged";

const FILE_OLD_DATABASE               = "extensions.rdf";
const FILE_DATABASE                   = "extensions.sqlite";
const FILE_INSTALL_MANIFEST           = "install.rdf";
const FILE_XPI_ADDONS_LIST            = "extensions.ini";

const KEY_PROFILEDIR                  = "ProfD";
const KEY_APPDIR                      = "XCurProcD";

const KEY_APP_PROFILE                 = "app-profile";
const KEY_APP_GLOBAL                  = "app-global";
const KEY_APP_SYSTEM_LOCAL            = "app-system-local";
const KEY_APP_SYSTEM_SHARE            = "app-system-share";
const KEY_APP_SYSTEM_USER             = "app-system-user";

const UNKNOWN_XPCOM_ABI               = "unknownABI";
const PREFIX_ITEM_URI                 = "urn:mozilla:item:";
const XPI_PERMISSION                  = "install";

const TOOLKIT_ID                      = "toolkit@mozilla.org";

const BRANCH_REGEXP                   = /^([^\.]+\.[0-9]+[a-z]*).*/gi;

const DB_SCHEMA                       = 1;
const REQ_VERSION                     = 2;

const PROP_METADATA      = ["id", "version", "type", "internalName", "updateURL",
                            "updateKey", "optionsURL", "aboutURL", "iconURL"]
const PROP_LOCALE_SINGLE = ["name", "description", "creator", "homepageURL"];
const PROP_LOCALE_MULTI  = ["developers", "translators", "contributors"];
const PROP_TARGETAPP     = ["id", "minVersion", "maxVersion"];

// Map new string type identifiers to old style nsIUpdateItem types
const TYPES = {
  extension: 2,
  theme: 4,
  locale: 8,
  bootstrapped: 64
};

/**
 * Valid IDs fit this pattern.
 */
var gIDTest = /^(\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}|[a-z0-9-\._]*\@[a-z0-9-\._]+)$/i;

/**
 * Logs a debug message.
 *
 * @param   str
 *          The string to log
 */
function LOG(str) {
  dump("*** addons.xpi: " + str + "\n");
}

/**
 * Logs a warning message.
 *
 * @param   str
 *          The string to log
 */
function WARN(str) {
  LOG(str);
}

/**
 * Logs an error message.
 *
 * @param   str
 *          The string to log
 */
function ERROR(str) {
  LOG(str);
}

/**
 * Gets the currently selected locale for display.
 * @return  the selected locale or "en-US" if none is selected
 */
function getLocale() {
  if (Prefs.getBoolPref(PREF_MATCH_OS_LOCALE), false)
    return Services.locale.getLocaleComponentForUserAgent();
  return Prefs.getCharPref(PREF_SELECTED_LOCALE, "en-US");
}

/**
 * Selects the closest matching locale from a list of locales.
 *
 * @param   locales
 *          An array of locales
 * @return  the best match for the currently selected locale
 */
function findClosestLocale(locales) {
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
    for each (var localized in locales) {
      for each (found in localized.locales) {
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
 * Calculates whether an add-on should be appDisabled or not.
 *
 * @param   addon
 *          The add-on to check
 * @return  true if the add-on should not be appDisabled
 */
function isUsableAddon(addon) {
  // Hack to ensure the default theme is always usable
  if (addon.type == "theme" && addon.internalName == XPIProvider.defaultSkin)
    return true;
  if (XPIProvider.checkCompatibility) {
    if (!addon.isCompatible)
      return false;
  }
  else {
    if (!addon.matchingTargetApplication)
      return false;
  }
  if (XPIProvider.checkUpdateSecurity && !addon.providesUpdatesSecurely)
    return false;
  return addon.blocklistState != Ci.nsIBlocklistService.STATE_BLOCKED;
}

/**
 * Reads an AddonInternal object from an RDF stream.
 *
 * @param   uri
 *          The URI that the manifest is being read from
 * @param   stream
 *          An open stream to read the RDF from
 * @return  an AddonInternal object
 * @throws  if the install manifest in the RDF stream is corrupt or could not
 *          be read
 */
function loadManifestFromRDF(uri, stream) {
  let RDF = Cc["@mozilla.org/rdf/rdf-service;1"].
            getService(Ci.nsIRDFService);
  const RDFURI_INSTALL_MANIFEST_ROOT = "urn:mozilla:install-manifest";
  const PREFIX_NS_EM = "http://www.mozilla.org/2004/em-rdf#";

  function EM_R(property) {
    return RDF.GetResource(PREFIX_NS_EM + property);
  }

  function getValue(literal) {
    if (literal instanceof Ci.nsIRDFLiteral)
      return literal.Value;
    if (literal instanceof Ci.nsIRDFResource)
      return literal.Value;
    if (literal instanceof Ci.nsIRDFInt)
      return literal.Value;
    return null;
  }

  function getProperty(ds, source, property) {
    return getValue(ds.GetTarget(source, EM_R(property), true));
  }

  function getPropertyArray(ds, source, property) {
    let values = [];
    let targets = ds.GetTargets(source, EM_R(property), true);
    while (targets.hasMoreElements())
      values.push(getValue(targets.getNext()));

    return values;
  }

  function readLocale(ds, source, isDefault) {
    let locale = { };
    if (!isDefault) {
      locale.locales = [];
      let targets = ds.GetTargets(source, EM_R("locale"), true);
      while (targets.hasMoreElements())
        locale.locales.push(getValue(targets.getNext()));

      if (locale.locales.length == 0)
        throw new Error("No locales given for localized properties");
    }

    PROP_LOCALE_SINGLE.forEach(function(prop) {
      locale[prop] = getProperty(ds, source, prop);
    });

    PROP_LOCALE_MULTI.forEach(function(prop) {
      locale[prop] = getPropertyArray(ds, source,
                                      prop.substring(0, prop.length - 1));
    });

    return locale;
  }

  let rdfParser = Cc["@mozilla.org/rdf/xml-parser;1"].
                  createInstance(Ci.nsIRDFXMLParser)
  let ds = Cc["@mozilla.org/rdf/datasource;1?name=in-memory-datasource"].
           createInstance(Ci.nsIRDFDataSource);
  let listener = rdfParser.parseAsync(ds, uri);
  let channel = Cc["@mozilla.org/network/input-stream-channel;1"].
                createInstance(Ci.nsIInputStreamChannel);
  channel.setURI(uri);
  channel.contentStream = stream;
  channel.QueryInterface(Ci.nsIChannel);
  channel.contentType = "text/xml";

  listener.onStartRequest(channel, null);

  try {
    let pos = 0;
    let count = stream.available();
    while (count > 0) {
      listener.onDataAvailable(channel, null, stream, pos, count);
      pos += count;
      count = stream.available();
    }
    listener.onStopRequest(channel, null, Components.results.NS_OK);
  }
  catch (e) {
    listener.onStopRequest(channel, null, e.result);
    throw e;
  }

  let root = RDF.GetResource(RDFURI_INSTALL_MANIFEST_ROOT);
  let addon = new AddonInternal();
  PROP_METADATA.forEach(function(prop) {
    addon[prop] = getProperty(ds, root, prop);
  });
  if (!addon.id || !addon.version)
    throw new Error("No ID or version in install manifest");

  if (!addon.type) {
    addon.type = addon.internalName ? "theme" : "extension";
  }
  else {
    for (let name in TYPES) {
      if (TYPES[name] == addon.type) {
        addon.type = name;
        break;
      }
    }
  }
  if (!(addon.type in TYPES))
    throw new Error("Install manifest specifies unknown type: " + addon.type);
  if (addon.type == "theme" && !addon.internalName)
    throw new Error("Themes must include an internalName property");

  addon.defaultLocale = readLocale(ds, root, true);

  addon.locales = [];
  let targets = ds.GetTargets(root, EM_R("localized"), true);
  while (targets.hasMoreElements()) {
    let target = targets.getNext().QueryInterface(Ci.nsIRDFResource);
    addon.locales.push(readLocale(ds, target, false));
  }

  addon.targetApplications = [];
  targets = ds.GetTargets(root, EM_R("targetApplication"), true);
  while (targets.hasMoreElements()) {
    let target = targets.getNext().QueryInterface(Ci.nsIRDFResource);
    let targetAppInfo = {};
    PROP_TARGETAPP.forEach(function(prop) {
      targetAppInfo[prop] = getProperty(ds, target, prop);
    });
    if (!targetAppInfo.id || !targetAppInfo.minVersion ||
        !targetAppInfo.maxVersion)
      throw new Error("Invalid targetApplication entry in install manifest");
    addon.targetApplications.push(targetAppInfo);
  }

  addon.targetPlatforms = getPropertyArray(ds, root, "targetPlatform");

  // Themes are disabled by default unless they are currently selected
  if (addon.type == "theme") {
    addon.userDisabled = addon.internalName != XPIProvider.selectedSkin;
  }
  else {
    addon.userDisabled = false;
  }
  addon.appDisabled = !isUsableAddon(addon);

  return addon;
}

/**
 * Loads an AddonInternal object from an add-on extracted in a directory.
 *
 * @param   dir
 *          The nsIFile directory holding the add-on
 * @return  an AddonInternal object
 * @throws  if the directory does not contain a valid install manifest
 */
function loadManifestFromDir(dir) {
  let file = dir.clone();
  file.append(FILE_INSTALL_MANIFEST);
  if (!file.exists() || !file.isFile())
    throw new Error("Directory " + dir.path + " does not contain a valid " +
                    "install manifest");

  let fis = Cc["@mozilla.org/network/file-input-stream;1"].
            createInstance(Ci.nsIFileInputStream);
  fis.init(file, -1, -1, false);
  let bis = Cc["@mozilla.org/network/buffered-input-stream;1"].
            createInstance(Ci.nsIBufferedInputStream);
  bis.init(fis, 4096);

  try {
    let addon = loadManifestFromRDF(Services.io.newFileURI(file), bis);
    addon._sourceBundle = dir.clone().QueryInterface(Ci.nsILocalFile);
    return addon;
  }
  finally {
    bis.close();
    fis.close();
  }
}

/**
 * Creates a jar: URI for a file inside a ZIP file.
 *
 * @param   jarfile
 *          The ZIP file as an nsIFile
 * @param   path
 *          The path inside the ZIP file
 * @return  an nsIURI for the file
 */
function buildJarURI(jarfile, path) {
  let uri = Services.io.newFileURI(jarfile);
  uri = "jar:" + uri.spec + "!/" + path;
  return NetUtil.newURI(uri);
}

/**
 * Extracts files from a ZIP file into a directory.
 *
 * @param   zipFile
 *          The source ZIP file that contains the add-on.
 * @param   dir
 *          The nsIFile to extract to.
 */
function extractFiles(zipFile, dir) {
  function getTargetFile(dir, entry) {
    let target = dir.clone();
    entry.split("/").forEach(function(part) {
      target.append(part);
    });
    return target;
  }

  let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                  createInstance(Ci.nsIZipReader);
  zipReader.open(zipFile);

  try {
    // create directories first
    let entries = zipReader.findEntries("*/");
    while (entries.hasMore()) {
      var entryName = entries.getNext();
      let target = getTargetFile(dir, entryName);
      if (!target.exists()) {
        try {
          target.create(Ci.nsILocalFile.DIRECTORY_TYPE,
                        FileUtils.PERMS_DIRECTORY);
        }
        catch (e) {
          ERROR("extractFiles: failed to create target directory for " +
                "extraction file = " + target.path + ", exception = " + e +
                "\n");
        }
      }
    }

    entries = zipReader.findEntries(null);
    while (entries.hasMore()) {
      let entryName = entries.getNext();
      let target = getTargetFile(dir, entryName);
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
 * @param   zip
 *          A nsIZipReader to check
 * @param   principal
 *          The nsIPrincipal to compare against
 * @return  true if all the contents that should be signed were signed by the
 *          principal
 */
function verifyZipSigning(zip, principal) {
  var count = 0;
  var entries = zip.findEntries(null);
  while (entries.hasMore()) {
    var entry = entries.getNext();
    // Nothing in META-INF is in the manifest.
    if (entry.substr(0, 9) == "META-INF/")
      continue;
    // Directory entries aren't in the manifest.
    if (entry.substr(-1) == "/")
      continue;
    count++;
    var entryPrincipal = zip.getCertificatePrincipal(entry);
    if (!entryPrincipal || !principal.equals(entryPrincipal))
      return false;
  }
  return zip.manifestEntriesCount == count;
}

/**
 * Replaces %...% strings in an addon url (update and updateInfo) with
 * appropriate values.
 *
 * @param   addon
 *          The AddonInternal representing the add-on
 * @param   uri
 *          The uri to escape
 * @param   updateType
 *          An optional number representing the type of update, only applicable
 *          when creating a url for retrieving an update manifest
 * @param   appVersion
 *          The optional application version to use for %APP_VERSION%
 * @return  the appropriately escaped uri.
 */
function escapeAddonURI(addon, uri, updateType, appVersion)
{
  var addonStatus = addon.userDisabled ? "userDisabled" : "userEnabled";

  if (!addon.isCompatible)
    addonStatus += ",incompatible";
  if (addon.blocklistState > 0)
    addonStatus += ",blocklisted";

  try {
    var xpcomABI = Services.appinfo.XPCOMABI;
  } catch (ex) {
    xpcomABI = UNKNOWN_XPCOM_ABI;
  }

  uri = uri.replace(/%ITEM_ID%/g, addon.id);
  uri = uri.replace(/%ITEM_VERSION%/g, addon.version);
  uri = uri.replace(/%ITEM_STATUS%/g, addonStatus);
  uri = uri.replace(/%APP_ID%/g, Services.appinfo.ID);
  uri = uri.replace(/%APP_VERSION%/g, appVersion ? appVersion :
                                                   Services.appinfo.version);
  uri = uri.replace(/%REQ_VERSION%/g, REQ_VERSION);
  uri = uri.replace(/%APP_OS%/g, Services.appinfo.OS);
  uri = uri.replace(/%APP_ABI%/g, xpcomABI);
  uri = uri.replace(/%APP_LOCALE%/g, getLocale());
  uri = uri.replace(/%CURRENT_APP_VERSION%/g, Services.appinfo.version);

  // If there is an updateType then replace the UPDATE_TYPE string
  if (updateType)
    uri = uri.replace(/%UPDATE_TYPE%/g, updateType);

  // If this add-on has compatibility information for either the current
  // application or toolkit then replace the ITEM_MAXAPPVERSION with the
  // maxVersion
  let app = addon.matchingTargetApplication;
  if (app)
    var maxVersion = app.maxVersion;
  else
    maxVersion = "";
  uri = uri.replace(/%ITEM_MAXAPPVERSION%/g, maxVersion);

  // Replace custom parameters (names of custom parameters must have at
  // least 3 characters to prevent lookups for something like %D0%C8)
  var catMan = null;
  uri = uri.replace(/%(\w{3,})%/g, function(match, param) {
    if (!catMan) {
      catMan = Cc["@mozilla.org/categorymanager;1"].
               getService(Ci.nsICategoryManager);
    }

    try {
      var contractID = catMan.getCategoryEntry(CATEGORY_UPDATE_PARAMS, param);
      var paramHandler = Cc[contractID].getService(Ci.nsIPropertyBag2);
      return paramHandler.getPropertyAsAString(param);
    }
    catch(e) {
      return match;
    }
  });

  // escape() does not properly encode + symbols in any embedded FVF strings.
  return uri.replace(/\+/g, "%2B");
}

/**
 * Copies properties from one object to another. If no target object is passed
 * a new object will be created and returned.
 *
 * @param   object
 *          An object to copy from
 * @param   properties
 *          An array of properties to be copied
 * @param   target
 *          An optional target object to copy the properties to
 * @return  the object that the properties were copied onto
 */
function copyProperties(object, properties, target) {
  if (!target)
    target = {};
  properties.forEach(function(prop) {
    target[prop] = object[prop];
  });
  return target;
}

/**
 * Copies properties from a mozIStorageRow to an object. If no target object is
 * passed a new object will be created and returned.
 *
 * @param   row
 *          A mozIStorageRow to copy from
 * @param   properties
 *          An array of properties to be copied
 * @param   target
 *          An optional target object to copy the properties to
 * @return  the object that the properties were copied onto
 */
function copyRowProperties(row, properties, target) {
  if (!target)
    target = {};
  properties.forEach(function(prop) {
    target[prop] = row.getResultByName(prop);
  });
  return target;
}

/**
 * A generator to synchronously return result rows from an mozIStorageStatement.
 *
 * @param   statement
 *          The statement to execute
 */
function resultRows(statement) {
  try {
    while (statement.executeStep())
      yield statement.row;
  }
  finally {
    statement.reset();
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
   * @param   name
   *          The name of the preference
   * @param   defaultValue
   *          A value to return if the preference does not exist
   * @return  the default value of the preference or defaultValue if there is
   *          none
   */
  getDefaultCharPref: function(name, defaultValue) {
    try {
      return Services.prefs.getDefaultBranch("").getCharPref(name);
    }
    catch (e) {
    }
    return defaultValue;
  },

  /**
   * Gets a string preference.
   *
   * @param   name
   *          The name of the preference
   * @param   defaultValue
   *          A value to return if the preference does not exist
   * @return  the value of the preference or defaultValue if there is none
   */
  getCharPref: function(name, defaultValue) {
    try {
      return Services.prefs.getCharPref(name);
    }
    catch (e) {
    }
    return defaultValue;
  },

  /**
   * Gets a boolean preference.
   *
   * @param   name
   *          The name of the preference
   * @param   defaultValue
   *          A value to return if the preference does not exist
   * @return  the value of the preference or defaultValue if there is none
   */
  getBoolPref: function(name, defaultValue) {
    try {
      return Services.prefs.getBoolPref(name);
    }
    catch (e) {
    }
    return defaultValue;
  },

  /**
   * Gets an integer preference.
   *
   * @param   name
   *          The name of the preference
   * @param   defaultValue
   *          A value to return if the preference does not exist
   * @return  the value of the preference or defaultValue if there is none
   */
  getIntPref: function(name, defaultValue) {
    try {
      return Services.prefs.getIntPref(name);
    }
    catch (e) {
    }
    return defaultValue;
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
  // The currently selected skin or the skin that will be switched to after a
  // restart
  selectedSkin: null,
  // The name of the checkCompatibility preference for the current application
  // version
  checkCompatibilityPref: null,
  // The value of the checkCompatibility preference
  checkCompatibility: true,
  // The value of the checkUpdateSecurity preference
  checkUpdateSecurity: true,
  // A dictionary of the file descriptors for bootstrappable add-ons by ID
  bootstrappedAddons: {},
  // A dictionary of JS scopes of loaded bootstrappable add-ons by ID
  bootstrapScopes: {},

  /**
   * Starts the XPI provider initializes the install locations and prefs.
   */
  startup: function XPI_startup() {
    LOG("startup");
    this.installs = [];
    this.installLocations = [];
    this.installLocationsByName = {};

    // These must be in order of priority for processFileChanges etc. to work
    [
      [KEY_APP_GLOBAL,       KEY_APPDIR,     [DIR_EXTENSIONS], true],
      [KEY_APP_SYSTEM_LOCAL, "XRESysLExtPD", [Services.appinfo.ID], true],
      [KEY_APP_SYSTEM_SHARE, "XRESysSExtPD", [Services.appinfo.ID], true],
      [KEY_APP_SYSTEM_USER,  "XREUSysExt",   [Services.appinfo.ID], true],
      [KEY_APP_PROFILE,      KEY_PROFILEDIR, [DIR_EXTENSIONS], false]
    ].forEach(function([name, key, paths, locked]) {
      try {
        let dir = FileUtils.getDir(key, paths);
        let location = new DirectoryInstallLocation(name, dir, locked);
        this.installLocations.push(location);
        this.installLocationsByName[location.name] = location;
      }
      catch (e) { }
    }, this);

    this.defaultSkin = Prefs.getDefaultCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN,
                                                "classic/1.0");
    this.selectedSkin = Prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN,
                                          this.defaultSkin);

    // Tell the Chrome Registry which Skin to select
    if (Prefs.getBoolPref(PREF_DSS_SWITCHPENDING, false)) {
      try {
        this.selectedSkin = Prefs.getCharPref(PREF_DSS_SKIN_TO_SELECT);
        Services.prefs.setCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN,
                                   this.selectedSkin);
        Services.prefs.clearUserPref(PREF_DSS_SKIN_TO_SELECT);
        LOG("Changed skin to " + this.selectedSkin);
      }
      catch (e) {
        ERROR(e);
      }
      Services.prefs.clearUserPref(PREF_DSS_SWITCHPENDING);
    }

    var version = Services.appinfo.version.replace(BRANCH_REGEXP, "$1");
    this.checkCompatibilityPref = PREF_EM_CHECK_COMPATIBILITY + "." + version;
    this.checkCompatibility = Prefs.getBoolPref(this.checkCompatibilityPref,
                                                true)
    this.checkUpdateSecurity = Prefs.getBoolPref(PREF_EM_CHECK_UPDATE_SECURITY,
                                                 true)

    Services.prefs.addObserver(this.checkCompatibilityPref, this, false);
    Services.prefs.addObserver(PREF_EM_CHECK_UPDATE_SECURITY, this, false);
  },

  /**
   * Shuts down the database and releases all references.
   */
  shutdown: function XPI_shutdown() {
    LOG("shutdown");

    Services.prefs.removeObserver(this.checkCompatibilityPref, this);
    Services.prefs.removeObserver(PREF_EM_CHECK_UPDATE_SECURITY, this);

    if (Prefs.getBoolPref(PREF_PENDING_OPERATIONS, false)) {
      XPIDatabase.updateActiveAddons();
      Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, false);
    }
    XPIDatabase.shutdown();
    this.installs = null;

    this.installLocations = null;
    this.installLocationsByName = null;
  },

  /**
   * Gets the add-on states for an install location.
   *
   * @param   location
   *          The install location to retrieve the add-on states for
   * @return  a dictionary mapping add-on IDs to objects with a descriptor
   *          property which contains the add-ons directory descriptor and an
   *          mtime property which contains the add-on's last modified time as
   *          the number of milliseconds since the epoch.
   */
  getAddonStates: function XPI_getAddonStates(location) {
    let addonStates = {};
    location.addonLocations.forEach(function(dir) {
      let id = location.getIDForLocation(dir);
      addonStates[id] = {
        descriptor: dir.persistentDescriptor,
        mtime: dir.lastModifiedTime
      };
    });

    return addonStates;
  },

  /**
   * Gets an array of install location states which uniquely describes all
   * installed add-ons with the add-on's InstallLocation name and last modified
   * time.
   *
   * @return  an array of add-on states for each install location. Each state
   *          is an object with a name property holding the location's name and
   *          an addons property holding the add-on states for the location
   */
  getInstallLocationStates: function XPI_getInstallLocationStates() {
    let states = [];
    this.installLocations.forEach(function(location) {
      let addons = location.addonLocations;
      if (addons.length == 0)
        return;

      let locationState = {
        name: location.name,
        addons: this.getAddonStates(location)
      };

      states.push(locationState);
    }, this);
    return states;
  },

  /**
   * Check the staging directories of install locations for any add-ons to be
   * installed or add-ons to be uninstalled.
   *
   * @param   manifests
   *          A dictionary to add detected install manifests to for the purpose
   *          of passing through updated compatibility information
   * @return  true if an add-on was installed or uninstalled
   */
  processPendingFileChanges: function XPI_processPendingFileChanges(manifests) {
    // TODO maybe this should be passed off to the install locations to handle?
    let changed = false;
    this.installLocations.forEach(function(location) {
      manifests[location.name] = {};
      // We can't install or uninstall anything in locked locations
      if (location.locked)
        return;

      let stagingDir = location.getStagingDir();
      if (!stagingDir || !stagingDir.exists())
        return;

      let entries = stagingDir.directoryEntries;
      while (entries.hasMoreElements()) {
        let stageDirEntry = entries.getNext().QueryInterface(Ci.nsILocalFile);

        // Only directories are important. Files may be updated manifests.
        if (!stageDirEntry.isDirectory()) {
          WARN("Ignoring file: " + stageDirEntry.path);
          continue;
        }

        // Check that the directory's name is a valid ID.
        let id = stageDirEntry.leafName;
        if (!gIDTest.test(id)) {
          WARN("Ignoring directory whose name is not a valid add-on ID: " +
               stageDirEntry.path);
          continue;
        }

        // Check if the directory contains an install manifest.
        let manifest = stageDirEntry.clone();
        manifest.append(FILE_INSTALL_MANIFEST);

        // If the install manifest doesn't exist uninstall this add-on in this
        // install location.
        if (!manifest.exists()) {
          LOG("Processing uninstall of " + id + " in " + location.name);
          location.uninstallAddon(id);
          // The file check later will spot the removal and cleanup the database
          changed = true;
          continue;
        }

        LOG("Processing install of " + id + " in " + location.name);
        try {
          var addonInstallDir = location.installAddon(id, stageDirEntry);
        }
        catch (e) {
          ERROR("Failed to install staged add-on " + id + " in " + location.name +
                ": " + e);
          continue;
        }

        manifests[location.name][id] = null;
        changed = true;

        // Check for a cached AddonInternal for this add-on, it may contain
        // updated compatibility information
        let jsonfile = stagingDir.clone();
        jsonfile.append(id + ".json");
        if (jsonfile.exists()) {
          LOG("Found updated manifest for " + id + " in " + location.name);
          let fis = Cc["@mozilla.org/network/file-input-stream;1"].
                       createInstance(Ci.nsIFileInputStream);
          let json = Cc["@mozilla.org/dom/json;1"].
                     createInstance(Ci.nsIJSON);

          try {
            fis.init(jsonfile, -1, 0, 0);
            manifests[location.name][id] = json.decodeFromStream(fis,
                                                                 jsonfile.fileSize);
            manifests[location.name][id]._sourceBundle = addonInstallDir;
          }
          catch (e) {
            ERROR("Unable to read add-on manifest for " + id + " in " +
                  location.name + ": " + e);
          }
          finally {
            fis.close();
          }
        }
      }

      try {
        stagingDir.remove(true);
      }
      catch (e) {
        // Non-critical, just saves some perf on startup if we clean this up.
        LOG("Error removing staging dir " + stagingDir.path + ": " + e);
      }
    });
    return changed;
  },

  /**
   * Compares the add-ons that are currently installed to those that were
   * known to be installed when the application last ran and applies any
   * changes found to the database.
   *
   * @param   state
   *          The array of current install location states
   * @param   manifests
   *          A dictionary of cached AddonInstalls for add-ons that have been
   *          installed
   * @param   updateCompatibility
   *          true to update add-ons appDisabled property when the application
   *          version has changed
   * @return  true if a change requiring a restart was detected
   */
  processFileChanges: function XPI_processFileChanges(state, manifests,
                                                      updateCompatibility,
                                                      migrateData) {
    let visibleAddons = {};

    /**
     * Updates an add-on's metadata and determines if a restart of the
     * application is necessary. This is called when either the add-on's
     * install directory path or last modified time has changed.
     *
     * @param   installLocation
     *          The install location containing the add-on
     * @param   oldAddon
     *          The AddonInternal as it appeared the last time the application
     *          ran
     * @param   addonState
     *          The new state of the add-on
     * @return  true if restarting the application is required to complete
     *          changing this add-on
     */
    function updateMetadata(installLocation, oldAddon, addonState) {
      LOG("Add-on " + oldAddon.id + " modified in " + installLocation.name);

      // Check if there is an updated install manifest for this add-on
      let newAddon = manifests[installLocation.name][oldAddon.id];

      try {
        // If not load it from the directory
        if (!newAddon) {
          let dir = installLocation.getLocationForID(oldAddon.id);
          newAddon = loadManifestFromDir(dir);
        }

        // The ID in the manifest that was loaded must match the ID of the old
        // add-on.
        if (newAddon.id != oldAddon.id)
          throw new Error("Incorrect id in install manifest");
      }
      catch (e) {
        WARN("Add-on is invalid: " + e);
        XPIDatabase.removeAddonMetadata(oldAddon);
        installLocation.uninstallAddon(oldAddon.id);
        // If this was an active add-on then we must force a restart
        if (oldAddon.active) {
          if (oldAddon.type == "bootstrapped")
            delete XPIProvider.bootstrappedAddons[oldAddon.id];
          else
            return true;
        }

        return false;
      }

      // Set the additional properties on the new AddonInternal
      newAddon._installLocation = installLocation;
      newAddon.userDisabled = oldAddon.userDisabled;
      newAddon.installDate = oldAddon.installDate;
      newAddon.updateDate = addonState.mtime;
      newAddon.visible = !(newAddon.id in visibleAddons);

      // Update the database
      XPIDatabase.updateAddonMetadata(oldAddon, newAddon, addonState.descriptor);
      if (newAddon.visible) {
        visibleAddons[newAddon.id] = newAddon;
        // If the old version was active and wasn't bootstrapped or the new
        // version will be active and isn't bootstrapped then we must force a
        // restart
        if ((oldAddon.active && oldAddon.type != "bootstrapped") ||
            (newAddon.active && newAddon.type != "bootstrapped")) {
          return true;
        }
      }

      return false;
    }

    /**
     * Called when no change has been detected for an add-on's metadata. The
     * add-on may have become visible due to other add-ons being removed or
     * the add-on may need to be updated when the application version has
     * changed.
     *
     * @param   installLocation
     *          The install location containing the add-on
     * @param   oldAddon
     *          The AddonInternal as it appeared the last time the application
     *          ran
     * @param   addonState
     *          The new state of the add-on
     * @return  a boolean indicating if restarting the application is required
     *          to complete changing this add-on
     */
    function updateVisibilityAndCompatibility(installLocation, oldAddon,
                                              addonState) {
      let changed = false;

      // This add-ons metadata has not changed but it may have become visible
      if (!(oldAddon.id in visibleAddons)) {
        visibleAddons[oldAddon.id] = oldAddon;

        if (!oldAddon.visible) {
          XPIDatabase.makeAddonVisible(oldAddon);

          // If the add-on is bootstrappable and it should be active then
          // mark it as active and add it to the list to be activated.
          if (oldAddon.type == "bootstrapped" && !oldAddon.appDisabled &&
              !oldAddon.userDisabled) {
            oldAddon.active = true;
            XPIDatabase.updateAddonActive(oldAddon);
            XPIProvider.bootstrappedAddons[oldAddon.id] = addonState.descriptor;
          }
          else {
            // Otherwise a restart is necessary
            changed = true;
          }
        }
      }

      // App version changed, we may need to update the appDisabled property.
      if (updateCompatibility) {
        let appDisabled = !isUsableAddon(oldAddon);

        // If the property has changed update the database.
        if (appDisabled != oldAddon.appDisabled) {
          LOG("Add-on " + oldAddon.id + " changed appDisabled state to " +
              appDisabled);
          XPIDatabase.setAddonProperties(oldAddon, {
            appDisabled: appDisabled
          });

          // If this is a visible add-on and it isn't userDisabled then we
          // may need a restart or to update the bootstrap list.
          if (oldAddon.visible && !oldAddon.userDisabled) {
            if (oldAddon.type == "bootstrapped") {
              // When visible and not userDisabled, active is the opposite of
              // appDisabled.
              oldAddon.active = !oldAddon.appDisabled;
              XPIDatabase.updateAddonActive(oldAddon);
              if (oldAddon.active)
                XPIProvider.bootstrappedAddons[oldAddon.id] = addonState.descriptor;
              else
                delete XPIProvider.bootstrappedAddons[oldAddon.id];
            }
            else {
              changed = true;
            }
          }
        }
      }

      return changed;
    }

    /**
     * Called when an add-on has been removed.
     *
     * @param   installLocation
     *          The install location containing the add-on
     * @param   oldAddon
     *          The AddonInternal as it appeared the last time the application
     *          ran
     * @return  a boolean indicating if restarting the application is required
     *          to complete changing this add-on
     */
    function removeMetadata(installLocation, oldAddon) {
      // This add-on has disappeared
      LOG("Add-on " + oldAddon.id + " removed from " + installLocation.name);
      XPIDatabase.removeAddonMetadata(oldAddon);
      if (oldAddon.active) {

        // Enable the default theme if the previously active theme has been
        // removed
        if (oldAddon.type == "theme")
          XPIProvider.enableDefaultTheme();

        // If this was an active add-on and bootstrapped we must remove it from
        // the bootstrapped list, otherwise we need to force a restart.
        if (oldAddon.type != "bootstrapped")
          return true;

        delete XPIProvider.bootstrappedAddons[oldAddon.id];
      }

      return false;
    }

    /**
     * Called when a new add-on has been detected.
     *
     * @param   installLocation
     *          The install location containing the add-on
     * @param   id
     *          The ID of the add-on
     * @param   addonState
     *          The new state of the add-on
     * @param   migrateData
     *          If during startup the database had to be upgraded this will
     *          contain data that used to be held about this add-on
     * @return  a boolean indicating if restarting the application is required
     *          to complete changing this add-on
     */
    function addMetadata(installLocation, id, addonState, migrateData) {
      LOG("New add-on " + id + " installed in " + installLocation.name);

      // Check the updated manifests lists for a manifest for this add-on
      let newAddon = manifests[installLocation.name][id];

      try {
        // Otherwise load the manifest from the add-on's directory.
        if (!newAddon)
          newAddon = loadManifestFromDir(installLocation.getLocationForID(id));
        // The add-on in the manifest should match the add-on ID.
        if (newAddon.id != id)
          throw new Error("Incorrect id in install manifest");
      }
      catch (e) {
        WARN("Add-on is invalid: " + e);

        // Remove the invalid add-on from the install location, no restart will
        // be necessary
        installLocation.uninstallAddon(id);
        return false;
      }

      // Update the AddonInternal properties.
      newAddon._installLocation = installLocation;
      newAddon.visible = !(newAddon.id in visibleAddons);
      newAddon.installDate = addonState.mtime;
      newAddon.updateDate = addonState.mtime;

      // If there is migration data then apply it.
      if (migrateData) {
        newAddon.userDisabled = migrateData.userDisabled;
        if ("installDate" in migrateData)
          newAddon.installDate = migrateData.installDate;
      }

      // Update the database.
      XPIDatabase.addAddonMetadata(newAddon, addonState.descriptor);

      // Visible bootstrapped add-ons need to be added to the bootstrap list.
      if (newAddon.visible) {
        visibleAddons[newAddon.id] = newAddon;
        if (newAddon.type != "bootstrapped")
          return true;

        XPIProvider.bootstrappedAddons[newAddon.id] = addonState.descriptor;
      }

      return false;
    }

    let changed = false;
    let knownLocations = XPIDatabase.getInstallLocations();

    // The install locations are iterated in reverse order of priority so when
    // there are multiple add-ons installed with the same ID the one that
    // should be visible is the first one encountered.
    state.reverse().forEach(function(st) {

      // We can't include the install location directly in the state as it has
      // to be cached as JSON.
      let installLocation = this.installLocationsByName[st.name];
      let addonStates = st.addons;

      // Check if the database knows about any add-ons in this install location.
      let pos = knownLocations.indexOf(installLocation.name);
      if (pos >= 0) {
        knownLocations.splice(pos, 1);
        let addons = XPIDatabase.getAddonsInLocation(installLocation.name);
        // Iterate through the add-ons installed the last time the application
        // ran
        addons.forEach(function(oldAddon) {
          // Check if the add-on is still installed
          if (oldAddon.id in addonStates) {
            let addonState = addonStates[oldAddon.id];
            delete addonStates[oldAddon.id];

            // The add-on has changed if the modification time has changed, or
            // the directory it is installed in has changed or we have an
            // updated manifest for it.
            if (oldAddon.id in manifests[installLocation.name] ||
                oldAddon.updateDate != addonState.mtime ||
                oldAddon._descriptor != addonState.descriptor) {
              changed = updateMetadata(installLocation, oldAddon, addonState) ||
                        changed;
            }
            else {
              changed = updateVisibilityAndCompatibility(installLocation,
                                                         oldAddon, addonState) ||
                        changed;
            }
          }
          else {
            changed = removeMetadata(installLocation, oldAddon) || changed;
          }
        }, this);
      }

      // All the remaining add-ons in this install location must be new.

      // Get the migration data for this install location.
      let locMigrateData = {};
      if (migrateData && installLocation.name in migrateData)
        locMigrateData = migrateData[installLocation.name];
      for (let id in addonStates) {
        changed = addMetadata(installLocation, id, addonStates[id],
                              locMigrateData[id]) || changed;
      }
    }, this);

    // The remaining locations that had add-ons installed in them no longer
    // have any add-ons installed in them, or the locations no longer exist.
    // The metadata for the add-ons that were in them must be removed from the
    // database.
    knownLocations.forEach(function(location) {
      let addons = XPIDatabase.getAddonsInLocation(location);
      addons.forEach(function(oldAddon) {
        changed = removeMetadata(location, oldAddon) || changed;
      }, this);
    }, this);

    // Cache the new install location states
    cache = JSON.stringify(this.getInstallLocationStates());
    Services.prefs.setCharPref(PREF_INSTALL_CACHE, cache);
    return changed;
  },

  /**
   * Checks for any changes that have occurred since the last time the
   * application was launched.
   *
   * @param   appChanged
   *          true if the application has changed version number since the last
   *          launch
   * @return  true if a change requiring a restart was detected
   */
  checkForChanges: function XPI_checkForChanges(appChanged) {
    LOG("checkForChanges");

    // First install any new add-ons into the locations, we'll detect these when
    // we read the install state
    let manifests = {};
    let changed = this.processPendingFileChanges(manifests);

    // We have to hold the DB scheme in prefs so we don't need to load the
    // database to see if we need to migrate data
    let schema = Prefs.getIntPref(PREF_DB_SCHEMA, 0);

    let migrateData = null;
    let cache = null;
    if (schema != DB_SCHEMA) {
      // The schema has changed so migrate data from the old schema
      migrateData = XPIDatabase.migrateData(schema);
    }
    else {
      // If the database exists then the previous file cache can be trusted
      let db = FileUtils.getFile(KEY_PROFILEDIR, [FILE_DATABASE], true);
      if (db.exists())
        cache = Prefs.getCharPref(PREF_INSTALL_CACHE, null);
    }

    // Load the list of bootstrapped add-ons first so processFileChanges can
    // modify it
    this.bootstrappedAddons = JSON.parse(Prefs.getCharPref(PREF_BOOTSTRAP_ADDONS,
                                         "{}"));
    let state = this.getInstallLocationStates();
    if (appChanged || changed || cache == null ||
        cache != JSON.stringify(state)) {
      try {
        changed = this.processFileChanges(state, manifests, appChanged,
                                          migrateData);
      }
      catch (e) {
        ERROR("Error processing file changes: " + e);
      }
    }

    // If the application crashed before completing any pending operations then
    // we should perform them now.
    if (changed || Prefs.getBoolPref(PREF_PENDING_OPERATIONS)) {
      LOG("Restart necessary");
      XPIDatabase.updateActiveAddons();
      Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, false);
      return true;
    }

    LOG("No changes found");

    // Check that the add-ons list still exists
    let addonsList = FileUtils.getFile(KEY_PROFILEDIR, [FILE_XPI_ADDONS_LIST],
                                       true);
    if (!addonsList.exists()) {
      LOG("Add-ons list is missing, recreating");
      XPIDatabase.writeAddonsList();
      return true;
    }

    let bootstrappedAddons = this.bootstrappedAddons;
    this.bootstrappedAddons = {};
    for (let id in bootstrappedAddons) {
      let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
      dir.persistentDescriptor = bootstrappedAddons[id];
      this.activateAddon(id, dir, true, false);
    }

    // Let these shutdown a little earlier when they still have access to most
    // of XPCOM
    Services.obs.addObserver({
      observe: function(subject, topic, data) {
        Services.prefs.setCharPref(PREF_BOOTSTRAP_ADDONS,
                                   JSON.stringify(XPIProvider.bootstrappedAddons));
        for (let id in XPIProvider.bootstrappedAddons)
          XPIProvider.deactivateAddon(id, true, false);
        Services.obs.removeObserver(this, "quit-application-granted");
      }
    }, "quit-application-granted", false);

    return false;
  },

  /**
   * Called to test whether this provider supports installing a particular
   * mimetype.
   *
   * @param   mimetype
   *          The mimetype to check for
   * @return  true if the mimetype is application/x-xpinstall
   */
  supportsMimetype: function XPI_supportsMimetype(mimetype) {
    return mimetype == "application/x-xpinstall";
  },

  /**
   * Called to test whether installing XPI add-ons is enabled.
   *
   * @return  true if installing is enabled
   */
  isInstallEnabled: function XPI_isInstallEnabled() {
    // Default to enabled if the preference does not exist
    return Prefs.getBoolPref(PREF_XPI_ENABLED, true);
  },

  /**
   * Called to test whether installing XPI add-ons from a URI is allowed.
   *
   * @param   uri
   *          The URI being installed from
   * @return  true if installing is allowed
   */
  isInstallAllowed: function XPI_isInstallAllowed(uri) {
    if (!this.isInstallEnabled())
      return false;

    if (!uri)
      return true;

    // file: and chrome: don't need whitelisted hosts
    if (uri.schemeIs("chrome") || uri.schemeIs("file"))
      return true;


    let permission = Services.perms.testPermission(uri, XPI_PERMISSION);
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
   * @param   url
   *          The URL to be installed
   * @param   hash
   *          A hash for the install
   * @param   name
   *          A name for the install
   * @param   iconURL
   *          An icon URL for the install
   * @param   version
   *          A version for the install
   * @param   loadgroup
   *          A loadgroup to associate requests with
   * @param   callback
   *          A callback to pass the AddonInstall to
   */
  getInstallForURL: function XPI_getInstallForURL(url, hash, name, iconURL,
                                                  version, loadgroup, callback) {
    AddonInstall.createDownload(function(install) {
      callback(install.wrapper);
    }, url, hash, name, iconURL, version, loadgroup);
  },

  /**
   * Called to get an AddonInstall to install an add-on from a local file.
   *
   * @param   file
   *          The file to be installed
   * @param   callback
   *          A callback to pass the AddonInstall to
   */
  getInstallForFile: function XPI_getInstallForFile(file, callback) {
    AddonInstall.createInstall(function(install) {
      if (install)
        callback(install.wrapper);
      else
        callback(null);
    }, file);
  },

  /**
   * Called to get an Addon with a particular ID.
   *
   * @param   id
   *          The ID of the add-on to retrieve
   * @param   callback
   *          A callback to pass the Addon to
   */
  getAddon: function XPI_getAddon(id, callback) {
    XPIDatabase.getVisibleAddonForID(id, function(addon) {
      if (addon)
        callback(createWrapper(addon));
      else
        callback(null);
    });
  },

  /**
   * Called to get Addons of a particular type.
   *
   * @param   types
   *          An array of types to fetch. Can be null to get all types.
   * @param   callback
   *          A callback to pass an array of Addons to
   */
  getAddonsByTypes: function XPI_getAddonsByTypes(types, callback) {
    XPIDatabase.getVisibleAddons(types, function(addons) {
      callback([createWrapper(a) for each (a in addons)]);
    });
  },

  /**
   * Called to get Addons that have pending operations.
   *
   * @param   types
   *          An array of types to fetch. Can be null to get all types
   * @param   callback
   *          A callback to pass an array of Addons to
   */
  getAddonsWithPendingOperations:
  function XPI_getAddonsWithPendingOperations(types, callback) {
    XPIDatabase.getVisibleAddonsWithPendingOperations(types, function(addons) {
      let results = [createWrapper(a) for each (a in addons)];
      XPIProvider.installs.forEach(function(install) {
        if (install.state == AddonManager.STATE_INSTALLED &&
            !(install.addon instanceof DBAddonInternal))
          results.push(createWrapper(install.addon));
      });
      callback(results);
    });
  },

  /**
   * Called to get the current AddonInstalls, optionally limiting to a list of
   * types.
   *
   * @param   types
   *          An array of types or null to get all types
   * @param   callback
   *          A callback to pass the array of AddonInstalls to
   */
  getInstalls: function XPI_getInstalls(types, callback) {
    let results = [];
    this.installs.forEach(function(install) {
      if (!types || types.indexOf(install.type) >= 0)
        results.push(install.wrapper);
    });
    callback(results);
  },

  /**
   * Called when a new add-on has been enabled when only one add-on of that type
   * can be enabled.
   *
   * @param   id
   *          The ID of the newly enabled add-on
   * @param   type
   *          The type of the newly enabled add-on
   * @param   pendingRestart
   *          true if the newly enabled add-on will only become enabled after a
   *          restart
   */
  addonChanged: function XPI_addonChanged(id, type, pendingRestart) {
    // We only care about themes in this provider
    if (type != "theme")
      return;

    if (!id) {
      // Fallback to the default theme when no theme was enabled
      this.enableDefaultTheme();
      return;
    }

    // Look for the previously enabled theme and find the internalName of the
    // currently selected theme
    let previousTheme = null;
    let newSkin = this.defaultSkin;
    let addons = XPIDatabase.getAddonsByType("theme");
    addons.forEach(function(a) {
      if (!a.visible)
        return;
      if (a.id == id)
        newSkin = a.internalName;
      else if (a.userDisabled == false && !a.pendingUninstall)
        previousTheme = a;
    }, this);

    if (pendingRestart) {
      Services.prefs.setBoolPref(PREF_DSS_SWITCHPENDING, true);
      Services.prefs.setCharPref(PREF_DSS_SKIN_TO_SELECT, newSkin);
    }
    else {
      Services.prefs.setCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN, newSkin);
    }
    this.selectedSkin = newSkin;

    // Mark the previous theme as disabled. This won't cause recursion since
    // only enabled calls notifyAddonChanged.
    if (previousTheme)
      this.updateAddonDisabledState(previousTheme, true);
  },

  /**
   * When the previously selected theme is removed this method will be called
   * to enable the default theme.
   */
  enableDefaultTheme: function XPI_enableDefaultTheme() {
    LOG("Activating default theme");
    let addons = XPIDatabase.getAddonsByType("theme");
    addons.forEach(function(a) {
      // If this is theme contains the default skin and it is currently visible
      // then mark it as enabled.
      if (a.internalName == this.defaultSkin && a.visible)
        this.updateAddonDisabledState(a, false);
    }, this);
  },

  /**
   * Notified when a preference we're interested in has changed.
   *
   * @see nsIObserver
   */
  observe: function XPI_observe(subject, topic, data) {
    switch (data) {
    case this.checkCompatibilityPref:
    case PREF_EM_CHECK_UPDATE_SECURITY:
      this.checkCompatibility = Prefs.getBoolPref(this.checkCompatibilityPref,
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
   * @param   addon
   *          The add-on to test
   * @return  true if the operation requires a restart
   */
  enableRequiresRestart: function XPI_enableRequiresRestart(addon) {
    // If the theme we're enabling is the skin currently selected then it doesn't
    // require a restart to enable it.
    if (addon.type == "theme")
      return addon.internalName !=
             Prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN);

    return addon.type != "bootstrapped";
  },

  /**
   * Tests whether disabling an add-on will require a restart.
   *
   * @param   addon
   *          The add-on to test
   * @return  true if the operation requires a restart
   */
  disableRequiresRestart: function XPI_disableRequiresRestart(addon) {
    // This sounds odd but it is correct. Themes are only ever asked to disable
    // after another theme has been enabled. Disabling the theme only requires
    // a restart if enabling the other theme does too. If the selected skin doesn't
    // match the current skin then a restart is necessary.
    if (addon.type == "theme")
      return this.selectedSkin !=
             Prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN);

    return addon.type != "bootstrapped";
  },

  /**
   * Tests whether installing an add-on will require a restart.
   *
   * @param   addon
   *          The add-on to test
   * @return  true if the operation requires a restart
   */
  installRequiresRestart: function XPI_installRequiresRestart(addon) {
    // Themes not currently in use can be installed immediately
    if (addon.type == "theme")
      return addon.internalName ==
             Prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN);

    return addon.type != "bootstrapped";
  },

  /**
   * Tests whether uninstalling an add-on will require a restart.
   *
   * @param   addon
   *          The add-on to test
   * @return  true if the operation requires a restart
   */
  uninstallRequiresRestart: function XPI_uninstallRequiresRestart(addon) {
    // Themes not currently in use can be uninstalled immediately
    if (addon.type == "theme")
      return addon.internalName ==
             Prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN);

    return addon.type != "bootstrapped";
  },

  /**
   * Calls a method in a bootstrap.js loaded scope logging any exceptions thrown.
   *
   * @param   id
   *          The ID of the add-on being bootstrapped
   * @param   scope
   *          The loaded JS scope to call into
   * @param   methods
   *          An array of methods. The first method in the array that exists in
   *          the scope will be called
   */
  callBootstrapMethod: function XPI_callBootstrapMethod(id, scope, methods) {
    for (let i = 0; i < methods.length; i++) {
      if (methods[i] in scope) {
        LOG("Calling bootstrap method " + methods[i] + " on " + id);

        try {
          scope[methods[i]](id);
        }
        catch (e) {
          WARN("Exception running bootstrap method " + methods[i] + " on " +
               id + ": " + e);
        }
        return;
      }
    }
  },

  /**
   * Activates a bootstrapped add-on by loading its JS scope and calling the
   * appropriate method on it. Adds the add-on and its scope to the
   * bootstrapScopes and bootstrappedAddons dictionaries.
   *
   * @param   id
   *          The ID of the add-on being activated
   * @param   dir
   *          The directory containing the add-on
   * @param   startup
   *          true if the add-on is being activated during startup
   * @param   install
   *          true if the add-on is being activated during installation
   */
  activateAddon: function XPI_activateAddon(id, dir, startup, install) {
    let methods = ["enable"];
    if (startup)
      methods.unshift("startup");
    if (install)
      methods.unshift("install");
    let bootstrap = dir.clone();
    bootstrap.append("bootstrap.js");
    if (bootstrap.exists()) {
      let uri = Services.io.newFileURI(bootstrap);
      let principal = Cc["@mozilla.org/systemprincipal;1"].
                      createInstance(Ci.nsIPrincipal);
      let scope = new Components.utils.Sandbox(principal);
      let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                   createInstance(Ci.mozIJSSubScriptLoader);

      try {
        loader.loadSubScript(uri.spec, scope);
      }
      catch (e) {
        WARN("Error loading bootstrap.js for " + id + ": " + e);
        return;
      }

      this.callBootstrapMethod(id, scope, methods);
      this.bootstrappedAddons[id] = dir.persistentDescriptor;
      this.bootstrapScopes[id] = scope;
    }
    else {
      WARN("Bootstrap missing for " + id);
    }
  },

  /**
   * Dectivates a bootstrapped add-on by by calling the appropriate method on
   * the cached JS scope. Removes the add-on and its scope from the
   * bootstrapScopes and bootstrappedAddons dictionaries.
   *
   * @param   id
   *          The ID of the add-on being deactivated
   * @param   shutdown
   *          true if the add-on is being deactivated during shutdown
   * @param   uninstall
   *          true if the add-on is being deactivated during uninstallation
   */
  deactivateAddon: function XPI_deactivateAddon(id, shutdown, uninstall) {
    if (!(id in this.bootstrappedAddons)) {
      ERROR("Attempted to deactivate an add-on that was never activated");
      return;
    }
    let scope = this.bootstrapScopes[id];
    delete this.bootstrappedAddons[id];
    delete this.bootstrapScopes[id];

    let methods = ["disable"];
    if (shutdown)
      methods.unshift("shutdown");
    if (uninstall)
      methods.unshift("uninstall");
    this.callBootstrapMethod(id, scope, methods);
  },

  /**
   * Updates the appDisabled property for all add-ons.
   */
  updateAllAddonDisabledStates: function XPI_updateAllAddonDisabledStates() {
    let addons = XPIDatabase.getAddons();
    addons.forEach(function(addon) {
      this.updateAddonDisabledState(addon);
    }, this);
  },

  /**
   * Updates the disabled state for an add-on. Its appDisabled property will be
   * calculated and if the add-on is changed appropriate notifications will be
   * sent out to the registered AddonListeners.
   *
   * @param   addon
   *          The DBAddonInternal to update
   * @param   userDisabled
   *          Value for the userDisabled property. If undefined the value will
   *          not change
   * @throws  if addon is not a DBAddonInternal
   */
  updateAddonDisabledState: function XPI_updateAddonDisabledState(addon,
                                                                  userDisabled) {
    if (!(addon instanceof DBAddonInternal))
      throw new Error("Can only update addon states for installed addons.");

    if (userDisabled === undefined)
      userDisabled = addon.userDisabled;

    let appDisabled = !isUsableAddon(addon);
    // No change means nothing to do here
    if (addon.userDisabled == userDisabled &&
        addon.appDisabled == appDisabled)
      return;

    let wasDisabled = addon.userDisabled || addon.appDisabled;
    let isDisabled = userDisabled || appDisabled;

    // Update the properties in the database
    XPIDatabase.setAddonProperties(addon, {
      userDisabled: userDisabled,
      appDisabled: appDisabled
    });

    // If the add-on is not visible or the add-on is not changing state then
    // there is no need to do anything else
    if (!addon.visible || (wasDisabled == isDisabled))
      return;

    // Flag that active states in the database need to be updated on shutdown
    Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);

    let wrapper = createWrapper(addon);
    // Have we just gone back to the current state?
    if (isDisabled != addon.active) {
      AddonManagerPrivate.callAddonListeners("onOperationCancelled", wrapper);
    }
    else {
      if (isDisabled) {
        var needsRestart = this.disableRequiresRestart(addon);
        AddonManagerPrivate.callAddonListeners("onDisabling", wrapper,
                                               needsRestart);
      }
      else {
        needsRestart = this.enableRequiresRestart(addon);
        AddonManagerPrivate.callAddonListeners("onEnabling", wrapper,
                                               needsRestart);
      }

      if (!needsRestart) {
        addon.active = !isDisabled;
        XPIDatabase.updateAddonActive(addon);
        if (isDisabled) {
          if (addon.type == "bootstrapped")
            this.deactivateAddon(addon.id, false, false);
          AddonManagerPrivate.callAddonListeners("onDisabled", wrapper);
        }
        else {
          if (addon.type == "bootstrapped") {
            let dir = addon._installLocation.getLocationForID(addon.id);
            this.activateAddon(addon.id, dir, false, false);
          }
          AddonManagerPrivate.callAddonListeners("onEnabled", wrapper);
        }
      }
    }

    // Notify any other providers that a new theme has been enabled
    if (addon.type == "theme" && !isDisabled)
      AddonManagerPrivate.notifyAddonChanged(addon.id, addon.type, true);
  },

  /**
   * Uninstalls an add-on, immediately if possible or marks it as pending
   * uninstall if not.
   *
   * @param   addon
   *          The DBAddonInternal to uninstall
   * @throws  if the addon cannot be uninstalled because it is in an install
   *          location that does not allow it
   */
  uninstallAddon: function XPI_uninstallAddon(addon) {
    if (!(addon instanceof DBAddonInternal))
      throw new Error("Can only uninstall installed addons.");

    if (addon._installLocation.locked)
      throw new Error("Cannot uninstall addons from locked install locations");

    // Inactive add-ons don't require a restart to uninstall
    let requiresRestart = addon.active && this.uninstallRequiresRestart(addon);

    if (requiresRestart) {
      // We create an empty directory in the staging directory to indicate that
      // an uninstall is necessary on next startup.
      let stage = addon._installLocation.getStagingDir();
      stage.append(addon.id);
      if (!stage.exists())
        stage.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

      XPIDatabase.setAddonProperties(addon, {
        pendingUninstall: true
      });
      Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);
    }

    // If the add-on is not visible then there is no need to notify listeners.
    if (!addon.visible)
      return;

    let wrapper = createWrapper(addon);
    AddonManagerPrivate.callAddonListeners("onUninstalling", wrapper,
                                           requiresRestart);

    if (!requiresRestart) {
      if (addon.type == "bootstrapped")
        this.deactivateAddon(addon.id, false, true);
      addon._installLocation.uninstallAddon(addon.id);
      XPIDatabase.removeAddonMetadata(addon);
      AddonManagerPrivate.callAddonListeners("onUninstalled", wrapper);
      // TODO reveal hidden add-ons (bug 557710)
    }

    // Notify any other providers that a new theme has been enabled
    if (addon.type == "theme" && addon.active)
      AddonManagerPrivate.notifyAddonChanged(null, addon.type, requiresRestart);
  },

  /**
   * Cancels the pending uninstall of an add-on.
   *
   * @param   addon
   *          The DBAddonInternal to cancel uninstall for
   */
  cancelUninstallAddon: function XPI_cancelUninstallAddon(addon) {
    if (!(addon instanceof DBAddonInternal))
      throw new Error("Can only cancel uninstall for installed addons.");

    let stagedAddon = addon._installLocation.getStagingDir();
    stagedAddon.append(addon.id);
    if (stagedAddon.exists())
      stagedAddon.remove(true);

    XPIDatabase.setAddonProperties(addon, {
      pendingUninstall: false
    });

    if (!addon.visible)
      return;

    Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);

    // TODO hide hidden add-ons (bug 557710)
    let wrapper = createWrapper(addon);
    AddonManagerPrivate.callAddonListeners("onOperationCancelled", wrapper);

    // Notify any other providers that this theme is now enabled again.
    if (addon.type == "theme" && addon.active)
      AddonManagerPrivate.notifyAddonChanged(addon.id, addon.type, false);
  }
};

const FIELDS_ADDON = "internal_id, id, location, version, type, internalName, " +
                     "updateURL, updateKey, optionsURL, aboutURL, iconURL, " +
                     "defaultLocale, visible, active, userDisabled, appDisabled, " +
                     "pendingUninstall, descriptor, installDate, updateDate";

// A helper function to simply log any errors that occur during async statements.
function asyncErrorLogger(error) {
  ERROR("SQL error " + error.result + ": " + error.message);
}

var XPIDatabase = {
  // true if the database connection has been opened
  initialized: false,
  // A cache of statements that are used and need to be finalized on shutdown
  statementCache: {},
  // A cache of weak referenced DBAddonInternals so we can reuse objects where
  // possible
  addonCache: [],

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
    _readLocaleStrings: "SELECT locale_id, type, value FROM locale_strings " +
                        "WHERE locale_id=:id",

    addAddonMetadata_addon: "INSERT INTO addon VALUES (NULL, :id, :location, " +
                            ":version, :type, :internalName, :updateURL, " +
                            ":updateKey, :optionsURL, :aboutURL, :iconURL, " +
                            ":locale, :visible, :active, :userDisabled," +
                            " :appDisabled, 0, :descriptor, :installDate, " +
                            ":updateDate)",
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

    clearVisibleAddons: "UPDATE addon SET visible=0 WHERE id=:id",
    deactivateThemes: "UPDATE addon SET active=:active WHERE " +
                      "internal_id=:internal_id",

    getActiveAddons: "SELECT " + FIELDS_ADDON + " FROM addon WHERE active=1 AND " +
                     "type<>'theme' AND type<>'bootstrapped'",
    getActiveTheme: "SELECT " + FIELDS_ADDON + " FROM addon WHERE " +
                    "internalName=:internalName AND type='theme'",

    getAddonInLocation: "SELECT " + FIELDS_ADDON + " FROM addon WHERE id=:id " +
                        "AND location=:location",
    getAddons: "SELECT " + FIELDS_ADDON + " FROM addon",
    getAddonsByType: "SELECT " + FIELDS_ADDON + " FROM addon WHERE type=:type",
    getAddonsInLocation: "SELECT " + FIELDS_ADDON + " FROM addon WHERE " +
                         "location=:location",
    getInstallLocations: "SELECT DISTINCT location FROM addon",
    getVisibleAddonForID: "SELECT " + FIELDS_ADDON + " FROM addon WHERE " +
                          "visible=1 AND id=:id",
    getVisibleAddons: "SELECT " + FIELDS_ADDON + " FROM addon WHERE visible=1",
    getVisibleAddonsWithPendingOperations: "SELECT " + FIELDS_ADDON + " FROM " +
                                           "addon WHERE visible=1 " +
                                           "AND (pendingUninstall=1 OR " +
                                           "MAX(userDisabled,appDisabled)=active)",

    makeAddonVisible: "UPDATE addon SET visible=1 WHERE internal_id=:internal_id",
    removeAddonMetadata: "DELETE FROM addon WHERE internal_id=:internal_id",
    // Equates to active = visible && !userDisabled && !appDisabled && !pendingUninstall
    setActiveAddons: "UPDATE addon SET active=MIN(visible, 1 - userDisabled, " +
                     "1 - appDisabled, 1 - pendingUninstall)",
    setAddonProperties: "UPDATE addon SET userDisabled=:userDisabled, " +
                        "appDisabled=:appDisabled, " +
                        "pendingUninstall=:pendingUninstall WHERE " +
                        "internal_id=:internal_id",
    updateTargetApplications: "UPDATE targetApplication SET " +
                              "minVersion=:minVersion, maxVersion=:maxVersion " +
                              "WHERE addon_internal_id=:internal_id AND id=:id",
  },

  /**
   * Opens a new connection to the database file.
   *
   * @return  the mozIStorageConnection for the database
   */
  openConnection: function XPIDB_openConnection() {
    this.initialized = true;
    let dbfile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_DATABASE], true);
    delete this.connection;
    return this.connection = Services.storage.openDatabase(dbfile);
  },

  /**
   * A lazy getter for the database connection.
   */
  get connection() {
    return this.openConnection();
  },

  /**
   * Migrates data from a previous database schema.
   *
   * @param   oldSchema
   *          The previous schema
   * @return  an object holding information about what add-ons were previously
   *          userDisabled
   */
  migrateData: function XPIDB_migrateData(oldSchema) {
    LOG("Migrating data from schema " + oldSchema);
    let migrateData = {};

    if (oldSchema == 0) {
      // Migrate data from extensions.rdf
      let rdffile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_OLD_DATABASE], true);
      if (rdffile.exists()) {
        let RDF = Cc["@mozilla.org/rdf/rdf-service;1"].
                  getService(Ci.nsIRDFService);
        const PREFIX_NS_EM = "http://www.mozilla.org/2004/em-rdf#";

        function EM_R(property) {
          return RDF.GetResource(PREFIX_NS_EM + property);
        }
        let ds = RDF.GetDataSourceBlocking(Services.io.newFileURI(rdffile).spec);

        // Look for any add-ons that were disabled or going to be disabled
        ["true", "needs-disable"].forEach(function(val) {
          let sources = ds.GetSources(EM_R("userDisabled"), RDF.GetLiteral(val),
                                      true);
          while (sources.hasMoreElements()) {
            let source = sources.getNext().QueryInterface(Ci.nsIRDFResource);
            let location = ds.GetTarget(source, EM_R("installLocation"), true);
            if (location instanceof Ci.nsIRDFLiteral) {
              if (!(location.Value in migrateData))
                migrateData[location.Value] = {};
              let id = source.ValueUTF8.substring(PREFIX_ITEM_URI.length);
              migrateData[location.Value][id] = {
                userDisabled: true
              }
            }
          }
        });
      }
    }
    else {
      // Attempt to migrate data from a different (even future!) version of the
      // database
      try {
        var stmt = this.connection.createStatement("SELECT id, location, " +
                                                   "userDisabled, installDate " +
                                                   "FROM addon");
        for (let row in resultRows(stmt)) {
          if (!(row.location in migrateData))
            migrateData[row.location] = {};
          migrateData[row.location][row.id] = {
            installDate: row.installDate,
            userDisabled: row.userDisabled == 1
          };
        }
      }
      catch (e) {
        // An error here means the schema is too different to read
        ERROR("Error migrating data: " + e);
      }
      finally {
        stmt.finalize();
      }
      this.connection.close();
    }

    // Create a clean database to work with
    let dbfile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_DATABASE], true);
    if (dbfile.exists())
      dbfile.remove(true);
    this.openConnection();
    this.createSchema();

    return migrateData;
  },

  /**
   * Shuts down the database connection and releases all cached objects.
   */
  shutdown: function XPIDB_shutdown() {
    if (this.initialized) {
      for each (let stmt in this.statementCache)
        stmt.finalize();
      this.statementCache = {};
      this.addonCache = [];
      this.connection.asyncClose();
      this.initialized = false;
      delete this.connection;

      // Re-create the connection smart getter to allow the database to be
      // re-loaded during testing.
      this.__defineGetter__("connection", function() {
        return this.openConnection();
      });
    }
  },

  /**
   * Gets a cached statement or creates a new statement if it doesn't already
   * exist.
   *
   * @param   key
   *          A unique key to reference the statement
   * @param   sql
   *          An optional SQL string to use for the query, otherwise a
   *          predefined sql string for the key will be used.
   * @return  a mozIStorageStatement for the passed SQL
   */
  getStatement: function XPIDB_getStatement(key, sql) {
    if (key in this.statementCache)
      return this.statementCache[key];
    if (!sql)
      sql = this.statements[key];

    try {
      return this.statementCache[key] = this.connection.createStatement(sql);
    }
    catch (e) {
      ERROR("Error creating statement " + key + " (" + sql + ")");
      throw e;
    }
  },

  /**
   * Creates the schema in the database.
   */
  createSchema: function XPIDB_createSchema() {
    LOG("Creating database schema");
    this.connection.createTable("addon",
                                "internal_id INTEGER PRIMARY KEY AUTOINCREMENT, " +
                                "id TEXT, location TEXT, version TEXT, " +
                                "type TEXT, internalName TEXT, updateURL TEXT, " +
                                "updateKey TEXT, optionsURL TEXT, aboutURL TEXT, " +
                                "iconURL TEXT, defaultLocale INTEGER, " +
                                "visible INTEGER, active INTEGER, " +
                                "userDisabled INTEGER, appDisabled INTEGER, " +
                                "pendingUninstall INTEGER, descriptor TEXT, " +
                                "installDate INTEGER, updateDate INTEGER, " +
                                "UNIQUE (id, location)");
    this.connection.createTable("targetApplication",
                                "addon_internal_id INTEGER, " +
                                "id TEXT, minVersion TEXT, maxVersion TEXT, " +
                                "UNIQUE (addon_internal_id, id)");
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
    Services.prefs.setIntPref(PREF_DB_SCHEMA, DB_SCHEMA);
  },

  /**
   * Synchronously reads the multi-value locale strings for a locale
   *
   * @param   locale
   *          The locale object to read into
   */
  _readLocaleStrings: function XPIDB__readLocaleStrings(locale) {
    let stmt = this.getStatement("_readLocaleStrings");

    stmt.params.id = locale.id;
    for (let row in resultRows(stmt)) {
      if (!(row.type in locale))
        locale[row.type] = [];
      locale[row.type].push(row.value);
    }
  },

  /**
   * Synchronously reads the locales for an add-on
   *
   * @param   addon
   *          The DBAddonInternal to read the locales for
   * @return  the array of locales
   */
  _getLocales: function XPIDB__getLocales(addon) {
    let stmt = this.getStatement("_getLocales");

    let locales = [];
    stmt.params.internal_id = addon._internal_id;
    for (let row in resultRows(stmt)) {
      let locale = {
        id: row.id,
        locales: [row.locale]
      };
      copyProperties(row, PROP_LOCALE_SINGLE, locale);
      locales.push(locale);
    }
    locales.forEach(function(locale) {
      this._readLocaleStrings(locale);
    }, this);
    return locales;
  },

  /**
   * Synchronously reads the default locale for an add-on
   *
   * @param   addon
   *          The DBAddonInternal to read the default locale for
   * @return  the default locale for the add-on
   * @throws  if the database does not contain the default locale information
   */
  _getDefaultLocale: function XPIDB__getDefaultLocale(addon) {
    let stmt = this.getStatement("_getDefaultLocale");

    stmt.params.id = addon._defaultLocale;
    if (!stmt.executeStep())
      throw new Error("Missing default locale for " + addon.id);
    let locale = copyProperties(stmt.row, PROP_LOCALE_SINGLE);
    locale.id = addon._defaultLocale;
    stmt.reset();
    this._readLocaleStrings(locale);
    return locale;
  },

  /**
   * Synchronously reads the target application entries for an add-on
   *
   * @param   addon
   *          The DBAddonInternal to read the target applications for
   * @return  an array of target applications
   */
  _getTargetApplications: function XPIDB__getTargetApplications(addon) {
    let stmt = this.getStatement("_getTargetApplications");

    stmt.params.internal_id = addon._internal_id;
    return [copyProperties(row, PROP_TARGETAPP) for each (row in resultRows(stmt))];
  },

  /**
   * Synchronously makes a DBAddonInternal from a storage row or returns one
   *
   * from the cache.
   * @param   row
   *          The storage row to make the DBAddonInternal from
   * @return  a DBAddonInternal
   */
  makeAddonFromRow: function XPIDB_makeAddonFromRow(row) {
    if (this.addonCache[row.internal_id]) {
      let addon = this.addonCache[row.internal_id].get();
      if (addon)
        return addon;
    }

    let addon = new DBAddonInternal();
    addon._internal_id = row.internal_id;
    addon._installLocation = XPIProvider.installLocationsByName[row.location];
    addon._descriptor = row.descriptor;
    copyProperties(row, PROP_METADATA, addon);
    addon._defaultLocale = row.defaultLocale;
    addon.installDate = row.installDate;
    addon.updateDate = row.updateDate;
    ["visible", "active", "userDisabled", "appDisabled",
     "pendingUninstall"].forEach(function(prop) {
      addon[prop] = row[prop] != 0;
    });
    this.addonCache[row.internal_id] = Components.utils.getWeakReference(addon);
    return addon;
  },

  /**
   * Asynchronously fetches additional metadata for a DBAddonInternal.
   *
   * @param   addon
   *          The DBAddonInternal
   * @param   callback
   *          The callback to call when the metadata is completely retrieved
   */
  fetchAddonMetadata: function XPIDB_fetchAddonMetadata(addon, callback) {
    function readLocaleStrings(locale, callback) {
      let stmt = XPIDatabase.getStatement("_readLocaleStrings");

      stmt.params.id = locale.id;
      stmt.executeAsync({
        handleResult: function(results) {
          let row = null;
          while (row = results.getNextRow()) {
            let type = row.getResultByName("type");
            if (!(type in locale))
              locale[type] = [];
            locale[type].push(row.getResultByName("value"));
          }
        },

        handleError: asyncErrorLogger,

        handleCompletion: function(reason) {
          callback();
        }
      });
    }

    function readDefaultLocale() {
      delete addon.defaultLocale;
      let stmt = XPIDatabase.getStatement("_getDefaultLocale");

      stmt.params.id = addon._defaultLocale;
      stmt.executeAsync({
        handleResult: function(results) {
          addon.defaultLocale = copyRowProperties(results.getNextRow(),
                                                  PROP_LOCALE_SINGLE);
          addon.defaultLocale.id = addon._defaultLocale;
        },

        handleError: asyncErrorLogger,

        handleCompletion: function(reason) {
          if (addon.defaultLocale) {
            readLocaleStrings(addon.defaultLocale, readLocales);
          }
          else {
            ERROR("Missing default locale for " + addon.id);
            readLocales();
          }
        }
      });
    }

    function readLocales() {
      delete addon.locales;
      addon.locales = [];
      let stmt = XPIDatabase.getStatement("_getLocales");

      stmt.params.internal_id = addon._internal_id;
      stmt.executeAsync({
        handleResult: function(results) {
          let row = null;
          while (row = results.getNextRow()) {
            let locale = {
              id: row.getResultByName("id"),
              locales: [row.getResultByName("locale")]
            };
            copyRowProperties(row, PROP_LOCALE_SINGLE, locale);
            addon.locales.push(locale);
          }
        },

        handleError: asyncErrorLogger,

        handleCompletion: function(reason) {
          let pos = 0;
          function readNextLocale() {
            if (pos < addon.locales.length)
              readLocaleStrings(addon.locales[pos++], readNextLocale);
            else
              readTargetApplications();
          }

          readNextLocale();
        }
      });
    }

    function readTargetApplications() {
      delete addon.targetApplications;
      addon.targetApplications = [];
      let stmt = XPIDatabase.getStatement("_getTargetApplications");

      stmt.params.internal_id = addon._internal_id;
      stmt.executeAsync({
        handleResult: function(results) {
          let row = null;
          while (row = results.getNextRow())
            addon.targetApplications.push(copyRowProperties(row, PROP_TARGETAPP));
        },

        handleError: asyncErrorLogger,

        handleCompletion: function(reason) {
          callback(addon);
        }
      });
    }

    readDefaultLocale();
  },

  /**
   * Synchronously makes a DBAddonInternal from a mozIStorageRow or returns one
   * from the cache.
   *
   * @param   row
   *          The mozIStorageRow to make the DBAddonInternal from
   * @return  a DBAddonInternal
   */
  makeAddonFromRowAsync: function XPIDB_makeAddonFromRowAsync(row) {
    let internal_id = row.getResultByName("internal_id");
    if (this.addonCache[internal_id]) {
      let addon = this.addonCache[internal_id].get();
      if (addon)
        return addon;
    }

    let addon = new DBAddonInternal();
    addon._internal_id = internal_id;
    let location = row.getResultByName("location");
    addon._installLocation = XPIProvider.installLocationsByName[location];
    addon._descriptor = row.getResultByName("descriptor");
    copyRowProperties(row, PROP_METADATA, addon);
    addon._defaultLocale = row.getResultByName("defaultLocale");
    addon.installDate = row.getResultByName("installDate");
    addon.updateDate = row.getResultByName("updateDate");
    ["visible", "active", "userDisabled", "appDisabled",
     "pendingUninstall"].forEach(function(prop) {
      addon[prop] = row.getResultByName(prop) != 0;
    });
    this.addonCache[internal_id] = Components.utils.getWeakReference(addon);
    return addon;
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
   * @param   location
   *          The name of the install location
   * @return  an array of DBAddonInternals
   */
  getAddonsInLocation: function XPIDB_getAddonsInLocation(location) {
    let stmt = this.getStatement("getAddonsInLocation");

    stmt.params.location = location;
    return [this.makeAddonFromRow(row) for each (row in resultRows(stmt))];
  },

  /**
   * Asynchronously gets an add-on with a particular ID in a particular
   * install location.
   *
   * @param   id
   *          The ID of the add-on to retrieve
   * @param   locations
   *          The name of the install location
   * @param   callback
   *          A callback to pass the DBAddonInternal to
   */
  getAddonInLocation: function XPIDB_getAddonInLocation(id, location, callback) {
    let stmt = this.getStatement("getAddonInLocation");

    stmt.params.id = id;
    stmt.params.location = location;
    stmt.executeAsync({
      addon: null,

      handleResult: function(results) {
        this.addon = XPIDatabase.makeAddonFromRowAsync(results.getNextRow());
      },

      handleError: asyncErrorLogger,

      handleCompletion: function(reason) {
        if (this.addon)
          XPIDatabase.fetchAddonMetadata(this.addon, callback);
        else
          callback(null);
      }
    });
  },

  /**
   * Asynchronously gets the add-on with an ID that is visible.
   *
   * @param   id
   *          The ID of the add-on to retrieve
   * @param   callback
   *          A callback to pass the DBAddonInternal to
   */
  getVisibleAddonForID: function XPIDB_getVisibleAddonForID(id, callback) {
    let stmt = this.getStatement("getVisibleAddonForID");

    stmt.params.id = id;
    stmt.executeAsync({
      addon: null,

      handleResult: function(results) {
        this.addon = XPIDatabase.makeAddonFromRowAsync(results.getNextRow());
      },

      handleError: asyncErrorLogger,

      handleCompletion: function(reason) {
        if (this.addon)
          XPIDatabase.fetchAddonMetadata(this.addon, callback);
        else
          callback(null);
      }
    });
  },

  /**
   * Asynchronously gets the visible add-ons, optionally restricting by type.
   *
   * @param   types
   *          An array of types to include or null to include all types
   * @param   callback
   *          A callback to pass the array of DBAddonInternals to
   */
  getVisibleAddons: function XPIDB_getVisibleAddons(types, callback) {
    let stmt = null;
    if (!types || types.length == 0) {
      stmt = this.getStatement("getVisibleAddons");
    }
    else {
      let sql = "SELECT * FROM addon WHERE visible=1 AND type IN (";
      for (let i = 1; i <= types.length; i++) {
        sql += "?" + i;
        if (i < types.length)
          sql += ",";
      }
      sql += ")";

      // Note that binding to index 0 sets the value for the ?1 parameter
      stmt = this.getStatement("getVisibleAddons_" + types.length, sql);
      for (let i = 0; i < types.length; i++)
        stmt.bindStringParameter(i, types[i]);
    }

    let addons = [];
    stmt.executeAsync({
      handleResult: function(results) {
        let row = null;
        while (row = results.getNextRow())
          addons.push(XPIDatabase.makeAddonFromRowAsync(row));
      },

      handleError: asyncErrorLogger,

      handleCompletion: function(reason) {
        let pos = 0;
        function readNextAddon() {
          if (pos < addons.length)
            XPIDatabase.fetchAddonMetadata(addons[pos++], readNextAddon);
          else
            callback(addons);
        }

        readNextAddon();
      }
    });
  },

  /**
   * Synchronously gets all add-ons of a particular type.
   *
   * @param   type
   *          The type of add-on to retrieve
   * @return  an array of DBAddonInternals
   */
  getAddonsByType: function XPIDB_getAddonsByType(type) {
    let stmt = this.getStatement("getAddonsByType");

    stmt.params.type = type;
    return [this.makeAddonFromRow(row) for each (row in resultRows(stmt))];;
  },

  /**
   * Asynchronously gets all add-ons with pending operations.
   *
   * @param   types
   *          The types of add-ons to retrieve or null to get all types
   * @param   callback
   *          A callback to pass the array of DBAddonInternal to
   */
  getVisibleAddonsWithPendingOperations:
    function XPIDB_getVisibleAddonsWithPendingOperations(types, callback) {
    let stmt = null;
    if (!types || types.length == 0) {
      stmt = this.getStatement("getVisibleAddonsWithPendingOperations");
    }
    else {
      let sql = "SELECT * FROM addon WHERE visible=1 AND " +
                "(pendingUninstall=1 OR MAX(userDisabled,appDisabled)=active) " +
                "AND type IN (";
      for (let i = 1; i <= types.length; i++) {
        sql += "?" + i;
        if (i < types.length)
          sql += ",";
      }
      sql += ")";

      // Note that binding to index 0 sets the value for the ?1 parameter
      stmt = this.getStatement("getVisibleAddonsWithPendingOperations_" +
                               types.length, sql);
      for (let i = 0; i < types.length; i++)
        stmt.bindStringParameter(i, types[i]);
    }

    let addons = [];
    stmt.executeAsync({
      handleResult: function(results) {
        let row = null;
        while (row = results.getNextRow())
          addons.push(XPIDatabase.makeAddonFromRowAsync(row));
      },

      handleError: asyncErrorLogger,

      handleCompletion: function(reason) {
        let pos = 0;
        function readNextAddon() {
          if (pos < addons.length)
            XPIDatabase.fetchAddonMetadata(addons[pos++], readNextAddon);
          else
            callback(addons);
        }

        readNextAddon();
      }
    });
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
   * @param   addon
   *          AddonInternal to add
   * @param   descriptor
   *          The file descriptor of the add-on's directory
   */
  addAddonMetadata: function XPIDB_addAddonMetadata(addon, descriptor) {
    let localestmt = this.getStatement("addAddonMetadata_locale");
    let stringstmt = this.getStatement("addAddonMetadata_strings");

    function insertLocale(locale) {
      copyProperties(locale, PROP_LOCALE_SINGLE, localestmt.params);
      localestmt.execute();
      let row = XPIDatabase.connection.lastInsertRowID;

      PROP_LOCALE_MULTI.forEach(function(prop) {
        locale[prop].forEach(function(str) {
          stringstmt.params.locale = row;
          stringstmt.params.type = prop;
          stringstmt.params.value = str;
          stringstmt.execute();
        });
      });
      return row;
    }

    if (addon.visible) {
      let stmt = this.getStatement("clearVisibleAddons");
      stmt.params.id = addon.id;
      stmt.execute();
    }

    let stmt = this.getStatement("addAddonMetadata_addon");

    stmt.params.locale = insertLocale(addon.defaultLocale);
    stmt.params.location = addon._installLocation.name;
    stmt.params.descriptor = descriptor;
    stmt.params.installDate = addon.installDate;
    stmt.params.updateDate = addon.updateDate;
    copyProperties(addon, PROP_METADATA, stmt.params);
    ["visible", "userDisabled", "appDisabled"].forEach(function(prop) {
      stmt.params[prop] = addon[prop] ? 1 : 0;
    });
    stmt.params.active = (addon.visible && !addon.userDisabled &&
                          !addon.appDisabled) ? 1 : 0;
    stmt.execute();
    let internal_id = this.connection.lastInsertRowID;

    stmt = this.getStatement("addAddonMetadata_addon_locale");
    addon.locales.forEach(function(locale) {
      let id = insertLocale(locale);
      locale.locales.forEach(function(name) {
        stmt.params.internal_id = internal_id;
        stmt.params.name = name;
        stmt.params.locale = insertLocale(locale);
        stmt.execute();
      });
    });

    stmt = this.getStatement("addAddonMetadata_targetApplication");

    addon.targetApplications.forEach(function(app) {
      stmt.params.internal_id = internal_id;
      stmt.params.id = app.id;
      stmt.params.minVersion = app.minVersion;
      stmt.params.maxVersion = app.maxVersion;
      stmt.execute();
    });
  },

  /**
   * Synchronously updates an add-ons metadata in the database. Currently just
   * removes and recreates.
   *
   * @param   oldAddon
   *          The DBAddonInternal to be replaced
   * @param   newAddon
   *          The new AddonInternal to add
   * @param   descriptor
   *          The file descriptor of the add-on's directory
   */
  updateAddonMetadata: function XPIDB_updateAddonMetadata(oldAddon, newAddon,
                                                          descriptor) {
    this.removeAddonMetadata(oldAddon);
    this.addAddonMetadata(newAddon, descriptor);
  },

  /**
   * Synchronously updates the target application entries for an add-on.
   *
   * @param   addon
   *          The DBAddonInternal being updated
   * @param   targets
   *          The array of target applications to update
   */
  updateTargetApplications: function XPIDB_updateTargetApplications(addon,
                                                                    targets) {
    let stmt = this.getStatement("updateTargetApplications");
    targets.forEach(function(target) {
      stmt.params.internal_id = addon._internal_id;
      stmt.params.id = target.id;
      stmt.params.minVersion = target.minVersion;
      stmt.params.maxVersion = target.maxVersion;
      stmt.execute();
    });
  },

  /**
   * Synchronously removes an add-on from the database.
   *
   * @param   addon
   *          The DBAddonInternal being removed
   */
  removeAddonMetadata: function XPIDB_removeAddonMetadata(addon) {
    let stmt = this.getStatement("removeAddonMetadata");
    stmt.params.internal_id = addon._internal_id;
    stmt.execute();
  },

  /**
   * Synchronously marks a DBAddonInternal as visible marking all other
   * instances with the same ID as not visible.
   *
   * @param   addon
   *          The DBAddonInternal to make visible
   * @param   locations
   *          The name of the install location
   * @param   callback
   *          A callback to pass the DBAddonInternal to
   */
  makeAddonVisible: function XPIDB_makeAddonVisible(addon) {
    let stmt = this.getStatement("clearVisibleAddons");
    stmt.params.id = addon.id;
    stmt.execute();

    stmt = this.getStatement("makeAddonVisible");
    stmt.params.internal_id = addon._internal_id;
    stmt.execute();

    addon.visible = true;
  },

  /**
   * Synchronously sets properties for an add-on.
   *
   * @param   addon
   *          The DBAddonInternal being updated
   * @param   properties
   *          A dictionary of properties to set
   */
  setAddonProperties: function XPIDB_setAddonProperties(addon, properties) {
    function convertBoolean(value) {
      return value ? 1 : 0;
    }

    let stmt = this.getStatement("setAddonProperties");
    stmt.params.internal_id = addon._internal_id;

    if ("userDisabled" in properties) {
      stmt.params.userDisabled = convertBoolean(properties.userDisabled);
      addon.userDisabled = properties.userDisabled;
    }
    else {
      stmt.params.userDisabled = convertBoolean(addon.userDisabled);
    }

    if ("appDisabled" in properties) {
      stmt.params.appDisabled = convertBoolean(properties.appDisabled);
      addon.appDisabled = properties.appDisabled;
    }
    else {
      stmt.params.appDisabled = convertBoolean(addon.appDisabled);
    }

    if ("pendingUninstall" in properties) {
      stmt.params.pendingUninstall = convertBoolean(properties.pendingUninstall);
      addon.pendingUninstall = properties.pendingUninstall;
    }
    else {
      stmt.params.pendingUninstall = convertBoolean(addon.pendingUninstall);
    }

    stmt.execute();
  },

  /**
   * Synchronously pdates an add-on's active flag in the database.
   *
   * @param   addon
   *          The DBAddonInternal to update
   */
  updateAddonActive: function XPIDB_updateAddonActive(addon) {
    LOG("Updating add-on state");

    stmt = this.getStatement("deactivateThemes");
    stmt.params.internal_id = addon._internal_id;
    stmt.params.active = addon.active ? 1 : 0;
    stmt.execute();
  },

  /**
   * Synchronously calculates and updates all the active flags in the database.
   */
  updateActiveAddons: function XPIDB_updateActiveAddons() {
    LOG("Updating add-on states");
    let stmt = this.getStatement("setActiveAddons");
    stmt.execute();

    this.writeAddonsList();
  },

  /**
   * Writes out the XPI add-ons list for the platform to read.
   */
  writeAddonsList: function XPIDB_writeAddonsList() {
    LOG("Writing add-ons list");
    Services.appinfo.invalidateCachesOnRestart();
    let addonsList = FileUtils.getFile(KEY_PROFILEDIR, [FILE_XPI_ADDONS_LIST],
                                       true);

    let text = "[ExtensionDirs]\r\n";
    let count = 0;

    let stmt = this.getStatement("getActiveAddons");

    for (let row in resultRows(stmt))
      text += "Extension" + (count++) + "=" + row.descriptor + "\r\n";

    // The selected skin may come from an inactive theme (the default theme
    // when a lightweight theme is applied for example)
    text += "\r\n[ThemeDirs]\r\n";
    stmt = this.getStatement("getActiveTheme");
    stmt.params.internalName = XPIProvider.selectedSkin;
    count = 0;
    for (let row in resultRows(stmt))
      text += "Extension" + (count++) + "=" + row.descriptor + "\r\n";

    var fos = FileUtils.openSafeFileOutputStream(addonsList);
    fos.write(text, text.length);
    FileUtils.closeSafeFileOutputStream(fos);
  }
};

/**
 * Instantiates an AddonInstall and passes the new object to a callback when
 * it is complete.
 *
 * @param   callback
 *          The callback to pass the AddonInstall to
 * @param   installLocation
 *          The install location the add-on will be installed into
 * @param   url
 *          The nsIURL to get the add-on from. If this is an nsIFileURL then
 *          the add-on will not need to be downloaded
 * @param   hash
 *          An optional hash for the add-on
 * @param   name
 *          An optional name for the add-on
 * @param   type
 *          An optional type for the add-on
 * @param   iconURL
 *          An optional icon for the add-on
 * @param   version
 *          An optional version for the add-on
 * @param   infoURL
 *          An optional URL of release notes for the add-on
 * @param   existingAddon
 *          The add-on this install will update if known
 * @param   loadgroup
 *          The nsILoadGroup to associate any requests with
 * @throws  if the url is the url of a local file and the hash does not match
 *          or the add-on does not contain an valid install manifest
 */
function AddonInstall(callback, installLocation, url, hash, name, type, iconURL,
                      version, infoURL, existingAddon, loadgroup) {
  this.wrapper = new AddonInstallWrapper(this);
  this.installLocation = installLocation;
  this.sourceURL = url;
  this.loadgroup = loadgroup;
  this.listeners = [];
  this.existingAddon = existingAddon;

  if (url instanceof Ci.nsIFileURL) {
    this.file = url.file.QueryInterface(Ci.nsILocalFile);
    this.state = AddonManager.STATE_DOWNLOADED;
    this.progress = this.file.fileSize;
    this.maxProgress = this.file.fileSize;

    if (this.hash) {
      let crypto = Cc["@mozilla.org/security/hash;1"].
                   createInstance(Ci.nsICryptoHash);
      let fis = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
      fis.init(this.file, -1, -1, false);
      crypto.updateFromStream(fis, this.file.fileSize);
      let hash = crypto.finish(true);
      if (hash != this.hash)
        throw new Error("Hash mismatch");
    }

    this.loadManifest();
    this.name = this.addon.selectedLocale.name;
    this.type = this.addon.type;
    this.version = this.addon.version;
    this.iconURL = this.addon.iconURL;

    let self = this;
    XPIDatabase.getVisibleAddonForID(this.addon.id, function(addon) {
      self.existingAddon = addon;

      if (!self.addon.isCompatible) {
        // TODO Should we send some event here?
        this.state = AddonManager.STATE_CHECKING;
        new UpdateChecker(self.addon, {
          onUpdateFinished: function(addon, status) {
            XPIProvider.installs.push(self);
            AddonManagerPrivate.callInstallListeners("onNewInstall", self.listeners,
                                                     self.wrapper);

            callback(self);
          }
        }, AddonManager.UPDATE_WHEN_ADDON_INSTALLED);
      }
      else {
        XPIProvider.installs.push(self);
        AddonManagerPrivate.callInstallListeners("onNewInstall", self.listeners,
                                                 self.wrapper);

        callback(self);
      }
    });
  }
  else {
    this.state = AddonManager.STATE_AVAILABLE;
    this.name = name;
    this.type = type;
    this.version = version;
    this.iconURL = iconURL;
    this.progress = 0;
    this.maxProgress = -1;
    this.hash = hash;

    XPIProvider.installs.push(this);
    AddonManagerPrivate.callInstallListeners("onNewInstall", this.listeners,
                                             this.wrapper);

    callback(this);
  }
}

AddonInstall.prototype = {
  installLocation: null,
  wrapper: null,
  stream: null,
  crypto: null,
  hash: null,
  loadgroup: null,
  listeners: null,

  name: null,
  type: null,
  version: null,
  iconURL: null,
  infoURL: null,
  sourceURL: null,
  file: null,
  certificate: null,
  certName: null,

  existingAddon: null,
  addon: null,

  state: null,
  progress: null,
  maxProgress: null,

  /**
   * Starts installation of this add-on from whatever state it is currently at
   * if possible.
   *
   * @throws  if installation cannot proceed from the current state
   */
  install: function AI_install() {
    switch (this.state) {
    case AddonManager.STATE_AVAILABLE:
      this.startDownload();
      break;
    case AddonManager.STATE_DOWNLOADED:
      this.startInstall();
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
   * @throws  if installation cannot be cancelled from the current state
   */
  cancel: function AI_cancel() {
    switch (this.state) {
    case AddonManager.STATE_DOWNLOADING:
      break;
    case AddonManager.STATE_AVAILABLE:
    case AddonManager.STATE_DOWNLOADED:
      LOG("Cancelling download of " + this.sourceURL.spec);
      this.state = AddonManager.STATE_CANCELLED;
      AddonManagerPrivate.callInstallListeners("onDownloadCancelled",
                                               this.listeners, this.wrapper);
      if (this.file && !(this.sourceURL instanceof Ci.nsIFileURL))
        this.file.remove(true);
      break;
    case AddonManager.STATE_INSTALLED:
      LOG("Cancelling install of " + this.addon.id);
      let stagedAddon = this.installLocation.getStagingDir();
      let stagedJSON = stagedAddon.clone();
      stagedAddon.append(this.addon.id);
      stagedJSON.append(this.addon.id + ".json");
      if (stagedAddon.exists())
        stagedAddon.remove(true);
      if (stagedJSON.exists())
        stagedJSON.remove(true);
      this.state = AddonManager.STATE_CANCELLED;
      AddonManagerPrivate.callInstallListeners("onInstallCancelled",
                                               this.listeners, this.wrapper);
      break;
    default:
      throw new Error("Cannot cancel from this state");
    }
  },

  /**
   * Adds an InstallListener for this instance if the listener is not already
   * registered.
   *
   * @param   listener
   *          The InstallListener to add
   */
  addListener: function AI_addListener(listener) {
    if (!this.listeners.some(function(i) { return i == listener; }))
      this.listeners.push(listener);
  },

  /**
   * Removes an InstallListener for this instance if it is registered.
   *
   * @param   listener
   *          The InstallListener to remove
   */
  removeListener: function AI_removeListener(listener) {
    this.listeners = this.listeners.filter(function(i) {
      return i != listener;
    });
  },

  /**
   * Called after the add-on is a local file and the signature and install
   * manifest can be read.
   *
   * @throws  if the add-on does not contain a valid install manifest or the
   *          XPI is incorrectly signed
   */
  loadManifest: function AI_loadManifest() {
    let zipreader = Cc["@mozilla.org/libjar/zip-reader;1"].
                    createInstance(Ci.nsIZipReader);
    zipreader.open(this.file);

    try {
      let principal = zipreader.getCertificatePrincipal(null);
      if (principal && principal.hasCertificate) {
        LOG("Verifying XPI signiture");
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
          throw new Error("XPI is incorrectly signed");
        }
      }

      if (!zipreader.hasEntry(FILE_INSTALL_MANIFEST)) {
        zipreader.close();
        throw new Error("Missing install.rdf");
      }

      let zis = zipreader.getInputStream(FILE_INSTALL_MANIFEST);
      let bis = Cc["@mozilla.org/network/buffered-input-stream;1"].
                createInstance(Ci.nsIBufferedInputStream);
      bis.init(zis, 4096);

      try {
        uri = buildJarURI(this.file, FILE_INSTALL_MANIFEST);
        this.addon = loadManifestFromRDF(uri, bis);
        this.addon._sourceBundle = this.file;
      }
      finally {
        bis.close();
        zis.close();
      }
    }
    finally {
      zipreader.close();
    }
  },

  /**
   * Starts downloading the add-on's XPI file.
   */
  startDownload: function AI_startDownload() {
    Components.utils.import("resource://gre/modules/CertUtils.jsm");

    this.state = AddonManager.STATE_DOWNLOADING;
    if (!AddonManagerPrivate.callInstallListeners("onDownloadStarted",
                                                  this.listeners, this.wrapper)) {
      this.state = AddonManager.STATE_CANCELLED;
      AddonManagerPrivate.callInstallListeners("onDownloadCancelled",
                                               this.listeners, this.wrapper)
      return;
    }

    this.crypto = Cc["@mozilla.org/security/hash;1"].
                  createInstance(Ci.nsICryptoHash);
    if (this.hash) {
      [alg, this.hash] = this.hash.split(":", 2);

      try {
        this.crypto.initWithString(alg);
      }
      catch (e) {
        WARN("Unknown hash algorithm " + alg);
        this.state = AddonManager.STATE_DOWNLOAD_FAILED;
        AddonManagerPrivate.callInstallListeners("onDownloadFailed",
                                                 this.listeners, this.wrapper,
                                                 AddonManager.ERROR_INCORRECT_HASH);
        return;
      }
    }
    else {
      // We always need something to consume data from the inputstream passed
      // to onDataAvailable so just create a dummy cryptohasher to do that.
      this.crypto.initWithString("sha1");
    }

    try {
      this.file = FileUtils.getDir("TmpD", []);
      let random =  Math.random().toString(36).replace(/0./, '').substr(-3);
      this.file.append("tmp-" + random + ".xpi");
      this.file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
      this.stream = Cc["@mozilla.org/network/file-output-stream;1"].
                    createInstance(Ci.nsIFileOutputStream);
      this.stream.init(this.file, FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE |
                       FileUtils.MODE_TRUNCATE, FileUtils.PERMS_FILE, 0);

      let listener = Cc["@mozilla.org/network/stream-listener-tee;1"].
                     createInstance(Ci.nsIStreamListenerTee);
      listener.init(this, this.stream);
      let channel = NetUtil.newChannel(this.sourceURL);
      if (this.loadGroup)
        channel.loadGroup = this.loadgroup;

      // Verify that we don't end up on an insecure channel if we haven't got a
      // hash to verify with (see bug 537761 for discussion)
      if (!this.hash)
        channel.notificationCallbacks = new BadCertHandler();
      channel.asyncOpen(listener, null);
    }
    catch (e) {
      WARN("Failed to start download: " + e);
      this.state = AddonManager.STATE_DOWNLOAD_FAILED;
      AddonManagerPrivate.callInstallListeners("onDownloadFailed",
                                               this.listeners, this.wrapper,
                                               AddonManager.ERROR_NETWORK_FAILURE);
    }
  },

  /**
   * Update the crypto hasher with the new data and call the progress listeners.
   *
   * @see nsIStreamListener
   */
  onDataAvailable: function AI_onDataAvailable(request, context, inputstream,
                                               offset, count) {
    this.crypto.updateFromStream(inputstream, count);
    this.progress += count;
    if (!AddonManagerPrivate.callInstallListeners("onDownloadProgress",
                                                  this.listeners, this.wrapper)) {
      // TODO cancel the download and make it available again (bug 553024)
    }
  },

  /**
   * This is the first chance to get at real headers on the channel.
   *
   * @see nsIStreamListener
   */
  onStartRequest: function AI_onStartRequest(request, context) {
    // We must remove the request from the load group otherwise if the user
    // closes the page that triggered it the download will be cancelled
    if (this.loadGroup)
      this.loadGroup.removeRequest(request, null, Cr.NS_BINDING_RETARGETED);

    this.progress = 0;
    if (request instanceof Ci.nsIChannel) {
      try {
        this.maxProgress = request.contentLength;
      }
      catch (e) {
      }
      LOG("Download started for " + this.sourceURL.spec + " to file " +
          this.file.path);
    }
  },

  /**
   * The download is complete.
   *
   * @see nsIStreamListener
   */
  onStopRequest: function AI_onStopRequest(request, context, status) {
    this.stream.close();
    LOG("Download of " + this.sourceURL.spec + " completed.");

    if (Components.isSuccessCode(status)) {
      if (!(request instanceof Ci.nsIHttpChannel) || request.requestSucceeded) {
        if (!this.hash && (request instanceof Ci.nsIChannel)) {
          try {
            checkCert(request);
          }
          catch (e) {
            this.downloadFailed(AddonManager.ERROR_NETWORK_FAILURE, e);
            return;
          }
        }

        // return the two-digit hexadecimal code for a byte
        function toHexString(charCode)
          ("0" + charCode.toString(16)).slice(-2);

        // convert the binary hash data to a hex string.
        let binary = this.crypto.finish(false);
        let hash = [toHexString(binary.charCodeAt(i)) for (i in binary)].join("")
        this.crypto = null;
        if (this.hash && hash != this.hash) {
          this.downloadFailed(AddonManager.ERROR_INCORRECT_HASH,
                              "Downloaded file hash (" + hash +
                              ") did not match provded hash (" + this.hash + ")");
          return;
        }
        try {
          this.loadManifest();
          this.name = this.addon.selectedLocale.name;
          this.type = this.addon.type;
          this.version = this.addon.version;
          // TODO fix this to not allow chrome URLs etc (bug 552744).
          //this.iconURL = this.addon.iconURL;
          if (this.addon.isCompatible) {
            this.downloadCompleted();
          }
          else {
            // TODO Should we send some event here (bug 557716)?
            this.state = AddonManager.STATE_CHECKING;
            let self = this;
            new UpdateChecker(this.addon, {
              onUpdateFinished: function(addon, status) {
                self.downloadCompleted();
              }
            }, AddonManager.UPDATE_WHEN_ADDON_INSTALLED);
          }
        }
        catch (e) {
          this.downloadFailed(AddonManager.ERROR_CORRUPT_FILE, e);
        }
      }
      else {
        if (request instanceof Ci.nsIHttpChannel)
          this.downloadFailed(AddonManager.ERROR_NETWORK_FAILURE,
                              request.responseStatus + " " +
                              request.responseStatusText);
        else
          this.downloadFailed(AddonManager.ERROR_NETWORK_FAILURE, status);
      }
    }
    else {
      this.downloadFailed(AddonManager.ERROR_NETWORK_FAILURE, status);
    }
  },

  /**
   * Notify listeners that the download failed.
   *
   * @param   reason
   *          Something to log about the failure
   * @param   error
   *          The error code to pass to the listeners
   */
  downloadFailed: function(reason, error) {
    WARN("Download failed: " + error + "\n");
    this.state = AddonManager.STATE_DOWNLOAD_FAILED;
    AddonManagerPrivate.callInstallListeners("onDownloadFailed", this.listeners,
                                             this.wrapper, reason);
    this.file.remove(true);
  },

  /**
   * Notify listeners that the download completed.
   */
  downloadCompleted: function() {
    let self = this;
    XPIDatabase.getVisibleAddonForID(this.addon.id, function(addon) {
      self.existingAddon = addon;
      self.state = AddonManager.STATE_DOWNLOADED;
      if (AddonManagerPrivate.callInstallListeners("onDownloadEnded",
                                                   self.listeners,
                                                   self.wrapper))
        self.install();
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
      AddonManagerPrivate.callInstallListeners("onInstallCancelled",
                                               this.listeners, this.wrapper)
      return;
    }

    let isUpgrade = this.existingAddon &&
                    this.existingAddon._installLocation == this.installLocation;
    let requiresRestart = XPIProvider.installRequiresRestart(this.addon);
    // Restarts is required if the existing add-on is active and disabling it
    // requires a restart
    if (!requiresRestart && this.existingAddon) {
      requiresRestart = this.existingAddon.active &&
                        XPIProvider.disableRequiresRestart(this.existingAddon);
    }

    LOG("Starting install of " + this.sourceURL.spec);
    AddonManagerPrivate.callAddonListeners("onInstalling",
                                           createWrapper(this.addon),
                                           requiresRestart);
    let stagedAddon = this.installLocation.getStagingDir();

    try {
      // First stage the file regardless of whether restarting is necessary
      let stagedJSON = stagedAddon.clone();
      stagedAddon.append(this.addon.id);
      if (stagedAddon.exists())
        stagedAddon.remove(true);
      stagedAddon.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
      extractFiles(this.file, stagedAddon);

      if (requiresRestart) {
        // Cache the AddonInternal as it may have updated compatibiltiy info
        stagedJSON.append(this.addon.id + ".json");
        if (stagedJSON.exists())
          stagedJSON.remove(true);
        let stream = Cc["@mozilla.org/network/file-output-stream;1"].
                     createInstance(Ci.nsIFileOutputStream);
        let converter = Cc["@mozilla.org/intl/converter-output-stream;1"].
                        createInstance(Ci.nsIConverterOutputStream);
        let json = Cc["@mozilla.org/dom/json;1"].
                   createInstance(Ci.nsIJSON);

        try {
          stream.init(stagedJSON, FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE |
                                  FileUtils.MODE_TRUNCATE, FileUtils.PERMS_FILE,
                                 0);
          converter.init(stream, "UTF-8", 0, 0x0000);

          // A little hacky but we can't (and shouldn't) cache the source bundle.
          let bundle = this.addon._sourceBundle;
          delete this.addon._sourceBundle;
          converter.writeString(json.encode(this.addon));
          this.addon._sourceBundle = bundle;
        }
        finally {
          converter.close();
          stream.close();
        }

        LOG("Install of " + this.sourceURL.spec + " completed.");
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
        // TODO We can probably reduce the number of DB operations going on here
        // We probably also want to support rolling back failed upgrades etc.
        // See bug 553015.

        // Deactivate and remove the old add-on as necessary
        if (this.existingAddon) {
          if (this.existingAddon.active) {
            if (this.existingAddon.type == "bootstrapped")
              XPIProvider.deactivateAddon(this.existingAddon.id, false,
                                          isUpgrade);
            // If this is an upgrade its metadata will be removed below
            if (!isUpgrade) {
              this.existingAddon.active = false;
              XPIDatabase.updateAddonActive(this.existingAddon);
            }
          }
          if (isUpgrade)
            this.installLocation.uninstallAddon(this.existingAddon.id);
        }

        // Install the new add-on into its final directory
        let dir = this.installLocation.installAddon(this.addon.id, stagedAddon);

        // Update the metadata in the database
        this.addon._installLocation = this.installLocation;
        this.addon.updateDate = dir.lastModifiedTime;
        this.addon.visible = true;
        if (isUpgrade) {
          this.addon.installDate = this.existingAddon.installDate;
          XPIDatabase.updateAddonMetadata(this.existingAddon, this.addon,
                                          dir.persistentDescriptor);
        }
        else {
          this.addon.installDate = this.addon.updateDate;
          XPIDatabase.addAddonMetadata(this.addon, dir.persistentDescriptor);
        }

        // Retrieve the new DBAddonInternal for the add-on we just added
        let self = this;
        XPIDatabase.getAddonInLocation(this.addon.id, this.installLocation.name,
                                       function(a) {
          self.addon = a;
          if (self.addon.active && self.addon.type == "bootstrapped")
            XPIProvider.activateAddon(self.addon.id, dir, false, true);
          AddonManagerPrivate.callAddonListeners("onInstalled",
                                                 createWrapper(self.addon));

          LOG("Install of " + self.sourceURL.spec + " completed.");
          self.state = AddonManager.STATE_INSTALLED;
          AddonManagerPrivate.callInstallListeners("onInstallEnded",
                                                   self.listeners, self.wrapper,
                                                   createWrapper(self.addon));
        });
      }
    }
    catch (e) {
      WARN("Failed to install: " + e);
      if (stagedAddon.exists())
        stagedAddon.remove(true);
      this.state = AddonManager.STATE_INSTALL_FAILED;
      AddonManagerPrivate.callInstallListeners("onInstallFailed",
                                               this.listeners,
                                               this.wrapper, e);
    }
    finally {
      // If the file was downloaded then delete it
      if (!(this.sourceURL instanceof Ci.nsIFileURL))
        this.file.remove(true);
    }
  }
}

/**
 * Creates a new AddonInstall to install an add-on from a local file. Installs
 * always go into the profile install location.
 *
 * @param   callback
 *          The callback to pass the new AddonInstall to
 * @param   file
 *          The file to install
 */
AddonInstall.createInstall = function(callback, file) {
  let location = XPIProvider.installLocationsByName[KEY_APP_PROFILE];
  let url = Services.io.newFileURI(file);

  try {
    new AddonInstall(callback, location, url);
  }
  catch(e) {
    callback(null);
  }
};

/**
 * Creates a new AddonInstall to download and install a URL.
 *
 * @param   callback
 *          The callback to pass the new AddonInstall to
 * @param   uri
 *          The URI to download
 * @param   hash
 *          A hash for the add-on
 * @param   name
 *          A name for the add-on
 * @param   iconURL
 *          An icon URL for the add-on
 * @param   version
 *          A version for the add-on
 * @param   loadgroup
 *          An nsILoadGroup to associate the download with
 */
AddonInstall.createDownload = function(callback, uri, hash, name, iconURL,
                                       version, loadgroup) {
  let location = XPIProvider.installLocationsByName[KEY_APP_PROFILE];
  let url = NetUtil.newURI(uri);
  new AddonInstall(callback, location, url, hash, name, null,
                   iconURL, version, null, null, loadgroup);
};

/**
 * Creates a new AddonInstall for an update.
 *
 * @param   callback
 *          The callback to pass the new AddonInstall to
 * @param   addon
 *          The add-on being updated
 * @param   update
 *          The metadata about the new version from the update manifest
 */
AddonInstall.createUpdate = function(callback, addon, update) {
  let url = NetUtil.newURI(update.updateURL);
  let infoURL = null;
  if (update.updateInfoURL)
    infoURL = escapeAddonURI(addon, update.updateInfoURL);
  new AddonInstall(callback, addon._installLocation, url, update.updateHash,
                   addon.selectedLocale.name, addon.type,
                   addon.iconURL, update.version, infoURL, addon);
};

/**
 * Creates a wrapper for an AddonInstall that only exposes the public API
 *
 * @param   install
 *          The AddonInstall to create a wrapper for
 */
function AddonInstallWrapper(install) {
  ["name", "type", "version", "iconURL", "infoURL", "file", "state", "progress",
   "maxProgress", "certificate", "certName"].forEach(function(prop) {
    this.__defineGetter__(prop, function() install[prop]);
  }, this);

  this.__defineGetter__("existingAddon", function() {
    return createWrapper(install.existingAddon);
  });
  this.__defineGetter__("addon", function() createWrapper(install.addon));
  this.__defineGetter__("sourceURL", function() install.sourceURL.spec);

  this.install = function() {
    install.install();
  }

  this.cancel = function() {
    install.cancel();
  }

  this.addListener = function(listener) {
    install.addListener(listener);
  }

  this.removeListener = function(listener) {
    install.removeListener(listener);
  }
}

AddonInstallWrapper.prototype = {};

/**
 * Creates a new update checker.
 *
 * @param   addon
 *          The add-on to check for updates
 * @param   listener
 *          An UpdateListener to notify of updates
 * @param   reason
 *          The reason for the update check
 * @param   appVersion
 *          An optional application version to check for updates for
 * @param   platformVersion
 *          An optional platform version to check for updates for
 * @throws  if the listener or reason arguments are not valid
 */
function UpdateChecker(addon, listener, reason, appVersion, platformVersion) {
  if (!listener || !reason)
    throw Cr.NS_ERROR_INVALID_ARG;

  Components.utils.import("resource://gre/modules/AddonUpdateChecker.jsm");

  this.addon = addon;
  this.listener = listener;
  this.appVersion = appVersion;
  this.platformVersion = platformVersion;

  let updateURL = addon.updateURL ? addon.updateURL :
                                    Services.prefs.getCharPref(PREF_EM_UPDATE_URL);

  const UPDATE_TYPE_COMPATIBILITY = 32;
  const UPDATE_TYPE_NEWVERSION = 64;

  if ("onCompatibilityUpdated" in this.listener)
    reason |= UPDATE_TYPE_COMPATIBILITY;
  if ("onUpdateAvailable" in this.listener)
    reason |= UPDATE_TYPE_NEWVERSION;

  let url = escapeAddonURI(addon, updateURL, reason, appVersion);
  AddonUpdateChecker.checkForUpdates(addon.id, addon.type, addon.updateKey, url, this);
}

UpdateChecker.prototype = {
  addon: null,
  listener: null,
  appVersion: null,
  platformVersion: null,

  /**
   * Called when AddonUpdateChecker completes the update check
   *
   * @param   updates
   *          The list of update details for the add-on
   */
  onUpdateCheckComplete: function UC_onUpdateCheckComplete(updates) {
    let AUC = AddonUpdateChecker;
    let compatUpdate = AUC.getCompatibilityUpdate(updates, this.addon.version,
                                                  this.appVersion,
                                                  this.platformVersion);
    if (compatUpdate && this.addon.applyCompatibilityUpdate(compatUpdate)) {
      if ("onCompatibilityUpdated" in this.listener)
        this.listener.onCompatibilityUpdated(createWrapper(this.addon));
    }
    if ("onUpdateAvailable" in this.listener) {
      let update = AUC.getNewestCompatibleUpdate(updates, this.appVersion,
                                                 this.platformVersion);
      if (update && Services.vc.compare(this.addon.version, update.version) < 0) {
        let self = this;
        AddonInstall.createUpdate(function(install) {
          self.listener.onUpdateAvailable(createWrapper(self.addon),
                                          install.wrapper);
        }, this.addon, update);
      }
    }
    if ("onUpdateFinished" in this.listener)
      this.listener.onUpdateFinished(createWrapper(this.addon), 0);
  },

  /**
   * Called when AddonUpdateChecker fails the update check
   *
   * @param   error
   *          An error status
   */
  onUpdateCheckError: function UC_onUpdateCheckError(error) {
    if ("onUpdateFinished" in this.listener)
      this.listener.onUpdateFinished(createWrapper(this.addon), error);
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
    let app = this.matchingTargetApplication;
    if (!app)
      return false;

    let version;
    if (app.id == Services.appinfo.ID)
      version = Services.appinfo.version;
    else if (app.id == TOOLKIT_ID)
      version = Services.appinfo.platformVersion

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

  applyCompatibilityUpdate: function(update) {
    let changed = false;
    this.targetApplications.forEach(function(ta) {
      update.targetApplications.forEach(function(updateTarget) {
        if (ta.id == updateTarget.id && (ta.minVersion != updateTarget.minVersion ||
                                         ta.maxVersion != updateTarget.maxVersion)) {
          ta.minVersion = updateTarget.minVersion;
          ta.maxVersion = updateTarget.maxVersion;
          changed = true;
        }
      });
    });
    this.appDisabled = !isUsableAddon(this);
    return changed;
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
  applyCompatibilityUpdate: function(update) {
    let changes = [];
    this.targetApplications.forEach(function(ta) {
      update.targetApplications.forEach(function(updateTarget) {
        if (ta.id == updateTarget.id && (ta.minVersion != updateTarget.minVersion ||
                                         ta.maxVersion != updateTarget.maxVersion)) {
          ta.minVersion = updateTarget.minVersion;
          ta.maxVersion = updateTarget.maxVersion;
          changes.push(ta);
        }
      });
    });
    XPIDatabase.updateTargetApplications(this, changes);
    XPIProvider.updateAddonDisabledState(this);
    return changes.length > 0;
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
function createWrapper(addon) {
  if (!addon)
    return null;
  if (!addon.wrapper)
    addon.wrapper = new AddonWrapper(addon);
  return addon.wrapper;
}

/**
 * The AddonWrapper wraps an Addon to provide the data visible to consumers of
 * the public API.
 */
function AddonWrapper(addon) {
  ["id", "version", "type", "optionsURL", "aboutURL", "isCompatible",
   "providesUpdatesSecurely", "blocklistState", "appDisabled",
   "userDisabled"].forEach(function(prop) {
     this.__defineGetter__(prop, function() addon[prop]);
  }, this);

  ["installDate", "updateDate"].forEach(function(prop) {
    this.__defineGetter__(prop, function() new Date(addon[prop]));
  }, this);

  this.__defineGetter__("iconURL", function() {
      return addon.active ? addon.iconURL : null;
  });

  PROP_LOCALE_SINGLE.forEach(function(prop) {
    this.__defineGetter__(prop, function() addon.selectedLocale[prop]);
  }, this);

  PROP_LOCALE_MULTI.forEach(function(prop) {
    this.__defineGetter__(prop, function() addon.selectedLocale[prop]);
  }, this);

  this.__defineGetter__("screenshots", function() {
    return [];
  });

  this.__defineGetter__("updateAutomatically", function() {
    return addon.updateAutomatically;
  });
  this.__defineSetter__("updateAutomatically", function(val) {
    // TODO store this in the DB (bug 557849)
    addon.updateAutomatically = val;
  });

  this.__defineGetter__("pendingUpgrade", function() {
    return createWrapper(addon.pendingUpgrade);
  });

  this.__defineGetter__("pendingOperations", function() {
    let pending = 0;
    if (!(addon instanceof DBAddonInternal))
      pending |= AddonManager.PENDING_INSTALL;
    else if (addon.pendingUninstall)
      pending |= AddonManager.PENDING_UNINSTALL;

    if (addon.active && (addon.userDisabled || addon.appDisabled))
      pending |= AddonManager.PENDING_DISABLE;
    else if (!addon.active && (!addon.userDisabled && !addon.appDisabled))
      pending |= AddonManager.PENDING_ENABLE;

    if (addon.pendingUpgrade)
      pending |= AddonManager.PENDING_UPGRADE;

    return pending;
  });

  this.__defineGetter__("permissions", function() {
    let permissions = 0;
    if (!addon.appDisabled) {
      if (addon.userDisabled)
        permissions |= AddonManager.PERM_CAN_ENABLE;
      else if (addon.type != "theme")
        permissions |= AddonManager.PERM_CAN_DISABLE;
    }
    if (addon._installLocation) {
      if (!addon._installLocation.locked) {
        permissions |= AddonManager.PERM_CAN_UPGRADE;
        if (!addon.pendingUninstall)
          permissions |= AddonManager.PERM_CAN_UNINSTALL;
      }
    }
    return permissions;
  });

  this.__defineGetter__("isActive", function() addon.active);
  this.__defineSetter__("userDisabled", function(val) {
    if (addon.type == "theme" && val)
      throw new Error("Cannot disable the active theme");

    if (addon instanceof DBAddonInternal)
      XPIProvider.updateAddonDisabledState(addon, val);
    else
      addon.userDisabled = val;
  });

  this.uninstall = function() {
    if (!(addon instanceof DBAddonInternal))
      throw new Error("Cannot uninstall an add-on that isn't installed");
    if (addon.pendingUninstall)
      throw new Error("Add-on is already marked to be uninstalled");
    XPIProvider.uninstallAddon(addon);
  };

  this.cancelUninstall = function() {
    if (!(addon instanceof DBAddonInternal))
      throw new Error("Cannot cancel uninstall for an add-on that isn't installed");
    if (!addon.pendingUninstall)
      throw new Error("Add-on is not marked to be uninstalled");
    XPIProvider.cancelUninstallAddon(addon);
  };

  this.findUpdates = function(listener, reason, appVersion, platformVersion) {
    new UpdateChecker(addon, listener, reason, appVersion, platformVersion);
  };

  this.hasResource = function(path) {
    let bundle = null;
    if (addon instanceof DBAddonInternal) {
      bundle = addon._sourceBundle = addon._installLocation
                                          .getLocationForID(addon.id);
    }
    else {
      bundle = addon._sourceBundle.clone();
    }

    if (bundle.isDirectory()) {
      bundle.append(path);
      return bundle.exists();
    }

    let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                    createInstance(Ci.nsIZipReader);
    zipReader.open(bundle);
    let result = zipReader.hasEntry(path);
    zipReader.close();
    return result;
  },

  this.getResourceURL = function(path) {
    let bundle = null;
    if (addon instanceof DBAddonInternal) {
      bundle = addon._sourceBundle = addon._installLocation
                                          .getLocationForID(addon.id);
    }
    else {
      bundle = addon._sourceBundle.clone();
    }

    if (bundle.isDirectory()) {
      bundle.append(path);
      return Services.io.newFileURI(bundle).spec;
    }

    return buildJarURI(bundle, path).spec;
  }
}

AddonWrapper.prototype = {
  get screenshots() {
    return [];
  }
};

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
 * @param   name
 *          The string identifier for the install location
 * @param   directory
 *          The nsIFile directory for the install location
 * @param   locked
 *          true if add-ons cannot be installed, uninstalled or upgraded in the
 *          install location
 */
function DirectoryInstallLocation(name, directory, locked) {
  this._name = name;
  this.locked = locked;
  this._directory = directory;
  this._IDToDirMap = {};
  this._DirToIDMap = {};

  if (!directory.exists())
    return;
  if (!directory.isDirectory())
    throw new Error("Location must be a directory.");

  this._readAddons();
}

DirectoryInstallLocation.prototype = {
  _name       : "",
  _directory   : null,
  _IDToDirMap : null,  // mapping from add-on ID to nsIFile directory
  _DirToIDMap : null,  // mapping from directory path to add-on ID

  /**
   * Reads a directory linked to in a file.
   *
   * @param   file
   *          The file containing the directory path
   * @return  a nsILocalFile object representing the linked directory.
   */
  _readDirectoryFromFile: function DirInstallLocation__readDirectoryFromFile(file) {
    let fis = Cc["@mozilla.org/network/file-input-stream;1"].
              createInstance(Ci.nsIFileInputStream);
    fis.init(file, -1, -1, false);
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
        linkedDirectory.setRelativeDescriptor(file.parent, line.value);
      }

      return linkedDirectory;
    }
    return null;
  },

  /**
   * Finds all the add-ons installed in this location.
   */
  _readAddons: function DirInstallLocation__readAddons() {
    try {
    let entries = this._directory.directoryEntries
                                 .QueryInterface(Ci.nsIDirectoryEnumerator);
    let entry;
    while (entry = entries.nextFile) {
      // Should never happen really
      if (!entry instanceof Ci.nsILocalFile)
        continue;

      let id = entry.leafName;

      if (id == DIR_STAGE)
        continue;

      if (!gIDTest.test(id)) {
        LOG("Ignoring file entry whose name is not a valid add-on ID: " +
             entry.path);
        continue;
      }

      // XXX Bug 530188 requires us to clone this entry
      entry = this._directory.clone().QueryInterface(Ci.nsILocalFile);
      entry.append(id);
      if (entry.isFile()) {
        newEntry = this._readDirectoryFromFile(entry);
        if (!newEntry || !newEntry.exists() || !newEntry.isDirectory()) {
          WARN("File pointer " + entry.path + " points to an invalid " +
               "directory " + newEntry.path);
          continue;
        }
        entry = newEntry;
      }
      else if (!entry.isDirectory()) {
        LOG("Ignoring entry which isn't a directory: " + entry.path);
        continue;
      }

      this._IDToDirMap[id] = entry;
      this._DirToIDMap[entry.path] = id;
    }
    entries.close();
    }
    catch (e) {
      ERROR(e);
    }
  },

  /**
   * Gets the name of this install location.
   */
  get name() {
    return this._name;
  },

  /**
   * Gets an array of nsIFiles for add-ons installed in this location.
   */
  get addonLocations() {
    let locations = [];
    for (let id in this._IDToDirMap) {
      locations.push(this._IDToDirMap[id].clone()
                         .QueryInterface(Ci.nsILocalFile));
    }
    return locations;
  },

  /**
   * Gets the staging directory to put add-ons that are pending install and
   * uninstall into.
   *
   * @return  an nsIFile
   */
  getStagingDir: function DirInstallLocation_getStagingDir() {
    let dir = this._directory.clone();
    dir.append(DIR_STAGE);
    return dir;
  },

  /**
   * Installs an add-on from a directory into the install location.
   *
   * @param   id
   *          The ID of the add-on to install
   * @param   source
   *          The directory to install from
   * @return  an nsIFile indicating where the add-on was installed to
   */
  installAddon: function DirInstallLocation_installAddon(id, source) {
    let dir = this._directory.clone().QueryInterface(Ci.nsILocalFile);
    dir.append(id);
    if (dir.exists())
      dir.remove(true);

    source = source.clone();
    source.moveTo(this._directory, id);
    this._DirToIDMap[dir.path] = id;
    this._IDToDirMap[id] = dir;

    return dir;
  },

  /**
   * Uninstalls an add-on from this location.
   *
   * @param   id
   *          The ID of the add-on to uninstall
   * @throws  if the ID does not match any of the add-ons installed
   */
  uninstallAddon: function DirInstallLocation_uninstallAddon(id) {
    let dir = this._directory.clone();
    dir.append(id);

    delete this._DirToIDMap[dir.path];
    delete this._IDToDirMap[id];

    if (!dir.exists())
      throw new Error("Attempt to uninstall unknown add-on " + id);

    dir.remove(true);
  },

  /**
   * Gets the ID of the add-on installed in the given directory.
   *
   * @param   dir
   *          The nsIFile directory to look in
   * @return  the ID
   * @throws  if the directory does not represent an installed add-on
   */
  getIDForLocation: function DirInstallLocation_getIDForLocation(dir) {
    if (dir.path in this._DirToIDMap)
      return this._DirToIDMap[dir.path];
    throw new Error("Unknown add-on location " + dir.path);
  },

  /**
   * Gets the directory that the add-on with the given ID is installed in.
   *
   * @param   id
   *          The ID of the add-on
   * @return  the directory
   * @throws  if the ID does not match any of the add-ons installed
   */
  getLocationForID: function DirInstallLocation_getLocationForID(id) {
    if (id in this._IDToDirMap)
      return this._IDToDirMap[id].clone().QueryInterface(Ci.nsILocalFile);
    throw new Error("Unknown add-on ID " + id);
  }
};

#ifdef XP_WIN
/**
 * An object that identifies a registry install location for add-ons. The location
 * consists of a registry key which contains string values mapping ID to the
 * directory where an add-on is installed
 *
 * @param   name
 *          The string identifier of this Install Location.
 * @param   rootKey
 *          The root key (one of the ROOT_KEY_ values from nsIWindowsRegKey).
 * @constructor
 */
function WinRegInstallLocation(name, rootKey) {
  this.locked = true;
  this._name = name;
  this._rootKey = rootKey;
  this._IDToDirMap = {};
  this._DirToIDMap = {};

  let path = this._appKeyPath + "\\Extensions";
  let key = Cc["@mozilla.org/windows-registry-key;1"].
            createInstance(Ci.nsIWindowsRegKey);

  // Reading the registry may throw an exception, and that's ok.  In error
  // cases, we just leave ourselves in the empty state.
  try {
    key.open(this._rootKey, path, Ci.nsIWindowsRegKey.ACCESS_READ);
    this._readAddons(key);
  }
  catch (e) { }
  if (key)
    key.close();
}

WinRegInstallLocation.prototype = {
  _name       : "",
  _rootKey    : null,
  _IDToDirMap : null,  // mapping from ID to directory object
  _DirToIDMap : null,  // mapping from directory path to ID

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
   * Read the registry and build a mapping between ID and directory for each
   * installed add-on.
   *
   * @param   key
   *          The key that contains the ID to path mapping
   */
  _readAddons: function RegInstallLocation__readAddons(key) {
    let count = key.valueCount;
    for (let i = 0; i < count; ++i) {
      let id = key.getValueName(i);

      let dir = Cc["@mozilla.org/file/local;1"].
                createInstance(Ci.nsILocalFile);
      dir.initWithPath(key.readStringValue(id));

      if (dir.exists() && dir.isDirectory()) {
        this._IDToDirMap[id] = dir;
        this._DirToIDMap[dir.path] = id;
      }
    }
  },

  /**
   * Gets the name of this install location.
   */
  get name() {
    return this._name;
  },

  /**
   * Gets an array of nsIFiles for add-ons installed in this location.
   */
  get addonLocations() {
    let locations = [];
    for (let id in this._IDToDirMap) {
      locations.push(this._IDToDirMap[id].clone()
                         .QueryInterface(Ci.nsILocalFile));
    }
    return locations;
  },

  /**
   * Gets the ID of the add-on installed in the given directory.
   *
   * @param   file
   *          The directory to look in
   * @return  the ID
   * @throws  if the directory does not represent an installed add-on
   */
  getIDForLocation: function RegInstallLocation_getIDForLocation(file) {
    if (file.path in this._DirToIDMap)
      return this._DirToIDMap[file.path];
    throw new Error("Unknown add-on location");
  },

  /**
   * Gets the directory that the add-on with the given ID is installed in.
   *
   * @param   id
   *          The ID of the add-on
   * @returns the directory
   */
  getLocationForID: function RegInstallLocation_getLocationForID(id) {
    if (id in this._IDToDirMap)
      return this._IDToDirMap[id].clone().QueryInterface(Ci.nsILocalFile);
    throw new Error("Unknown add-on ID");
  }
};
#endif

AddonManagerPrivate.registerProvider(XPIProvider);
