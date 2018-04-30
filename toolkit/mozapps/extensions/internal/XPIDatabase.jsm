/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This file contains most of the logic required to maintain the
 * extensions database, including querying and modifying extension
 * metadata. In general, we try to avoid loading it during startup when
 * at all possible. Please keep that in mind when deciding whether to
 * add code here or elsewhere.
 */

/* eslint "valid-jsdoc": [2, {requireReturn: false, requireReturnDescription: false, prefer: {return: "returns"}}] */

var EXPORTED_SYMBOLS = ["AddonInternal", "XPIDatabase", "XPIDatabaseReconcile"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.jsm",
  AddonRepository: "resource://gre/modules/addons/AddonRepository.jsm",
  AddonSettings: "resource://gre/modules/addons/AddonSettings.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  Services: "resource://gre/modules/Services.jsm",

  Blocklist: "resource://gre/modules/Blocklist.jsm",
  UpdateChecker: "resource://gre/modules/addons/XPIInstall.jsm",
  XPIInstall: "resource://gre/modules/addons/XPIInstall.jsm",
  XPIInternal: "resource://gre/modules/addons/XPIProvider.jsm",
});

const {nsIBlocklistService} = Ci;

// These are injected from XPIProvider.jsm
/* globals
 *         BOOTSTRAP_REASONS,
 *         DB_SCHEMA,
 *         SIGNED_TYPES,
 *         XPIProvider,
 *         XPIStates,
 *         descriptorToPath,
 *         isTheme,
 *         isWebExtension,
 *         recordAddonTelemetry,
 */

for (let sym of [
  "BOOTSTRAP_REASONS",
  "DB_SCHEMA",
  "SIGNED_TYPES",
  "XPIProvider",
  "XPIStates",
  "descriptorToPath",
  "isTheme",
  "isWebExtension",
  "recordAddonTelemetry",
]) {
  XPCOMUtils.defineLazyGetter(this, sym, () => XPIInternal[sym]);
}

ChromeUtils.import("resource://gre/modules/Log.jsm");
const LOGGER_ID = "addons.xpi-utils";

const nsIFile = Components.Constructor("@mozilla.org/file/local;1", "nsIFile",
                                       "initWithPath");

// Create a new logger for use by the Addons XPI Provider Utils
// (Requires AddonManager.jsm)
var logger = Log.repository.getLogger(LOGGER_ID);

const KEY_PROFILEDIR                  = "ProfD";
const FILE_JSON_DB                    = "extensions.json";

// The last version of DB_SCHEMA implemented in SQLITE
const LAST_SQLITE_DB_SCHEMA           = 14;

const PREF_BLOCKLIST_ITEM_URL         = "extensions.blocklist.itemURL";
const PREF_DB_SCHEMA                  = "extensions.databaseSchema";
const PREF_EM_AUTO_DISABLED_SCOPES    = "extensions.autoDisableScopes";
const PREF_EM_EXTENSION_FORMAT        = "extensions.";
const PREF_PENDING_OPERATIONS         = "extensions.pendingOperations";
const PREF_XPI_SIGNATURES_DEV_ROOT    = "xpinstall.signatures.dev-root";

const TOOLKIT_ID                      = "toolkit@mozilla.org";

const KEY_APP_SYSTEM_ADDONS           = "app-system-addons";
const KEY_APP_SYSTEM_DEFAULTS         = "app-system-defaults";
const KEY_APP_SYSTEM_LOCAL            = "app-system-local";
const KEY_APP_SYSTEM_SHARE            = "app-system-share";
const KEY_APP_GLOBAL                  = "app-global";
const KEY_APP_PROFILE                 = "app-profile";
const KEY_APP_TEMPORARY               = "app-temporary";

// Properties to cache and reload when an addon installation is pending
const PENDING_INSTALL_METADATA =
    ["syncGUID", "targetApplications", "userDisabled", "softDisabled",
     "existingAddonID", "sourceURI", "releaseNotesURI", "installDate",
     "updateDate", "applyBackgroundUpdates", "compatibilityOverrides"];

const COMPATIBLE_BY_DEFAULT_TYPES = {
  extension: true,
  dictionary: true
};

// Properties that exist in the install manifest
const PROP_LOCALE_SINGLE = ["name", "description", "creator", "homepageURL"];
const PROP_LOCALE_MULTI  = ["developers", "translators", "contributors"];

// Properties to save in JSON file
const PROP_JSON_FIELDS = ["id", "syncGUID", "location", "version", "type",
                          "updateURL", "optionsURL",
                          "optionsType", "optionsBrowserStyle", "aboutURL",
                          "defaultLocale", "visible", "active", "userDisabled",
                          "appDisabled", "pendingUninstall", "installDate",
                          "updateDate", "applyBackgroundUpdates", "bootstrap", "path",
                          "skinnable", "size", "sourceURI", "releaseNotesURI",
                          "softDisabled", "foreignInstall",
                          "strictCompatibility", "locales", "targetApplications",
                          "targetPlatforms", "signedState",
                          "seen", "dependencies", "hasEmbeddedWebExtension",
                          "userPermissions", "icons", "iconURL", "icon64URL",
                          "blocklistState", "blocklistURL", "startupData"];

const LEGACY_TYPES = new Set([
  "extension",
]);

// Time to wait before async save of XPI JSON database, in milliseconds
const ASYNC_SAVE_DELAY_MS = 20;

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
 * Asynchronously fill in the _repositoryAddon field for one addon
 *
 * @param {AddonInternal} aAddon
 *        The add-on to annotate.
 * @returns {AddonInternal}
 *        The annotated add-on.
 */
async function getRepositoryAddon(aAddon) {
  if (aAddon) {
    aAddon._repositoryAddon = await AddonRepository.getCachedAddonByID(aAddon.id);
  }
  return aAddon;
}

/**
 * Copies properties from one object to another. If no target object is passed
 * a new object will be created and returned.
 *
 * @param {object} aObject
 *        An object to copy from
 * @param {string[]} aProperties
 *        An array of properties to be copied
 * @param {object?} [aTarget]
 *        An optional target object to copy the properties to
 * @returns {Object}
 *        The object that the properties were copied onto
 */
function copyProperties(aObject, aProperties, aTarget) {
  if (!aTarget)
    aTarget = {};
  aProperties.forEach(function(aProp) {
    if (aProp in aObject)
      aTarget[aProp] = aObject[aProp];
  });
  return aTarget;
}

// Maps instances of AddonInternal to AddonWrapper
const wrapperMap = new WeakMap();
let addonFor = wrapper => wrapperMap.get(wrapper);

const EMPTY_ARRAY = Object.freeze([]);

let AddonWrapper;

/**
 * The AddonInternal is an internal only representation of add-ons. It may
 * have come from the database (see DBAddonInternal in XPIDatabase.jsm)
 * or an install manifest.
 */
class AddonInternal {
  constructor() {
    this._hasResourceCache = new Map();

    this._wrapper = null;
    this._selectedLocale = null;
    this.active = false;
    this.visible = false;
    this.userDisabled = false;
    this.appDisabled = false;
    this.softDisabled = false;
    this.blocklistState = Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
    this.blocklistURL = null;
    this.sourceURI = null;
    this.releaseNotesURI = null;
    this.foreignInstall = false;
    this.seen = true;
    this.skinnable = false;
    this.startupData = null;

    /**
     * @property {Array<string>} dependencies
     *   An array of bootstrapped add-on IDs on which this add-on depends.
     *   The add-on will remain appDisabled if any of the dependent
     *   add-ons is not installed and enabled.
     */
    this.dependencies = EMPTY_ARRAY;
    this.hasEmbeddedWebExtension = false;
  }

  get wrapper() {
    if (!this._wrapper) {
      this._wrapper = new AddonWrapper(this);
    }
    return this._wrapper;
  }

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
  }

  get providesUpdatesSecurely() {
    return !this.updateURL || this.updateURL.startsWith("https:");
  }

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
  }

  get unpack() {
    return this.type === "dictionary";
  }

  get isCompatible() {
    return this.isCompatibleWith();
  }

  get disabled() {
    return (this.userDisabled || this.appDisabled || this.softDisabled);
  }

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
  }

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
  }

  get matchingTargetApplication() {
    let app = null;
    for (let targetApp of this.targetApplications) {
      if (targetApp.id == Services.appinfo.ID)
        return targetApp;
      if (targetApp.id == TOOLKIT_ID)
        app = targetApp;
    }
    return app;
  }

  async findBlocklistEntry() {
    let staticItem = findMatchingStaticBlocklistItem(this);
    if (staticItem) {
      let url = Services.urlFormatter.formatURLPref(PREF_BLOCKLIST_ITEM_URL);
      return {
        state: staticItem.level,
        url: url.replace(/%blockID%/g, staticItem.blockID)
      };
    }

    return Blocklist.getAddonBlocklistEntry(this.wrapper);
  }

  async updateBlocklistState(options = {}) {
    let {applySoftBlock = true, oldAddon = null, updateDatabase = true} = options;

    if (oldAddon) {
      this.userDisabled = oldAddon.userDisabled;
      this.softDisabled = oldAddon.softDisabled;
      this.blocklistState = oldAddon.blocklistState;
    }
    let oldState = this.blocklistState;

    let entry = await this.findBlocklistEntry();
    let newState = entry ? entry.state : Services.blocklist.STATE_NOT_BLOCKED;

    this.blocklistState = newState;
    this.blocklistURL = entry && entry.url;

    let userDisabled, softDisabled;
    // After a blocklist update, the blocklist service manually applies
    // new soft blocks after displaying a UI, in which cases we need to
    // skip updating it here.
    if (applySoftBlock && oldState != newState) {
      if (newState == Services.blocklist.STATE_SOFTBLOCKED) {
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
      XPIDatabase.updateAddonDisabledState(this, userDisabled, softDisabled);
      XPIDatabase.saveChanges();
    } else {
      this.appDisabled = !XPIDatabase.isUsableAddon(this);
      if (userDisabled !== undefined) {
        this.userDisabled = userDisabled;
      }
      if (softDisabled !== undefined) {
        this.softDisabled = softDisabled;
      }
    }
  }

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
    this.appDisabled = !XPIDatabase.isUsableAddon(this);
  }

  /**
   * toJSON is called by JSON.stringify in order to create a filtered version
   * of this object to be serialized to a JSON file. A new object is returned
   * with copies of all non-private properties. Functions, getters and setters
   * are not copied.
   *
   * @returns {Object}
   *       An object containing copies of the properties of this object
   *       ignoring private properties, functions, getters and setters.
   */
  toJSON() {
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
  }

  /**
   * When an add-on install is pending its metadata will be cached in a file.
   * This method reads particular properties of that metadata that may be newer
   * than that in the install manifest, like compatibility information.
   *
   * @param {Object} aObj
   *        A JS object containing the cached metadata
   */
  importMetadata(aObj) {
    for (let prop of PENDING_INSTALL_METADATA) {
      if (!(prop in aObj))
        continue;

      this[prop] = aObj[prop];
    }

    // Compatibility info may have changed so update appDisabled
    this.appDisabled = !XPIDatabase.isUsableAddon(this);
  }

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
      // System add-on upgrades are triggered through a different mechanism (see updateSystemAddons())
      let isSystem = this._installLocation.isSystem;
      // Add-ons that are installed by a file link cannot be upgraded.
      if (!this._installLocation.isLinkedAddon(this.id) && !isSystem) {
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
  }
}

/**
 * The AddonWrapper wraps an Addon to provide the data visible to consumers of
 * the public API.
 *
 * @param {AddonInternal} aAddon
 *        The add-on object to wrap.
 */
AddonWrapper = class {
  constructor(aAddon) {
    wrapperMap.set(this, aAddon);
  }

  get __AddonInternal__() {
    return AppConstants.DEBUG ? addonFor(this) : undefined;
  }

  get seen() {
    return addonFor(this).seen;
  }

  get hasEmbeddedWebExtension() {
    return addonFor(this).hasEmbeddedWebExtension;
  }

  markAsSeen() {
    addonFor(this).seen = true;
    XPIDatabase.saveChanges();
  }

  get type() {
    return XPIInternal.getExternalType(addonFor(this).type);
  }

  get isWebExtension() {
    return isWebExtension(addonFor(this).type);
  }

  get temporarilyInstalled() {
    return addonFor(this)._installLocation == XPIInternal.TemporaryInstallLocation;
  }

  get aboutURL() {
    return this.isActive ? addonFor(this).aboutURL : null;
  }

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
  }

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
  }

  get optionsBrowserStyle() {
    let addon = addonFor(this);
    return addon.optionsBrowserStyle;
  }

  async getBlocklistURL() {
    return addonFor(this).blocklistURL;
  }

  get iconURL() {
    return AddonManager.getPreferredIconURL(this, 48);
  }

  get icon64URL() {
    return AddonManager.getPreferredIconURL(this, 64);
  }

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

    let canUseIconURLs = this.isActive;
    if (canUseIconURLs && addon.iconURL) {
      icons[32] = addon.iconURL;
      icons[48] = addon.iconURL;
    }

    if (canUseIconURLs && addon.icon64URL) {
      icons[64] = addon.icon64URL;
    }

    Object.freeze(icons);
    return icons;
  }

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
  }

  get applyBackgroundUpdates() {
    return addonFor(this).applyBackgroundUpdates;
  }
  set applyBackgroundUpdates(val) {
    let addon = addonFor(this);
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
  }

  set syncGUID(val) {
    let addon = addonFor(this);
    if (addon.syncGUID == val)
      return val;

    if (addon.inDatabase)
      XPIDatabase.setAddonSyncGUID(addon, val);

    addon.syncGUID = val;

    return val;
  }

  get install() {
    let addon = addonFor(this);
    if (!("_install" in addon) || !addon._install)
      return null;
    return addon._install.wrapper;
  }

  get pendingUpgrade() {
    let addon = addonFor(this);
    return addon.pendingUpgrade ? addon.pendingUpgrade.wrapper : null;
  }

  get scope() {
    let addon = addonFor(this);
    if (addon._installLocation)
      return addon._installLocation.scope;

    return AddonManager.SCOPE_PROFILE;
  }

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
  }

  get operationsRequiringRestart() {
    return 0;
  }

  get isDebuggable() {
    return this.isActive && addonFor(this).bootstrap;
  }

  get permissions() {
    return addonFor(this).permissions();
  }

  get isActive() {
    let addon = addonFor(this);
    if (!addon.active)
      return false;
    if (!Services.appinfo.inSafeMode)
      return true;
    return addon.bootstrap && XPIInternal.canRunInSafeMode(addon);
  }

  get startupPromise() {
    let addon = addonFor(this);
    if (!addon.bootstrap || !this.isActive)
      return null;

    let activeAddon = XPIProvider.activeAddons.get(addon.id);
    if (activeAddon)
      return activeAddon.startupPromise || null;
    return null;
  }

  updateBlocklistState(applySoftBlock = true) {
    return addonFor(this).updateBlocklistState({applySoftBlock});
  }

  get userDisabled() {
    let addon = addonFor(this);
    return addon.softDisabled || addon.userDisabled;
  }
  set userDisabled(val) {
    let addon = addonFor(this);
    if (val == this.userDisabled) {
      return val;
    }

    if (addon.inDatabase) {
      // hidden and system add-ons should not be user disabled,
      // as there is no UI to re-enable them.
      if (this.hidden) {
        throw new Error(`Cannot disable hidden add-on ${addon.id}`);
      }
      XPIDatabase.updateAddonDisabledState(addon, val);
    } else {
      addon.userDisabled = val;
      // When enabling remove the softDisabled flag
      if (!val)
        addon.softDisabled = false;
    }

    return val;
  }

  set softDisabled(val) {
    let addon = addonFor(this);
    if (val == addon.softDisabled)
      return val;

    if (addon.inDatabase) {
      // When softDisabling a theme just enable the active theme
      if (isTheme(addon.type) && val && !addon.userDisabled) {
        if (isWebExtension(addon.type))
          XPIDatabase.updateAddonDisabledState(addon, undefined, val);
      } else {
        XPIDatabase.updateAddonDisabledState(addon, undefined, val);
      }
    } else if (!addon.userDisabled) {
      // Only set softDisabled if not already disabled
      addon.softDisabled = val;
    }

    return val;
  }

  get hidden() {
    let addon = addonFor(this);
    if (addon._installLocation.name == KEY_APP_TEMPORARY)
      return false;

    return addon._installLocation.isSystem;
  }

  get isSystem() {
    let addon = addonFor(this);
    return addon._installLocation.isSystem;
  }

  // Returns true if Firefox Sync should sync this addon. Only addons
  // in the profile install location are considered syncable.
  get isSyncable() {
    let addon = addonFor(this);
    return (addon._installLocation.name == KEY_APP_PROFILE);
  }

  get userPermissions() {
    return addonFor(this).userPermissions;
  }

  isCompatibleWith(aAppVersion, aPlatformVersion) {
    return addonFor(this).isCompatibleWith(aAppVersion, aPlatformVersion);
  }

  uninstall(alwaysAllowUndo) {
    let addon = addonFor(this);
    XPIProvider.uninstallAddon(addon, alwaysAllowUndo);
  }

  cancelUninstall() {
    let addon = addonFor(this);
    XPIProvider.cancelUninstallAddon(addon);
  }

  findUpdates(aListener, aReason, aAppVersion, aPlatformVersion) {
    new UpdateChecker(addonFor(this), aListener, aReason, aAppVersion, aPlatformVersion);
  }

  // Returns true if there was an update in progress, false if there was no update to cancel
  cancelUpdate() {
    let addon = addonFor(this);
    if (addon._updateCheck) {
      addon._updateCheck.cancel();
      return true;
    }
    return false;
  }

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
  }

  /**
   * Reloads the add-on.
   *
   * For temporarily installed add-ons, this uninstalls and re-installs the
   * add-on. Otherwise, the addon is disabled and then re-enabled, and the cache
   * is flushed.
   *
   * @returns {Promise}
   */
  reload() {
    return new Promise((resolve) => {
      const addon = addonFor(this);

      logger.debug(`reloading add-on ${addon.id}`);

      if (!this.temporarilyInstalled) {
        let addonFile = addon.getResourceURI;
        XPIDatabase.updateAddonDisabledState(addon, true);
        Services.obs.notifyObservers(addonFile, "flush-cache-entry");
        XPIDatabase.updateAddonDisabledState(addon, false);
        resolve();
      } else {
        // This function supports re-installing an existing add-on.
        resolve(AddonManager.installTemporaryAddon(addon._sourceBundle));
      }
    });
  }

  /**
   * Returns a URI to the selected resource or to the add-on bundle if aPath
   * is null. URIs to the bundle will always be file: URIs. URIs to resources
   * will be file: URIs if the add-on is unpacked or jar: URIs if the add-on is
   * still an XPI file.
   *
   * @param {string?} aPath
   *        The path in the add-on to get the URI for or null to get a URI to
   *        the file or directory the add-on is installed as.
   * @returns {nsIURI}
   */
  getResourceURI(aPath) {
    let addon = addonFor(this);
    if (!aPath)
      return Services.io.newFileURI(addon._sourceBundle);

    return XPIInternal.getURIForResourceInFile(addon._sourceBundle, aPath);
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
 "providesUpdatesSecurely", "blocklistState", "appDisabled",
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
 * The DBAddonInternal is a special AddonInternal that has been retrieved from
 * the database. The constructor will initialize the DBAddonInternal with a set
 * of fields, which could come from either the JSON store or as an
 * XPIProvider.AddonInternal created from an addon's manifest
 * @constructor
 * @param {Object} aLoaded
 *        Addon data fields loaded from JSON or the addon manifest.
 */
class DBAddonInternal extends AddonInternal {
  constructor(aLoaded) {
    super();

    if (aLoaded.descriptor) {
      if (!aLoaded.path) {
        aLoaded.path = descriptorToPath(aLoaded.descriptor);
      }
      delete aLoaded.descriptor;
    }

    copyProperties(aLoaded, PROP_JSON_FIELDS, this);

    if (!this.dependencies)
      this.dependencies = [];
    Object.freeze(this.dependencies);

    if (aLoaded._installLocation) {
      this._installLocation = aLoaded._installLocation;
      this.location = aLoaded._installLocation.name;
    } else if (aLoaded.location) {
      this._installLocation = XPIProvider.installLocationsByName[this.location];
    }

    this._key = this.location + ":" + this.id;

    if (!aLoaded._sourceBundle) {
      throw new Error("Expected passed argument to contain a path");
    }

    this._sourceBundle = aLoaded._sourceBundle;
  }

  applyCompatibilityUpdate(aUpdate, aSyncCompatibility) {
    let wasCompatible = this.isCompatible;

    this.targetApplications.forEach(function(aTargetApp) {
      aUpdate.targetApplications.forEach(function(aUpdateTarget) {
        if (aTargetApp.id == aUpdateTarget.id && (aSyncCompatibility ||
            Services.vc.compare(aTargetApp.maxVersion, aUpdateTarget.maxVersion) < 0)) {
          aTargetApp.minVersion = aUpdateTarget.minVersion;
          aTargetApp.maxVersion = aUpdateTarget.maxVersion;
          XPIDatabase.saveChanges();
        }
      });
    });

    if (wasCompatible != this.isCompatible)
      XPIDatabase.updateAddonDisabledState(this);
  }

  toJSON() {
    return copyProperties(this, PROP_JSON_FIELDS);
  }

  get inDatabase() {
    return true;
  }
}

/**
 * @typedef {Map<string, DBAddonInternal>} AddonDB
 */

/**
 * Internal interface: find an addon from an already loaded addonDB.
 *
 * @param {AddonDB} addonDB
 *        The add-on database.
 * @param {function(DBAddonInternal) : boolean} aFilter
 *        The filter predecate. The first add-on for which it returns
 *        true will be returned.
 * @returns {DBAddonInternal?}
 *        The first matching add-on, if one is found.
 */
function _findAddon(addonDB, aFilter) {
  for (let addon of addonDB.values()) {
    if (aFilter(addon)) {
      return addon;
    }
  }
  return null;
}

/**
 * Internal interface to get a filtered list of addons from a loaded addonDB
 *
 * @param {AddonDB} addonDB
 *        The add-on database.
 * @param {function(DBAddonInternal) : boolean} aFilter
 *        The filter predecate. Add-ons which match this predicate will
 *        be returned.
 * @returns {Array<DBAddonInternal>}
 *        The list of matching add-ons.
 */
function _filterDB(addonDB, aFilter) {
  return Array.from(addonDB.values()).filter(aFilter);
}

this.XPIDatabase = {
  // true if the database connection has been opened
  initialized: false,
  // The database file
  jsonFile: FileUtils.getFile(KEY_PROFILEDIR, [FILE_JSON_DB], true),
  // Migration data loaded from an old version of the database.
  migrateData: null,
  // Active add-on directories loaded from extensions.ini and prefs at startup.
  activeBundles: null,

  _saveTask: null,

  // Saved error object if we fail to read an existing database
  _loadError: null,

  // Saved error object if we fail to save the database
  _saveError: null,

  // Error reported by our most recent attempt to read or write the database, if any
  get lastError() {
    if (this._loadError)
      return this._loadError;
    if (this._saveError)
      return this._saveError;
    return null;
  },

  async _saveNow() {
    try {
      let json = JSON.stringify(this);
      let path = this.jsonFile.path;
      await OS.File.writeAtomic(path, json, {tmpPath: `${path}.tmp`});

      if (!this._schemaVersionSet) {
        // Update the XPIDB schema version preference the first time we
        // successfully save the database.
        logger.debug("XPI Database saved, setting schema version preference to " + DB_SCHEMA);
        Services.prefs.setIntPref(PREF_DB_SCHEMA, DB_SCHEMA);
        this._schemaVersionSet = true;

        // Reading the DB worked once, so we don't need the load error
        this._loadError = null;
      }
    } catch (error) {
      logger.warn("Failed to save XPI database", error);
      this._saveError = error;
      throw error;
    }
  },

  /**
   * Mark the current stored data dirty, and schedule a flush to disk
   */
  saveChanges() {
    if (!this.initialized) {
      throw new Error("Attempt to use XPI database when it is not initialized");
    }

    if (XPIProvider._closing) {
      // use an Error here so we get a stack trace.
      let err = new Error("XPI database modified after shutdown began");
      logger.warn(err);
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_late_stack", Log.stackTrace(err));
    }

    if (!this._saveTask) {
      this._saveTask = new DeferredTask(() => this._saveNow(),
                                        ASYNC_SAVE_DELAY_MS);
    }

    this._saveTask.arm();
  },

  async finalize() {
    // handle the "in memory only" and "saveChanges never called" cases
    if (!this._saveTask) {
      return;
    }

    await this._saveTask.finalize();
  },

  /**
   * Converts the current internal state of the XPI addon database to
   * a JSON.stringify()-ready structure
   *
   * @returns {Object}
   */
  toJSON() {
    if (!this.addonDB) {
      // We never loaded the database?
      throw new Error("Attempt to save database without loading it first");
    }

    let toSave = {
      schemaVersion: DB_SCHEMA,
      addons: Array.from(this.addonDB.values())
                   .filter(addon => addon.location != KEY_APP_TEMPORARY),
    };
    return toSave;
  },

  /**
   * Synchronously opens and reads the database file, upgrading from old
   * databases or making a new DB if needed.
   *
   * The possibilities, in order of priority, are:
   * 1) Perfectly good, up to date database
   * 2) Out of date JSON database needs to be upgraded => upgrade
   * 3) JSON database exists but is mangled somehow => build new JSON
   * 4) no JSON DB, but a usable SQLITE db we can upgrade from => upgrade
   * 5) useless SQLITE DB => build new JSON
   * 6) usable RDF DB => upgrade
   * 7) useless RDF DB => build new JSON
   * 8) Nothing at all => build new JSON
   *
   * @param {boolean} aRebuildOnError
   *        A boolean indicating whether add-on information should be loaded
   *        from the install locations if the database needs to be rebuilt.
   *        (if false, caller is XPIProvider.checkForChanges() which will rebuild)
   */
  syncLoadDB(aRebuildOnError) {
    this.migrateData = null;
    let fstream = null;
    let data = "";
    try {
      let readTimer = AddonManagerPrivate.simpleTimer("XPIDB_syncRead_MS");
      logger.debug("Opening XPI database " + this.jsonFile.path);
      fstream = Cc["@mozilla.org/network/file-input-stream;1"].
              createInstance(Ci.nsIFileInputStream);
      fstream.init(this.jsonFile, -1, 0, 0);
      let cstream = null;
      try {
        cstream = Cc["@mozilla.org/intl/converter-input-stream;1"].
                createInstance(Ci.nsIConverterInputStream);
        cstream.init(fstream, "UTF-8", 0, 0);

        let str = {};
        let read = 0;
        do {
          read = cstream.readString(0xffffffff, str); // read as much as we can and put it in str.value
          data += str.value;
        } while (read != 0);

        readTimer.done();
        this.parseDB(data, aRebuildOnError);
      } catch (e) {
        logger.error("Failed to load XPI JSON data from profile", e);
        let rebuildTimer = AddonManagerPrivate.simpleTimer("XPIDB_rebuildReadFailed_MS");
        this.rebuildDatabase(aRebuildOnError);
        rebuildTimer.done();
      } finally {
        if (cstream)
          cstream.close();
      }
    } catch (e) {
      if (e.result === Cr.NS_ERROR_FILE_NOT_FOUND) {
        this.upgradeDB(aRebuildOnError);
      } else {
        this.rebuildUnreadableDB(e, aRebuildOnError);
      }
    } finally {
      if (fstream)
        fstream.close();
    }
    // If an async load was also in progress, record in telemetry.
    if (this._dbPromise) {
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_overlapped_load", 1);
    }
    this._dbPromise = Promise.resolve(this.addonDB);
    Services.obs.notifyObservers(this.addonDB, "xpi-database-loaded");
  },

  /**
   * Parse loaded data, reconstructing the database if the loaded data is not valid
   *
   * @param {string} aData
   *        The stringified add-on JSON to parse.
   * @param {boolean} aRebuildOnError
   *        If true, synchronously reconstruct the database from installed add-ons
   */
  parseDB(aData, aRebuildOnError) {
    let parseTimer = AddonManagerPrivate.simpleTimer("XPIDB_parseDB_MS");
    try {
      let inputAddons = JSON.parse(aData);
      // Now do some sanity checks on our JSON db
      if (!("schemaVersion" in inputAddons) || !("addons" in inputAddons)) {
        parseTimer.done();
        // Content of JSON file is bad, need to rebuild from scratch
        logger.error("bad JSON file contents");
        AddonManagerPrivate.recordSimpleMeasure("XPIDB_startupError", "badJSON");
        let rebuildTimer = AddonManagerPrivate.simpleTimer("XPIDB_rebuildBadJSON_MS");
        this.rebuildDatabase(aRebuildOnError);
        rebuildTimer.done();
        return;
      }
      if (inputAddons.schemaVersion != DB_SCHEMA) {
        // Handle mismatched JSON schema version. For now, we assume
        // compatibility for JSON data, though we throw away any fields we
        // don't know about (bug 902956)
        AddonManagerPrivate.recordSimpleMeasure("XPIDB_startupError",
                                                "schemaMismatch-" + inputAddons.schemaVersion);
        logger.debug("JSON schema mismatch: expected " + DB_SCHEMA +
            ", actual " + inputAddons.schemaVersion);
        // When we rev the schema of the JSON database, we need to make sure we
        // force the DB to save so that the DB_SCHEMA value in the JSON file and
        // the preference are updated.
      }
      // If we got here, we probably have good data
      // Make AddonInternal instances from the loaded data and save them
      let addonDB = new Map();
      for (let loadedAddon of inputAddons.addons) {
        try {
          if (!loadedAddon.path) {
            loadedAddon.path = descriptorToPath(loadedAddon.descriptor);
          }
          loadedAddon._sourceBundle = new nsIFile(loadedAddon.path);
        } catch (e) {
          // We can fail here when the path is invalid, usually from the
          // wrong OS
          logger.warn("Could not find source bundle for add-on " + loadedAddon.id, e);
        }

        let newAddon = new DBAddonInternal(loadedAddon);
        addonDB.set(newAddon._key, newAddon);
      }
      parseTimer.done();
      this.addonDB = addonDB;
      logger.debug("Successfully read XPI database");
      this.initialized = true;
    } catch (e) {
      // If we catch and log a SyntaxError from the JSON
      // parser, the xpcshell test harness fails the test for us: bug 870828
      parseTimer.done();
      if (e.name == "SyntaxError") {
        logger.error("Syntax error parsing saved XPI JSON data");
        AddonManagerPrivate.recordSimpleMeasure("XPIDB_startupError", "syntax");
      } else {
        logger.error("Failed to load XPI JSON data from profile", e);
        AddonManagerPrivate.recordSimpleMeasure("XPIDB_startupError", "other");
      }
      let rebuildTimer = AddonManagerPrivate.simpleTimer("XPIDB_rebuildReadFailed_MS");
      this.rebuildDatabase(aRebuildOnError);
      rebuildTimer.done();
    }
  },

  /**
   * Upgrade database from earlier (sqlite or RDF) version if available
   *
   * @param {boolean} aRebuildOnError
   *        If true, synchronously reconstruct the database from installed add-ons
   */
  upgradeDB(aRebuildOnError) {
    let upgradeTimer = AddonManagerPrivate.simpleTimer("XPIDB_upgradeDB_MS");

    let schemaVersion = Services.prefs.getIntPref(PREF_DB_SCHEMA, 0);
    if (schemaVersion > LAST_SQLITE_DB_SCHEMA) {
      // we've upgraded before but the JSON file is gone, fall through
      // and rebuild from scratch
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_startupError", "dbMissing");
    }

    this.rebuildDatabase(aRebuildOnError);
    upgradeTimer.done();
  },

  /**
   * Reconstruct when the DB file exists but is unreadable
   * (for example because read permission is denied)
   *
   * @param {Error} aError
   *        The error that triggered the rebuild.
   * @param {boolean} aRebuildOnError
   *        If true, synchronously reconstruct the database from installed add-ons
   */
  rebuildUnreadableDB(aError, aRebuildOnError) {
    let rebuildTimer = AddonManagerPrivate.simpleTimer("XPIDB_rebuildUnreadableDB_MS");
    logger.warn("Extensions database " + this.jsonFile.path +
        " exists but is not readable; rebuilding", aError);
    // Remember the error message until we try and write at least once, so
    // we know at shutdown time that there was a problem
    this._loadError = aError;
    AddonManagerPrivate.recordSimpleMeasure("XPIDB_startupError", "unreadable");
    this.rebuildDatabase(aRebuildOnError);
    rebuildTimer.done();
  },

  /**
   * Open and read the XPI database asynchronously, upgrading if
   * necessary. If any DB load operation fails, we need to
   * synchronously rebuild the DB from the installed extensions.
   *
   * @returns {Promise<AddonDB>}
   *        Resolves to the Map of loaded JSON data stored in
   *        this.addonDB; never rejects.
   */
  asyncLoadDB() {
    // Already started (and possibly finished) loading
    if (this._dbPromise) {
      return this._dbPromise;
    }

    logger.debug("Starting async load of XPI database " + this.jsonFile.path);
    AddonManagerPrivate.recordSimpleMeasure("XPIDB_async_load", XPIProvider.runPhase);
    let readOptions = {
      outExecutionDuration: 0
    };
    this._dbPromise = OS.File.read(this.jsonFile.path, null, readOptions).then(
      byteArray => {
        logger.debug("Async JSON file read took " + readOptions.outExecutionDuration + " MS");
        AddonManagerPrivate.recordSimpleMeasure("XPIDB_asyncRead_MS",
          readOptions.outExecutionDuration);

        if (this.addonDB) {
          logger.debug("Synchronous load completed while waiting for async load");
          return this.addonDB;
        }
        logger.debug("Finished async read of XPI database, parsing...");
        let decodeTimer = AddonManagerPrivate.simpleTimer("XPIDB_decode_MS");
        let decoder = new TextDecoder();
        let data = decoder.decode(byteArray);
        decodeTimer.done();
        this.parseDB(data, true);
        return this.addonDB;
      })
    .catch(
      error => {
        if (this.addonDB) {
          logger.debug("Synchronous load completed while waiting for async load");
          return this.addonDB;
        }
        if (error.becauseNoSuchFile) {
          this.upgradeDB(true);
        } else {
          // it's there but unreadable
          this.rebuildUnreadableDB(error, true);
        }
        return this.addonDB;
      });

    this._dbPromise.then(() => {
      Services.obs.notifyObservers(this.addonDB, "xpi-database-loaded");
    });

    return this._dbPromise;
  },

  /**
   * Rebuild the database from addon install directories. If this.migrateData
   * is available, uses migrated information for settings on the addons found
   * during rebuild
   *
   * @param {boolean} aRebuildOnError
   *        A boolean indicating whether add-on information should be loaded
   *        from the install locations if the database needs to be rebuilt.
   *        (if false, caller is XPIProvider.checkForChanges() which will rebuild)
   */
  rebuildDatabase(aRebuildOnError) {
    this.addonDB = new Map();
    this.initialized = true;

    if (XPIStates.size == 0) {
      // No extensions installed, so we're done
      logger.debug("Rebuilding XPI database with no extensions");
      return;
    }

    // If there is no migration data then load the list of add-on directories
    // that were active during the last run
    if (!this.migrateData) {
      this.activeBundles = Array.from(XPIStates.initialEnabledAddons(),
                                      addon => addon.path);
      if (!this.activeBundles.length)
        this.activeBundles = null;
    }


    if (aRebuildOnError) {
      logger.warn("Rebuilding add-ons database from installed extensions.");
      try {
        XPIDatabaseReconcile.processFileChanges({}, false);
      } catch (e) {
        logger.error("Failed to rebuild XPI database from installed extensions", e);
      }
      // Make sure to update the active add-ons and add-ons list on shutdown
      Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);
    }
  },

  /**
   * Shuts down the database connection and releases all cached objects.
   * Return: Promise{integer} resolves / rejects with the result of the DB
   *                          flush after the database is flushed and
   *                          all cleanup is done
   */
  async shutdown() {
    logger.debug("shutdown");
    if (this.initialized) {
      // If our last database I/O had an error, try one last time to save.
      if (this.lastError)
        this.saveChanges();

      this.initialized = false;

      // If we're shutting down while still loading, finish loading
      // before everything else!
      if (this._dbPromise) {
        await this._dbPromise;
      }

      // Await any pending DB writes and finish cleaning up.
      await this.finalize();

      if (this._saveError) {
        // If our last attempt to read or write the DB failed, force a new
        // extensions.ini to be written to disk on the next startup
        Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);
      }

      // Clear out the cached addons data loaded from JSON
      delete this.addonDB;
      delete this._dbPromise;
      // same for the deferred save
      delete this._saveTask;
      // re-enable the schema version setter
      delete this._schemaVersionSet;
    }
  },

  /**
   * Asynchronously list all addons that match the filter function
   *
   * @param {function(DBAddonInternal) : boolean} aFilter
   *        Function that takes an addon instance and returns
   *        true if that addon should be included in the selected array
   *
   * @returns {Array<DBAddonInternal>}
   *        A Promise that resolves to the list of add-ons matching
   *        aFilter or an empty array if none match
   */
  async getAddonList(aFilter) {
    try {
      let addonDB = await this.asyncLoadDB();
      let addonList = _filterDB(addonDB, aFilter);
      let addons = await Promise.all(addonList.map(addon => getRepositoryAddon(addon)));
      return addons;
    } catch (error) {
      logger.error("getAddonList failed", error);
      return [];
    }
  },

  /**
   * Get the first addon that matches the filter function
   *
   * @param {function(DBAddonInternal) : boolean} aFilter
   *        Function that takes an addon instance and returns
   *        true if that addon should be selected
   * @returns {Promise<DBAddonInternal?>}
   */
  getAddon(aFilter) {
    return this.asyncLoadDB()
      .then(addonDB => getRepositoryAddon(_findAddon(addonDB, aFilter)))
      .catch(
        error => {
          logger.error("getAddon failed", error);
        });
  },

  syncGetAddon(aFilter) {
    return _findAddon(this.addonDB, aFilter);
  },

  /**
   * Asynchronously gets an add-on with a particular ID in a particular
   * install location.
   *
   * @param {string} aId
   *        The ID of the add-on to retrieve
   * @param {string} aLocation
   *        The name of the install location
   * @returns {Promise<DBAddonInternal?>}
   */
  getAddonInLocation(aId, aLocation) {
    return this.asyncLoadDB().then(
        addonDB => getRepositoryAddon(addonDB.get(aLocation + ":" + aId)));
  },

  /**
   * Asynchronously get all the add-ons in a particular install location.
   *
   * @param {string} aLocation
   *        The name of the install location
   * @returns {Promise<Array<DBAddonInternal>>}
   */
  getAddonsInLocation(aLocation) {
    return this.getAddonList(aAddon => aAddon._installLocation.name == aLocation);
  },

  /**
   * Asynchronously gets the add-on with the specified ID that is visible.
   *
   * @param {string} aId
   *        The ID of the add-on to retrieve
   * @returns {Promise<DBAddonInternal?>}
   */
  getVisibleAddonForID(aId) {
    return this.getAddon(aAddon => ((aAddon.id == aId) && aAddon.visible));
  },

  syncGetVisibleAddonForID(aId) {
    return this.syncGetAddon(aAddon => ((aAddon.id == aId) && aAddon.visible));
  },

  /**
   * Asynchronously gets the visible add-ons, optionally restricting by type.
   *
   * @param {Array<string>?} aTypes
   *        An array of types to include or null to include all types
   * @returns {Promise<Array<DBAddonInternal>>}
   */
  getVisibleAddons(aTypes) {
    return this.getAddonList(aAddon => (aAddon.visible &&
                                        (!aTypes || (aTypes.length == 0) ||
                                         (aTypes.indexOf(aAddon.type) > -1))));
  },

  /**
   * Synchronously gets all add-ons of a particular type(s).
   *
   * @param {Array<string>} aTypes
   *        The type(s) of add-on to retrieve
   * @returns {Array<DBAddonInternal>}
   */
  getAddonsByType(...aTypes) {
    if (!this.addonDB) {
      // jank-tastic! Must synchronously load DB if the theme switches from
      // an XPI theme to a lightweight theme before the DB has loaded,
      // because we're called from sync XPIProvider.addonChanged
      logger.warn(`Synchronous load of XPI database due to ` +
                  `getAddonsByType([${aTypes.join(", ")}]) ` +
                  `Stack: ${Error().stack}`);
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_lateOpen_byType", XPIProvider.runPhase);
      this.syncLoadDB(true);
    }

    return _filterDB(this.addonDB, aAddon => aTypes.includes(aAddon.type));
  },

  /**
   * Asynchronously gets all add-ons with pending operations.
   *
   * @param {Array<string>?} aTypes
   *        The types of add-ons to retrieve or null to get all types
   * @returns {Promise<Array<DBAddonInternal>>}
   */
  getVisibleAddonsWithPendingOperations(aTypes) {
    return this.getAddonList(
        aAddon => (aAddon.visible &&
                   aAddon.pendingUninstall &&
                   (!aTypes || (aTypes.length == 0) || (aTypes.indexOf(aAddon.type) > -1))));
  },

  /**
   * Asynchronously get an add-on by its Sync GUID.
   *
   * @param {string} aGUID
   *        Sync GUID of add-on to fetch
   * @returns {Promise<DBAddonInternal?>}
   */
  getAddonBySyncGUID(aGUID) {
    return this.getAddon(aAddon => aAddon.syncGUID == aGUID);
  },

  /**
   * Synchronously gets all add-ons in the database.
   * This is only called from the preference observer for the default
   * compatibility version preference, so we can return an empty list if
   * we haven't loaded the database yet.
   *
   * @returns {Array<DBAddonInternal>}
   */
  getAddons() {
    if (!this.addonDB) {
      return [];
    }
    return _filterDB(this.addonDB, aAddon => true);
  },


  /**
   * Returns true if signing is required for the given add-on type.
   *
   * @param {string} aType
   *        The add-on type to check.
   * @returns {boolean}
   */
  mustSign(aType) {
    if (!SIGNED_TYPES.has(aType))
      return false;

    if (aType == "webextension-langpack") {
      return AddonSettings.LANGPACKS_REQUIRE_SIGNING;
    }

    return AddonSettings.REQUIRE_SIGNING;
  },

  /**
   * Determine if this addon should be disabled due to being legacy
   *
   * @param {Addon} addon The addon to check
   *
   * @returns {boolean} Whether the addon should be disabled for being legacy
   */
  isDisabledLegacy(addon) {
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
  },

  /**
   * Calculates whether an add-on should be appDisabled or not.
   *
   * @param {AddonInternal} aAddon
   *        The add-on to check
   * @returns {boolean}
   *        True if the add-on should not be appDisabled
   */
  isUsableAddon(aAddon) {
    if (this.mustSign(aAddon.type) && !aAddon.isCorrectlySigned) {
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

    if (this.isDisabledLegacy(aAddon)) {
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
  },

  /**
   * Synchronously adds an AddonInternal's metadata to the database.
   *
   * @param {AddonInternal} aAddon
   *        AddonInternal to add
   * @param {string} aPath
   *        The file path of the add-on
   * @returns {DBAddonInternal}
   *        the DBAddonInternal that was added to the database
   */
  addAddonMetadata(aAddon, aPath) {
    if (!this.addonDB) {
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_lateOpen_addMetadata",
          XPIProvider.runPhase);
      this.syncLoadDB(false);
    }

    let newAddon = new DBAddonInternal(aAddon);
    newAddon.path = aPath;
    this.addonDB.set(newAddon._key, newAddon);
    if (newAddon.visible) {
      this.makeAddonVisible(newAddon);
    }

    this.saveChanges();
    return newAddon;
  },

  /**
   * Synchronously updates an add-on's metadata in the database. Currently just
   * removes and recreates.
   *
   * @param {DBAddonInternal} aOldAddon
   *        The DBAddonInternal to be replaced
   * @param {AddonInternal} aNewAddon
   *        The new AddonInternal to add
   * @param {string} aPath
   *        The file path of the add-on
   * @returns {DBAddonInternal}
   *        The DBAddonInternal that was added to the database
   */
  updateAddonMetadata(aOldAddon, aNewAddon, aPath) {
    this.removeAddonMetadata(aOldAddon);
    aNewAddon.syncGUID = aOldAddon.syncGUID;
    aNewAddon.installDate = aOldAddon.installDate;
    aNewAddon.applyBackgroundUpdates = aOldAddon.applyBackgroundUpdates;
    aNewAddon.foreignInstall = aOldAddon.foreignInstall;
    aNewAddon.seen = aOldAddon.seen;
    aNewAddon.active = (aNewAddon.visible && !aNewAddon.disabled && !aNewAddon.pendingUninstall);

    // addAddonMetadata does a saveChanges()
    return this.addAddonMetadata(aNewAddon, aPath);
  },

  /**
   * Synchronously removes an add-on from the database.
   *
   * @param {DBAddonInternal} aAddon
   *        The DBAddonInternal being removed
   */
  removeAddonMetadata(aAddon) {
    this.addonDB.delete(aAddon._key);
    this.saveChanges();
  },

  updateXPIStates(addon) {
    let xpiState = XPIStates.getAddon(addon.location, addon.id);
    if (xpiState) {
      xpiState.syncWithDB(addon);
      XPIStates.save();
    }
  },

  /**
   * Synchronously marks a DBAddonInternal as visible marking all other
   * instances with the same ID as not visible.
   *
   * @param {DBAddonInternal} aAddon
   *        The DBAddonInternal to make visible
   */
  makeAddonVisible(aAddon) {
    logger.debug("Make addon " + aAddon._key + " visible");
    for (let [, otherAddon] of this.addonDB) {
      if ((otherAddon.id == aAddon.id) && (otherAddon._key != aAddon._key)) {
        logger.debug("Hide addon " + otherAddon._key);
        otherAddon.visible = false;
        otherAddon.active = false;

        this.updateXPIStates(otherAddon);
      }
    }
    aAddon.visible = true;
    this.updateXPIStates(aAddon);
    this.saveChanges();
  },

  /**
   * Synchronously marks a given add-on ID visible in a given location,
   * instances with the same ID as not visible.
   *
   * @param {string} aId
   *        The ID of the add-on to make visible
   * @param {InstallLocation} aLocation
   *        The location in which to make the add-on visible.
   * @returns {DBAddonInternal?}
   *        The add-on instance which was marked visible, if any.
   */
  makeAddonLocationVisible(aId, aLocation) {
    logger.debug(`Make addon ${aId} visible in location ${aLocation}`);
    let result;
    for (let [, addon] of this.addonDB) {
      if (addon.id != aId) {
        continue;
      }
      if (addon.location == aLocation) {
        logger.debug("Reveal addon " + addon._key);
        addon.visible = true;
        addon.active = true;
        this.updateXPIStates(addon);
        result = addon;
      } else {
        logger.debug("Hide addon " + addon._key);
        addon.visible = false;
        addon.active = false;
        this.updateXPIStates(addon);
      }
    }
    this.saveChanges();
    return result;
  },

  /**
   * Synchronously sets properties for an add-on.
   *
   * @param {DBAddonInternal} aAddon
   *        The DBAddonInternal being updated
   * @param {Object} aProperties
   *        A dictionary of properties to set
   */
  setAddonProperties(aAddon, aProperties) {
    for (let key in aProperties) {
      aAddon[key] = aProperties[key];
    }
    this.saveChanges();
  },

  /**
   * Synchronously sets the Sync GUID for an add-on.
   * Only called when the database is already loaded.
   *
   * @param {DBAddonInternal} aAddon
   *        The DBAddonInternal being updated
   * @param {string} aGUID
   *        GUID string to set the value to
   * @throws if another addon already has the specified GUID
   */
  setAddonSyncGUID(aAddon, aGUID) {
    // Need to make sure no other addon has this GUID
    function excludeSyncGUID(otherAddon) {
      return (otherAddon._key != aAddon._key) && (otherAddon.syncGUID == aGUID);
    }
    let otherAddon = _findAddon(this.addonDB, excludeSyncGUID);
    if (otherAddon) {
      throw new Error("Addon sync GUID conflict for addon " + aAddon._key +
          ": " + otherAddon._key + " already has GUID " + aGUID);
    }
    aAddon.syncGUID = aGUID;
    this.saveChanges();
  },

  /**
   * Synchronously updates an add-on's active flag in the database.
   *
   * @param {DBAddonInternal} aAddon
   *        The DBAddonInternal to update
   * @param {boolean} aActive
   *        The new active state for the add-on.
   */
  updateAddonActive(aAddon, aActive) {
    logger.debug("Updating active state for add-on " + aAddon.id + " to " + aActive);

    aAddon.active = aActive;
    this.saveChanges();
  },

  /**
   * Synchronously calculates and updates all the active flags in the database.
   */
  updateActiveAddons() {
    if (!this.addonDB) {
      logger.warn("updateActiveAddons called when DB isn't loaded");
      // force the DB to load
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_lateOpen_updateActive",
          XPIProvider.runPhase);
      this.syncLoadDB(true);
    }
    logger.debug("Updating add-on states");
    for (let [, addon] of this.addonDB) {
      let newActive = (addon.visible && !addon.disabled && !addon.pendingUninstall);
      if (newActive != addon.active) {
        addon.active = newActive;
        this.saveChanges();
      }
    }
  },

  /**
   * Updates the disabled state for an add-on. Its appDisabled property will be
   * calculated and if the add-on is changed the database will be saved and
   * appropriate notifications will be sent out to the registered AddonListeners.
   *
   * @param {DBAddonInternal} aAddon
   *        The DBAddonInternal to update
   * @param {boolean?} [aUserDisabled]
   *        Value for the userDisabled property. If undefined the value will
   *        not change
   * @param {boolean?} [aSoftDisabled]
   *        Value for the softDisabled property. If undefined the value will
   *        not change. If true this will force userDisabled to be true
   * @param {boolean?} [aBecauseSelecting]
   *        True if we're disabling this add-on because we're selecting
   *        another.
   * @returns {boolean?}
   *       A tri-state indicating the action taken for the add-on:
   *           - undefined: The add-on did not change state
   *           - true: The add-on because disabled
   *           - false: The add-on became enabled
   * @throws if addon is not a DBAddonInternal
   */
  updateAddonDisabledState(aAddon, aUserDisabled, aSoftDisabled, aBecauseSelecting) {
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

    let appDisabled = !this.isUsableAddon(aAddon);
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
    this.setAddonProperties(aAddon, {
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

      this.updateAddonActive(aAddon, !isDisabled);

      if (isDisabled) {
        if (aAddon.bootstrap && XPIProvider.activeAddons.has(aAddon.id)) {
          XPIProvider.callBootstrapMethod(aAddon, aAddon._sourceBundle, "shutdown",
                                          BOOTSTRAP_REASONS.ADDON_DISABLE);
          XPIProvider.unloadBootstrapScope(aAddon.id);
        }
        AddonManagerPrivate.callAddonListeners("onDisabled", wrapper);
      } else {
        if (aAddon.bootstrap) {
          XPIProvider.callBootstrapMethod(aAddon, aAddon._sourceBundle, "startup",
                                          BOOTSTRAP_REASONS.ADDON_ENABLE);
        }
        AddonManagerPrivate.callAddonListeners("onEnabled", wrapper);
      }
    }

    // Notify any other providers that a new theme has been enabled
    if (isTheme(aAddon.type)) {
      if (!isDisabled) {
        AddonManagerPrivate.notifyAddonChanged(aAddon.id, aAddon.type);

        if (xpiState) {
          xpiState.syncWithDB(aAddon);
          XPIStates.save();
        }
      } else if (isDisabled && !aBecauseSelecting) {
        AddonManagerPrivate.notifyAddonChanged(null, "theme");
      }
    }

    return isDisabled;
  },

  /**
   * Record a bit of per-addon telemetry.
   *
   * Yes, this description is extremely helpful. How dare you question its
   * utility?
   *
   * @param {AddonInternal} aAddon
   *        The addon to record
   */
  recordAddonTelemetry(aAddon) {
    let locale = aAddon.defaultLocale;
    if (locale) {
      if (locale.name)
        XPIProvider.setTelemetry(aAddon.id, "name", locale.name);
      if (locale.creator)
        XPIProvider.setTelemetry(aAddon.id, "creator", locale.creator);
    }
  },
};

this.XPIDatabaseReconcile = {
  /**
   * Returns a map of ID -> add-on. When the same add-on ID exists in multiple
   * install locations the highest priority location is chosen.
   *
   * @param {Map<String, AddonInternal>} addonMap
   *        The add-on map to flatten.
   * @param {string?} [hideLocation]
   *        An optional location from which to hide any add-ons.
   * @returns {Map<string, AddonInternal>}
   */
  flattenByID(addonMap, hideLocation) {
    let map = new Map();

    for (let installLocation of XPIProvider.installLocations) {
      if (installLocation.name == hideLocation)
        continue;

      let locationMap = addonMap.get(installLocation.name);
      if (!locationMap)
        continue;

      for (let [id, addon] of locationMap) {
        if (!map.has(id))
          map.set(id, addon);
      }
    }

    return map;
  },

  /**
   * Finds the visible add-ons from the map.
   *
   * @param {Map<String, AddonInternal>} addonMap
   *        The add-on map to filter.
   * @returns {Map<string, AddonInternal>}
   */
  getVisibleAddons(addonMap) {
    let map = new Map();

    for (let addons of addonMap.values()) {
      for (let [id, addon] of addons) {
        if (!addon.visible)
          continue;

        if (map.has(id)) {
          logger.warn("Previous database listed more than one visible add-on with id " + id);
          continue;
        }

        map.set(id, addon);
      }
    }

    return map;
  },

  /**
   * Called to add the metadata for an add-on in one of the install locations
   * to the database. This can be called in three different cases. Either an
   * add-on has been dropped into the location from outside of Firefox, or
   * an add-on has been installed through the application, or the database
   * has been upgraded or become corrupt and add-on data has to be reloaded
   * into it.
   *
   * @param {InstallLocation} aInstallLocation
   *        The install location containing the add-on
   * @param {string} aId
   *        The ID of the add-on
   * @param {XPIState} aAddonState
   *        The new state of the add-on
   * @param {AddonInternal?} [aNewAddon]
   *        The manifest for the new add-on if it has already been loaded
   * @param {string?} [aOldAppVersion]
   *        The version of the application last run with this profile or null
   *        if it is a new profile or the version is unknown
   * @param {string?} [aOldPlatformVersion]
   *        The version of the platform last run with this profile or null
   *        if it is a new profile or the version is unknown
   * @returns {boolean}
   *        A boolean indicating if flushing caches is required to complete
   *        changing this add-on
   */
  addMetadata(aInstallLocation, aId, aAddonState, aNewAddon, aOldAppVersion,
              aOldPlatformVersion) {
    logger.debug("New add-on " + aId + " installed in " + aInstallLocation.name);

    // If we had staged data for this add-on or we aren't recovering from a
    // corrupt database and we don't have migration data for this add-on then
    // this must be a new install.
    let isNewInstall = !!aNewAddon || !XPIDatabase.activeBundles;

    // If it's a new install and we haven't yet loaded the manifest then it
    // must be something dropped directly into the install location
    let isDetectedInstall = isNewInstall && !aNewAddon;

    // Load the manifest if necessary and sanity check the add-on ID
    try {
      if (!aNewAddon) {
        // Load the manifest from the add-on.
        let file = new nsIFile(aAddonState.path);
        aNewAddon = XPIInstall.syncLoadManifestFromFile(file, aInstallLocation);
      }
      // The add-on in the manifest should match the add-on ID.
      if (aNewAddon.id != aId) {
        throw new Error("Invalid addon ID: expected addon ID " + aId +
                        ", found " + aNewAddon.id + " in manifest");
      }
    } catch (e) {
      logger.warn("addMetadata: Add-on " + aId + " is invalid", e);

      // Remove the invalid add-on from the install location if the install
      // location isn't locked
      if (aInstallLocation.isLinkedAddon(aId))
        logger.warn("Not uninstalling invalid item because it is a proxy file");
      else if (aInstallLocation.locked)
        logger.warn("Could not uninstall invalid item from locked install location");
      else
        aInstallLocation.uninstallAddon(aId);
      return null;
    }

    // Update the AddonInternal properties.
    aNewAddon.installDate = aAddonState.mtime;
    aNewAddon.updateDate = aAddonState.mtime;

    // Assume that add-ons in the system add-ons install location aren't
    // foreign and should default to enabled.
    aNewAddon.foreignInstall = isDetectedInstall &&
                               aInstallLocation.name != KEY_APP_SYSTEM_ADDONS &&
                               aInstallLocation.name != KEY_APP_SYSTEM_DEFAULTS;

    // appDisabled depends on whether the add-on is a foreignInstall so update
    aNewAddon.appDisabled = !XPIDatabase.isUsableAddon(aNewAddon);

    if (isDetectedInstall && aNewAddon.foreignInstall) {
      // If the add-on is a foreign install and is in a scope where add-ons
      // that were dropped in should default to disabled then disable it
      let disablingScopes = Services.prefs.getIntPref(PREF_EM_AUTO_DISABLED_SCOPES, 0);
      if (aInstallLocation.scope & disablingScopes) {
        logger.warn("Disabling foreign installed add-on " + aNewAddon.id + " in "
            + aInstallLocation.name);
        aNewAddon.userDisabled = true;
        aNewAddon.seen = false;
      }
    }

    return XPIDatabase.addAddonMetadata(aNewAddon, aAddonState.path);
  },

  /**
   * Called when an add-on has been removed.
   *
   * @param {AddonInternal} aOldAddon
   *        The AddonInternal as it appeared the last time the application
   *        ran
   */
  removeMetadata(aOldAddon) {
    // This add-on has disappeared
    logger.debug("Add-on " + aOldAddon.id + " removed from " + aOldAddon.location);
    XPIDatabase.removeAddonMetadata(aOldAddon);
  },

  /**
   * Updates an add-on's metadata and determines. This is called when either the
   * add-on's install directory path or last modified time has changed.
   *
   * @param {InstallLocation} aInstallLocation
   *        The install location containing the add-on
   * @param {AddonInternal} aOldAddon
   *        The AddonInternal as it appeared the last time the application
   *        ran
   * @param {XPIState} aAddonState
   *        The new state of the add-on
   * @param {AddonInternal?} [aNewAddon]
   *        The manifest for the new add-on if it has already been loaded
   * @returns {boolean?}
   *        A boolean indicating if flushing caches is required to complete
   *        changing this add-on
   */
  updateMetadata(aInstallLocation, aOldAddon, aAddonState, aNewAddon) {
    logger.debug("Add-on " + aOldAddon.id + " modified in " + aInstallLocation.name);

    try {
      // If there isn't an updated install manifest for this add-on then load it.
      if (!aNewAddon) {
        let file = new nsIFile(aAddonState.path);
        aNewAddon = XPIInstall.syncLoadManifestFromFile(file, aInstallLocation, aOldAddon);
      }

      // The ID in the manifest that was loaded must match the ID of the old
      // add-on.
      if (aNewAddon.id != aOldAddon.id)
        throw new Error("Incorrect id in install manifest for existing add-on " + aOldAddon.id);
    } catch (e) {
      logger.warn("updateMetadata: Add-on " + aOldAddon.id + " is invalid", e);
      XPIDatabase.removeAddonMetadata(aOldAddon);
      XPIStates.removeAddon(aOldAddon.location, aOldAddon.id);
      if (!aInstallLocation.locked)
        aInstallLocation.uninstallAddon(aOldAddon.id);
      else
        logger.warn("Could not uninstall invalid item from locked install location");

      return null;
    }

    // Set the additional properties on the new AddonInternal
    aNewAddon.updateDate = aAddonState.mtime;

    // Update the database
    return XPIDatabase.updateAddonMetadata(aOldAddon, aNewAddon, aAddonState.path);
  },

  /**
   * Updates an add-on's path for when the add-on has moved in the
   * filesystem but hasn't changed in any other way.
   *
   * @param {InstallLocation} aInstallLocation
   *        The install location containing the add-on
   * @param {AddonInternal} aOldAddon
   *        The AddonInternal as it appeared the last time the application
   *        ran
   * @param {XPIState} aAddonState
   *        The new state of the add-on
   * @returns {AddonInternal}
   */
  updatePath(aInstallLocation, aOldAddon, aAddonState) {
    logger.debug("Add-on " + aOldAddon.id + " moved to " + aAddonState.path);
    aOldAddon.path = aAddonState.path;
    aOldAddon._sourceBundle = new nsIFile(aAddonState.path);

    return aOldAddon;
  },

  /**
   * Called when no change has been detected for an add-on's metadata but the
   * application has changed so compatibility may have changed.
   *
   * @param {InstallLocation} aInstallLocation
   *        The install location containing the add-on
   * @param {AddonInternal} aOldAddon
   *        The AddonInternal as it appeared the last time the application
   *        ran
   * @param {XPIState} aAddonState
   *        The new state of the add-on
   * @param {boolean} [aReloadMetadata = false]
   *        A boolean which indicates whether metadata should be reloaded from
   *        the addon manifests. Default to false.
   * @returns {DBAddonInternal}
   *        The new addon.
   */
  updateCompatibility(aInstallLocation, aOldAddon, aAddonState, aReloadMetadata) {
    logger.debug("Updating compatibility for add-on " + aOldAddon.id + " in " + aInstallLocation.name);

    let checkSigning = (aOldAddon.signedState === undefined &&
                        AddonSettings.ADDON_SIGNING &&
                        SIGNED_TYPES.has(aOldAddon.type));

    let manifest = null;
    if (checkSigning || aReloadMetadata) {
      try {
        let file = new nsIFile(aAddonState.path);
        manifest = XPIInstall.syncLoadManifestFromFile(file, aInstallLocation);
      } catch (err) {
        // If we can no longer read the manifest, it is no longer compatible.
        aOldAddon.brokenManifest = true;
        aOldAddon.appDisabled = true;
        return aOldAddon;
      }
    }

    // If updating from a version of the app that didn't support signedState
    // then update that property now
    if (checkSigning) {
      aOldAddon.signedState = manifest.signedState;
    }

    // May be updating from a version of the app that didn't support all the
    // properties of the currently-installed add-ons.
    if (aReloadMetadata) {
      // Avoid re-reading these properties from manifest,
      // use existing addon instead.
      // TODO - consider re-scanning for targetApplications.
      let remove = ["syncGUID", "foreignInstall", "visible", "active",
                    "userDisabled", "applyBackgroundUpdates", "sourceURI",
                    "releaseNotesURI", "targetApplications"];

      let props = PROP_JSON_FIELDS.filter(a => !remove.includes(a));
      copyProperties(manifest, props, aOldAddon);
    }

    aOldAddon.appDisabled = !XPIDatabase.isUsableAddon(aOldAddon);

    return aOldAddon;
  },

  /**
   * Compares the add-ons that are currently installed to those that were
   * known to be installed when the application last ran and applies any
   * changes found to the database. Also sends "startupcache-invalidate" signal to
   * observerservice if it detects that data may have changed.
   * Always called after XPIDatabase.jsm and extensions.json have been loaded.
   *
   * @param {Object} aManifests
   *        A dictionary of cached AddonInstalls for add-ons that have been
   *        installed
   * @param {boolean} aUpdateCompatibility
   *        true to update add-ons appDisabled property when the application
   *        version has changed
   * @param {string?} [aOldAppVersion]
   *        The version of the application last run with this profile or null
   *        if it is a new profile or the version is unknown
   * @param {string?} [aOldPlatformVersion]
   *        The version of the platform last run with this profile or null
   *        if it is a new profile or the version is unknown
   * @param {boolean} aSchemaChange
   *        The schema has changed and all add-on manifests should be re-read.
   * @returns {boolean}
   *        A boolean indicating if a change requiring flushing the caches was
   *        detected
   */
  processFileChanges(aManifests, aUpdateCompatibility, aOldAppVersion, aOldPlatformVersion,
                     aSchemaChange) {
    let loadedManifest = (aInstallLocation, aId) => {
      if (!(aInstallLocation.name in aManifests))
        return null;
      if (!(aId in aManifests[aInstallLocation.name]))
        return null;
      return aManifests[aInstallLocation.name][aId];
    };

    // Add-ons loaded from the database can have an uninitialized _sourceBundle
    // if the path was invalid. Swallow that error and say they don't exist.
    let exists = (aAddon) => {
      try {
        return aAddon._sourceBundle.exists();
      } catch (e) {
        if (e.result == Cr.NS_ERROR_NOT_INITIALIZED)
          return false;
        throw e;
      }
    };

    // Get the previous add-ons from the database and put them into maps by location
    let previousAddons = new Map();
    for (let a of XPIDatabase.getAddons()) {
      let locationAddonMap = previousAddons.get(a.location);
      if (!locationAddonMap) {
        locationAddonMap = new Map();
        previousAddons.set(a.location, locationAddonMap);
      }
      locationAddonMap.set(a.id, a);
    }

    // Keep track of add-ons whose blocklist status may have changed. We'll check this
    // after everything else.
    let addonsToCheckAgainstBlocklist = [];

    // Build the list of current add-ons into similar maps. When add-ons are still
    // present we re-use the add-on objects from the database and update their
    // details directly
    let currentAddons = new Map();
    for (let installLocation of XPIProvider.installLocations) {
      let locationAddonMap = new Map();
      currentAddons.set(installLocation.name, locationAddonMap);

      // Get all the on-disk XPI states for this location, and keep track of which
      // ones we see in the database.
      let states = XPIStates.getLocation(installLocation.name);

      // Iterate through the add-ons installed the last time the application
      // ran
      let dbAddons = previousAddons.get(installLocation.name);
      if (dbAddons) {
        for (let [id, oldAddon] of dbAddons) {
          // Check if the add-on is still installed
          let xpiState = states && states.get(id);
          if (xpiState) {
            // Here the add-on was present in the database and on disk
            XPIDatabase.recordAddonTelemetry(oldAddon);

            // Check if the add-on has been changed outside the XPI provider
            if (oldAddon.updateDate != xpiState.mtime) {
              // Did time change in the wrong direction?
              if (xpiState.mtime < oldAddon.updateDate) {
                XPIProvider.setTelemetry(oldAddon.id, "olderFile", {
                  mtime: xpiState.mtime,
                  oldtime: oldAddon.updateDate
                });
              }
            }

            let oldPath = oldAddon.path || descriptorToPath(oldAddon.descriptor);

            // The add-on has changed if the modification time has changed, if
            // we have an updated manifest for it, or if the schema version has
            // changed.
            //
            // Also reload the metadata for add-ons in the application directory
            // when the application version has changed.
            let newAddon = loadedManifest(installLocation, id);
            if (newAddon || oldAddon.updateDate != xpiState.mtime ||
                (aUpdateCompatibility && (installLocation.name == KEY_APP_GLOBAL ||
                                          installLocation.name == KEY_APP_SYSTEM_DEFAULTS))) {
              newAddon = this.updateMetadata(installLocation, oldAddon, xpiState, newAddon);
            } else if (oldPath != xpiState.path) {
              newAddon = this.updatePath(installLocation, oldAddon, xpiState);
            } else if (aUpdateCompatibility || aSchemaChange) {
              // Check compatility when the application version and/or schema
              // version has changed. A schema change also reloads metadata from
              // the manifests.
              newAddon = this.updateCompatibility(installLocation, oldAddon, xpiState,
                                                  aSchemaChange);
              // We need to do a blocklist check later, but the add-on may have changed by then.
              // Avoid storing the current copy and just get one when we need one instead.
              addonsToCheckAgainstBlocklist.push(newAddon.id);
            } else {
              // No change
              newAddon = oldAddon;
            }

            if (newAddon)
              locationAddonMap.set(newAddon.id, newAddon);
          } else {
            // The add-on is in the DB, but not in xpiState (and thus not on disk).
            this.removeMetadata(oldAddon);
          }
        }
      }

      // Any add-on in our current location that we haven't seen needs to
      // be added to the database.
      // Get the migration data for this install location so we can include that as
      // we add, in case this is a database upgrade or rebuild.
      let locMigrateData = {};
      if (XPIDatabase.migrateData && installLocation.name in XPIDatabase.migrateData)
        locMigrateData = XPIDatabase.migrateData[installLocation.name];

      if (states) {
        for (let [id, xpiState] of states) {
          if (locationAddonMap.has(id))
            continue;
          let migrateData = id in locMigrateData ? locMigrateData[id] : null;
          let newAddon = loadedManifest(installLocation, id);
          let addon = this.addMetadata(installLocation, id, xpiState, newAddon,
                                       aOldAppVersion, aOldPlatformVersion, migrateData);
          if (addon)
            locationAddonMap.set(addon.id, addon);
        }
      }
    }

    // previousAddons may contain locations where the database contains add-ons
    // but the browser is no longer configured to use that location. The metadata
    // for those add-ons must be removed from the database.
    for (let [locationName, addons] of previousAddons) {
      if (!currentAddons.has(locationName)) {
        for (let oldAddon of addons.values())
          this.removeMetadata(oldAddon);
      }
    }

    // Validate the updated system add-ons
    let systemAddonLocation = XPIProvider.installLocationsByName[KEY_APP_SYSTEM_ADDONS];
    let addons = currentAddons.get(KEY_APP_SYSTEM_ADDONS) || new Map();

    let hideLocation;

    if (!systemAddonLocation.isValid(addons)) {
      // Hide the system add-on updates if any are invalid.
      logger.info("One or more updated system add-ons invalid, falling back to defaults.");
      hideLocation = KEY_APP_SYSTEM_ADDONS;
    }

    let previousVisible = this.getVisibleAddons(previousAddons);
    let currentVisible = this.flattenByID(currentAddons, hideLocation);

    // Pass over the new set of visible add-ons, record any changes that occurred
    // during startup and call bootstrap install/uninstall scripts as necessary
    for (let [id, currentAddon] of currentVisible) {
      let previousAddon = previousVisible.get(id);

      let isActive = !currentAddon.disabled && !currentAddon.pendingUninstall;
      let wasActive = previousAddon ? previousAddon.active : currentAddon.active;

      if (!previousAddon) {
        // If we had a manifest for this add-on it was a staged install and
        // so wasn't something recovered from a corrupt database
        let wasStaged = !!loadedManifest(currentAddon._installLocation, id);

        // We might be recovering from a corrupt database, if so use the
        // list of known active add-ons to update the new add-on
        if (!wasStaged && XPIDatabase.activeBundles) {
          isActive = XPIDatabase.activeBundles.includes(currentAddon.path);

          if (currentAddon.type == "webextension-theme")
            currentAddon.userDisabled = !isActive;

          // If the add-on wasn't active and it isn't already disabled in some way
          // then it was probably either softDisabled or userDisabled
          if (!isActive && !currentAddon.disabled) {
            // If the add-on is softblocked then assume it is softDisabled
            if (currentAddon.blocklistState == Services.blocklist.STATE_SOFTBLOCKED)
              currentAddon.softDisabled = true;
            else
              currentAddon.userDisabled = true;
          }
        } else {
          // This is a new install
          if (currentAddon.foreignInstall)
            AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_INSTALLED, id);

          if (currentAddon.bootstrap) {
            AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_INSTALLED, id);
            // Visible bootstrapped add-ons need to have their install method called
            XPIProvider.callBootstrapMethod(currentAddon, currentAddon._sourceBundle,
                                            "install", BOOTSTRAP_REASONS.ADDON_INSTALL);
            if (!isActive)
              XPIProvider.unloadBootstrapScope(currentAddon.id);
          }
        }
      } else {
        if (previousAddon !== currentAddon) {
          // This is an add-on that has changed, either the metadata was reloaded
          // or the version in a different location has become visible
          AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_CHANGED, id);

          let installReason = Services.vc.compare(previousAddon.version, currentAddon.version) < 0 ?
                              BOOTSTRAP_REASONS.ADDON_UPGRADE :
                              BOOTSTRAP_REASONS.ADDON_DOWNGRADE;

          // If the previous add-on was in a different path, bootstrapped
          // and still exists then call its uninstall method.
          if (previousAddon.bootstrap && previousAddon._installLocation &&
              exists(previousAddon) &&
              currentAddon._sourceBundle.path != previousAddon._sourceBundle.path) {

            XPIProvider.callBootstrapMethod(previousAddon, previousAddon._sourceBundle,
                                            "uninstall", installReason,
                                            { newVersion: currentAddon.version });
            XPIProvider.unloadBootstrapScope(previousAddon.id);
          }

          // Make sure to flush the cache when an old add-on has gone away
          XPIInstall.flushChromeCaches();

          if (currentAddon.bootstrap) {
            // Visible bootstrapped add-ons need to have their install method called
            let file = currentAddon._sourceBundle.clone();
            XPIProvider.callBootstrapMethod(currentAddon, file,
                                            "install", installReason,
                                            { oldVersion: previousAddon.version });
            if (currentAddon.disabled)
              XPIProvider.unloadBootstrapScope(currentAddon.id);
          }
        }

        if (isActive != wasActive) {
          let change = isActive ? AddonManager.STARTUP_CHANGE_ENABLED
                                : AddonManager.STARTUP_CHANGE_DISABLED;
          AddonManagerPrivate.addStartupChange(change, id);
        }
      }

      XPIDatabase.makeAddonVisible(currentAddon);
      currentAddon.active = isActive;
    }

    // Pass over the set of previously visible add-ons that have now gone away
    // and record the change.
    for (let [id, previousAddon] of previousVisible) {
      if (currentVisible.has(id))
        continue;

      // This add-on vanished

      // If the previous add-on was bootstrapped and still exists then call its
      // uninstall method.
      if (previousAddon.bootstrap && exists(previousAddon)) {
        XPIProvider.callBootstrapMethod(previousAddon, previousAddon._sourceBundle,
                                        "uninstall", BOOTSTRAP_REASONS.ADDON_UNINSTALL);
        XPIProvider.unloadBootstrapScope(previousAddon.id);
      }
      AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_UNINSTALLED, id);
      XPIStates.removeAddon(previousAddon.location, id);

      // Make sure to flush the cache when an old add-on has gone away
      XPIInstall.flushChromeCaches();
    }

    // Make sure add-ons from hidden locations are marked invisible and inactive
    let locationAddonMap = currentAddons.get(hideLocation);
    if (locationAddonMap) {
      for (let addon of locationAddonMap.values()) {
        addon.visible = false;
        addon.active = false;
      }
    }

    // Finally update XPIStates to match everything
    for (let [locationName, locationAddonMap] of currentAddons) {
      for (let [id, addon] of locationAddonMap) {
        let xpiState = XPIStates.getAddon(locationName, id);
        xpiState.syncWithDB(addon);
      }
    }
    XPIStates.save();

    // Clear out any cached migration data.
    XPIDatabase.migrateData = null;
    XPIDatabase.saveChanges();

    // Do some blocklist checks. These will happen after we've just saved everything,
    // because they're async and depend on the blocklist loading. When we're done, save
    // the data if any of the add-ons' blocklist state has changed.
    AddonManager.shutdown.addBlocker(
      "Update add-on blocklist state into add-on DB",
      (async () => {
        // Avoid querying the AddonManager immediately to give startup a chance
        // to complete.
        await Promise.resolve();
        let addons = await AddonManager.getAddonsByIDs(addonsToCheckAgainstBlocklist);
        await Promise.all(addons.map(addon => {
          if (addon) {
            return addon.updateBlocklistState({updateDatabase: false});
          }
          return null;
        }));
        XPIDatabase.saveChanges();
      })().catch(Cu.reportError)
    );

    return true;
  },
};
