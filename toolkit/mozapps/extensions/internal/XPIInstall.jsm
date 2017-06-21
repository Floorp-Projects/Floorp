 /* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

this.EXPORTED_SYMBOLS = [
  "DownloadAddonInstall",
  "LocalAddonInstall",
  "StagedAddonInstall",
  "UpdateChecker",
  "loadManifestFromFile",
  "verifyBundleSignedState",
];

/* globals DownloadAddonInstall, LocalAddonInstall */

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonRepository",
                                  "resource://gre/modules/addons/AddonRepository.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonSettings",
                                  "resource://gre/modules/addons/AddonSettings.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyGetter(this, "CertUtils",
                            () => Cu.import("resource://gre/modules/CertUtils.jsm", {}));
XPCOMUtils.defineLazyModuleGetter(this, "ChromeManifestParser",
                                  "resource://gre/modules/ChromeManifestParser.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionData",
                                  "resource://gre/modules/Extension.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Locale",
                                  "resource://gre/modules/Locale.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyGetter(this, "IconDetails", () => {
  return Cu.import("resource://gre/modules/ExtensionParent.jsm", {}).ExtensionParent.IconDetails;
});
XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeManager",
                                  "resource://gre/modules/LightweightThemeManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ZipUtils",
                                  "resource://gre/modules/ZipUtils.jsm");

const {nsIBlocklistService} = Ci;

const nsIFile = Components.Constructor("@mozilla.org/file/local;1", "nsIFile",
                                       "initWithPath");

XPCOMUtils.defineLazyServiceGetter(this, "gCertDB",
                                   "@mozilla.org/security/x509certdb;1",
                                   "nsIX509CertDB");
XPCOMUtils.defineLazyServiceGetter(this, "gRDF", "@mozilla.org/rdf/rdf-service;1",
                                   Ci.nsIRDFService);


XPCOMUtils.defineLazyModuleGetter(this, "XPIInternal",
                                  "resource://gre/modules/addons/XPIProvider.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "XPIProvider",
                                  "resource://gre/modules/addons/XPIProvider.jsm");

/* globals AddonInternal, BOOTSTRAP_REASONS, KEY_APP_SYSTEM_ADDONS, KEY_APP_SYSTEM_DEFAULTS, KEY_APP_TEMPORARY, TEMPORARY_ADDON_SUFFIX, TOOLKIT_ID, XPIDatabase, XPIStates, applyBlocklistChanges, getExternalType, isTheme, isUsableAddon, isWebExtension, recordAddonTelemetry */
const XPI_INTERNAL_SYMBOLS = [
  "AddonInternal",
  "BOOTSTRAP_REASONS",
  "KEY_APP_SYSTEM_ADDONS",
  "KEY_APP_SYSTEM_DEFAULTS",
  "KEY_APP_TEMPORARY",
  "TEMPORARY_ADDON_SUFFIX",
  "TOOLKIT_ID",
  "XPIDatabase",
  "XPIStates",
  "applyBlocklistChanges",
  "getExternalType",
  "isTheme",
  "isUsableAddon",
  "isWebExtension",
  "recordAddonTelemetry",
];

for (let name of XPI_INTERNAL_SYMBOLS) {
  XPCOMUtils.defineLazyGetter(this, name, () => XPIInternal[name]);
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

const PREF_EM_UPDATE_BACKGROUND_URL   = "extensions.update.background.url";
const PREF_EM_UPDATE_URL              = "extensions.update.url";
const PREF_XPI_SIGNATURES_DEV_ROOT    = "xpinstall.signatures.dev-root";
const PREF_XPI_UNPACK                 = "extensions.alwaysUnpack";
const PREF_INSTALL_REQUIREBUILTINCERTS = "extensions.install.requireBuiltInCerts";
const PREF_EM_HOTFIX_ID               = "extensions.hotfix.id";
const PREF_EM_CERT_CHECKATTRIBUTES    = "extensions.hotfix.cert.checkAttributes";
const PREF_EM_HOTFIX_CERTS            = "extensions.hotfix.certs.";

const FILE_RDF_MANIFEST               = "install.rdf";
const FILE_WEB_MANIFEST               = "manifest.json";

const KEY_TEMPDIR                     = "TmpD";

const RDFURI_INSTALL_MANIFEST_ROOT    = "urn:mozilla:install-manifest";
const PREFIX_NS_EM                    = "http://www.mozilla.org/2004/em-rdf#";

// Properties that exist in the install manifest
const PROP_METADATA      = ["id", "version", "type", "internalName", "updateURL",
                            "updateKey", "optionsURL", "optionsType", "aboutURL",
                            "iconURL", "icon64URL"];
const PROP_LOCALE_SINGLE = ["name", "description", "creator", "homepageURL"];
const PROP_LOCALE_MULTI  = ["developers", "translators", "contributors"];
const PROP_TARGETAPP     = ["id", "minVersion", "maxVersion"];

// Map new string type identifiers to old style nsIUpdateItem types
// Type 32 was previously used for multipackage xpi files so it should
// not be re-used since old files with that type may be floating around.
const TYPES = {
  extension: 2,
  theme: 4,
  locale: 8,
  dictionary: 64,
  experiment: 128,
  apiextension: 256,
};

const COMPATIBLE_BY_DEFAULT_TYPES = {
  extension: true,
  dictionary: true,
};

const RESTARTLESS_TYPES = new Set([
  "apiextension",
  "dictionary",
  "experiment",
  "locale",
  "webextension",
  "webextension-theme",
]);

const SIGNED_TYPES = new Set([
  "apiextension",
  "extension",
  "experiment",
  "webextension",
  "webextension-theme",
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

  return AddonSettings.REQUIRE_SIGNING;
}

const MSG_JAR_FLUSH = "AddonJarFlush";
const MSG_MESSAGE_MANAGER_CACHES_FLUSH = "AddonMessageManagerCachesFlush";


/**
 * Valid IDs fit this pattern.
 */
var gIDTest = /^(\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}|[a-z0-9-\._]*\@[a-z0-9-\._]+)$/i;

Cu.import("resource://gre/modules/Log.jsm");
const LOGGER_ID = "addons.xpi";

// Create a new logger for use by all objects in this Addons XPI Provider module
// (Requires AddonManager.jsm)
var logger = Log.repository.getLogger(LOGGER_ID);


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

function EM_R(aProperty) {
  return gRDF.GetResource(PREFIX_NS_EM + aProperty);
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

function getManifestEntryForZipReader(aZipReader) {
  if (aZipReader.hasEntry(FILE_RDF_MANIFEST))
    return FILE_RDF_MANIFEST;
  if (aZipReader.hasEntry(FILE_WEB_MANIFEST))
    return FILE_WEB_MANIFEST;
  return null;
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
async function loadManifestFromWebManifest(aUri) {
  // We're passed the URI for the manifest file. Get the URI for its
  // parent directory.
  let uri = Services.io.newURI("./", null, aUri);

  let extension = new ExtensionData(uri);

  let manifest = await extension.loadManifest();

  // Read the list of available locales, and pre-load messages for
  // all locales.
  let locales = (extension.errors.length == 0) ?
                await extension.initAllLocales() : null;

  if (extension.errors.length > 0) {
    throw new Error("Extension is invalid");
  }

  let theme = Boolean(manifest.theme);

  let bss = (manifest.browser_specific_settings && manifest.browser_specific_settings.gecko)
      || (manifest.applications && manifest.applications.gecko) || {};
  if (manifest.browser_specific_settings && manifest.applications) {
    logger.warn("Ignoring applications property in manifest");
  }

  // A * is illegal in strict_min_version
  if (bss.strict_min_version && bss.strict_min_version.split(".").some(part => part == "*")) {
    throw new Error("The use of '*' in strict_min_version is invalid");
  }

  let addon = new AddonInternal();
  addon.id = bss.id;
  addon.version = manifest.version;
  addon.type = "webextension" + (theme ? "-theme" : "");
  addon.unpack = false;
  addon.strictCompatibility = true;
  addon.bootstrap = true;
  addon.hasBinaryComponents = false;
  addon.multiprocessCompatible = true;
  addon.internalName = null;
  addon.updateURL = bss.update_url;
  addon.updateKey = null;
  addon.optionsBrowserStyle = true;
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

    if (manifest.options_ui.browser_style === null)
      logger.warn("Please specify whether you want browser_style " +
          "or not in your options_ui options.");
    else
      addon.optionsBrowserStyle = manifest.options_ui.browser_style;
  }

  // WebExtensions don't use iconURLs
  addon.iconURL = null;
  addon.icon64URL = null;
  addon.icons = manifest.icons || {};
  addon.userPermissions = extension.userPermissions;

  addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DEFAULT;

  function getLocale(aLocale) {
    // Use the raw manifest, here, since we need values with their
    // localization placeholders still in place.
    let rawManifest = extension.rawManifest;

    // As a convenience, allow author to be set if its a string bug 1313567.
    let creator = typeof(rawManifest.author) === "string" ? rawManifest.author : null;
    let homepageURL = rawManifest.homepage_url;

    // Allow developer to override creator and homepage_url.
    if (rawManifest.developer) {
      if (rawManifest.developer.name) {
        creator = rawManifest.developer.name;
      }
      if (rawManifest.developer.url) {
        homepageURL = rawManifest.developer.url;
      }
    }

    let result = {
      name: extension.localize(rawManifest.name, aLocale),
      description: extension.localize(rawManifest.description, aLocale),
      creator: extension.localize(creator, aLocale),
      homepageURL: extension.localize(homepageURL, aLocale),

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
    minVersion: bss.strict_min_version,
    maxVersion: bss.strict_max_version,
  }];

  addon.targetPlatforms = [];
  // Themes are disabled by default, except when they're installed from a web page.
  addon.userDisabled = theme;
  addon.softDisabled = addon.blocklistState == nsIBlocklistService.STATE_SOFTBLOCKED;

  return addon;
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
async function loadManifestFromRDF(aUri, aStream) {
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
  } catch (e) {
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
  } else {
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

  if (!addon.id)
    throw new Error("No ID in install manifest");
  if (!gIDTest.test(addon.id))
    throw new Error("Illegal add-on ID " + addon.id);
  if (!addon.version)
    throw new Error("No version in install manifest");

  addon.strictCompatibility = !(addon.type in COMPATIBLE_BY_DEFAULT_TYPES) ||
                              getRDFProperty(ds, root, "strictCompatibility") == "true";

  // Only read these properties for extensions.
  if (addon.type == "extension") {
    addon.bootstrap = getRDFProperty(ds, root, "bootstrap") == "true";

    let mpcValue = getRDFProperty(ds, root, "multiprocessCompatible");
    addon.multiprocessCompatible = mpcValue == "true";
    addon.mpcOptedOut = mpcValue == "false";

    addon.hasEmbeddedWebExtension = getRDFProperty(ds, root, "hasEmbeddedWebExtension") == "true";

    if (addon.optionsType &&
        addon.optionsType != AddonManager.OPTIONS_TYPE_DIALOG &&
        addon.optionsType != AddonManager.OPTIONS_TYPE_INLINE &&
        addon.optionsType != AddonManager.OPTIONS_TYPE_TAB &&
        addon.optionsType != AddonManager.OPTIONS_TYPE_INLINE_INFO) {
      throw new Error("Install manifest specifies unknown type: " + addon.optionsType);
    }

    if (addon.hasEmbeddedWebExtension) {
      let uri = Services.io.newURI("webextension/manifest.json", null, aUri);
      let embeddedAddon = await loadManifestFromWebManifest(uri);
      if (embeddedAddon.optionsURL) {
        if (addon.optionsType || addon.optionsURL)
          logger.warn(`Addon ${addon.id} specifies optionsType or optionsURL ` +
                      `in both install.rdf and manifest.json`);

        addon.optionsURL = embeddedAddon.optionsURL;
        addon.optionsType = embeddedAddon.optionsType;
      }
    }
  } else {
    // Some add-on types are always restartless.
    if (RESTARTLESS_TYPES.has(addon.type)) {
      addon.bootstrap = true;
    }

    // Only extensions are allowed to provide an optionsURL, optionsType,
    // optionsBrowserStyle, or aboutURL. For all other types they are silently ignored
    addon.aboutURL = null;
    addon.optionsBrowserStyle = null;
    addon.optionsType = null;
    addon.optionsURL = null;

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
    } else {
      platform.os = targetPlatform;
    }

    addon.targetPlatforms.push(platform);
  }

  // A theme's userDisabled value is true if the theme is not the selected skin
  // or if there is an active lightweight theme. We ignore whether softblocking
  // is in effect since it would change the active theme.
  if (isTheme(addon.type)) {
    addon.userDisabled = !!LightweightThemeManager.currentTheme ||
                         addon.internalName != XPIProvider.selectedSkin;
  } else if (addon.type == "experiment") {
    // Experiments are disabled by default. It is up to the Experiments Manager
    // to enable them (it drives installation).
    addon.userDisabled = true;
  } else {
    addon.userDisabled = false;
  }

  addon.softDisabled = addon.blocklistState == nsIBlocklistService.STATE_SOFTBLOCKED;
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
  addon.userPermissions = null;

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

// Generate a unique ID based on the path to this temporary add-on location.
function generateTemporaryInstallID(aFile) {
  const hasher = Cc["@mozilla.org/security/hash;1"]
        .createInstance(Ci.nsICryptoHash);
  hasher.init(hasher.SHA1);
  const data = new TextEncoder().encode(aFile.path);
  // Make it so this ID cannot be guessed.
  const sess = TEMP_INSTALL_ID_GEN_SESSION;
  hasher.update(sess, sess.length);
  hasher.update(data, data.length);
  let id = `${getHashStringForCrypto(hasher)}${TEMPORARY_ADDON_SUFFIX}`;
  logger.info(`Generated temp id ${id} (${sess.join("")}) for ${aFile.path}`);
  return id;
}

/**
 * Loads an AddonInternal object from an add-on extracted in a directory.
 *
 * @param  aDir
 *         The nsIFile directory holding the add-on
 * @return an AddonInternal object
 * @throws if the directory does not contain a valid install manifest
 */
var loadManifestFromDir = async function(aDir, aInstallLocation) {
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

  async function loadFromRDF(aUri) {
    let fis = Cc["@mozilla.org/network/file-input-stream;1"].
              createInstance(Ci.nsIFileInputStream);
    fis.init(aUri.file, -1, -1, false);
    let bis = Cc["@mozilla.org/network/buffered-input-stream;1"].
              createInstance(Ci.nsIBufferedInputStream);
    bis.init(fis, 4096);
    try {
      var addon = await loadManifestFromRDF(aUri, bis);
    } finally {
      bis.close();
      fis.close();
    }

    let iconFile = getFile("icon.png", aDir);

    if (iconFile.exists()) {
      addon.icons[32] = "icon.png";
      addon.icons[48] = "icon.png";
    }

    let icon64File = getFile("icon64.png", aDir);

    if (icon64File.exists()) {
      addon.icons[64] = "icon64.png";
    }

    let file = getFile("chrome.manifest", aDir);
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
    addon = await loadManifestFromWebManifest(uri);
    if (!addon.id) {
      if (aInstallLocation.name == KEY_APP_TEMPORARY) {
        addon.id = generateTemporaryInstallID(aDir);
      } else {
        addon.id = aDir.leafName;
      }
    }
  } else {
    addon = await loadFromRDF(uri);
  }

  addon._sourceBundle = aDir.clone();
  addon._installLocation = aInstallLocation;
  addon.size = getFileSize(aDir);
  addon.signedState = await verifyDirSignedState(aDir, addon)
    .then(({signedState}) => signedState);
  addon.appDisabled = !isUsableAddon(addon);

  defineSyncGUID(addon);

  return addon;
};

/**
 * Loads an AddonInternal object from an nsIZipReader for an add-on.
 *
 * @param  aZipReader
 *         An open nsIZipReader for the add-on's files
 * @return an AddonInternal object
 * @throws if the XPI file does not contain a valid install manifest
 */
var loadManifestFromZipReader = async function(aZipReader, aInstallLocation) {
  async function loadFromRDF(aUri) {
    let zis = aZipReader.getInputStream(entry);
    let bis = Cc["@mozilla.org/network/buffered-input-stream;1"].
              createInstance(Ci.nsIBufferedInputStream);
    bis.init(zis, 4096);
    try {
      var addon = await loadManifestFromRDF(aUri, bis);
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
              await loadManifestFromWebManifest(uri) :
              await loadFromRDF(uri);

  addon._sourceBundle = aZipReader.file;
  addon._installLocation = aInstallLocation;

  addon.size = 0;
  let entries = aZipReader.findEntries(null);
  while (entries.hasMore())
    addon.size += aZipReader.getEntry(entries.getNext()).realSize;

  let {signedState, cert} = await verifyZipSignedState(aZipReader.file, addon);
  addon.signedState = signedState;
  if (isWebExtension && !addon.id) {
    if (cert) {
      addon.id = cert.commonName;
      if (!gIDTest.test(addon.id)) {
        throw new Error(`Webextension is signed with an invalid id (${addon.id})`);
      }
    }
    if (!addon.id && aInstallLocation.name == KEY_APP_TEMPORARY) {
      addon.id = generateTemporaryInstallID(aZipReader.file);
    }
  }
  addon.appDisabled = !isUsableAddon(addon);

  defineSyncGUID(addon);

  return addon;
};

/**
 * Loads an AddonInternal object from an add-on in an XPI file.
 *
 * @param  aXPIFile
 *         An nsIFile pointing to the add-on's XPI file
 * @return an AddonInternal object
 * @throws if the XPI file does not contain a valid install manifest
 */
var loadManifestFromZipFile = async function(aXPIFile, aInstallLocation) {
  let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                  createInstance(Ci.nsIZipReader);
  try {
    zipReader.open(aXPIFile);

    // Can't return this promise because that will make us close the zip reader
    // before it has finished loading the manifest. Wait for the result and then
    // return.
    let manifest = await loadManifestFromZipReader(zipReader, aInstallLocation);
    return manifest;
  } finally {
    zipReader.close();
  }
};

this.loadManifestFromFile = function(aFile, aInstallLocation) {
  if (aFile.isFile())
    return loadManifestFromZipFile(aFile, aInstallLocation);
  return loadManifestFromDir(aFile, aInstallLocation);
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
 * Sends local and remote notifications to flush a JAR file cache entry
 *
 * @param aJarFile
 *        The ZIP/XPI/JAR file as a nsIFile
 */
function flushJarCache(aJarFile) {
  Services.obs.notifyObservers(aJarFile, "flush-cache-entry");
  Services.mm.broadcastAsyncMessage(MSG_JAR_FLUSH, aJarFile.path);
}

function flushChromeCaches() {
  // Init this, so it will get the notification.
  Services.obs.notifyObservers(null, "startupcache-invalidate");
  // Flush message manager cached scripts
  Services.obs.notifyObservers(null, "message-manager-flush-caches");
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
  let random = Math.random().toString(36).replace(/0./, "").substr(-3);
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
        } catch (e) {
          logger.warn("The hotfix add-on was not signed by the expected " +
                      "certificate and so will not be installed.", e);
          return AddonManager.SIGNEDSTATE_BROKEN;
        }
      }

      if (aCert.organizationalUnit == "Mozilla Components") {
        return AddonManager.SIGNEDSTATE_SYSTEM;
      }

      if (aCert.organizationalUnit == "Mozilla Extensions") {
        return AddonManager.SIGNEDSTATE_PRIVILEGED;
      }

      return /preliminary/i.test(aCert.organizationalUnit)
               ? AddonManager.SIGNEDSTATE_PRELIMINARY
               : AddonManager.SIGNEDSTATE_SIGNED;
    case Cr.NS_ERROR_SIGNED_JAR_NOT_SIGNED:
      return AddonManager.SIGNEDSTATE_MISSING;
    case Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID:
    case Cr.NS_ERROR_SIGNED_JAR_ENTRY_INVALID:
    case Cr.NS_ERROR_SIGNED_JAR_ENTRY_MISSING:
    case Cr.NS_ERROR_SIGNED_JAR_ENTRY_TOO_LARGE:
    case Cr.NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY:
    case Cr.NS_ERROR_SIGNED_JAR_MODIFIED_ENTRY:
      return AddonManager.SIGNEDSTATE_BROKEN;
    default:
      // Any other error indicates that either the add-on isn't signed or it
      // is signed by a signature that doesn't chain to the trusted root.
      return AddonManager.SIGNEDSTATE_UNKNOWN;
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
  return AddonSettings.ADDON_SIGNING && SIGNED_TYPES.has(aAddon.type);
}

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
  if (!AppConstants.MOZ_REQUIRE_SIGNING && Preferences.get(PREF_XPI_SIGNATURES_DEV_ROOT, false))
    root = Ci.nsIX509CertDB.AddonsStageRoot;

  return new Promise(resolve => {
    let callback = {
      openSignedAppFileFinished(aRv, aZipReader, aCert) {
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
  if (!AppConstants.MOZ_REQUIRE_SIGNING && Preferences.get(PREF_XPI_SIGNATURES_DEV_ROOT, false))
    root = Ci.nsIX509CertDB.AddonsStageRoot;

  return new Promise(resolve => {
    let callback = {
      verifySignedDirectoryFinished(aRv, aCert) {
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
this.verifyBundleSignedState = function(aBundle, aAddon) {
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
function escapeAddonURI(aAddon, aUri, aUpdateType, aAppVersion) {
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

async function removeAsync(aFile) {
  let info = null;
  try {
    info = await OS.File.stat(aFile.path);
    if (info.isDir)
      await OS.File.removeDir(aFile.path);
    else
      await OS.File.remove(aFile.path);
  } catch (e) {
    if (!(e instanceof OS.File.Error) || !e.becauseNoSuchFile)
      throw e;
    // The file has already gone away
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
  } catch (e) {
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
  } catch (e) {
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
  } catch (e) {
    logger.error("Failed to remove empty directory " + aFile.path, e);
    throw e;
  }
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

function getHashStringForCrypto(aCrypto) {
  // return the two-digit hexadecimal code for a byte
  let toHexString = charCode => ("0" + charCode.toString(16)).slice(-2);

  // convert the binary hash data to a hex string.
  let binary = aCrypto.finish(false);
  let hash = Array.from(binary, c => toHexString(c.charCodeAt(0)))
  return hash.join("").toLowerCase();
}

/**
 * Base class for objects that manage the installation of an addon.
 * This class isn't instantiated directly, see the derived classes below.
 */
class AddonInstall {
  /**
   * Instantiates an AddonInstall.
   *
   * @param  installLocation
   *         The install location the add-on will be installed into
   * @param  url
   *         The nsIURL to get the add-on from. If this is an nsIFileURL then
   *         the add-on will not need to be downloaded
   * @param  options
   *         Additional options for the install
   * @param  options.hash
   *         An optional hash for the add-on
   * @param  options.existingAddon
   *         The add-on this install will update if known
   * @param  options.name
   *         An optional name for the add-on
   * @param  options.type
   *         An optional type for the add-on
   * @param  options.icons
   *         Optional icons for the add-on
   * @param  options.version
   *         An optional version for the add-on
   * @param  options.promptHandler
   *         A callback to prompt the user before installing.
   */
  constructor(installLocation, url, options = {}) {
    this.wrapper = new AddonInstallWrapper(this);
    this.installLocation = installLocation;
    this.sourceURI = url;

    if (options.hash) {
      let hashSplit = options.hash.toLowerCase().split(":");
      this.originalHash = {
        algorithm: hashSplit[0],
        data: hashSplit[1]
      };
    }
    this.hash = this.originalHash;
    this.existingAddon = options.existingAddon || null;
    this.promptHandler = options.promptHandler || (() => Promise.resolve());
    this.releaseNotesURI = null;

    this.listeners = [];
    this.icons = options.icons || {};
    this.error = 0;

    this.progress = 0;
    this.maxProgress = -1;

    // Giving each instance of AddonInstall a reference to the logger.
    this.logger = logger;

    this.name = options.name || null;
    this.type = options.type || null;
    this.version = options.version || null;

    this.file = null;
    this.ownsTempFile = null;
    this.certificate = null;
    this.certName = null;

    this.addon = null;
    this.state = null;

    XPIProvider.installs.add(this);
  }

  /**
   * Starts installation of this add-on from whatever state it is currently at
   * if possible.
   *
   * Note this method is overridden to handle additional state in
   * the subclassses below.
   *
   * @throws if installation cannot proceed from the current state
   */
  install() {
    switch (this.state) {
    case AddonManager.STATE_DOWNLOADED:
      this.checkPrompt();
      break;
    case AddonManager.STATE_PROMPTS_DONE:
      this.checkForBlockers();
      break;
    case AddonManager.STATE_READY:
      this.startInstall();
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
  }

  /**
   * Cancels installation of this add-on.
   *
   * Note this method is overridden to handle additional state in
   * the subclass DownloadAddonInstall.
   *
   * @throws if installation cannot be cancelled from the current state
   */
  cancel() {
    switch (this.state) {
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
      let xpi = getFile(`${this.addon.id}.xpi`, this.installLocation.getStagingDir());
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
  }

  /**
   * Adds an InstallListener for this instance if the listener is not already
   * registered.
   *
   * @param  aListener
   *         The InstallListener to add
   */
  addListener(aListener) {
    if (!this.listeners.some(function(i) { return i == aListener; }))
      this.listeners.push(aListener);
  }

  /**
   * Removes an InstallListener for this instance if it is registered.
   *
   * @param  aListener
   *         The InstallListener to remove
   */
  removeListener(aListener) {
    this.listeners = this.listeners.filter(function(i) {
      return i != aListener;
    });
  }

  /**
   * Removes the temporary file owned by this AddonInstall if there is one.
   */
  removeTemporaryFile() {
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
    } catch (e) {
      this.logger.warn("Failed to remove temporary file " + this.file.path + " for addon " +
          this.sourceURI.spec,
          e);
    }
  }

  /**
   * Updates the sourceURI and releaseNotesURI values on the Addon being
   * installed by this AddonInstall instance.
   */
  updateAddonURIs() {
    this.addon.sourceURI = this.sourceURI.spec;
    if (this.releaseNotesURI)
      this.addon.releaseNotesURI = this.releaseNotesURI.spec;
  }

  /**
   * Called after the add-on is a local file and the signature and install
   * manifest can be read.
   *
   * @param  aCallback
   *         A function to call when the manifest has been loaded
   * @throws if the add-on does not contain a valid install manifest or the
   *         XPI is incorrectly signed
   */
  async loadManifest(file) {
    let zipreader = Cc["@mozilla.org/libjar/zip-reader;1"].
        createInstance(Ci.nsIZipReader);
    try {
      zipreader.open(file);
    } catch (e) {
      zipreader.close();
      return Promise.reject([AddonManager.ERROR_CORRUPT_FILE, e]);
    }

    try {
      // loadManifestFromZipReader performs the certificate verification for us
      this.addon = await loadManifestFromZipReader(zipreader, this.installLocation);
    } catch (e) {
      zipreader.close();
      return Promise.reject([AddonManager.ERROR_CORRUPT_FILE, e]);
    }

    if (!this.addon.id) {
      let err = new Error(`Cannot find id for addon ${file.path}`);
      return Promise.reject([AddonManager.ERROR_CORRUPT_FILE, err]);
    }

    if (this.existingAddon) {
      // Check various conditions related to upgrades
      if (this.addon.id != this.existingAddon.id) {
        zipreader.close();
        return Promise.reject([AddonManager.ERROR_INCORRECT_ID,
                               `Refusing to upgrade addon ${this.existingAddon.id} to different ID ${this.addon.id}`]);
      }

      if (isWebExtension(this.existingAddon.type) && !isWebExtension(this.addon.type)) {
        zipreader.close();
        return Promise.reject([AddonManager.ERROR_UNEXPECTED_ADDON_TYPE,
                               "WebExtensions may not be upated to other extension types"]);
      }
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
    } else if (this.addon.signedState == AddonManager.SIGNEDSTATE_UNKNOWN ||
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

    zipreader.close();

    this.updateAddonURIs();

    this.addon._install = this;
    this.name = this.addon.selectedLocale.name;
    this.type = this.addon.type;
    this.version = this.addon.version;

    // Setting the iconURL to something inside the XPI locks the XPI and
    // makes it impossible to delete on Windows.

    // Try to load from the existing cache first
    let repoAddon = await new Promise(resolve => AddonRepository.getCachedAddonByID(this.addon.id, resolve));

    // It wasn't there so try to re-download it
    if (!repoAddon) {
      await new Promise(resolve => AddonRepository.cacheAddons([this.addon.id], resolve));
      repoAddon = await new Promise(resolve => AddonRepository.getCachedAddonByID(this.addon.id, resolve));
    }

    this.addon._repositoryAddon = repoAddon;
    this.name = this.name || this.addon._repositoryAddon.name;
    this.addon.compatibilityOverrides = repoAddon ?
      repoAddon.compatibilityOverrides :
      null;
    this.addon.appDisabled = !isUsableAddon(this.addon);
    return undefined;
  }

  getIcon(desiredSize = 64) {
    if (!this.addon.icons || !this.file) {
      return null;
    }

    let {icon} = IconDetails.getPreferredIcon(this.addon.icons, null, desiredSize);
    if (icon.startsWith("chrome://")) {
      return icon;
    }
    return buildJarURI(this.file, icon).spec;
  }

  /**
   * This method should be called when the XPI is ready to be installed,
   * i.e., when a download finishes or when a local file has been verified.
   * It should only be called from install() when the install is in
   * STATE_DOWNLOADED (which actually means that the file is available
   * and has been verified).
   */
  checkPrompt() {
    (async () => {
      if (this.promptHandler) {
        let info = {
          existingAddon: this.existingAddon ? this.existingAddon.wrapper : null,
          addon: this.addon.wrapper,
          icon: this.getIcon(),
        };

        try {
          await this.promptHandler(info);
        } catch (err) {
          logger.info(`Install of ${this.addon.id} cancelled by user`);
          this.state = AddonManager.STATE_CANCELLED;
          XPIProvider.removeActiveInstall(this);
          AddonManagerPrivate.callInstallListeners("onInstallCancelled",
                                                   this.listeners, this.wrapper);
          return;
        }
      }
      this.state = AddonManager.STATE_PROMPTS_DONE;
      this.install();
    })();
  }

  /**
   * This method should be called when we have the XPI and any needed
   * permissions prompts have been completed.  If there are any upgrade
   * listeners, they are invoked and the install moves into STATE_POSTPONED.
   * Otherwise, the install moves into STATE_INSTALLING
   */
  checkForBlockers() {
    // If an upgrade listener is registered for this add-on, pass control
    // over the upgrade to the add-on.
    if (AddonManagerPrivate.hasUpgradeListener(this.addon.id)) {
      logger.info(`add-on ${this.addon.id} has an upgrade listener, postponing upgrade until restart`);
      let resumeFn = () => {
        logger.info(`${this.addon.id} has resumed a previously postponed upgrade`);
        this.state = AddonManager.STATE_READY;
        this.install();
      }
      this.postpone(resumeFn);
      return;
    }

    this.state = AddonManager.STATE_READY;
    this.install();
  }

  // TODO This relies on the assumption that we are always installing into the
  // highest priority install location so the resulting add-on will be visible
  // overriding any existing copy in another install location (bug 557710).
  /**
   * Installs the add-on into the install location.
   */
  startInstall() {
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

    let stagedAddon = this.installLocation.getStagingDir();

    (async () => {
      let installedUnpacked = 0;

      await this.installLocation.requestStagingDir();

      // remove any previously staged files
      await this.unstageInstall(stagedAddon);

      stagedAddon.append(`${this.addon.id}.xpi`);

      installedUnpacked = await this.stageInstall(requiresRestart, stagedAddon, isUpgrade);

      if (requiresRestart) {
        this.state = AddonManager.STATE_INSTALLED;
        AddonManagerPrivate.callInstallListeners("onInstallEnded",
                                                 this.listeners, this.wrapper,
                                                 this.addon.wrapper);
      } else {
        // The install is completed so it should be removed from the active list
        XPIProvider.removeActiveInstall(this);

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
        let file = this.installLocation.installAddon({
          id: this.addon.id,
          source: stagedAddon,
          existingAddonID
        });

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
        } else {
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
          } else {
            // XXX this makes it dangerous to do some things in onInstallEnded
            // listeners because important cleanup hasn't been done yet
            XPIProvider.unloadBootstrapScope(this.addon.id);
          }
        }
        XPIProvider.setTelemetry(this.addon.id, "unpacked", installedUnpacked);
        recordAddonTelemetry(this.addon);

        // Notify providers that a new theme has been enabled.
        if (isTheme(this.addon.type) && this.addon.active)
          AddonManagerPrivate.notifyAddonChanged(this.addon.id, this.addon.type, requiresRestart);
      }
    })().catch((e) => {
      logger.warn(`Failed to install ${this.file.path} from ${this.sourceURI.spec} to ${stagedAddon.path}`, e);

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
  }

  /**
   * Stages an upgrade for next application restart.
   */
  async stageInstall(restartRequired, stagedAddon, isUpgrade) {
    let stagedJSON = stagedAddon.clone();
    stagedJSON.leafName = this.addon.id + ".json";

    let installedUnpacked = 0;

    // First stage the file regardless of whether restarting is necessary
    if (this.addon.unpack || Preferences.get(PREF_XPI_UNPACK, false)) {
      logger.debug("Addon " + this.addon.id + " will be installed as " +
                   "an unpacked directory");
      stagedAddon.leafName = this.addon.id;
      await OS.File.makeDir(stagedAddon.path);
      await ZipUtils.extractFilesAsync(this.file, stagedAddon);
      installedUnpacked = 1;
    } else {
      logger.debug(`Addon ${this.addon.id} will be installed as a packed xpi`);
      stagedAddon.leafName = this.addon.id + ".xpi";

      await OS.File.copy(this.file.path, stagedAddon.path);
    }

    if (restartRequired) {
      // Point the add-on to its extracted files as the xpi may get deleted
      this.addon._sourceBundle = stagedAddon;

      // Cache the AddonInternal as it may have updated compatibility info
      writeStringToFile(stagedJSON, JSON.stringify(this.addon));

      logger.debug("Staged install of " + this.addon.id + " from " + this.sourceURI.spec + " ready; waiting for restart.");
      if (isUpgrade) {
        delete this.existingAddon.pendingUpgrade;
        this.existingAddon.pendingUpgrade = this.addon;
      }
    }

    return installedUnpacked;
  }

  /**
   * Removes any previously staged upgrade.
   */
  async unstageInstall(stagedAddon) {
    let stagedJSON = getFile(`${this.addon.id}.json`, stagedAddon);
    if (stagedJSON.exists()) {
      stagedJSON.remove(true);
    }

    await removeAsync(getFile(this.addon.id, stagedAddon));

    await removeAsync(getFile(`${this.addon.id}.xpi`, stagedAddon));
  }

  /**
    * Postone a pending update, until restart or until the add-on resumes.
    *
    * @param {Function} resumeFn - a function for the add-on to run
    *                                    when resuming.
    */
  async postpone(resumeFn) {
    this.state = AddonManager.STATE_POSTPONED;

    let stagingDir = this.installLocation.getStagingDir();

    await this.installLocation.requestStagingDir();
    await this.unstageInstall(stagingDir);

    let stagedAddon = getFile(`${this.addon.id}.xpi`, stagingDir);

    await this.stageInstall(true, stagedAddon, true);

    AddonManagerPrivate.callInstallListeners("onInstallPostponed",
                                             this.listeners, this.wrapper)

    // upgrade has been staged for restart, provide a way for it to call the
    // resume function.
    let callback = AddonManagerPrivate.getUpgradeListener(this.addon.id);
    if (callback) {
      callback({
        version: this.version,
        install: () => {
          switch (this.state) {
          case AddonManager.STATE_POSTPONED:
            if (resumeFn) {
              resumeFn();
            }
            break;
          default:
            logger.warn(`${this.addon.id} cannot resume postponed upgrade from state (${this.state})`);
            break;
          }
        },
      });
    }
    // Release the staging directory lock, but since the staging dir is populated
    // it will not be removed until resumed or installed by restart.
    // See also cleanStagingDir()
    this.installLocation.releaseStagingDir();
  }
}

this.LocalAddonInstall = class extends AddonInstall {
  /**
   * Initialises this install to be an install from a local file.
   *
   * @returns Promise
   *          A Promise that resolves when the object is ready to use.
   */
  async init() {
    this.file = this.sourceURI.QueryInterface(Ci.nsIFileURL).file;

    if (!this.file.exists()) {
      logger.warn("XPI file " + this.file.path + " does not exist");
      this.state = AddonManager.STATE_DOWNLOAD_FAILED;
      this.error = AddonManager.ERROR_NETWORK_FAILURE;
      XPIProvider.removeActiveInstall(this);
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
      } catch (e) {
        logger.warn("Unknown hash algorithm '" + this.hash.algorithm + "' for addon " + this.sourceURI.spec, e);
        this.state = AddonManager.STATE_DOWNLOAD_FAILED;
        this.error = AddonManager.ERROR_INCORRECT_HASH;
        XPIProvider.removeActiveInstall(this);
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
        XPIProvider.removeActiveInstall(this);
        return;
      }
    }

    try {
      await this.loadManifest(this.file);
    } catch ([error, message]) {
      logger.warn("Invalid XPI", message);
      this.state = AddonManager.STATE_DOWNLOAD_FAILED;
      this.error = error;
      XPIProvider.removeActiveInstall(this);
      AddonManagerPrivate.callInstallListeners("onNewInstall",
                                               this.listeners,
                                               this.wrapper);
      flushJarCache(this.file);
      return;
    }

    let addon = await new Promise(resolve => {
      XPIDatabase.getVisibleAddonForID(this.addon.id, resolve);
    });

    this.existingAddon = addon;
    if (addon)
      applyBlocklistChanges(addon, this.addon);
    this.addon.updateDate = Date.now();
    this.addon.installDate = addon ? addon.installDate : this.addon.updateDate;

    if (!this.addon.isCompatible) {
      this.state = AddonManager.STATE_CHECKING;

      await new Promise(resolve => {
        new UpdateChecker(this.addon, {
          onUpdateFinished: aAddon => {
            this.state = AddonManager.STATE_DOWNLOADED;
            AddonManagerPrivate.callInstallListeners("onNewInstall",
                                                     this.listeners,
                                                     this.wrapper);
            resolve();
          }
        }, AddonManager.UPDATE_WHEN_ADDON_INSTALLED);
      });
    } else {
      AddonManagerPrivate.callInstallListeners("onNewInstall",
                                               this.listeners,
                                               this.wrapper);

    }
  }

  install() {
    if (this.state == AddonManager.STATE_DOWNLOAD_FAILED) {
      // For a local install, this state means that verification of the
      // file failed (e.g., the hash or signature or manifest contents
      // were invalid).  It doesn't make sense to retry anything in this
      // case but we have callers who don't know if their AddonInstall
      // object is a local file or a download so accomodate them here.
      AddonManagerPrivate.callInstallListeners("onDownloadFailed",
                                               this.listeners, this.wrapper);
      return;
    }
    super.install();
  }
}

this.DownloadAddonInstall = class extends AddonInstall {
  /**
   * Instantiates a DownloadAddonInstall
   *
   * @param  installLocation
   *         The InstallLocation the add-on will be installed into
   * @param  url
   *         The nsIURL to get the add-on from
   * @param  options
   *         Additional options for the install
   * @param  options.hash
   *         An optional hash for the add-on
   * @param  options.existingAddon
   *         The add-on this install will update if known
   * @param  options.browser
   *         The browser performing the install, used to display
   *         authentication prompts.
   * @param  options.name
   *         An optional name for the add-on
   * @param  options.type
   *         An optional type for the add-on
   * @param  options.icons
   *         Optional icons for the add-on
   * @param  options.version
   *         An optional version for the add-on
   * @param  options.promptHandler
   *         A callback to prompt the user before installing.
   */
  constructor(installLocation, url, options = {}) {
    super(installLocation, url, options);

    this.browser = options.browser;

    this.state = AddonManager.STATE_AVAILABLE;

    this.stream = null;
    this.crypto = null;
    this.badCertHandler = null;
    this.restartDownload = false;

    AddonManagerPrivate.callInstallListeners("onNewInstall", this.listeners,
                                            this.wrapper);
  }

  install() {
    switch (this.state) {
    case AddonManager.STATE_AVAILABLE:
      this.startDownload();
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
      this.startDownload();
      break;
    default:
      super.install();
    }
  }

  cancel() {
    if (this.state == AddonManager.STATE_DOWNLOADING) {
      if (this.channel) {
        logger.debug("Cancelling download of " + this.sourceURI.spec);
        this.channel.cancel(Cr.NS_BINDING_ABORTED);
      }
    } else {
      super.cancel();
    }
  }

  observe(aSubject, aTopic, aData) {
    // Network is going offline
    this.cancel();
  }

  /**
   * Starts downloading the add-on's XPI file.
   */
  startDownload() {
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
  }

  openChannel() {
    this.restartDownload = false;

    try {
      this.file = getTemporaryFile();
      this.ownsTempFile = true;
      this.stream = Cc["@mozilla.org/network/file-output-stream;1"].
                    createInstance(Ci.nsIFileOutputStream);
      this.stream.init(this.file, FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE |
                       FileUtils.MODE_TRUNCATE, FileUtils.PERMS_FILE, 0);
    } catch (e) {
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

      Services.obs.addObserver(this, "network:offline-about-to-go-offline");
    } catch (e) {
      logger.warn("Failed to start download for addon " + this.sourceURI.spec, e);
      this.state = AddonManager.STATE_DOWNLOAD_FAILED;
      this.error = AddonManager.ERROR_NETWORK_FAILURE;
      XPIProvider.removeActiveInstall(this);
      AddonManagerPrivate.callInstallListeners("onDownloadFailed",
                                               this.listeners, this.wrapper);
    }
  }

  /**
   * Update the crypto hasher with the new data and call the progress listeners.
   *
   * @see nsIStreamListener
   */
  onDataAvailable(aRequest, aContext, aInputstream, aOffset, aCount) {
    this.crypto.updateFromStream(aInputstream, aCount);
    this.progress += aCount;
    if (!AddonManagerPrivate.callInstallListeners("onDownloadProgress",
                                                  this.listeners, this.wrapper)) {
      // TODO cancel the download and make it available again (bug 553024)
    }
  }

  /**
   * Check the redirect response for a hash of the target XPI and verify that
   * we don't end up on an insecure channel.
   *
   * @see nsIChannelEventSink
   */
  asyncOnChannelRedirect(aOldChannel, aNewChannel, aFlags, aCallback) {
    if (!this.hash && aOldChannel.originalURI.schemeIs("https") &&
        aOldChannel instanceof Ci.nsIHttpChannel) {
      try {
        let hashStr = aOldChannel.getResponseHeader("X-Target-Digest");
        let hashSplit = hashStr.toLowerCase().split(":");
        this.hash = {
          algorithm: hashSplit[0],
          data: hashSplit[1]
        };
      } catch (e) {
      }
    }

    // Verify that we don't end up on an insecure channel if we haven't got a
    // hash to verify with (see bug 537761 for discussion)
    if (!this.hash)
      this.badCertHandler.asyncOnChannelRedirect(aOldChannel, aNewChannel, aFlags, aCallback);
    else
      aCallback.onRedirectVerifyCallback(Cr.NS_OK);

    this.channel = aNewChannel;
  }

  /**
   * This is the first chance to get at real headers on the channel.
   *
   * @see nsIStreamListener
   */
  onStartRequest(aRequest, aContext) {
    this.crypto = Cc["@mozilla.org/security/hash;1"].
                  createInstance(Ci.nsICryptoHash);
    if (this.hash) {
      try {
        this.crypto.initWithString(this.hash.algorithm);
      } catch (e) {
        logger.warn("Unknown hash algorithm '" + this.hash.algorithm + "' for addon " + this.sourceURI.spec, e);
        this.state = AddonManager.STATE_DOWNLOAD_FAILED;
        this.error = AddonManager.ERROR_INCORRECT_HASH;
        XPIProvider.removeActiveInstall(this);
        AddonManagerPrivate.callInstallListeners("onDownloadFailed",
                                                 this.listeners, this.wrapper);
        aRequest.cancel(Cr.NS_BINDING_ABORTED);
        return;
      }
    } else {
      // We always need something to consume data from the inputstream passed
      // to onDataAvailable so just create a dummy cryptohasher to do that.
      this.crypto.initWithString("sha1");
    }

    this.progress = 0;
    if (aRequest instanceof Ci.nsIChannel) {
      try {
        this.maxProgress = aRequest.contentLength;
      } catch (e) {
      }
      logger.debug("Download started for " + this.sourceURI.spec + " to file " +
          this.file.path);
    }
  }

  /**
   * The download is complete.
   *
   * @see nsIStreamListener
   */
  onStopRequest(aRequest, aContext, aStatus) {
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
          } catch (e) {
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
          } else {
            // TODO Should we send some event here (bug 557716)?
            this.state = AddonManager.STATE_CHECKING;
            new UpdateChecker(this.addon, {
              onUpdateFinished: aAddon => this.downloadCompleted(),
            }, AddonManager.UPDATE_WHEN_ADDON_INSTALLED);
          }
        }, ([error, message]) => {
          this.removeTemporaryFile();
          this.downloadFailed(error, message);
        });
      } else if (aRequest instanceof Ci.nsIHttpChannel) {
        this.downloadFailed(AddonManager.ERROR_NETWORK_FAILURE,
                            aRequest.responseStatus + " " +
                            aRequest.responseStatusText);
      } else {
        this.downloadFailed(AddonManager.ERROR_NETWORK_FAILURE, aStatus);
      }
    } else {
      this.downloadFailed(AddonManager.ERROR_NETWORK_FAILURE, aStatus);
    }
  }

  /**
   * Notify listeners that the download failed.
   *
   * @param  aReason
   *         Something to log about the failure
   * @param  error
   *         The error code to pass to the listeners
   */
  downloadFailed(aReason, aError) {
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
    } else
      logger.debug("downloadFailed: listener changed AddonInstall state for " +
          this.sourceURI.spec + " to " + this.state);
  }

  /**
   * Notify listeners that the download completed.
   */
  downloadCompleted() {
    XPIDatabase.getVisibleAddonForID(this.addon.id, aAddon => {
      if (aAddon)
        this.existingAddon = aAddon;

      this.state = AddonManager.STATE_DOWNLOADED;
      this.addon.updateDate = Date.now();

      if (this.existingAddon) {
        this.addon.existingAddonID = this.existingAddon.id;
        this.addon.installDate = this.existingAddon.installDate;
        applyBlocklistChanges(this.existingAddon, this.addon);
      } else {
        this.addon.installDate = this.addon.updateDate;
      }

      if (AddonManagerPrivate.callInstallListeners("onDownloadEnded",
                                                   this.listeners,
                                                   this.wrapper)) {
        // If a listener changed our state then do not proceed with the install
        if (this.state != AddonManager.STATE_DOWNLOADED)
          return;

        // proceed with the install state machine.
        this.install();
      }
    });
  }

  getInterface(iid) {
    if (iid.equals(Ci.nsIAuthPrompt2)) {
      let win = null;
      if (this.browser) {
        win = this.browser.contentWindow || this.browser.ownerGlobal;
      }

      let factory = Cc["@mozilla.org/prompter;1"].
                    getService(Ci.nsIPromptFactory);
      let prompt = factory.getPrompt(win, Ci.nsIAuthPrompt2);

      if (this.browser && prompt instanceof Ci.nsILoginManagerPrompter)
        prompt.browser = this.browser;

      return prompt;
    } else if (iid.equals(Ci.nsIChannelEventSink)) {
      return this;
    }

    return this.badCertHandler.getInterface(iid);
  }
}

/**
 * This class exists just for the specific case of staged add-ons that
 * fail to install at startup.  When that happens, the add-on remains
 * staged but we want to keep track of it like other installs so that we
 * can clean it up if the same add-on is installed again (see the comment
 * about "pending installs for the same add-on" in AddonInstall.startInstall)
 */
this.StagedAddonInstall = class extends AddonInstall {
  constructor(installLocation, dir, manifest) {
    super(installLocation, dir);

    this.name = manifest.name;
    this.type = manifest.type;
    this.version = manifest.version;
    this.icons = manifest.icons;
    this.releaseNotesURI = manifest.releaseNotesURI ?
                           Services.io.newURI(manifest.releaseNotesURI) :
                           null;
    this.sourceURI = manifest.sourceURI ?
                     Services.io.newURI(manifest.sourceURI) :
                     null;
    this.file = null;
    this.addon = manifest;

    this.state = AddonManager.STATE_INSTALLED;
  }
}

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
function createUpdate(aCallback, aAddon, aUpdate) {
  let url = Services.io.newURI(aUpdate.updateURL);

  (async function() {
    let opts = {
      hash: aUpdate.updateHash,
      existingAddon: aAddon,
      name: aAddon.selectedLocale.name,
      type: aAddon.type,
      icons: aAddon.icons,
      version: aUpdate.version,
    };
    let install;
    if (url instanceof Ci.nsIFileURL) {
      install = new LocalAddonInstall(aAddon._installLocation, url, opts);
      await install.init();
    } else {
      install = new DownloadAddonInstall(aAddon._installLocation, url, opts);
    }
    try {
      if (aUpdate.updateInfoURL)
        install.releaseNotesURI = Services.io.newURI(escapeAddonURI(aAddon, aUpdate.updateInfoURL));
    } catch (e) {
      // If the releaseNotesURI cannot be parsed then just ignore it.
    }

    aCallback(install);
  })();
}

// Maps instances of AddonInstall to AddonInstallWrapper
const wrapperMap = new WeakMap();
let installFor = wrapper => wrapperMap.get(wrapper);

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

  set promptHandler(handler) {
    installFor(this).promptHandler = handler;
  },

  install() {
    installFor(this).install();
  },

  cancel() {
    installFor(this).cancel();
  },

  addListener(listener) {
    installFor(this).addListener(listener);
  },

  removeListener(listener) {
    installFor(this).removeListener(listener);
  },
};

["name", "version", "icons", "releaseNotesURI", "file", "state", "error",
 "progress", "maxProgress", "certificate", "certName"].forEach(function(aProp) {
  Object.defineProperty(AddonInstallWrapper.prototype, aProp, {
    get() {
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
this.UpdateChecker = function(aAddon, aListener, aReason, aAppVersion, aPlatformVersion) {
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
  callListener(aMethod, ...aArgs) {
    if (!(aMethod in this.listener))
      return;

    try {
      this.listener[aMethod].apply(this.listener, aArgs);
    } catch (e) {
      logger.warn("Exception calling UpdateListener method " + aMethod, e);
    }
  },

  /**
   * Called when AddonUpdateChecker completes the update check
   *
   * @param  updates
   *         The list of update details for the add-on
   */
  onUpdateCheckComplete(aUpdates) {
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
      } else {
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
        } else
          sendUpdateAvailableMessages(this, null);
        return;
      }

      createUpdate(aInstall => {
        sendUpdateAvailableMessages(this, aInstall);
      }, this.addon, update);
    } else {
      sendUpdateAvailableMessages(this, null);
    }
  },

  /**
   * Called when AddonUpdateChecker fails the update check
   *
   * @param  aError
   *         An error status
   */
  onUpdateCheckError(aError) {
    XPIProvider.done(this.addon._updateCheck);
    this.addon._updateCheck = null;
    this.callListener("onNoCompatibilityUpdateAvailable", this.addon.wrapper);
    this.callListener("onNoUpdateAvailable", this.addon.wrapper);
    this.callListener("onUpdateFinished", this.addon.wrapper, aError);
  },

  /**
   * Called to cancel an in-progress update check
   */
  cancel() {
    let parser = this._parser;
    if (parser) {
      this._parser = null;
      // This will call back to onUpdateCheckError with a CANCELLED error
      parser.cancel();
    }
  }
};
