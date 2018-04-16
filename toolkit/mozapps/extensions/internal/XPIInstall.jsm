/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "DownloadAddonInstall",
  "LocalAddonInstall",
  "UpdateChecker",
  "XPIInstall",
  "loadManifestFromFile",
  "verifyBundleSignedState",
];

/* globals DownloadAddonInstall, LocalAddonInstall */

Cu.importGlobalProperties(["TextDecoder", "TextEncoder", "fetch"]);

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/AddonManager.jsm");

ChromeUtils.defineModuleGetter(this, "AddonRepository",
                               "resource://gre/modules/addons/AddonRepository.jsm");
ChromeUtils.defineModuleGetter(this, "AddonSettings",
                               "resource://gre/modules/addons/AddonSettings.jsm");
ChromeUtils.defineModuleGetter(this, "AppConstants",
                               "resource://gre/modules/AppConstants.jsm");
ChromeUtils.defineModuleGetter(this, "CertUtils",
                               "resource://gre/modules/CertUtils.jsm");
ChromeUtils.defineModuleGetter(this, "ExtensionData",
                               "resource://gre/modules/Extension.jsm");
ChromeUtils.defineModuleGetter(this, "FileUtils",
                               "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyGetter(this, "IconDetails", () => {
  return ChromeUtils.import("resource://gre/modules/ExtensionParent.jsm", {}).ExtensionParent.IconDetails;
});
ChromeUtils.defineModuleGetter(this, "LightweightThemeManager",
                               "resource://gre/modules/LightweightThemeManager.jsm");
ChromeUtils.defineModuleGetter(this, "NetUtil",
                               "resource://gre/modules/NetUtil.jsm");
ChromeUtils.defineModuleGetter(this, "OS",
                               "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(this, "ZipUtils",
                               "resource://gre/modules/ZipUtils.jsm");

const {nsIBlocklistService} = Ci;

const nsIFile = Components.Constructor("@mozilla.org/file/local;1", "nsIFile",
                                       "initWithPath");
const CryptoHash = Components.Constructor("@mozilla.org/security/hash;1",
                                          "nsICryptoHash", "initWithString");
const ZipReader = Components.Constructor("@mozilla.org/libjar/zip-reader;1",
                                         "nsIZipReader", "open");

const RDFDataSource = Components.Constructor(
  "@mozilla.org/rdf/datasource;1?name=in-memory-datasource", "nsIRDFDataSource");
const parseRDFString = Components.Constructor(
  "@mozilla.org/rdf/xml-parser;1", "nsIRDFXMLParser", "parseString");

XPCOMUtils.defineLazyServiceGetters(this, {
  gCertDB: ["@mozilla.org/security/x509certdb;1", "nsIX509CertDB"],
  gRDF: ["@mozilla.org/rdf/rdf-service;1", "nsIRDFService"],
});

ChromeUtils.defineModuleGetter(this, "XPIInternal",
                               "resource://gre/modules/addons/XPIProvider.jsm");
ChromeUtils.defineModuleGetter(this, "XPIProvider",
                               "resource://gre/modules/addons/XPIProvider.jsm");

const PREF_ALLOW_NON_RESTARTLESS      = "extensions.legacy.non-restartless.enabled";

const DEFAULT_SKIN = "classic/1.0";

/* globals AddonInternal, BOOTSTRAP_REASONS, KEY_APP_SYSTEM_ADDONS, KEY_APP_SYSTEM_DEFAULTS, KEY_APP_TEMPORARY, TEMPORARY_ADDON_SUFFIX, SIGNED_TYPES, TOOLKIT_ID, XPIDatabase, XPIStates, getExternalType, isTheme, isUsableAddon, isWebExtension, mustSign, recordAddonTelemetry */
const XPI_INTERNAL_SYMBOLS = [
  "AddonInternal",
  "BOOTSTRAP_REASONS",
  "KEY_APP_SYSTEM_ADDONS",
  "KEY_APP_SYSTEM_DEFAULTS",
  "KEY_APP_TEMPORARY",
  "SIGNED_TYPES",
  "TEMPORARY_ADDON_SUFFIX",
  "TOOLKIT_ID",
  "XPIDatabase",
  "XPIStates",
  "getExternalType",
  "isTheme",
  "isUsableAddon",
  "isWebExtension",
  "mustSign",
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

const PREF_EM_UPDATE_BACKGROUND_URL   = "extensions.update.background.url";
const PREF_EM_UPDATE_URL              = "extensions.update.url";
const PREF_XPI_SIGNATURES_DEV_ROOT    = "xpinstall.signatures.dev-root";
const PREF_INSTALL_REQUIREBUILTINCERTS = "extensions.install.requireBuiltInCerts";
const FILE_WEB_MANIFEST               = "manifest.json";

const KEY_TEMPDIR                     = "TmpD";

const RDFURI_INSTALL_MANIFEST_ROOT    = "urn:mozilla:install-manifest";
const PREFIX_NS_EM                    = "http://www.mozilla.org/2004/em-rdf#";

// Properties that exist in the install manifest
const PROP_METADATA      = ["id", "version", "type", "internalName", "updateURL",
                            "optionsURL", "optionsType", "aboutURL",
                            "iconURL", "icon64URL"];
const PROP_LOCALE_SINGLE = ["name", "description", "creator", "homepageURL"];
const PROP_LOCALE_MULTI  = ["developers", "translators", "contributors"];
const PROP_TARGETAPP     = ["id", "minVersion", "maxVersion"];

// Map new string type identifiers to old style nsIUpdateItem types.
// Retired values:
// 8 = locale
// 32 = multipackage xpi file
// 8 = locale
// 256 = apiextension
// 128 = experiment
const TYPES = {
  extension: 2,
  theme: 4,
  dictionary: 64,
};

const COMPATIBLE_BY_DEFAULT_TYPES = {
  extension: true,
  dictionary: true,
};

const RESTARTLESS_TYPES = new Set([
  "dictionary",
  "webextension",
  "webextension-theme",
]);

// This is a random number array that can be used as "salt" when generating
// an automatic ID based on the directory path of an add-on. It will prevent
// someone from creating an ID for a permanent add-on that could be replaced
// by a temporary add-on (because that would be confusing, I guess).
const TEMP_INSTALL_ID_GEN_SESSION =
  new Uint8Array(Float64Array.of(Math.random()).buffer);

const MSG_JAR_FLUSH = "AddonJarFlush";
const MSG_MESSAGE_MANAGER_CACHES_FLUSH = "AddonMessageManagerCachesFlush";


/**
 * Valid IDs fit this pattern.
 */
var gIDTest = /^(\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}|[a-z0-9-\._]*\@[a-z0-9-\._]+)$/i;

ChromeUtils.import("resource://gre/modules/Log.jsm");
const LOGGER_ID = "addons.xpi";

// Create a new logger for use by all objects in this Addons XPI Provider module
// (Requires AddonManager.jsm)
var logger = Log.repository.getLogger(LOGGER_ID);

function getJarURI(file, path = "") {
  if (file instanceof Ci.nsIFile) {
    file = Services.io.newFileURI(file);
  }
  if (file instanceof Ci.nsIURI) {
    file = file.spec;
  }
  return Services.io.newURI(`jar:${file}!/${path}`);
}

let DirPackage;
let XPIPackage;
class Package {
  static get(file) {
    if (file.isFile()) {
      return new XPIPackage(file);
    }
    return new DirPackage(file);
  }

  constructor(file, rootURI) {
    this.file = file;
    this.filePath = file.path;
    this.rootURI = rootURI;
  }

  close() {}

  getURI(...path) {
    return Services.io.newURI(path.join("/"), null, this.rootURI);
  }

  async getManifestFile() {
    if (await this.hasResource("manifest.json")) {
      return "manifest.json";
    }
    if (await this.hasResource("install.rdf")) {
      return "install.rdf";
    }
    return null;
  }

  async readString(...path) {
    let buffer = await this.readBinary(...path);
    return new TextDecoder().decode(buffer);
  }

  async verifySignedState(addon) {
    if (!shouldVerifySignedState(addon)) {
      return {
        signedState: AddonManager.SIGNEDSTATE_NOT_REQUIRED,
        cert: null
      };
    }

    let root = Ci.nsIX509CertDB.AddonsPublicRoot;
    if (!AppConstants.MOZ_REQUIRE_SIGNING &&
        Services.prefs.getBoolPref(PREF_XPI_SIGNATURES_DEV_ROOT, false)) {
      root = Ci.nsIX509CertDB.AddonsStageRoot;
    }

    return this.verifySignedStateForRoot(addon, root);
  }

  flushCache() {}
}

DirPackage = class DirPackage extends Package {
  constructor(file) {
    super(file, Services.io.newFileURI(file));
  }

  hasResource(...path) {
    return OS.File.exists(OS.Path.join(this.filePath, ...path));
  }

  async iterDirectory(path, callback) {
    let fullPath = OS.Path.join(this.filePath, ...path);

    let iter = new OS.File.DirectoryIterator(fullPath);
    await iter.forEach(callback);
    iter.close();
  }

  iterFiles(callback, path = []) {
    return this.iterDirectory(path, async entry => {
      let entryPath = [...path, entry.name];
      if (entry.isDir) {
        callback({
          path: entryPath.join("/"),
          isDir: true,
        });
        await this.iterFiles(callback, entryPath);
      } else {
        let stat = await OS.File.stat(OS.Path.join(this.filePath, ...entryPath));
        callback({
          path: entryPath.join("/"),
          isDir: false,
          size: stat.size,
        });
      }
    });
  }

  readBinary(...path) {
    return OS.File.read(OS.Path.join(this.filePath, ...path));
  }

  verifySignedStateForRoot(addon, root) {
    return new Promise(resolve => {
      let callback = {
        verifySignedDirectoryFinished(aRv, aCert) {
          resolve({
            signedState: getSignedStatus(aRv, aCert, addon.id),
            cert: aCert,
          });
        }
      };
      // This allows the certificate DB to get the raw JS callback object so the
      // test code can pass through objects that XPConnect would reject.
      callback.wrappedJSObject = callback;

      gCertDB.verifySignedDirectoryAsync(root, this.file, callback);
    });
  }
};

XPIPackage = class XPIPackage extends Package {
  constructor(file) {
    super(file, getJarURI(file));

    this.zipReader = new ZipReader(file);
    this.needFlush = false;
  }

  close() {
    this.zipReader.close();
    this.zipReader = null;

    if (this.needFlush) {
      this.flushCache();
    }
  }

  async hasResource(...path) {
    return this.zipReader.hasEntry(path.join("/"));
  }

  async iterFiles(callback) {
    for (let path of XPCOMUtils.IterStringEnumerator(this.zipReader.findEntries("*"))) {
      let entry = this.zipReader.getEntry(path);
      callback({
        path,
        isDir: entry.isDirectory,
        size: entry.realSize,
      });
    }
  }

  async readBinary(...path) {
    this.needFlush = true;
    let response = await fetch(this.rootURI.resolve(path.join("/")));
    return response.arrayBuffer();
  }

  verifySignedStateForRoot(addon, root) {
    return new Promise(resolve => {
      let callback = {
        openSignedAppFileFinished(aRv, aZipReader, aCert) {
          if (aZipReader)
            aZipReader.close();
          resolve({
            signedState: getSignedStatus(aRv, aCert, addon.id),
            cert: aCert
          });
        }
      };
      // This allows the certificate DB to get the raw JS callback object so the
      // test code can pass through objects that XPConnect would reject.
      callback.wrappedJSObject = callback;

      gCertDB.openSignedAppFileAsync(root, this.file, callback);
    });
  }

  flushCache() {
    flushJarCache(this.file);
    this.needFlush = false;
  }
};

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
  addon.type = extension.type === "extension" ?
               "webextension" : `webextension-${extension.type}`;
  addon.strictCompatibility = true;
  addon.bootstrap = true;
  addon.internalName = null;
  addon.updateURL = bss.update_url;
  addon.optionsBrowserStyle = true;
  addon.optionsURL = null;
  addon.optionsType = null;
  addon.aboutURL = null;
  addon.dependencies = Object.freeze(Array.from(extension.dependencies));
  addon.startupData = extension.startupData;

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
  addon.userPermissions = extension.manifestPermissions;

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
  addon.userDisabled = (extension.type === "theme");
  addon.softDisabled = addon.blocklistState == nsIBlocklistService.STATE_SOFTBLOCKED;

  return addon;
}

/**
 * Reads an AddonInternal object from an RDF stream.
 *
 * @param  aUri
 *         The URI that the manifest is being read from
 * @param  aData
 *         The manifest text
 * @return an AddonInternal object
 * @throws if the install manifest in the RDF stream is corrupt or could not
 *         be read
 */
async function loadManifestFromRDF(aUri, aData) {
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
        if (aSeenLocales.includes(localeName)) {
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

  let ds = new RDFDataSource();
  parseRDFString(ds, aUri, aData);

  let root = gRDF.GetResource(RDFURI_INSTALL_MANIFEST_ROOT);
  let addon = new AddonInternal();
  for (let prop of PROP_METADATA) {
    addon[prop] = getRDFProperty(ds, root, prop);
  }

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
    if (!addon.bootstrap && !Services.prefs.getBoolPref(PREF_ALLOW_NON_RESTARTLESS, false))
        throw new Error(`Non-restartless extensions no longer supported`);

    addon.hasEmbeddedWebExtension = getRDFProperty(ds, root, "hasEmbeddedWebExtension") == "true";

    if (addon.optionsType &&
        addon.optionsType != AddonManager.OPTIONS_INLINE_BROWSER &&
        addon.optionsType != AddonManager.OPTIONS_TYPE_TAB) {
      throw new Error("Install manifest specifies unknown optionsType: " + addon.optionsType);
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
    if (seenApplications.includes(targetAppInfo.id)) {
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
                         addon.internalName != DEFAULT_SKIN;
  } else {
    addon.userDisabled = false;
  }

  addon.softDisabled = addon.blocklistState == nsIBlocklistService.STATE_SOFTBLOCKED;
  addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DEFAULT;

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
  const hasher = CryptoHash("sha1");
  const data = new TextEncoder().encode(aFile.path);
  // Make it so this ID cannot be guessed.
  const sess = TEMP_INSTALL_ID_GEN_SESSION;
  hasher.update(sess, sess.length);
  hasher.update(data, data.length);
  let id = `${getHashStringForCrypto(hasher)}${TEMPORARY_ADDON_SUFFIX}`;
  logger.info(`Generated temp id ${id} (${sess.join("")}) for ${aFile.path}`);
  return id;
}

var loadManifest = async function(aPackage, aInstallLocation) {
  async function loadFromRDF(aUri) {
    let manifest = await aPackage.readString("install.rdf");
    let addon = await loadManifestFromRDF(aUri, manifest);

    if (await aPackage.hasResource("icon.png")) {
      addon.icons[32] = "icon.png";
      addon.icons[48] = "icon.png";
    }

    if (await aPackage.hasResource("icon64.png")) {
      addon.icons[64] = "icon64.png";
    }

    return addon;
  }

  let entry = await aPackage.getManifestFile();
  if (!entry) {
    throw new Error("File " + aPackage.filePath + " does not contain a valid " +
                    "install manifest");
  }

  let isWebExtension = entry == FILE_WEB_MANIFEST;
  let addon = isWebExtension ?
              await loadManifestFromWebManifest(aPackage.rootURI) :
              await loadFromRDF(aPackage.getURI("install.rdf"));

  addon._sourceBundle = aPackage.file;
  addon._installLocation = aInstallLocation;

  addon.size = 0;
  await aPackage.iterFiles(entry => {
    if (!entry.isDir) {
      addon.size += entry.size;
    }
  });

  let {signedState, cert} = await aPackage.verifySignedState(addon);
  addon.signedState = signedState;

  if (isWebExtension && !addon.id) {
    if (cert) {
      addon.id = cert.commonName;
      if (!gIDTest.test(addon.id)) {
        throw new Error(`Webextension is signed with an invalid id (${addon.id})`);
      }
    }
    if (!addon.id && aInstallLocation.name == KEY_APP_TEMPORARY) {
      addon.id = generateTemporaryInstallID(aPackage.file);
    }
  }

  addon.updateBlocklistState();
  addon.appDisabled = !isUsableAddon(addon);

  defineSyncGUID(addon);

  return addon;
};

var loadManifestFromFile = async function(aFile, aInstallLocation) {
  let pkg = Package.get(aFile);
  try {
    let addon = await loadManifest(pkg, aInstallLocation);
    return addon;
  } finally {
    pkg.close();
  }
};

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
  let random = Math.round(Math.random() * 36 ** 3).toString(36);
  file.append("tmp-" + random + ".xpi");
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  return file;
}

/**
 * Returns the signedState for a given return code and certificate by verifying
 * it against the expected ID.
 */
function getSignedStatus(aRv, aCert, aAddonID) {
  let expectedCommonName = aAddonID;
  if (aAddonID && aAddonID.length > 64) {
    let data = new Uint8Array(new TextEncoder().encode(aAddonID));

    let crypto = CryptoHash("sha256");
    crypto.update(data, data.length);
    expectedCommonName = getHashStringForCrypto(crypto);
  }

  switch (aRv) {
    case Cr.NS_OK:
      if (expectedCommonName && expectedCommonName != aCert.commonName)
        return AddonManager.SIGNEDSTATE_BROKEN;

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

  // Otherwise only check signatures if signing is enabled and the add-on is one
  // of the signed types.
  return AddonSettings.ADDON_SIGNING && SIGNED_TYPES.has(aAddon.type);
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
var verifyBundleSignedState = async function(aBundle, aAddon) {
  let pkg = Package.get(aBundle);
  try {
    let {signedState} = await pkg.verifySignedState(aAddon);
    return signedState;
  } finally {
    pkg.close();
  }
};

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

function getHashStringForCrypto(aCrypto) {
  // return the two-digit hexadecimal code for a byte
  let toHexString = charCode => ("0" + charCode.toString(16)).slice(-2);

  // convert the binary hash data to a hex string.
  let binary = aCrypto.finish(false);
  let hash = Array.from(binary, c => toHexString(c.charCodeAt(0)));
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
   * Called during XPIProvider shutdown so that we can do any necessary
   * pre-shutdown cleanup.
   */
  onShutdown() {
    switch (this.state) {
    case AddonManager.STATE_POSTPONED:
      this.removeTemporaryFile();
      break;
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
      this.installLocation.cleanStagingDir([this.addon.id, this.addon.id + ".xpi"]);
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
    let pkg;
    try {
      pkg = Package.get(file);
    } catch (e) {
      return Promise.reject([AddonManager.ERROR_CORRUPT_FILE, e]);
    }

    try {
      try {
        this.addon = await loadManifest(pkg, this.installLocation);
      } catch (e) {
        return Promise.reject([AddonManager.ERROR_CORRUPT_FILE, e]);
      }

      if (!this.addon.id) {
        let err = new Error(`Cannot find id for addon ${file.path}`);
        return Promise.reject([AddonManager.ERROR_CORRUPT_FILE, err]);
      }

      if (this.existingAddon) {
        // Check various conditions related to upgrades
        if (this.addon.id != this.existingAddon.id) {
          return Promise.reject([AddonManager.ERROR_INCORRECT_ID,
                                 `Refusing to upgrade addon ${this.existingAddon.id} to different ID ${this.addon.id}`]);
        }

        if (isWebExtension(this.existingAddon.type) && !isWebExtension(this.addon.type)) {
          return Promise.reject([AddonManager.ERROR_UNEXPECTED_ADDON_TYPE,
                                 "WebExtensions may not be updated to other extension types"]);
        }
      }

      if (mustSign(this.addon.type)) {
        if (this.addon.signedState <= AddonManager.SIGNEDSTATE_MISSING) {
          // This add-on isn't properly signed by a signature that chains to the
          // trusted root.
          let state = this.addon.signedState;
          this.addon = null;

          if (state == AddonManager.SIGNEDSTATE_MISSING)
            return Promise.reject([AddonManager.ERROR_SIGNEDSTATE_REQUIRED,
                                   "signature is required but missing"]);

          return Promise.reject([AddonManager.ERROR_CORRUPT_FILE,
                                 "signature verification failed"]);
        }
      }
    } finally {
      pkg.close();
    }

    this.updateAddonURIs();

    this.addon._install = this;
    this.name = this.addon.selectedLocale.name;
    this.type = this.addon.type;
    this.version = this.addon.version;

    // Setting the iconURL to something inside the XPI locks the XPI and
    // makes it impossible to delete on Windows.

    // Try to load from the existing cache first
    let repoAddon = await AddonRepository.getCachedAddonByID(this.addon.id);

    // It wasn't there so try to re-download it
    if (!repoAddon) {
      try {
        [repoAddon] = await AddonRepository.cacheAddons([this.addon.id]);
      } catch (err) {
        logger.debug(`Error getting metadata for ${this.addon.id}: ${err.message}`);
      }
    }

    this.addon._repositoryAddon = repoAddon;
    this.name = this.name || this.addon._repositoryAddon.name;
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
    return getJarURI(this.file, icon).spec;
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
      };
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
      this.removeTemporaryFile();
      XPIProvider.removeActiveInstall(this);
      AddonManagerPrivate.callInstallListeners("onInstallCancelled",
                                               this.listeners, this.wrapper);
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

    logger.debug("Starting install of " + this.addon.id + " from " + this.sourceURI.spec);
    AddonManagerPrivate.callAddonListeners("onInstalling",
                                           this.addon.wrapper,
                                           false);

    let stagedAddon = this.installLocation.getStagingDir();

    (async () => {
      await this.installLocation.requestStagingDir();

      // remove any previously staged files
      await this.unstageInstall(stagedAddon);

      stagedAddon.append(`${this.addon.id}.xpi`);

      await this.stageInstall(false, stagedAddon, isUpgrade);

      // The install is completed so it should be removed from the active list
      XPIProvider.removeActiveInstall(this);

      // Deactivate and remove the old add-on as necessary
      let reason = BOOTSTRAP_REASONS.ADDON_INSTALL;
      let callUpdate = false;
      if (this.existingAddon) {
        if (Services.vc.compare(this.existingAddon.version, this.addon.version) < 0)
          reason = BOOTSTRAP_REASONS.ADDON_UPGRADE;
        else
          reason = BOOTSTRAP_REASONS.ADDON_DOWNGRADE;

        callUpdate = isWebExtension(this.addon.type) && isWebExtension(this.existingAddon.type);

        if (this.existingAddon.bootstrap) {
          let file = this.existingAddon._sourceBundle;
          if (this.existingAddon.active) {
            XPIProvider.callBootstrapMethod(this.existingAddon, file,
                                            "shutdown", reason,
                                            { newVersion: this.addon.version });
          }

          if (!callUpdate) {
            XPIProvider.callBootstrapMethod(this.existingAddon, file,
                                            "uninstall", reason,
                                            { newVersion: this.addon.version });
          }
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
                                                      file.path);
        let state = XPIStates.getAddon(this.installLocation.name, this.addon.id);
        if (state) {
          state.syncWithDB(this.addon, true);
        } else {
          logger.warn("Unexpected missing XPI state for add-on ${id}", this.addon);
        }
      } else {
        this.addon.active = (this.addon.visible && !this.addon.disabled);
        this.addon = XPIDatabase.addAddonMetadata(this.addon, file.path);
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
        let method = callUpdate ? "update" : "install";
        XPIProvider.callBootstrapMethod(this.addon, file, method,
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
      recordAddonTelemetry(this.addon);

      // Notify providers that a new theme has been enabled.
      if (isTheme(this.addon.type) && this.addon.active)
        AddonManagerPrivate.notifyAddonChanged(this.addon.id, this.addon.type);
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
    // First stage the file regardless of whether restarting is necessary
    if (this.addon.unpack) {
      logger.debug("Addon " + this.addon.id + " will be installed as " +
                   "an unpacked directory");
      stagedAddon.leafName = this.addon.id;
      await OS.File.makeDir(stagedAddon.path);
      await ZipUtils.extractFilesAsync(this.file, stagedAddon);
    } else {
      logger.debug(`Addon ${this.addon.id} will be installed as a packed xpi`);
      stagedAddon.leafName = this.addon.id + ".xpi";

      await OS.File.copy(this.file.path, stagedAddon.path);
    }

    if (restartRequired) {
      // Point the add-on to its extracted files as the xpi may get deleted
      this.addon._sourceBundle = stagedAddon;

      // Cache the AddonInternal as it may have updated compatibility info
      XPIStates.getLocation(this.installLocation.name).stageAddon(this.addon.id,
                                                                  this.addon.toJSON());

      logger.debug(`Staged install of ${this.addon.id} from ${this.sourceURI.spec} ready; waiting for restart.`);
      if (isUpgrade) {
        delete this.existingAddon.pendingUpgrade;
        this.existingAddon.pendingUpgrade = this.addon;
      }
    }
  }

  /**
   * Removes any previously staged upgrade.
   */
  async unstageInstall(stagedAddon) {
    XPIStates.getLocation(this.installLocation.name).unstageAddon(this.addon.id);

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
                                             this.listeners, this.wrapper);

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

var LocalAddonInstall = class extends AddonInstall {
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
      let crypto;
      try {
        crypto = CryptoHash(this.hash.algorithm);
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
    this.addon.updateBlocklistState({oldAddon: this.existingAddon});
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
      // object is a local file or a download so accommodate them here.
      AddonManagerPrivate.callInstallListeners("onDownloadFailed",
                                               this.listeners, this.wrapper);
      return;
    }
    super.install();
  }
};

var DownloadAddonInstall = class extends AddonInstall {
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
                                               this.listeners, this.wrapper);
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
      let requireBuiltIn = Services.prefs.getBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, true);
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
    if (this.hash) {
      try {
        this.crypto = CryptoHash(this.hash.algorithm);
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
      this.crypto = CryptoHash("sha1");
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
                                !Services.prefs.getBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, true));
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
      } else {
        this.addon.installDate = this.addon.updateDate;
      }
      this.addon.updateBlocklistState({oldAddon: this.existingAddon});

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
 "progress", "maxProgress"].forEach(function(aProp) {
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
var UpdateChecker = function(aAddon, aListener, aReason, aAppVersion, aPlatformVersion) {
  if (!aListener || !aReason)
    throw Cr.NS_ERROR_INVALID_ARG;

  ChromeUtils.import("resource://gre/modules/addons/AddonUpdateChecker.jsm");

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
  this._parser = AddonUpdateChecker.checkForUpdates(aAddon.id, url, this);
};

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
  async onUpdateCheckComplete(aUpdates) {
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
               !this.addon.strictCompatibility) {
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
                          await AddonRepository.getCompatibilityOverrides(this.addon.id);

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

var XPIInstall = {
  flushChromeCaches,
  flushJarCache,
  recursiveRemove,
};
