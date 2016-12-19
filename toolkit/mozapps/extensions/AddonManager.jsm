/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

// Cannot use Services.appinfo here, or else xpcshell-tests will blow up, as
// most tests later register different nsIAppInfo implementations, which
// wouldn't be reflected in Services.appinfo anymore, as the lazy getter
// underlying it would have been initialized if we used it here.
if ("@mozilla.org/xre/app-info;1" in Cc) {
  let runtime = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);
  if (runtime.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
    // Refuse to run in child processes.
    throw new Error("You cannot use the AddonManager in child processes!");
  }
}

Cu.import("resource://gre/modules/AppConstants.jsm");

const MOZ_COMPATIBILITY_NIGHTLY = !['aurora', 'beta', 'release', 'esr'].includes(AppConstants.MOZ_UPDATE_CHANNEL);

const PREF_BLOCKLIST_PINGCOUNTVERSION = "extensions.blocklist.pingCountVersion";
const PREF_DEFAULT_PROVIDERS_ENABLED  = "extensions.defaultProviders.enabled";
const PREF_EM_UPDATE_ENABLED          = "extensions.update.enabled";
const PREF_EM_LAST_APP_VERSION        = "extensions.lastAppVersion";
const PREF_EM_LAST_PLATFORM_VERSION   = "extensions.lastPlatformVersion";
const PREF_EM_AUTOUPDATE_DEFAULT      = "extensions.update.autoUpdateDefault";
const PREF_EM_STRICT_COMPATIBILITY    = "extensions.strictCompatibility";
const PREF_EM_CHECK_UPDATE_SECURITY   = "extensions.checkUpdateSecurity";
const PREF_EM_UPDATE_BACKGROUND_URL   = "extensions.update.background.url";
const PREF_APP_UPDATE_ENABLED         = "app.update.enabled";
const PREF_APP_UPDATE_AUTO            = "app.update.auto";
const PREF_EM_HOTFIX_ID               = "extensions.hotfix.id";
const PREF_EM_HOTFIX_LASTVERSION      = "extensions.hotfix.lastVersion";
const PREF_EM_HOTFIX_URL              = "extensions.hotfix.url";
const PREF_EM_CERT_CHECKATTRIBUTES    = "extensions.hotfix.cert.checkAttributes";
const PREF_EM_HOTFIX_CERTS            = "extensions.hotfix.certs.";
const PREF_MATCH_OS_LOCALE            = "intl.locale.matchOS";
const PREF_SELECTED_LOCALE            = "general.useragent.locale";
const UNKNOWN_XPCOM_ABI               = "unknownABI";

const PREF_MIN_WEBEXT_PLATFORM_VERSION = "extensions.webExtensionsMinPlatformVersion";
const PREF_WEBAPI_TESTING             = "extensions.webapi.testing";

const UPDATE_REQUEST_VERSION          = 2;
const CATEGORY_UPDATE_PARAMS          = "extension-update-params";

const XMLURI_BLOCKLIST                = "http://www.mozilla.org/2006/addons-blocklist";

const KEY_PROFILEDIR                  = "ProfD";
const KEY_APPDIR                      = "XCurProcD";
const FILE_BLOCKLIST                  = "blocklist.xml";

const BRANCH_REGEXP                   = /^([^\.]+\.[0-9]+[a-z]*).*/gi;
const PREF_EM_CHECK_COMPATIBILITY_BASE = "extensions.checkCompatibility";
var PREF_EM_CHECK_COMPATIBILITY = MOZ_COMPATIBILITY_NIGHTLY ?
                                  PREF_EM_CHECK_COMPATIBILITY_BASE + ".nightly" :
                                  undefined;

const VALID_TYPES_REGEXP = /^[\w\-]+$/;

const WEBAPI_INSTALL_HOSTS = ["addons.mozilla.org", "testpilot.firefox.com"];
const WEBAPI_TEST_INSTALL_HOSTS = [
  "addons.allizom.org", "addons-dev.allizom.org",
  "testpilot.stage.mozaws.net", "testpilot.dev.mozaws.net",
  "example.com",
];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AsyncShutdown.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonRepository",
                                  "resource://gre/modules/addons/AddonRepository.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Extension",
                                  "resource://gre/modules/Extension.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "CertUtils", function() {
  let certUtils = {};
  Components.utils.import("resource://gre/modules/CertUtils.jsm", certUtils);
  return certUtils;
});

const INTEGER = /^[1-9]\d*$/;

this.EXPORTED_SYMBOLS = [ "AddonManager", "AddonManagerPrivate" ];

const CATEGORY_PROVIDER_MODULE = "addon-provider-module";

// A list of providers to load by default
const DEFAULT_PROVIDERS = [
  "resource://gre/modules/addons/XPIProvider.jsm",
  "resource://gre/modules/LightweightThemeManager.jsm"
];

Cu.import("resource://gre/modules/Log.jsm");
// Configure a logger at the parent 'addons' level to format
// messages for all the modules under addons.*
const PARENT_LOGGER_ID = "addons";
var parentLogger = Log.repository.getLogger(PARENT_LOGGER_ID);
parentLogger.level = Log.Level.Warn;
var formatter = new Log.BasicFormatter();
// Set parent logger (and its children) to append to
// the Javascript section of the Browser Console
parentLogger.addAppender(new Log.ConsoleAppender(formatter));
// Set parent logger (and its children) to
// also append to standard out
parentLogger.addAppender(new Log.DumpAppender(formatter));

// Create a new logger (child of 'addons' logger)
// for use by the Addons Manager
const LOGGER_ID = "addons.manager";
var logger = Log.repository.getLogger(LOGGER_ID);

// Provide the ability to enable/disable logging
// messages at runtime.
// If the "extensions.logging.enabled" preference is
// missing or 'false', messages at the WARNING and higher
// severity should be logged to the JS console and standard error.
// If "extensions.logging.enabled" is set to 'true', messages
// at DEBUG and higher should go to JS console and standard error.
const PREF_LOGGING_ENABLED = "extensions.logging.enabled";
const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";

const UNNAMED_PROVIDER = "<unnamed-provider>";
function providerName(aProvider) {
  return aProvider.name || UNNAMED_PROVIDER;
}

/**
 * Preference listener which listens for a change in the
 * "extensions.logging.enabled" preference and changes the logging level of the
 * parent 'addons' level logger accordingly.
 */
var PrefObserver = {
    init: function() {
      Services.prefs.addObserver(PREF_LOGGING_ENABLED, this, false);
      Services.obs.addObserver(this, "xpcom-shutdown", false);
      this.observe(null, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, PREF_LOGGING_ENABLED);
    },

    observe: function(aSubject, aTopic, aData) {
      if (aTopic == "xpcom-shutdown") {
        Services.prefs.removeObserver(PREF_LOGGING_ENABLED, this);
        Services.obs.removeObserver(this, "xpcom-shutdown");
      }
      else if (aTopic == NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) {
        let debugLogEnabled = false;
        try {
          debugLogEnabled = Services.prefs.getBoolPref(PREF_LOGGING_ENABLED);
        }
        catch (e) {
        }
        if (debugLogEnabled) {
          parentLogger.level = Log.Level.Debug;
        }
        else {
          parentLogger.level = Log.Level.Warn;
        }
      }
    }
};

PrefObserver.init();

/**
 * Calls a callback method consuming any thrown exception. Any parameters after
 * the callback parameter will be passed to the callback.
 *
 * @param  aCallback
 *         The callback method to call
 */
function safeCall(aCallback, ...aArgs) {
  try {
    aCallback.apply(null, aArgs);
  }
  catch (e) {
    logger.warn("Exception calling callback", e);
  }
}

/**
 * Creates a function that will call the passed callback catching and logging
 * any exceptions.
 *
 * @param  aCallback
 *         The callback method to call
 */
function makeSafe(aCallback) {
  return function(...aArgs) {
    safeCall(aCallback, ...aArgs);
  }
}

/**
 * Given a promise and an optional callback, either:
 *
 * 1) Returns the promise, if no callback was provided, or,
 * 2) Calls the callback with the promise resolution value, and reports
 *    any errors.
 *
 * @param {Promise} promise
 *        The promise to return, or report to the callback function.
 * @param {function | null} callback
 *        The optional callback function to call with the promise
 *        resolution.
 * @returns {Promise?}
 */
function promiseOrCallback(promise, callback) {
  if (!callback)
    return promise;

  if (typeof callback !== "function")
    throw Components.Exception("Callback must be a function",
                               Cr.NS_ERROR_INVALID_ARG);

  promise.then(makeSafe(callback)).catch(error => {
    logger.error(error);
  });

  return undefined;
}

/**
 * Report an exception thrown by a provider API method.
 */
function reportProviderError(aProvider, aMethod, aError) {
  let method = `provider ${providerName(aProvider)}.${aMethod}`;
  AddonManagerPrivate.recordException("AMI", method, aError);
  logger.error("Exception calling " + method, aError);
}

/**
 * Calls a method on a provider if it exists and consumes any thrown exception.
 * Any parameters after the aDefault parameter are passed to the provider's method.
 *
 * @param  aProvider
 *         The provider to call
 * @param  aMethod
 *         The method name to call
 * @param  aDefault
 *         A default return value if the provider does not implement the named
 *         method or throws an error.
 * @return the return value from the provider, or aDefault if the provider does not
 *         implement method or throws an error
 */
function callProvider(aProvider, aMethod, aDefault, ...aArgs) {
  if (!(aMethod in aProvider))
    return aDefault;

  try {
    return aProvider[aMethod].apply(aProvider, aArgs);
  }
  catch (e) {
    reportProviderError(aProvider, aMethod, e);
    return aDefault;
  }
}

/**
 * Calls a method on a provider if it exists and consumes any thrown exception.
 * Parameters after aMethod are passed to aProvider.aMethod().
 * The last parameter must be a callback function.
 * If the provider does not implement the method, or the method throws, calls
 * the callback with 'undefined'.
 *
 * @param  aProvider
 *         The provider to call
 * @param  aMethod
 *         The method name to call
 */
function callProviderAsync(aProvider, aMethod, ...aArgs) {
  let callback = aArgs[aArgs.length - 1];
  if (!(aMethod in aProvider)) {
    callback(undefined);
    return undefined;
  }
  try {
    return aProvider[aMethod].apply(aProvider, aArgs);
  }
  catch (e) {
    reportProviderError(aProvider, aMethod, e);
    callback(undefined);
    return undefined;
  }
}

/**
 * Calls a method on a provider if it exists and consumes any thrown exception.
 * Parameters after aMethod are passed to aProvider.aMethod() and an additional
 * callback is added for the provider to return a result to.
 *
 * @param  aProvider
 *         The provider to call
 * @param  aMethod
 *         The method name to call
 * @return {Promise}
 * @resolves The result the provider returns, or |undefined| if the provider
 *           does not implement the method or the method throws.
 * @rejects  Never
 */
function promiseCallProvider(aProvider, aMethod, ...aArgs) {
  return new Promise(resolve => {
    callProviderAsync(aProvider, aMethod, ...aArgs, resolve);
  });
}

/**
 * Gets the currently selected locale for display.
 * @return  the selected locale or "en-US" if none is selected
 */
function getLocale() {
  try {
    if (Services.prefs.getBoolPref(PREF_MATCH_OS_LOCALE))
      return Services.locale.getLocaleComponentForUserAgent();
  }
  catch (e) { }

  try {
    let locale = Services.prefs.getComplexValue(PREF_SELECTED_LOCALE,
                                                Ci.nsIPrefLocalizedString);
    if (locale)
      return locale;
  }
  catch (e) { }

  try {
    return Services.prefs.getCharPref(PREF_SELECTED_LOCALE);
  }
  catch (e) { }

  return "en-US";
}

function webAPIForAddon(addon) {
  if (!addon) {
    return null;
  }

  let result = {};

  // By default just pass through any plain property, the webidl will
  // control access.  Also filter out private properties, regular Addon
  // objects are okay but MockAddon used in tests has non-serializable
  // private properties.
  for (let prop in addon) {
    if (prop[0] != "_" && typeof(addon[prop]) != "function") {
      result[prop] = addon[prop];
    }
  }

  // A few properties are computed for a nicer API
  result.isEnabled = !addon.userDisabled;
  result.canUninstall = Boolean(addon.permissions & AddonManager.PERM_CAN_UNINSTALL);

  return result;
}

/**
 * Listens for a browser changing origin and cancels the installs that were
 * started by it.
 */
function BrowserListener(aBrowser, aInstallingPrincipal, aInstalls) {
  this.browser = aBrowser;
  this.principal = aInstallingPrincipal;
  this.installs = aInstalls;
  this.installCount = aInstalls.length;

  aBrowser.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_LOCATION);
  Services.obs.addObserver(this, "message-manager-close", true);

  for (let install of this.installs)
    install.addListener(this);

  this.registered = true;
}

BrowserListener.prototype = {
  browser: null,
  installs: null,
  installCount: null,
  registered: false,

  unregister: function() {
    if (!this.registered)
      return;
    this.registered = false;

    Services.obs.removeObserver(this, "message-manager-close");
    // The browser may have already been detached
    if (this.browser.removeProgressListener)
      this.browser.removeProgressListener(this);

    for (let install of this.installs)
      install.removeListener(this);
    this.installs = null;
  },

  cancelInstalls: function() {
    for (let install of this.installs) {
      try {
        install.cancel();
      }
      catch (e) {
        // Some installs may have already failed or been cancelled, ignore these
      }
    }
  },

  observe: function(subject, topic, data) {
    if (subject != this.browser.messageManager)
      return;

    // The browser's message manager has closed and so the browser is
    // going away, cancel all installs
    this.cancelInstalls();
  },

  onLocationChange: function(webProgress, request, location) {
    if (this.browser.contentPrincipal && this.principal.subsumes(this.browser.contentPrincipal))
      return;

    // The browser has navigated to a new origin so cancel all installs
    this.cancelInstalls();
  },

  onDownloadCancelled: function(install) {
    // Don't need to hear more events from this install
    install.removeListener(this);

    // Once all installs have ended unregister everything
    if (--this.installCount == 0)
      this.unregister();
  },

  onDownloadFailed: function(install) {
    this.onDownloadCancelled(install);
  },

  onInstallFailed: function(install) {
    this.onDownloadCancelled(install);
  },

  onInstallEnded: function(install) {
    this.onDownloadCancelled(install);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference,
                                         Ci.nsIWebProgressListener,
                                         Ci.nsIObserver])
};

/**
 * This represents an author of an add-on (e.g. creator or developer)
 *
 * @param  aName
 *         The name of the author
 * @param  aURL
 *         The URL of the author's profile page
 */
function AddonAuthor(aName, aURL) {
  this.name = aName;
  this.url = aURL;
}

AddonAuthor.prototype = {
  name: null,
  url: null,

  // Returns the author's name, defaulting to the empty string
  toString: function() {
    return this.name || "";
  }
}

/**
 * This represents an screenshot for an add-on
 *
 * @param  aURL
 *         The URL to the full version of the screenshot
 * @param  aWidth
 *         The width in pixels of the screenshot
 * @param  aHeight
 *         The height in pixels of the screenshot
 * @param  aThumbnailURL
 *         The URL to the thumbnail version of the screenshot
 * @param  aThumbnailWidth
 *         The width in pixels of the thumbnail version of the screenshot
 * @param  aThumbnailHeight
 *         The height in pixels of the thumbnail version of the screenshot
 * @param  aCaption
 *         The caption of the screenshot
 */
function AddonScreenshot(aURL, aWidth, aHeight, aThumbnailURL,
                         aThumbnailWidth, aThumbnailHeight, aCaption) {
  this.url = aURL;
  if (aWidth) this.width = aWidth;
  if (aHeight) this.height = aHeight;
  if (aThumbnailURL) this.thumbnailURL = aThumbnailURL;
  if (aThumbnailWidth) this.thumbnailWidth = aThumbnailWidth;
  if (aThumbnailHeight) this.thumbnailHeight = aThumbnailHeight;
  if (aCaption) this.caption = aCaption;
}

AddonScreenshot.prototype = {
  url: null,
  width: null,
  height: null,
  thumbnailURL: null,
  thumbnailWidth: null,
  thumbnailHeight: null,
  caption: null,

  // Returns the screenshot URL, defaulting to the empty string
  toString: function() {
    return this.url || "";
  }
}


/**
 * This represents a compatibility override for an addon.
 *
 * @param  aType
 *         Overrride type - "compatible" or "incompatible"
 * @param  aMinVersion
 *         Minimum version of the addon to match
 * @param  aMaxVersion
 *         Maximum version of the addon to match
 * @param  aAppID
 *         Application ID used to match appMinVersion and appMaxVersion
 * @param  aAppMinVersion
 *         Minimum version of the application to match
 * @param  aAppMaxVersion
 *         Maximum version of the application to match
 */
function AddonCompatibilityOverride(aType, aMinVersion, aMaxVersion, aAppID,
                                    aAppMinVersion, aAppMaxVersion) {
  this.type = aType;
  this.minVersion = aMinVersion;
  this.maxVersion = aMaxVersion;
  this.appID = aAppID;
  this.appMinVersion = aAppMinVersion;
  this.appMaxVersion = aAppMaxVersion;
}

AddonCompatibilityOverride.prototype = {
  /**
   * Type of override - "incompatible" or "compatible".
   * Only "incompatible" is supported for now.
   */
  type: null,

  /**
   * Min version of the addon to match.
   */
  minVersion: null,

  /**
   * Max version of the addon to match.
   */
  maxVersion: null,

  /**
   * Application ID to match.
   */
  appID: null,

  /**
   * Min version of the application to match.
   */
  appMinVersion: null,

  /**
   * Max version of the application to match.
   */
  appMaxVersion: null
};


/**
 * A type of add-on, used by the UI to determine how to display different types
 * of add-ons.
 *
 * @param  aID
 *         The add-on type ID
 * @param  aLocaleURI
 *         The URI of a localized properties file to get the displayable name
 *         for the type from
 * @param  aLocaleKey
 *         The key for the string in the properties file or the actual display
 *         name if aLocaleURI is null. Include %ID% to include the type ID in
 *         the key
 * @param  aViewType
 *         The optional type of view to use in the UI
 * @param  aUIPriority
 *         The priority is used by the UI to list the types in order. Lower
 *         values push the type higher in the list.
 * @param  aFlags
 *         An option set of flags that customize the display of the add-on in
 *         the UI.
 */
function AddonType(aID, aLocaleURI, aLocaleKey, aViewType, aUIPriority, aFlags) {
  if (!aID)
    throw Components.Exception("An AddonType must have an ID", Cr.NS_ERROR_INVALID_ARG);

  if (aViewType && aUIPriority === undefined)
    throw Components.Exception("An AddonType with a defined view must have a set UI priority",
                               Cr.NS_ERROR_INVALID_ARG);

  if (!aLocaleKey)
    throw Components.Exception("An AddonType must have a displayable name",
                               Cr.NS_ERROR_INVALID_ARG);

  this.id = aID;
  this.uiPriority = aUIPriority;
  this.viewType = aViewType;
  this.flags = aFlags;

  if (aLocaleURI) {
    XPCOMUtils.defineLazyGetter(this, "name", () => {
      let bundle = Services.strings.createBundle(aLocaleURI);
      return bundle.GetStringFromName(aLocaleKey.replace("%ID%", aID));
    });
  }
  else {
    this.name = aLocaleKey;
  }
}

var gStarted = false;
var gStartupComplete = false;
var gCheckCompatibility = true;
var gStrictCompatibility = true;
var gCheckUpdateSecurityDefault = true;
var gCheckUpdateSecurity = gCheckUpdateSecurityDefault;
var gUpdateEnabled = true;
var gAutoUpdateDefault = true;
var gHotfixID = null;
var gWebExtensionsMinPlatformVersion = null;
var gShutdownBarrier = null;
var gRepoShutdownState = "";
var gShutdownInProgress = false;
var gPluginPageListener = null;

/**
 * This is the real manager, kept here rather than in AddonManager to keep its
 * contents hidden from API users.
 */
var AddonManagerInternal = {
  managerListeners: [],
  installListeners: [],
  addonListeners: [],
  typeListeners: [],
  pendingProviders: new Set(),
  providers: new Set(),
  providerShutdowns: new Map(),
  types: {},
  startupChanges: {},
  // Store telemetry details per addon provider
  telemetryDetails: {},
  upgradeListeners: new Map(),

  recordTimestamp: function(name, value) {
    this.TelemetryTimestamps.add(name, value);
  },

  validateBlocklist: function() {
    let appBlocklist = FileUtils.getFile(KEY_APPDIR, [FILE_BLOCKLIST]);

    // If there is no application shipped blocklist then there is nothing to do
    if (!appBlocklist.exists())
      return;

    let profileBlocklist = FileUtils.getFile(KEY_PROFILEDIR, [FILE_BLOCKLIST]);

    // If there is no blocklist in the profile then copy the application shipped
    // one there
    if (!profileBlocklist.exists()) {
      try {
        appBlocklist.copyTo(profileBlocklist.parent, FILE_BLOCKLIST);
      }
      catch (e) {
        logger.warn("Failed to copy the application shipped blocklist to the profile", e);
      }
      return;
    }

    let fileStream = Cc["@mozilla.org/network/file-input-stream;1"].
                     createInstance(Ci.nsIFileInputStream);
    try {
      let cstream = Cc["@mozilla.org/intl/converter-input-stream;1"].
                    createInstance(Ci.nsIConverterInputStream);
      fileStream.init(appBlocklist, FileUtils.MODE_RDONLY, FileUtils.PERMS_FILE, 0);
      cstream.init(fileStream, "UTF-8", 0, 0);

      let data = "";
      let str = {};
      let read = 0;
      do {
        read = cstream.readString(0xffffffff, str);
        data += str.value;
      } while (read != 0);

      let parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                   createInstance(Ci.nsIDOMParser);
      var doc = parser.parseFromString(data, "text/xml");
    }
    catch (e) {
      logger.warn("Application shipped blocklist could not be loaded", e);
      return;
    }
    finally {
      try {
        fileStream.close();
      }
      catch (e) {
        logger.warn("Unable to close blocklist file stream", e);
      }
    }

    // If the namespace is incorrect then ignore the application shipped
    // blocklist
    if (doc.documentElement.namespaceURI != XMLURI_BLOCKLIST) {
      logger.warn("Application shipped blocklist has an unexpected namespace (" +
                  doc.documentElement.namespaceURI + ")");
      return;
    }

    // If there is no lastupdate information then ignore the application shipped
    // blocklist
    if (!doc.documentElement.hasAttribute("lastupdate"))
      return;

    // If the application shipped blocklist is older than the profile blocklist
    // then do nothing
    if (doc.documentElement.getAttribute("lastupdate") <=
        profileBlocklist.lastModifiedTime)
      return;

    // Otherwise copy the application shipped blocklist to the profile
    try {
      appBlocklist.copyTo(profileBlocklist.parent, FILE_BLOCKLIST);
    }
    catch (e) {
      logger.warn("Failed to copy the application shipped blocklist to the profile", e);
    }
  },

  /**
   * Start up a provider, and register its shutdown hook if it has one
   */
  _startProvider(aProvider, aAppChanged, aOldAppVersion, aOldPlatformVersion) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    logger.debug(`Starting provider: ${providerName(aProvider)}`);
    callProvider(aProvider, "startup", null, aAppChanged, aOldAppVersion, aOldPlatformVersion);
    if ('shutdown' in aProvider) {
      let name = providerName(aProvider);
      let AMProviderShutdown = () => {
        // If the provider has been unregistered, it will have been removed from
        // this.providers. If it hasn't been unregistered, then this is a normal
        // shutdown - and we move it to this.pendingProviders incase we're
        // running in a test that will start AddonManager again.
        if (this.providers.has(aProvider)) {
          this.providers.delete(aProvider);
          this.pendingProviders.add(aProvider);
        }

        return new Promise((resolve, reject) => {
            logger.debug("Calling shutdown blocker for " + name);
            resolve(aProvider.shutdown());
          })
          .catch(err => {
            logger.warn("Failure during shutdown of " + name, err);
            AddonManagerPrivate.recordException("AMI", "Async shutdown of " + name, err);
          });
      };
      logger.debug("Registering shutdown blocker for " + name);
      this.providerShutdowns.set(aProvider, AMProviderShutdown);
      AddonManager.shutdown.addBlocker(name, AMProviderShutdown);
    }

    this.pendingProviders.delete(aProvider);
    this.providers.add(aProvider);
    logger.debug(`Provider finished startup: ${providerName(aProvider)}`);
  },

  _getProviderByName(aName) {
    for (let provider of this.providers) {
      if (providerName(provider) == aName)
        return provider;
    }
    return undefined;
  },

  /**
   * Initializes the AddonManager, loading any known providers and initializing
   * them.
   */
  startup: function() {
    try {
      if (gStarted)
        return;

      this.recordTimestamp("AMI_startup_begin");

      // clear this for xpcshell test restarts
      for (let provider in this.telemetryDetails)
        delete this.telemetryDetails[provider];

      let appChanged = undefined;

      let oldAppVersion = null;
      try {
        oldAppVersion = Services.prefs.getCharPref(PREF_EM_LAST_APP_VERSION);
        appChanged = Services.appinfo.version != oldAppVersion;
      }
      catch (e) { }

      Extension.browserUpdated = appChanged;

      let oldPlatformVersion = null;
      try {
        oldPlatformVersion = Services.prefs.getCharPref(PREF_EM_LAST_PLATFORM_VERSION);
      }
      catch (e) { }

      if (appChanged !== false) {
        logger.debug("Application has been upgraded");
        Services.prefs.setCharPref(PREF_EM_LAST_APP_VERSION,
                                   Services.appinfo.version);
        Services.prefs.setCharPref(PREF_EM_LAST_PLATFORM_VERSION,
                                   Services.appinfo.platformVersion);
        Services.prefs.setIntPref(PREF_BLOCKLIST_PINGCOUNTVERSION,
                                  (appChanged === undefined ? 0 : -1));
        this.validateBlocklist();
      }

      if (!MOZ_COMPATIBILITY_NIGHTLY) {
        PREF_EM_CHECK_COMPATIBILITY = PREF_EM_CHECK_COMPATIBILITY_BASE + "." +
                                      Services.appinfo.version.replace(BRANCH_REGEXP, "$1");
      }

      try {
        gCheckCompatibility = Services.prefs.getBoolPref(PREF_EM_CHECK_COMPATIBILITY);
      } catch (e) {}
      Services.prefs.addObserver(PREF_EM_CHECK_COMPATIBILITY, this, false);

      try {
        gStrictCompatibility = Services.prefs.getBoolPref(PREF_EM_STRICT_COMPATIBILITY);
      } catch (e) {}
      Services.prefs.addObserver(PREF_EM_STRICT_COMPATIBILITY, this, false);

      try {
        let defaultBranch = Services.prefs.getDefaultBranch("");
        gCheckUpdateSecurityDefault = defaultBranch.getBoolPref(PREF_EM_CHECK_UPDATE_SECURITY);
      } catch (e) {}

      try {
        gCheckUpdateSecurity = Services.prefs.getBoolPref(PREF_EM_CHECK_UPDATE_SECURITY);
      } catch (e) {}
      Services.prefs.addObserver(PREF_EM_CHECK_UPDATE_SECURITY, this, false);

      try {
        gUpdateEnabled = Services.prefs.getBoolPref(PREF_EM_UPDATE_ENABLED);
      } catch (e) {}
      Services.prefs.addObserver(PREF_EM_UPDATE_ENABLED, this, false);

      try {
        gAutoUpdateDefault = Services.prefs.getBoolPref(PREF_EM_AUTOUPDATE_DEFAULT);
      } catch (e) {}
      Services.prefs.addObserver(PREF_EM_AUTOUPDATE_DEFAULT, this, false);

      try {
        gHotfixID = Services.prefs.getCharPref(PREF_EM_HOTFIX_ID);
      } catch (e) {}
      Services.prefs.addObserver(PREF_EM_HOTFIX_ID, this, false);

      try {
        gWebExtensionsMinPlatformVersion = Services.prefs.getCharPref(PREF_MIN_WEBEXT_PLATFORM_VERSION);
      } catch (e) {}
      Services.prefs.addObserver(PREF_MIN_WEBEXT_PLATFORM_VERSION, this, false);

      let defaultProvidersEnabled = true;
      try {
        defaultProvidersEnabled = Services.prefs.getBoolPref(PREF_DEFAULT_PROVIDERS_ENABLED);
      } catch (e) {}
      AddonManagerPrivate.recordSimpleMeasure("default_providers", defaultProvidersEnabled);

      // Ensure all default providers have had a chance to register themselves
      if (defaultProvidersEnabled) {
        for (let url of DEFAULT_PROVIDERS) {
          try {
            let scope = {};
            Components.utils.import(url, scope);
            // Sanity check - make sure the provider exports a symbol that
            // has a 'startup' method
            let syms = Object.keys(scope);
            if ((syms.length < 1) ||
                (typeof scope[syms[0]].startup != "function")) {
              logger.warn("Provider " + url + " has no startup()");
              AddonManagerPrivate.recordException("AMI", "provider " + url, "no startup()");
            }
            logger.debug("Loaded provider scope for " + url + ": " + Object.keys(scope).toSource());
          }
          catch (e) {
            AddonManagerPrivate.recordException("AMI", "provider " + url + " load failed", e);
            logger.error("Exception loading default provider \"" + url + "\"", e);
          }
        }
      }

      // Load any providers registered in the category manager
      let catman = Cc["@mozilla.org/categorymanager;1"].
                   getService(Ci.nsICategoryManager);
      let entries = catman.enumerateCategory(CATEGORY_PROVIDER_MODULE);
      while (entries.hasMoreElements()) {
        let entry = entries.getNext().QueryInterface(Ci.nsISupportsCString).data;
        let url = catman.getCategoryEntry(CATEGORY_PROVIDER_MODULE, entry);

        try {
          Components.utils.import(url, {});
          logger.debug(`Loaded provider scope for ${url}`);
        }
        catch (e) {
          AddonManagerPrivate.recordException("AMI", "provider " + url + " load failed", e);
          logger.error("Exception loading provider " + entry + " from category \"" +
                url + "\"", e);
        }
      }

      // Register our shutdown handler with the AsyncShutdown manager
      gShutdownBarrier = new AsyncShutdown.Barrier("AddonManager: Waiting for providers to shut down.");
      AsyncShutdown.profileBeforeChange.addBlocker("AddonManager: shutting down.",
                                                   this.shutdownManager.bind(this),
                                                   {fetchState: this.shutdownState.bind(this)});

      // Once we start calling providers we must allow all normal methods to work.
      gStarted = true;

      for (let provider of this.pendingProviders) {
        this._startProvider(provider, appChanged, oldAppVersion, oldPlatformVersion);
      }

      // If this is a new profile just pretend that there were no changes
      if (appChanged === undefined) {
        for (let type in this.startupChanges)
          delete this.startupChanges[type];
      }

      // Support for remote about:plugins. Note that this module isn't loaded
      // at the top because Services.appinfo is defined late in tests.
      let { RemotePages } = Cu.import("resource://gre/modules/RemotePageManager.jsm", {});

      gPluginPageListener = new RemotePages("about:plugins");
      gPluginPageListener.addMessageListener("RequestPlugins", this.requestPlugins);

      gStartupComplete = true;
      this.recordTimestamp("AMI_startup_end");
    }
    catch (e) {
      logger.error("startup failed", e);
      AddonManagerPrivate.recordException("AMI", "startup failed", e);
    }

    logger.debug("Completed startup sequence");
    this.callManagerListeners("onStartup");
  },

  /**
   * Registers a new AddonProvider.
   *
   * @param  aProvider
   *         The provider to register
   * @param  aTypes
   *         An optional array of add-on types
   */
  registerProvider: function(aProvider, aTypes) {
    if (!aProvider || typeof aProvider != "object")
      throw Components.Exception("aProvider must be specified",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aTypes && !Array.isArray(aTypes))
      throw Components.Exception("aTypes must be an array or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    this.pendingProviders.add(aProvider);

    if (aTypes) {
      for (let type of aTypes) {
        if (!(type.id in this.types)) {
          if (!VALID_TYPES_REGEXP.test(type.id)) {
            logger.warn("Ignoring invalid type " + type.id);
            return;
          }

          this.types[type.id] = {
            type: type,
            providers: [aProvider]
          };

          let typeListeners = this.typeListeners.slice(0);
          for (let listener of typeListeners)
            safeCall(() => listener.onTypeAdded(type));
        }
        else {
          this.types[type.id].providers.push(aProvider);
        }
      }
    }

    // If we're registering after startup call this provider's startup.
    if (gStarted) {
      this._startProvider(aProvider);
    }
  },

  /**
   * Unregisters an AddonProvider.
   *
   * @param  aProvider
   *         The provider to unregister
   * @return Whatever the provider's 'shutdown' method returns (if anything).
   *         For providers that have async shutdown methods returning Promises,
   *         the caller should wait for that Promise to resolve.
   */
  unregisterProvider: function(aProvider) {
    if (!aProvider || typeof aProvider != "object")
      throw Components.Exception("aProvider must be specified",
                                 Cr.NS_ERROR_INVALID_ARG);

    this.providers.delete(aProvider);
    // The test harness will unregister XPIProvider *after* shutdown, which is
    // after the provider will have been moved from providers to
    // pendingProviders.
    this.pendingProviders.delete(aProvider);

    for (let type in this.types) {
      this.types[type].providers = this.types[type].providers.filter(p => p != aProvider);
      if (this.types[type].providers.length == 0) {
        let oldType = this.types[type].type;
        delete this.types[type];

        let typeListeners = this.typeListeners.slice(0);
        for (let listener of typeListeners)
          safeCall(() => listener.onTypeRemoved(oldType));
      }
    }

    // If we're unregistering after startup but before shutting down,
    // remove the blocker for this provider's shutdown and call it.
    // If we're already shutting down, just let gShutdownBarrier call it to avoid races.
    if (gStarted && !gShutdownInProgress) {
      logger.debug("Unregistering shutdown blocker for " + providerName(aProvider));
      let shutter = this.providerShutdowns.get(aProvider);
      if (shutter) {
        this.providerShutdowns.delete(aProvider);
        gShutdownBarrier.client.removeBlocker(shutter);
        return shutter();
      }
    }
    return undefined;
  },

  /**
   * Mark a provider as safe to access via AddonManager APIs, before its
   * startup has completed.
   *
   * Normally a provider isn't marked as safe until after its (synchronous)
   * startup() method has returned. Until a provider has been marked safe,
   * it won't be used by any of the AddonManager APIs. markProviderSafe()
   * allows a provider to mark itself as safe during its startup; this can be
   * useful if the provider wants to perform tasks that block startup, which
   * happen after its required initialization tasks and therefore when the
   * provider is in a safe state.
   *
   * @param aProvider Provider object to mark safe
   */
  markProviderSafe: function(aProvider) {
    if (!gStarted) {
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);
    }

    if (!aProvider || typeof aProvider != "object") {
      throw Components.Exception("aProvider must be specified",
                                 Cr.NS_ERROR_INVALID_ARG);
    }

    if (!this.pendingProviders.has(aProvider)) {
      return;
    }

    this.pendingProviders.delete(aProvider);
    this.providers.add(aProvider);
  },

  /**
   * Calls a method on all registered providers if it exists and consumes any
   * thrown exception. Return values are ignored. Any parameters after the
   * method parameter are passed to the provider's method.
   * WARNING: Do not use for asynchronous calls; callProviders() does not
   * invoke callbacks if provider methods throw synchronous exceptions.
   *
   * @param  aMethod
   *         The method name to call
   * @see    callProvider
   */
  callProviders: function(aMethod, ...aArgs) {
    if (!aMethod || typeof aMethod != "string")
      throw Components.Exception("aMethod must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    let providers = [...this.providers];
    for (let provider of providers) {
      try {
        if (aMethod in provider)
          provider[aMethod].apply(provider, aArgs);
      }
      catch (e) {
        reportProviderError(provider, aMethod, e);
      }
    }
  },

  /**
   * Report the current state of asynchronous shutdown
   */
  shutdownState() {
    let state = [];
    if (gShutdownBarrier) {
      state.push({
        name: gShutdownBarrier.client.name,
        state: gShutdownBarrier.state
      });
    }
    state.push({
      name: "AddonRepository: async shutdown",
      state: gRepoShutdownState
    });
    return state;
  },

  /**
   * Shuts down the addon manager and all registered providers, this must clean
   * up everything in order for automated tests to fake restarts.
   * @return Promise{null} that resolves when all providers and dependent modules
   *                       have finished shutting down
   */
  shutdownManager: Task.async(function*() {
    logger.debug("shutdown");
    this.callManagerListeners("onShutdown");

    gRepoShutdownState = "pending";
    gShutdownInProgress = true;
    // Clean up listeners
    Services.prefs.removeObserver(PREF_EM_CHECK_COMPATIBILITY, this);
    Services.prefs.removeObserver(PREF_EM_STRICT_COMPATIBILITY, this);
    Services.prefs.removeObserver(PREF_EM_CHECK_UPDATE_SECURITY, this);
    Services.prefs.removeObserver(PREF_EM_UPDATE_ENABLED, this);
    Services.prefs.removeObserver(PREF_EM_AUTOUPDATE_DEFAULT, this);
    Services.prefs.removeObserver(PREF_EM_HOTFIX_ID, this);
    gPluginPageListener.destroy();
    gPluginPageListener = null;

    let savedError = null;
    // Only shut down providers if they've been started.
    if (gStarted) {
      try {
        yield gShutdownBarrier.wait();
      }
      catch (err) {
        savedError = err;
        logger.error("Failure during wait for shutdown barrier", err);
        AddonManagerPrivate.recordException("AMI", "Async shutdown of AddonManager providers", err);
      }
    }

    // Shut down AddonRepository after providers (if any).
    try {
      gRepoShutdownState = "in progress";
      yield AddonRepository.shutdown();
      gRepoShutdownState = "done";
    }
    catch (err) {
      savedError = err;
      logger.error("Failure during AddonRepository shutdown", err);
      AddonManagerPrivate.recordException("AMI", "Async shutdown of AddonRepository", err);
    }

    logger.debug("Async provider shutdown done");
    this.managerListeners.splice(0, this.managerListeners.length);
    this.installListeners.splice(0, this.installListeners.length);
    this.addonListeners.splice(0, this.addonListeners.length);
    this.typeListeners.splice(0, this.typeListeners.length);
    this.providerShutdowns.clear();
    for (let type in this.startupChanges)
      delete this.startupChanges[type];
    gStarted = false;
    gStartupComplete = false;
    gShutdownBarrier = null;
    gShutdownInProgress = false;
    if (savedError) {
      throw savedError;
    }
  }),

  requestPlugins: function({ target: port }) {
    // Lists all the properties that plugins.html needs
    const NEEDED_PROPS = ["name", "pluginLibraries", "pluginFullpath", "version",
                          "isActive", "blocklistState", "description",
                          "pluginMimeTypes"];
    function filterProperties(plugin) {
      let filtered = {};
      for (let prop of NEEDED_PROPS) {
        filtered[prop] = plugin[prop];
      }
      return filtered;
    }

    AddonManager.getAddonsByTypes(["plugin"], function(aPlugins) {
      port.sendAsyncMessage("PluginList", aPlugins.map(filterProperties));
    });
  },

  /**
   * Notified when a preference we're interested in has changed.
   *
   * @see nsIObserver
   */
  observe: function(aSubject, aTopic, aData) {
    switch (aData) {
      case PREF_EM_CHECK_COMPATIBILITY: {
        let oldValue = gCheckCompatibility;
        try {
          gCheckCompatibility = Services.prefs.getBoolPref(PREF_EM_CHECK_COMPATIBILITY);
        } catch (e) {
          gCheckCompatibility = true;
        }

        this.callManagerListeners("onCompatibilityModeChanged");

        if (gCheckCompatibility != oldValue)
          this.updateAddonAppDisabledStates();

        break;
      }
      case PREF_EM_STRICT_COMPATIBILITY: {
        let oldValue = gStrictCompatibility;
        try {
          gStrictCompatibility = Services.prefs.getBoolPref(PREF_EM_STRICT_COMPATIBILITY);
        } catch (e) {
          gStrictCompatibility = true;
        }

        this.callManagerListeners("onCompatibilityModeChanged");

        if (gStrictCompatibility != oldValue)
          this.updateAddonAppDisabledStates();

        break;
      }
      case PREF_EM_CHECK_UPDATE_SECURITY: {
        let oldValue = gCheckUpdateSecurity;
        try {
          gCheckUpdateSecurity = Services.prefs.getBoolPref(PREF_EM_CHECK_UPDATE_SECURITY);
        } catch (e) {
          gCheckUpdateSecurity = true;
        }

        this.callManagerListeners("onCheckUpdateSecurityChanged");

        if (gCheckUpdateSecurity != oldValue)
          this.updateAddonAppDisabledStates();

        break;
      }
      case PREF_EM_UPDATE_ENABLED: {
        try {
          gUpdateEnabled = Services.prefs.getBoolPref(PREF_EM_UPDATE_ENABLED);
        } catch (e) {
          gUpdateEnabled = true;
        }

        this.callManagerListeners("onUpdateModeChanged");
        break;
      }
      case PREF_EM_AUTOUPDATE_DEFAULT: {
        try {
          gAutoUpdateDefault = Services.prefs.getBoolPref(PREF_EM_AUTOUPDATE_DEFAULT);
        } catch (e) {
          gAutoUpdateDefault = true;
        }

        this.callManagerListeners("onUpdateModeChanged");
        break;
      }
      case PREF_EM_HOTFIX_ID: {
        try {
          gHotfixID = Services.prefs.getCharPref(PREF_EM_HOTFIX_ID);
        } catch (e) {
          gHotfixID = null;
        }
        break;
      }
      case PREF_MIN_WEBEXT_PLATFORM_VERSION: {
        gWebExtensionsMinPlatformVersion = Services.prefs.getCharPref(PREF_MIN_WEBEXT_PLATFORM_VERSION);
        break;
      }
    }
  },

  /**
   * Replaces %...% strings in an addon url (update and updateInfo) with
   * appropriate values.
   *
   * @param  aAddon
   *         The Addon representing the add-on
   * @param  aUri
   *         The string representation of the URI to escape
   * @param  aAppVersion
   *         The optional application version to use for %APP_VERSION%
   * @return The appropriately escaped URI.
   */
  escapeAddonURI: function(aAddon, aUri, aAppVersion)
  {
    if (!aAddon || typeof aAddon != "object")
      throw Components.Exception("aAddon must be an Addon object",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!aUri || typeof aUri != "string")
      throw Components.Exception("aUri must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aAppVersion && typeof aAppVersion != "string")
      throw Components.Exception("aAppVersion must be a string or null",
                                 Cr.NS_ERROR_INVALID_ARG);

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
    uri = uri.replace(/%REQ_VERSION%/g, UPDATE_REQUEST_VERSION);
    uri = uri.replace(/%APP_OS%/g, Services.appinfo.OS);
    uri = uri.replace(/%APP_ABI%/g, xpcomABI);
    uri = uri.replace(/%APP_LOCALE%/g, getLocale());
    uri = uri.replace(/%CURRENT_APP_VERSION%/g, Services.appinfo.version);

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
      catch (e) {
        return aMatch;
      }
    });

    // escape() does not properly encode + symbols in any embedded FVF strings.
    return uri.replace(/\+/g, "%2B");
  },

  /**
   * Performs a background update check by starting an update for all add-ons
   * that can be updated.
   * @return Promise{null} Resolves when the background update check is complete
   *                       (the resulting addon installations may still be in progress).
   */
  backgroundUpdateCheck: function() {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    let buPromise = Task.spawn(function*() {
      let hotfixID = this.hotfixID;

      let appUpdateEnabled = Services.prefs.getBoolPref(PREF_APP_UPDATE_ENABLED) &&
                             Services.prefs.getBoolPref(PREF_APP_UPDATE_AUTO);
      let checkHotfix = hotfixID && appUpdateEnabled;

      logger.debug("Background update check beginning");

      Services.obs.notifyObservers(null, "addons-background-update-start", null);

      if (this.updateEnabled) {
        let scope = {};
        Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm", scope);
        scope.LightweightThemeManager.updateCurrentTheme();

        let allAddons = yield this.getAllAddons();

        // Repopulate repository cache first, to ensure compatibility overrides
        // are up to date before checking for addon updates.
        yield AddonRepository.backgroundUpdateCheck();

        // Keep track of all the async add-on updates happening in parallel
        let updates = [];

        for (let addon of allAddons) {
          if (addon.id == hotfixID) {
            continue;
          }

          // Check all add-ons for updates so that any compatibility updates will
          // be applied
          updates.push(new Promise((resolve, reject) => {
            addon.findUpdates({
              onUpdateAvailable: function(aAddon, aInstall) {
                // Start installing updates when the add-on can be updated and
                // background updates should be applied.
                logger.debug("Found update for add-on ${id}", aAddon);
                if (aAddon.permissions & AddonManager.PERM_CAN_UPGRADE &&
                    AddonManager.shouldAutoUpdate(aAddon)) {
                  // XXX we really should resolve when this install is done,
                  // not when update-available check completes, no?
                  logger.debug(`Starting upgrade install of ${aAddon.id}`);
                  aInstall.install();
                }
              },

              onUpdateFinished: aAddon => { logger.debug("onUpdateFinished for ${id}", aAddon); resolve(); }
            }, AddonManager.UPDATE_WHEN_PERIODIC_UPDATE);
          }));
        }
        yield Promise.all(updates);
      }

      if (checkHotfix) {
        var hotfixVersion = "";
        try {
          hotfixVersion = Services.prefs.getCharPref(PREF_EM_HOTFIX_LASTVERSION);
        }
        catch (e) { }

        let url = null;
        if (Services.prefs.getPrefType(PREF_EM_HOTFIX_URL) == Ci.nsIPrefBranch.PREF_STRING)
          url = Services.prefs.getCharPref(PREF_EM_HOTFIX_URL);
        else
          url = Services.prefs.getCharPref(PREF_EM_UPDATE_BACKGROUND_URL);

        // Build the URI from a fake add-on data.
        url = AddonManager.escapeAddonURI({
          id: hotfixID,
          version: hotfixVersion,
          userDisabled: false,
          appDisabled: false
        }, url);

        Components.utils.import("resource://gre/modules/addons/AddonUpdateChecker.jsm");
        let update = null;
        try {
          let foundUpdates = yield new Promise((resolve, reject) => {
            AddonUpdateChecker.checkForUpdates(hotfixID, null, url, {
              onUpdateCheckComplete: resolve,
              onUpdateCheckError: reject
            });
          });
          update = AddonUpdateChecker.getNewestCompatibleUpdate(foundUpdates);
        } catch (e) {
          // AUC.checkForUpdates already logged the error
        }

        // Check that we have a hotfix update, and it's newer than the one we already
        // have installed (if any)
        if (update) {
          if (Services.vc.compare(hotfixVersion, update.version) < 0) {
            logger.debug("Downloading hotfix version " + update.version);
            let aInstall = yield AddonManagerInternal.getInstallForURL(
                update.updateURL, "application/x-xpinstall", update.updateHash,
                null, null, update.version);

            aInstall.addListener({
              onDownloadEnded: function(aInstall) {
                if (aInstall.addon.id != hotfixID) {
                  logger.warn("The downloaded hotfix add-on did not have the " +
                              "expected ID and so will not be installed.");
                  aInstall.cancel();
                  return;
                }

                // If XPIProvider has reported the hotfix as properly signed then
                // there is nothing more to do here
                if (aInstall.addon.signedState == AddonManager.SIGNEDSTATE_SIGNED)
                  return;

                try {
                  if (!Services.prefs.getBoolPref(PREF_EM_CERT_CHECKATTRIBUTES))
                    return;
                }
                catch (e) {
                  // By default don't do certificate checks.
                  return;
                }

                try {
                  CertUtils.validateCert(aInstall.certificate,
                                         CertUtils.readCertPrefs(PREF_EM_HOTFIX_CERTS));
                }
                catch (e) {
                  logger.warn("The hotfix add-on was not signed by the expected " +
                       "certificate and so will not be installed.", e);
                  aInstall.cancel();
                }
              },

              onInstallEnded: function(aInstall) {
                // Remember the last successfully installed version.
                Services.prefs.setCharPref(PREF_EM_HOTFIX_LASTVERSION,
                                           aInstall.version);
              },

              onInstallCancelled: function(aInstall) {
                // Revert to the previous version if the installation was
                // cancelled.
                Services.prefs.setCharPref(PREF_EM_HOTFIX_LASTVERSION,
                                           hotfixVersion);
              }
            });

            aInstall.install();
          }
        }
      }

      if (appUpdateEnabled) {
        try {
          yield AddonManagerInternal._getProviderByName("XPIProvider").updateSystemAddons();
        }
        catch (e) {
          logger.warn("Failed to update system addons", e);
        }
      }

      logger.debug("Background update check complete");
      Services.obs.notifyObservers(null,
                                   "addons-background-update-complete",
                                   null);
    }.bind(this));
    // Fork the promise chain so we can log the error and let our caller see it too.
    buPromise.then(null, e => logger.warn("Error in background update", e));
    return buPromise;
  },

  /**
   * Adds a add-on to the list of detected changes for this startup. If
   * addStartupChange is called multiple times for the same add-on in the same
   * startup then only the most recent change will be remembered.
   *
   * @param  aType
   *         The type of change as a string. Providers can define their own
   *         types of changes or use the existing defined STARTUP_CHANGE_*
   *         constants
   * @param  aID
   *         The ID of the add-on
   */
  addStartupChange: function(aType, aID) {
    if (!aType || typeof aType != "string")
      throw Components.Exception("aType must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!aID || typeof aID != "string")
      throw Components.Exception("aID must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (gStartupComplete)
      return;
    logger.debug("Registering startup change '" + aType + "' for " + aID);

    // Ensure that an ID is only listed in one type of change
    for (let type in this.startupChanges)
      this.removeStartupChange(type, aID);

    if (!(aType in this.startupChanges))
      this.startupChanges[aType] = [];
    this.startupChanges[aType].push(aID);
  },

  /**
   * Removes a startup change for an add-on.
   *
   * @param  aType
   *         The type of change
   * @param  aID
   *         The ID of the add-on
   */
  removeStartupChange: function(aType, aID) {
    if (!aType || typeof aType != "string")
      throw Components.Exception("aType must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!aID || typeof aID != "string")
      throw Components.Exception("aID must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (gStartupComplete)
      return;

    if (!(aType in this.startupChanges))
      return;

    this.startupChanges[aType] = this.startupChanges[aType].filter(aItem => aItem != aID);
  },

  /**
   * Calls all registered AddonManagerListeners with an event. Any parameters
   * after the method parameter are passed to the listener.
   *
   * @param  aMethod
   *         The method on the listeners to call
   */
  callManagerListeners: function(aMethod, ...aArgs) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aMethod || typeof aMethod != "string")
      throw Components.Exception("aMethod must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    let managerListeners = this.managerListeners.slice(0);
    for (let listener of managerListeners) {
      try {
        if (aMethod in listener)
          listener[aMethod].apply(listener, aArgs);
      }
      catch (e) {
        logger.warn("AddonManagerListener threw exception when calling " + aMethod, e);
      }
    }
  },

  /**
   * Calls all registered InstallListeners with an event. Any parameters after
   * the extraListeners parameter are passed to the listener.
   *
   * @param  aMethod
   *         The method on the listeners to call
   * @param  aExtraListeners
   *         An optional array of extra InstallListeners to also call
   * @return false if any of the listeners returned false, true otherwise
   */
  callInstallListeners: function(aMethod,
                                 aExtraListeners, ...aArgs) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aMethod || typeof aMethod != "string")
      throw Components.Exception("aMethod must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aExtraListeners && !Array.isArray(aExtraListeners))
      throw Components.Exception("aExtraListeners must be an array or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    let result = true;
    let listeners;
    if (aExtraListeners)
      listeners = aExtraListeners.concat(this.installListeners);
    else
      listeners = this.installListeners.slice(0);

    for (let listener of listeners) {
      try {
        if (aMethod in listener) {
          if (listener[aMethod].apply(listener, aArgs) === false)
            result = false;
        }
      }
      catch (e) {
        logger.warn("InstallListener threw exception when calling " + aMethod, e);
      }
    }
    return result;
  },

  /**
   * Calls all registered AddonListeners with an event. Any parameters after
   * the method parameter are passed to the listener.
   *
   * @param  aMethod
   *         The method on the listeners to call
   */
  callAddonListeners: function(aMethod, ...aArgs) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aMethod || typeof aMethod != "string")
      throw Components.Exception("aMethod must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    let addonListeners = this.addonListeners.slice(0);
    for (let listener of addonListeners) {
      try {
        if (aMethod in listener)
          listener[aMethod].apply(listener, aArgs);
      }
      catch (e) {
        logger.warn("AddonListener threw exception when calling " + aMethod, e);
      }
    }
  },

  /**
   * Notifies all providers that an add-on has been enabled when that type of
   * add-on only supports a single add-on being enabled at a time. This allows
   * the providers to disable theirs if necessary.
   *
   * @param  aID
   *         The ID of the enabled add-on
   * @param  aType
   *         The type of the enabled add-on
   * @param  aPendingRestart
   *         A boolean indicating if the change will only take place the next
   *         time the application is restarted
   */
  notifyAddonChanged: function(aID, aType, aPendingRestart) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (aID && typeof aID != "string")
      throw Components.Exception("aID must be a string or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!aType || typeof aType != "string")
      throw Components.Exception("aType must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    // Temporary hack until bug 520124 lands.
    // We can get here during synchronous startup, at which point it's
    // considered unsafe (and therefore disallowed by AddonManager.jsm) to
    // access providers that haven't been initialized yet. Since this is when
    // XPIProvider is starting up, XPIProvider can't access itself via APIs
    // going through AddonManager.jsm. Furthermore, LightweightThemeManager may
    // not be initialized until after XPIProvider is, and therefore would also
    // be unaccessible during XPIProvider startup. Thankfully, these are the
    // only two uses of this API, and we know it's safe to use this API with
    // both providers; so we have this hack to allow bypassing the normal
    // safetey guard.
    // The notifyAddonChanged/addonChanged API will be unneeded and therefore
    // removed by bug 520124, so this is a temporary quick'n'dirty hack.
    let providers = [...this.providers, ...this.pendingProviders];
    for (let provider of providers) {
      callProvider(provider, "addonChanged", null, aID, aType, aPendingRestart);
    }
  },

  /**
   * Notifies all providers they need to update the appDisabled property for
   * their add-ons in response to an application change such as a blocklist
   * update.
   */
  updateAddonAppDisabledStates: function() {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    this.callProviders("updateAddonAppDisabledStates");
  },

  /**
   * Notifies all providers that the repository has updated its data for
   * installed add-ons.
   */
  updateAddonRepositoryData: function() {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    return Task.spawn(function*() {
      for (let provider of this.providers) {
        yield promiseCallProvider(provider, "updateAddonRepositoryData");
      }

      // only tests should care about this
      Services.obs.notifyObservers(null, "TEST:addon-repository-data-updated", null);
    }.bind(this));
  },

  /**
   * Asynchronously gets an AddonInstall for a URL.
   *
   * @param  aUrl
   *         The string represenation of the URL the add-on is located at
   * @param  aMimetype
   *         The mimetype of the add-on
   * @param  aHash
   *         An optional hash of the add-on
   * @param  aName
   *         An optional placeholder name while the add-on is being downloaded
   * @param  aIcons
   *         Optional placeholder icons while the add-on is being downloaded
   * @param  aVersion
   *         An optional placeholder version while the add-on is being downloaded
   * @param  aBrowser
   *         An optional <browser> element for download permissions prompts.
   * @throws if the aUrl, aCallback or aMimetype arguments are not specified
   */
  getInstallForURL: function(aUrl, aMimetype, aHash, aName,
                             aIcons, aVersion, aBrowser) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aUrl || typeof aUrl != "string")
      throw Components.Exception("aURL must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!aMimetype || typeof aMimetype != "string")
      throw Components.Exception("aMimetype must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aHash && typeof aHash != "string")
      throw Components.Exception("aHash must be a string or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aName && typeof aName != "string")
      throw Components.Exception("aName must be a string or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aIcons) {
      if (typeof aIcons == "string")
        aIcons = { "32": aIcons };
      else if (typeof aIcons != "object")
        throw Components.Exception("aIcons must be a string, an object or null",
                                   Cr.NS_ERROR_INVALID_ARG);
    } else {
      aIcons = {};
    }

    if (aVersion && typeof aVersion != "string")
      throw Components.Exception("aVersion must be a string or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aBrowser && !(aBrowser instanceof Ci.nsIDOMElement))
      throw Components.Exception("aBrowser must be a nsIDOMElement or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    for (let provider of this.providers) {
      if (callProvider(provider, "supportsMimetype", false, aMimetype)) {
        return promiseCallProvider(
          provider, "getInstallForURL", aUrl, aHash, aName, aIcons,
          aVersion, aBrowser);
      }
    }

    return Promise.resolve(null);
  },

  /**
   * Asynchronously gets an AddonInstall for an nsIFile.
   *
   * @param  aFile
   *         The nsIFile where the add-on is located
   * @param  aMimetype
   *         An optional mimetype hint for the add-on
   * @throws if the aFile or aCallback arguments are not specified
   */
  getInstallForFile: function(aFile, aMimetype) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!(aFile instanceof Ci.nsIFile))
      throw Components.Exception("aFile must be a nsIFile",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aMimetype && typeof aMimetype != "string")
      throw Components.Exception("aMimetype must be a string or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    return Task.spawn(function*() {
      for (let provider of this.providers) {
        let install = yield promiseCallProvider(
          provider, "getInstallForFile", aFile);

        if (install)
          return install;
      }

      return null;
    }.bind(this));
  },

  /**
   * Asynchronously gets all current AddonInstalls optionally limiting to a list
   * of types.
   *
   * @param  aTypes
   *         An optional array of types to retrieve. Each type is a string name
   * @throws If the aCallback argument is not specified
   */
  getInstallsByTypes: function(aTypes) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (aTypes && !Array.isArray(aTypes))
      throw Components.Exception("aTypes must be an array or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    return Task.spawn(function*() {
      let installs = [];

      for (let provider of this.providers) {
        let providerInstalls = yield promiseCallProvider(
          provider, "getInstallsByTypes", aTypes);

        if (providerInstalls)
          installs.push(...providerInstalls);
      }

      return installs;
    }.bind(this));
  },

  /**
   * Asynchronously gets all current AddonInstalls.
   */
  getAllInstalls: function() {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    return this.getInstallsByTypes(null);
  },

  /**
   * Synchronously map a URI to the corresponding Addon ID.
   *
   * Mappable URIs are limited to in-application resources belonging to the
   * add-on, such as Javascript compartments, XUL windows, XBL bindings, etc.
   * but do not include URIs from meta data, such as the add-on homepage.
   *
   * @param  aURI
   *         nsIURI to map to an addon id
   * @return string containing the Addon ID or null
   * @see    amIAddonManager.mapURIToAddonID
   */
  mapURIToAddonID: function(aURI) {
    if (!(aURI instanceof Ci.nsIURI)) {
      throw Components.Exception("aURI is not a nsIURI",
                                 Cr.NS_ERROR_INVALID_ARG);
    }

    // Try all providers
    let providers = [...this.providers];
    for (let provider of providers) {
      var id = callProvider(provider, "mapURIToAddonID", null, aURI);
      if (id !== null) {
        return id;
      }
    }

    return null;
  },

  /**
   * Checks whether installation is enabled for a particular mimetype.
   *
   * @param  aMimetype
   *         The mimetype to check
   * @return true if installation is enabled for the mimetype
   */
  isInstallEnabled: function(aMimetype) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aMimetype || typeof aMimetype != "string")
      throw Components.Exception("aMimetype must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    let providers = [...this.providers];
    for (let provider of providers) {
      if (callProvider(provider, "supportsMimetype", false, aMimetype) &&
          callProvider(provider, "isInstallEnabled"))
        return true;
    }
    return false;
  },

  /**
   * Checks whether a particular source is allowed to install add-ons of a
   * given mimetype.
   *
   * @param  aMimetype
   *         The mimetype of the add-on
   * @param  aInstallingPrincipal
   *         The nsIPrincipal that initiated the install
   * @return true if the source is allowed to install this mimetype
   */
  isInstallAllowed: function(aMimetype, aInstallingPrincipal) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aMimetype || typeof aMimetype != "string")
      throw Components.Exception("aMimetype must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!aInstallingPrincipal || !(aInstallingPrincipal instanceof Ci.nsIPrincipal))
      throw Components.Exception("aInstallingPrincipal must be a nsIPrincipal",
                                 Cr.NS_ERROR_INVALID_ARG);

    let providers = [...this.providers];
    for (let provider of providers) {
      if (callProvider(provider, "supportsMimetype", false, aMimetype) &&
          callProvider(provider, "isInstallAllowed", null, aInstallingPrincipal))
        return true;
    }
    return false;
  },

  /**
   * Starts installation of an array of AddonInstalls notifying the registered
   * web install listener of blocked or started installs.
   *
   * @param  aMimetype
   *         The mimetype of add-ons being installed
   * @param  aBrowser
   *         The optional browser element that started the installs
   * @param  aInstallingPrincipal
   *         The nsIPrincipal that initiated the install
   * @param  aInstalls
   *         The array of AddonInstalls to be installed
   */
  installAddonsFromWebpage: function(aMimetype, aBrowser,
                                     aInstallingPrincipal, aInstalls) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aMimetype || typeof aMimetype != "string")
      throw Components.Exception("aMimetype must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (aBrowser && !(aBrowser instanceof Ci.nsIDOMElement))
      throw Components.Exception("aSource must be a nsIDOMElement, or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!aInstallingPrincipal || !(aInstallingPrincipal instanceof Ci.nsIPrincipal))
      throw Components.Exception("aInstallingPrincipal must be a nsIPrincipal",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!Array.isArray(aInstalls))
      throw Components.Exception("aInstalls must be an array",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!("@mozilla.org/addons/web-install-listener;1" in Cc)) {
      logger.warn("No web installer available, cancelling all installs");
      for (let install of aInstalls)
        install.cancel();
      return;
    }

    // When a chrome in-content UI has loaded a <browser> inside to host a
    // website we want to do our security checks on the inner-browser but
    // notify front-end that install events came from the outer-browser (the
    // main tab's browser). Check this by seeing if the browser we've been
    // passed is in a content type docshell and if so get the outer-browser.
    let topBrowser = aBrowser;
    let docShell = aBrowser.ownerDocument.defaultView
                           .QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDocShell)
                           .QueryInterface(Ci.nsIDocShellTreeItem);
    if (docShell.itemType == Ci.nsIDocShellTreeItem.typeContent)
      topBrowser = docShell.chromeEventHandler;

    try {
      let weblistener = Cc["@mozilla.org/addons/web-install-listener;1"].
                        getService(Ci.amIWebInstallListener);

      if (!this.isInstallEnabled(aMimetype)) {
        for (let install of aInstalls)
          install.cancel();

        weblistener.onWebInstallDisabled(topBrowser, aInstallingPrincipal.URI,
                                         aInstalls, aInstalls.length);
        return;
      }
      else if (!aBrowser.contentPrincipal || !aInstallingPrincipal.subsumes(aBrowser.contentPrincipal)) {
        for (let install of aInstalls)
          install.cancel();

        if (weblistener instanceof Ci.amIWebInstallListener2) {
          weblistener.onWebInstallOriginBlocked(topBrowser, aInstallingPrincipal.URI,
                                                aInstalls, aInstalls.length);
        }
        return;
      }

      // The installs may start now depending on the web install listener,
      // listen for the browser navigating to a new origin and cancel the
      // installs in that case.
      new BrowserListener(aBrowser, aInstallingPrincipal, aInstalls);

      if (!this.isInstallAllowed(aMimetype, aInstallingPrincipal)) {
        if (weblistener.onWebInstallBlocked(topBrowser, aInstallingPrincipal.URI,
                                            aInstalls, aInstalls.length)) {
          for (let install of aInstalls)
            install.install();
        }
      }
      else if (weblistener.onWebInstallRequested(topBrowser, aInstallingPrincipal.URI,
                                                 aInstalls, aInstalls.length)) {
        for (let install of aInstalls)
          install.install();
      }
    }
    catch (e) {
      // In the event that the weblistener throws during instantiation or when
      // calling onWebInstallBlocked or onWebInstallRequested all of the
      // installs should get cancelled.
      logger.warn("Failure calling web installer", e);
      for (let install of aInstalls)
        install.cancel();
    }
  },

  /**
   * Adds a new InstallListener if the listener is not already registered.
   *
   * @param  aListener
   *         The InstallListener to add
   */
  addInstallListener: function(aListener) {
    if (!aListener || typeof aListener != "object")
      throw Components.Exception("aListener must be a InstallListener object",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!this.installListeners.some(function(i) {
      return i == aListener; }))
      this.installListeners.push(aListener);
  },

  /**
   * Removes an InstallListener if the listener is registered.
   *
   * @param  aListener
   *         The InstallListener to remove
   */
  removeInstallListener: function(aListener) {
    if (!aListener || typeof aListener != "object")
      throw Components.Exception("aListener must be a InstallListener object",
                                 Cr.NS_ERROR_INVALID_ARG);

    let pos = 0;
    while (pos < this.installListeners.length) {
      if (this.installListeners[pos] == aListener)
        this.installListeners.splice(pos, 1);
      else
        pos++;
    }
  },
  /*
   * Adds new or overrides existing UpgradeListener.
   *
   * @param  aInstanceID
   *         The instance ID of an addon to register a listener for.
   * @param  aCallback
   *         The callback to invoke when updates are available for this addon.
   * @throws if there is no addon matching the instanceID
   */
  addUpgradeListener: function(aInstanceID, aCallback) {
   if (!aInstanceID || typeof aInstanceID != "symbol")
     throw Components.Exception("aInstanceID must be a symbol",
                                Cr.NS_ERROR_INVALID_ARG);

  if (!aCallback || typeof aCallback != "function")
    throw Components.Exception("aCallback must be a function",
                               Cr.NS_ERROR_INVALID_ARG);

   this.getAddonByInstanceID(aInstanceID).then(wrapper => {
     if (!wrapper) {
       throw Error("No addon matching instanceID:", aInstanceID.toString());
     }
     let addonId = wrapper.addonId();
     logger.debug(`Registering upgrade listener for ${addonId}`);
     this.upgradeListeners.set(addonId, aCallback);
   });
  },

  /**
   * Removes an UpgradeListener if the listener is registered.
   *
   * @param  aInstanceID
   *         The instance ID of the addon to remove
   */
  removeUpgradeListener: function(aInstanceID) {
    if (!aInstanceID || typeof aInstanceID != "symbol")
      throw Components.Exception("aInstanceID must be a symbol",
                                 Cr.NS_ERROR_INVALID_ARG);

    this.getAddonByInstanceID(aInstanceID).then(addon => {
      if (!addon) {
        throw Error("No addon for instanceID:", aInstanceID.toString());
      }
      if (this.upgradeListeners.has(addon.id)) {
        this.upgradeListeners.delete(addon.id);
      } else {
        throw Error("No upgrade listener registered for addon ID:", addon.id);
      }
    });
  },

  /**
   * Installs a temporary add-on from a local file or directory.
   * @param  aFile
   *         An nsIFile for the file or directory of the add-on to be
   *         temporarily installed.
   * @return a Promise that rejects if the add-on is not a valid restartless
   *         add-on or if the same ID is already temporarily installed.
   */
  installTemporaryAddon: function(aFile) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!(aFile instanceof Ci.nsIFile))
      throw Components.Exception("aFile must be a nsIFile",
                                 Cr.NS_ERROR_INVALID_ARG);

    return AddonManagerInternal._getProviderByName("XPIProvider")
                               .installTemporaryAddon(aFile);
  },

  installAddonFromSources: function(aFile) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!(aFile instanceof Ci.nsIFile))
      throw Components.Exception("aFile must be a nsIFile",
                                 Cr.NS_ERROR_INVALID_ARG);

    return AddonManagerInternal._getProviderByName("XPIProvider")
                               .installAddonFromSources(aFile);
  },

  /**
   * Returns an Addon corresponding to an instance ID.
   * @param aInstanceID
   *        An Addon Instance ID symbol
   * @return {Promise}
   * @resolves The found Addon or null if no such add-on exists.
   * @rejects  Never
   * @throws if the aInstanceID argument is not specified
   *         or the AddonManager is not initialized
   */
   getAddonByInstanceID: function(aInstanceID) {
     if (!gStarted)
       throw Components.Exception("AddonManager is not initialized",
                                  Cr.NS_ERROR_NOT_INITIALIZED);

     if (!aInstanceID || typeof aInstanceID != "symbol")
       throw Components.Exception("aInstanceID must be a Symbol()",
                                  Cr.NS_ERROR_INVALID_ARG);

     return AddonManagerInternal._getProviderByName("XPIProvider")
                                .getAddonByInstanceID(aInstanceID);
   },

  /**
   * Gets an icon from the icon set provided by the add-on
   * that is closest to the specified size.
   *
   * The optional window parameter will be used to determine
   * the screen resolution and select a more appropriate icon.
   * Calling this method with 48px on retina screens will try to
   * match an icon of size 96px.
   *
   * @param  aAddon
   *         An addon object, meaning:
   *         An object with either an icons property that is a key-value
   *         list of icon size and icon URL, or an object having an iconURL
   *         and icon64URL property.
   * @param  aSize
   *         Ideal icon size in pixels
   * @param  aWindow
   *         Optional window object for determining the correct scale.
   * @return {String} The absolute URL of the icon or null if the addon doesn't have icons
   */
  getPreferredIconURL: function(aAddon, aSize, aWindow = undefined) {
    if (aWindow && aWindow.devicePixelRatio) {
      aSize *= aWindow.devicePixelRatio;
    }

    let icons = aAddon.icons;

    // certain addon-types only have iconURLs
    if (!icons) {
      icons = {};
      if (aAddon.iconURL) {
        icons[32] = aAddon.iconURL;
        icons[48] = aAddon.iconURL;
      }
      if (aAddon.icon64URL) {
        icons[64] = aAddon.icon64URL;
      }
    }

    // quick return if the exact size was found
    if (icons[aSize]) {
      return icons[aSize];
    }

    let bestSize = null;

    for (let size of Object.keys(icons)) {
      if (!INTEGER.test(size)) {
        throw Components.Exception("Invalid icon size, must be an integer",
                                   Cr.NS_ERROR_ILLEGAL_VALUE);
      }

      size = parseInt(size, 10);

      if (!bestSize) {
        bestSize = size;
        continue;
      }

      if (size > aSize && bestSize > aSize) {
        // If both best size and current size are larger than the wanted size then choose
        // the one closest to the wanted size
        bestSize = Math.min(bestSize, size);
      }
      else {
        // Otherwise choose the largest of the two so we'll prefer sizes as close to below aSize
        // or above aSize
        bestSize = Math.max(bestSize, size);
      }
    }

    return icons[bestSize] || null;
  },

  /**
   * Asynchronously gets an add-on with a specific ID.
   *
   * @param  aID
   *         The ID of the add-on to retrieve
   * @return {Promise}
   * @resolves The found Addon or null if no such add-on exists.
   * @rejects  Never
   * @throws if the aID argument is not specified
   */
  getAddonByID: function(aID) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aID || typeof aID != "string")
      throw Components.Exception("aID must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    let promises = Array.from(this.providers,
      p => promiseCallProvider(p, "getAddonByID", aID));
    return Promise.all(promises).then(aAddons => {
      return aAddons.find(a => !!a) || null;
    });
  },

  /**
   * Asynchronously get an add-on with a specific Sync GUID.
   *
   * @param  aGUID
   *         String GUID of add-on to retrieve
   * @throws if the aGUID argument is not specified
   */
  getAddonBySyncGUID: function(aGUID) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!aGUID || typeof aGUID != "string")
      throw Components.Exception("aGUID must be a non-empty string",
                                 Cr.NS_ERROR_INVALID_ARG);

    return Task.spawn(function*() {
      for (let provider of this.providers) {
        let addon = yield promiseCallProvider(
          provider, "getAddonBySyncGUID", aGUID);

        if (addon)
          return addon;
      }

      return null;
    }.bind(this));
  },

  /**
   * Asynchronously gets an array of add-ons.
   *
   * @param  aIDs
   *         The array of IDs to retrieve
   * @return {Promise}
   * @resolves The array of found add-ons.
   * @rejects  Never
   * @throws if the aIDs argument is not specified
   */
  getAddonsByIDs: function(aIDs) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (!Array.isArray(aIDs))
      throw Components.Exception("aIDs must be an array",
                                 Cr.NS_ERROR_INVALID_ARG);

    let promises = aIDs.map(a => AddonManagerInternal.getAddonByID(a));
    return Promise.all(promises);
  },

  /**
   * Asynchronously gets add-ons of specific types.
   *
   * @param  aTypes
   *         An optional array of types to retrieve. Each type is a string name
   */
  getAddonsByTypes: function(aTypes) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (aTypes && !Array.isArray(aTypes))
      throw Components.Exception("aTypes must be an array or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    return Task.spawn(function*() {
      let addons = [];

      for (let provider of this.providers) {
        let providerAddons = yield promiseCallProvider(
          provider, "getAddonsByTypes", aTypes);

        if (providerAddons)
          addons.push(...providerAddons);
      }

      return addons;
    }.bind(this));
  },

  /**
   * Asynchronously gets all installed add-ons.
   */
  getAllAddons: function() {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    return this.getAddonsByTypes(null);
  },

  /**
   * Asynchronously gets add-ons that have operations waiting for an application
   * restart to complete.
   *
   * @param  aTypes
   *         An optional array of types to retrieve. Each type is a string name
   */
  getAddonsWithOperationsByTypes: function(aTypes) {
    if (!gStarted)
      throw Components.Exception("AddonManager is not initialized",
                                 Cr.NS_ERROR_NOT_INITIALIZED);

    if (aTypes && !Array.isArray(aTypes))
      throw Components.Exception("aTypes must be an array or null",
                                 Cr.NS_ERROR_INVALID_ARG);

    return Task.spawn(function*() {
      let addons = [];

      for (let provider of this.providers) {
        let providerAddons = yield promiseCallProvider(
          provider, "getAddonsWithOperationsByTypes", aTypes);

        if (providerAddons)
          addons.push(...providerAddons);
      }

      return addons;
    }.bind(this));
  },

  /**
   * Adds a new AddonManagerListener if the listener is not already registered.
   *
   * @param  aListener
   *         The listener to add
   */
  addManagerListener: function(aListener) {
    if (!aListener || typeof aListener != "object")
      throw Components.Exception("aListener must be an AddonManagerListener object",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!this.managerListeners.some(i => i == aListener))
      this.managerListeners.push(aListener);
  },

  /**
   * Removes an AddonManagerListener if the listener is registered.
   *
   * @param  aListener
   *         The listener to remove
   */
  removeManagerListener: function(aListener) {
    if (!aListener || typeof aListener != "object")
      throw Components.Exception("aListener must be an AddonManagerListener object",
                                 Cr.NS_ERROR_INVALID_ARG);

    let pos = 0;
    while (pos < this.managerListeners.length) {
      if (this.managerListeners[pos] == aListener)
        this.managerListeners.splice(pos, 1);
      else
        pos++;
    }
  },

  /**
   * Adds a new AddonListener if the listener is not already registered.
   *
   * @param  aListener
   *         The AddonListener to add
   */
  addAddonListener: function(aListener) {
    if (!aListener || typeof aListener != "object")
      throw Components.Exception("aListener must be an AddonListener object",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!this.addonListeners.some(i => i == aListener))
      this.addonListeners.push(aListener);
  },

  /**
   * Removes an AddonListener if the listener is registered.
   *
   * @param  aListener
   *         The AddonListener to remove
   */
  removeAddonListener: function(aListener) {
    if (!aListener || typeof aListener != "object")
      throw Components.Exception("aListener must be an AddonListener object",
                                 Cr.NS_ERROR_INVALID_ARG);

    let pos = 0;
    while (pos < this.addonListeners.length) {
      if (this.addonListeners[pos] == aListener)
        this.addonListeners.splice(pos, 1);
      else
        pos++;
    }
  },

  /**
   * Adds a new TypeListener if the listener is not already registered.
   *
   * @param  aListener
   *         The TypeListener to add
   */
  addTypeListener: function(aListener) {
    if (!aListener || typeof aListener != "object")
      throw Components.Exception("aListener must be a TypeListener object",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!this.typeListeners.some(i => i == aListener))
      this.typeListeners.push(aListener);
  },

  /**
   * Removes an TypeListener if the listener is registered.
   *
   * @param  aListener
   *         The TypeListener to remove
   */
  removeTypeListener: function(aListener) {
    if (!aListener || typeof aListener != "object")
      throw Components.Exception("aListener must be a TypeListener object",
                                 Cr.NS_ERROR_INVALID_ARG);

    let pos = 0;
    while (pos < this.typeListeners.length) {
      if (this.typeListeners[pos] == aListener)
        this.typeListeners.splice(pos, 1);
      else
        pos++;
    }
  },

  get addonTypes() {
    // A read-only wrapper around the types dictionary
    return new Proxy(this.types, {
      defineProperty(target, property, descriptor) {
        // Not allowed to define properties
        return false;
      },

      deleteProperty(target, property) {
        // Not allowed to delete properties
        return false;
      },

      get(target, property, receiver) {
        if (!target.hasOwnProperty(property))
          return undefined;

        return target[property].type;
      },

      getOwnPropertyDescriptor(target, property) {
        if (!target.hasOwnProperty(property))
          return undefined;

        return {
          value: target[property].type,
          writable: false,
          // Claim configurability to maintain the proxy invariants.
          configurable: true,
          enumerable: true
        }
      },

      preventExtensions(target) {
        // Not allowed to prevent adding new properties
        return false;
      },

      set(target, property, value, receiver) {
        // Not allowed to set properties
        return false;
      },

      setPrototypeOf(target, prototype) {
        // Not allowed to change prototype
        return false;
      }
    });
  },

  get autoUpdateDefault() {
    return gAutoUpdateDefault;
  },

  set autoUpdateDefault(aValue) {
    aValue = !!aValue;
    if (aValue != gAutoUpdateDefault)
      Services.prefs.setBoolPref(PREF_EM_AUTOUPDATE_DEFAULT, aValue);
    return aValue;
  },

  get checkCompatibility() {
    return gCheckCompatibility;
  },

  set checkCompatibility(aValue) {
    aValue = !!aValue;
    if (aValue != gCheckCompatibility) {
      if (!aValue)
        Services.prefs.setBoolPref(PREF_EM_CHECK_COMPATIBILITY, false);
      else
        Services.prefs.clearUserPref(PREF_EM_CHECK_COMPATIBILITY);
    }
    return aValue;
  },

  get strictCompatibility() {
    return gStrictCompatibility;
  },

  set strictCompatibility(aValue) {
    aValue = !!aValue;
    if (aValue != gStrictCompatibility)
      Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, aValue);
    return aValue;
  },

  get checkUpdateSecurityDefault() {
    return gCheckUpdateSecurityDefault;
  },

  get checkUpdateSecurity() {
    return gCheckUpdateSecurity;
  },

  set checkUpdateSecurity(aValue) {
    aValue = !!aValue;
    if (aValue != gCheckUpdateSecurity) {
      if (aValue != gCheckUpdateSecurityDefault)
        Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, aValue);
      else
        Services.prefs.clearUserPref(PREF_EM_CHECK_UPDATE_SECURITY);
    }
    return aValue;
  },

  get updateEnabled() {
    return gUpdateEnabled;
  },

  set updateEnabled(aValue) {
    aValue = !!aValue;
    if (aValue != gUpdateEnabled)
      Services.prefs.setBoolPref(PREF_EM_UPDATE_ENABLED, aValue);
    return aValue;
  },

  get hotfixID() {
    return gHotfixID;
  },

  webAPI: {
    // installs maps integer ids to AddonInstall instances.
    installs: new Map(),
    nextInstall: 0,

    sendEvent: null,
    setEventHandler(fn) {
      this.sendEvent = fn;
    },

    getAddonByID(target, id) {
      return new Promise(resolve => {
        AddonManager.getAddonByID(id, (addon) => {
          resolve(webAPIForAddon(addon));
        });
      });
    },

    // helper to copy (and convert) the properties we care about
    copyProps(install, obj) {
      obj.state = AddonManager.stateToString(install.state);
      obj.error = AddonManager.errorToString(install.error);
      obj.progress = install.progress;
      obj.maxProgress = install.maxProgress;
    },

    makeListener(id, mm) {
      const events = [
        "onDownloadStarted",
        "onDownloadProgress",
        "onDownloadEnded",
        "onDownloadCancelled",
        "onDownloadFailed",
        "onInstallStarted",
        "onInstallEnded",
        "onInstallCancelled",
        "onInstallFailed",
      ];

      let listener = {};
      events.forEach(event => {
        listener[event] = (install) => {
          let data = {event, id};
          AddonManager.webAPI.copyProps(install, data);
          this.sendEvent(mm, data);
        }
      });
      return listener;
    },

    forgetInstall(id) {
      let info = this.installs.get(id);
      if (!info) {
        throw new Error(`forgetInstall cannot find ${id}`);
      }
      info.install.removeListener(info.listener);
      this.installs.delete(id);
    },

    createInstall(target, options) {
      // Throw an appropriate error if the given URL is not valid
      // as an installation source.  Return silently if it is okay.
      function checkInstallUrl(url) {
        let host = Services.io.newURI(options.url, null, null).host;
        if (WEBAPI_INSTALL_HOSTS.includes(host)) {
          return;
        }
        if (Services.prefs.getBoolPref(PREF_WEBAPI_TESTING)
            && WEBAPI_TEST_INSTALL_HOSTS.includes(host)) {
          return;
        }

        throw new Error(`Install from ${host} not permitted`);
      }

      try {
        checkInstallUrl(options.url);
      } catch (err) {
        return Promise.reject({message: err.message});
      }

      return AddonManagerInternal.getInstallForURL(options.url, "application/x-xpinstall",
                                                   options.hash).then(install => {
        let id = this.nextInstall++;
        let listener = this.makeListener(id, target.messageManager);
        install.addListener(listener);

        this.installs.set(id, {install, target, listener});

        let result = {id};
        this.copyProps(install, result);
        return result;
      });
    },

    addonUninstall(target, id) {
      return new Promise(resolve => {
        AddonManager.getAddonByID(id, addon => {
          if (!addon) {
            resolve(false);
          }

          try {
            addon.uninstall();
            resolve(true);
          } catch (err) {
            Cu.reportError(err);
            resolve(false);
          }
        });
      });
    },

    addonSetEnabled(target, id, value) {
      return new Promise((resolve, reject) => {
        AddonManager.getAddonByID(id, addon => {
          if (!addon) {
            reject({message: `No such addon ${id}`});
          }
          addon.userDisabled = !value;
          resolve();
        });
      });
    },

    addonInstallDoInstall(target, id) {
      let state = this.installs.get(id);
      if (!state) {
        return Promise.reject(`invalid id ${id}`);
      }
      return Promise.resolve(state.install.install());
    },

    addonInstallCancel(target, id) {
      let state = this.installs.get(id);
      if (!state) {
        return Promise.reject(`invalid id ${id}`);
      }
      return Promise.resolve(state.install.cancel());
    },

    clearInstalls(ids) {
      for (let id of ids) {
        this.forgetInstall(id);
      }
    },

    clearInstallsFrom(mm) {
      for (let [id, info] of this.installs) {
        if (info.target.messageManager == mm) {
          this.forgetInstall(id);
        }
      }
    },
  },
};

/**
 * Should not be used outside of core Mozilla code. This is a private API for
 * the startup and platform integration code to use. Refer to the methods on
 * AddonManagerInternal for documentation however note that these methods are
 * subject to change at any time.
 */
this.AddonManagerPrivate = {
  startup: function() {
    AddonManagerInternal.startup();
  },

  registerProvider: function(aProvider, aTypes) {
    AddonManagerInternal.registerProvider(aProvider, aTypes);
  },

  unregisterProvider: function(aProvider) {
    AddonManagerInternal.unregisterProvider(aProvider);
  },

  markProviderSafe: function(aProvider) {
    AddonManagerInternal.markProviderSafe(aProvider);
  },

  backgroundUpdateCheck: function() {
    return AddonManagerInternal.backgroundUpdateCheck();
  },

  backgroundUpdateTimerHandler() {
    // Don't call through to the real update check if no checks are enabled.
    let checkHotfix = AddonManagerInternal.hotfixID &&
                      Services.prefs.getBoolPref(PREF_APP_UPDATE_ENABLED) &&
                      Services.prefs.getBoolPref(PREF_APP_UPDATE_AUTO);

    if (!AddonManagerInternal.updateEnabled && !checkHotfix) {
      logger.info("Skipping background update check");
      return;
    }
    // Don't return the promise here, since the caller doesn't care.
    AddonManagerInternal.backgroundUpdateCheck();
  },

  addStartupChange: function(aType, aID) {
    AddonManagerInternal.addStartupChange(aType, aID);
  },

  removeStartupChange: function(aType, aID) {
    AddonManagerInternal.removeStartupChange(aType, aID);
  },

  notifyAddonChanged: function(aID, aType, aPendingRestart) {
    AddonManagerInternal.notifyAddonChanged(aID, aType, aPendingRestart);
  },

  updateAddonAppDisabledStates: function() {
    AddonManagerInternal.updateAddonAppDisabledStates();
  },

  updateAddonRepositoryData: function(aCallback) {
    return promiseOrCallback(
      AddonManagerInternal.updateAddonRepositoryData(),
      aCallback);
  },

  callInstallListeners: function(...aArgs) {
    return AddonManagerInternal.callInstallListeners.apply(AddonManagerInternal,
                                                           aArgs);
  },

  callAddonListeners: function(...aArgs) {
    AddonManagerInternal.callAddonListeners.apply(AddonManagerInternal, aArgs);
  },

  AddonAuthor: AddonAuthor,

  AddonScreenshot: AddonScreenshot,

  AddonCompatibilityOverride: AddonCompatibilityOverride,

  AddonType: AddonType,

  recordTimestamp: function(name, value) {
    AddonManagerInternal.recordTimestamp(name, value);
  },

  _simpleMeasures: {},
  recordSimpleMeasure: function(name, value) {
    this._simpleMeasures[name] = value;
  },

  recordException: function(aModule, aContext, aException) {
    let report = {
      module: aModule,
      context: aContext
    };

    if (typeof aException == "number") {
      report.message = Components.Exception("", aException).name;
    }
    else {
      report.message = aException.toString();
      if (aException.fileName) {
        report.file = aException.fileName;
        report.line = aException.lineNumber;
      }
    }

    this._simpleMeasures.exception = report;
  },

  getSimpleMeasures: function() {
    return this._simpleMeasures;
  },

  getTelemetryDetails: function() {
    return AddonManagerInternal.telemetryDetails;
  },

  setTelemetryDetails: function(aProvider, aDetails) {
    AddonManagerInternal.telemetryDetails[aProvider] = aDetails;
  },

  // Start a timer, record a simple measure of the time interval when
  // timer.done() is called
  simpleTimer: function(aName) {
    let startTime = Cu.now();
    return {
      done: () => this.recordSimpleMeasure(aName, Math.round(Cu.now() - startTime))
    };
  },

  /**
   * Helper to call update listeners when no update is available.
   *
   * This can be used as an implementation for Addon.findUpdates() when
   * no update mechanism is available.
   */
  callNoUpdateListeners: function(addon, listener, reason, appVersion, platformVersion) {
    if ("onNoCompatibilityUpdateAvailable" in listener) {
      safeCall(listener.onNoCompatibilityUpdateAvailable.bind(listener), addon);
    }
    if ("onNoUpdateAvailable" in listener) {
      safeCall(listener.onNoUpdateAvailable.bind(listener), addon);
    }
    if ("onUpdateFinished" in listener) {
      safeCall(listener.onUpdateFinished.bind(listener), addon);
    }
  },

  get webExtensionsMinPlatformVersion() {
    return gWebExtensionsMinPlatformVersion;
  },

  hasUpgradeListener: function(aId) {
    return AddonManagerInternal.upgradeListeners.has(aId);
  },

  getUpgradeListener: function(aId) {
    return AddonManagerInternal.upgradeListeners.get(aId);
  },
};

/**
 * This is the public API that UI and developers should be calling. All methods
 * just forward to AddonManagerInternal.
 */
this.AddonManager = {
  // Constants for the AddonInstall.state property
  // These will show up as AddonManager.STATE_* (eg, STATE_AVAILABLE)
  _states: new Map([
    // The install is available for download.
    ["STATE_AVAILABLE",  0],
    // The install is being downloaded.
    ["STATE_DOWNLOADING",  1],
    // The install is checking for compatibility information.
    ["STATE_CHECKING", 2],
    // The install is downloaded and ready to install.
    ["STATE_DOWNLOADED", 3],
    // The download failed.
    ["STATE_DOWNLOAD_FAILED", 4],
    // The install may not proceed until the user accepts permissions
    ["STATE_AWAITING_PERMISSIONS", 5],
    // Any permission prompts are done
    ["STATE_PERMISSION_GRANTED", 6],
    // The install has been postponed.
    ["STATE_POSTPONED", 7],
    // The install is ready to be applied.
    ["STATE_READY", 8],
    // The add-on is being installed.
    ["STATE_INSTALLING", 9],
    // The add-on has been installed.
    ["STATE_INSTALLED", 10],
    // The install failed.
    ["STATE_INSTALL_FAILED", 11],
    // The install has been cancelled.
    ["STATE_CANCELLED", 12],
  ]),

  // Constants representing different types of errors while downloading an
  // add-on.
  // These will show up as AddonManager.ERROR_* (eg, ERROR_NETWORK_FAILURE)
  _errors: new Map([
    // The download failed due to network problems.
    ["ERROR_NETWORK_FAILURE", -1],
    // The downloaded file did not match the provided hash.
    ["ERROR_INCORRECT_HASH", -2],
    // The downloaded file seems to be corrupted in some way.
    ["ERROR_CORRUPT_FILE", -3],
    // An error occured trying to write to the filesystem.
    ["ERROR_FILE_ACCESS", -4],
    // The add-on must be signed and isn't.
    ["ERROR_SIGNEDSTATE_REQUIRED", -5],
    // The downloaded add-on had a different type than expected.
    ["ERROR_UNEXPECTED_ADDON_TYPE", -6],
    // The addon did not have the expected ID
    ["ERROR_INCORRECT_ID", -7],
  ]),

  // These must be kept in sync with AddonUpdateChecker.
  // No error was encountered.
  UPDATE_STATUS_NO_ERROR: 0,
  // The update check timed out
  UPDATE_STATUS_TIMEOUT: -1,
  // There was an error while downloading the update information.
  UPDATE_STATUS_DOWNLOAD_ERROR: -2,
  // The update information was malformed in some way.
  UPDATE_STATUS_PARSE_ERROR: -3,
  // The update information was not in any known format.
  UPDATE_STATUS_UNKNOWN_FORMAT: -4,
  // The update information was not correctly signed or there was an SSL error.
  UPDATE_STATUS_SECURITY_ERROR: -5,
  // The update was cancelled.
  UPDATE_STATUS_CANCELLED: -6,

  // Constants to indicate why an update check is being performed
  // Update check has been requested by the user.
  UPDATE_WHEN_USER_REQUESTED: 1,
  // Update check is necessary to see if the Addon is compatibile with a new
  // version of the application.
  UPDATE_WHEN_NEW_APP_DETECTED: 2,
  // Update check is necessary because a new application has been installed.
  UPDATE_WHEN_NEW_APP_INSTALLED: 3,
  // Update check is a regular background update check.
  UPDATE_WHEN_PERIODIC_UPDATE: 16,
  // Update check is needed to check an Addon that is being installed.
  UPDATE_WHEN_ADDON_INSTALLED: 17,

  // Constants for operations in Addon.pendingOperations
  // Indicates that the Addon has no pending operations.
  PENDING_NONE: 0,
  // Indicates that the Addon will be enabled after the application restarts.
  PENDING_ENABLE: 1,
  // Indicates that the Addon will be disabled after the application restarts.
  PENDING_DISABLE: 2,
  // Indicates that the Addon will be uninstalled after the application restarts.
  PENDING_UNINSTALL: 4,
  // Indicates that the Addon will be installed after the application restarts.
  PENDING_INSTALL: 8,
  PENDING_UPGRADE: 16,

  // Constants for operations in Addon.operationsRequiringRestart
  // Indicates that restart isn't required for any operation.
  OP_NEEDS_RESTART_NONE: 0,
  // Indicates that restart is required for enabling the addon.
  OP_NEEDS_RESTART_ENABLE: 1,
  // Indicates that restart is required for disabling the addon.
  OP_NEEDS_RESTART_DISABLE: 2,
  // Indicates that restart is required for uninstalling the addon.
  OP_NEEDS_RESTART_UNINSTALL: 4,
  // Indicates that restart is required for installing the addon.
  OP_NEEDS_RESTART_INSTALL: 8,

  // Constants for permissions in Addon.permissions.
  // Indicates that the Addon can be uninstalled.
  PERM_CAN_UNINSTALL: 1,
  // Indicates that the Addon can be enabled by the user.
  PERM_CAN_ENABLE: 2,
  // Indicates that the Addon can be disabled by the user.
  PERM_CAN_DISABLE: 4,
  // Indicates that the Addon can be upgraded.
  PERM_CAN_UPGRADE: 8,
  // Indicates that the Addon can be set to be optionally enabled
  // on a case-by-case basis.
  PERM_CAN_ASK_TO_ACTIVATE: 16,

  // General descriptions of where items are installed.
  // Installed in this profile.
  SCOPE_PROFILE: 1,
  // Installed for all of this user's profiles.
  SCOPE_USER: 2,
  // Installed and owned by the application.
  SCOPE_APPLICATION: 4,
  // Installed for all users of the computer.
  SCOPE_SYSTEM: 8,
  // Installed temporarily
  SCOPE_TEMPORARY: 16,
  // The combination of all scopes.
  SCOPE_ALL: 31,

  // Add-on type is expected to be displayed in the UI in a list.
  VIEW_TYPE_LIST: "list",

  // Constants describing how add-on types behave.

  // If no add-ons of a type are installed, then the category for that add-on
  // type should be hidden in the UI.
  TYPE_UI_HIDE_EMPTY: 16,
  // Indicates that this add-on type supports the ask-to-activate state.
  // That is, add-ons of this type can be set to be optionally enabled
  // on a case-by-case basis.
  TYPE_SUPPORTS_ASK_TO_ACTIVATE: 32,
  // The add-on type natively supports undo for restartless uninstalls.
  // If this flag is not specified, the UI is expected to handle this via
  // disabling the add-on, and performing the actual uninstall at a later time.
  TYPE_SUPPORTS_UNDO_RESTARTLESS_UNINSTALL: 64,

  // Constants for Addon.applyBackgroundUpdates.
  // Indicates that the Addon should not update automatically.
  AUTOUPDATE_DISABLE: 0,
  // Indicates that the Addon should update automatically only if
  // that's the global default.
  AUTOUPDATE_DEFAULT: 1,
  // Indicates that the Addon should update automatically.
  AUTOUPDATE_ENABLE: 2,

  // Constants for how Addon options should be shown.
  // Options will be opened in a new window
  OPTIONS_TYPE_DIALOG: 1,
  // Options will be displayed within the AM detail view
  OPTIONS_TYPE_INLINE: 2,
  // Options will be displayed in a new tab, if possible
  OPTIONS_TYPE_TAB: 3,
  // Same as OPTIONS_TYPE_INLINE, but no Preferences button will be shown.
  // Used to indicate that only non-interactive information will be shown.
  OPTIONS_TYPE_INLINE_INFO: 4,
  // Similar to OPTIONS_TYPE_INLINE, but rather than generating inline
  // options from a specially-formatted XUL file, the contents of the
  // file are simply displayed in an inline <browser> element.
  OPTIONS_TYPE_INLINE_BROWSER: 5,

  // Constants for displayed or hidden options notifications
  // Options notification will be displayed
  OPTIONS_NOTIFICATION_DISPLAYED: "addon-options-displayed",
  // Options notification will be hidden
  OPTIONS_NOTIFICATION_HIDDEN: "addon-options-hidden",

  // Constants for getStartupChanges, addStartupChange and removeStartupChange
  // Add-ons that were detected as installed during startup. Doesn't include
  // add-ons that were pending installation the last time the application ran.
  STARTUP_CHANGE_INSTALLED: "installed",
  // Add-ons that were detected as changed during startup. This includes an
  // add-on moving to a different location, changing version or just having
  // been detected as possibly changed.
  STARTUP_CHANGE_CHANGED: "changed",
  // Add-ons that were detected as uninstalled during startup. Doesn't include
  // add-ons that were pending uninstallation the last time the application ran.
  STARTUP_CHANGE_UNINSTALLED: "uninstalled",
  // Add-ons that were detected as disabled during startup, normally because of
  // an application change making an add-on incompatible. Doesn't include
  // add-ons that were pending being disabled the last time the application ran.
  STARTUP_CHANGE_DISABLED: "disabled",
  // Add-ons that were detected as enabled during startup, normally because of
  // an application change making an add-on compatible. Doesn't include
  // add-ons that were pending being enabled the last time the application ran.
  STARTUP_CHANGE_ENABLED: "enabled",

  // Constants for Addon.signedState. Any states that should cause an add-on
  // to be unusable in builds that require signing should have negative values.
  // Add-on signing is not required, e.g. because the pref is disabled.
  SIGNEDSTATE_NOT_REQUIRED: undefined,
  // Add-on is signed but signature verification has failed.
  SIGNEDSTATE_BROKEN: -2,
  // Add-on may be signed but by an certificate that doesn't chain to our
  // our trusted certificate.
  SIGNEDSTATE_UNKNOWN: -1,
  // Add-on is unsigned.
  SIGNEDSTATE_MISSING: 0,
  // Add-on is preliminarily reviewed.
  SIGNEDSTATE_PRELIMINARY: 1,
  // Add-on is fully reviewed.
  SIGNEDSTATE_SIGNED: 2,
  // Add-on is system add-on.
  SIGNEDSTATE_SYSTEM: 3,

  // Constants for the Addon.userDisabled property
  // Indicates that the userDisabled state of this add-on is currently
  // ask-to-activate. That is, it can be conditionally enabled on a
  // case-by-case basis.
  STATE_ASK_TO_ACTIVATE: "askToActivate",

  get __AddonManagerInternal__() {
    return AppConstants.DEBUG ? AddonManagerInternal : undefined;
  },

  get isReady() {
    return gStartupComplete && !gShutdownInProgress;
  },

  init() {
    this._stateToString = new Map();
    for (let [name, value] of this._states) {
      this[name] = value;
      this._stateToString.set(value, name);
    }
    this._errorToString = new Map();
    for (let [name, value] of this._errors) {
      this[name] = value;
      this._errorToString.set(value, name);
    }
  },

  stateToString(state) {
    return this._stateToString.get(state);
  },

  errorToString(err) {
    return err ? this._errorToString.get(err) : null;
  },

  getInstallForURL: function(aUrl, aCallback, aMimetype,
                                                 aHash, aName, aIcons,
                                                 aVersion, aBrowser) {
    return promiseOrCallback(
      AddonManagerInternal.getInstallForURL(aUrl, aMimetype, aHash,
                                            aName, aIcons, aVersion, aBrowser),
      aCallback);
  },

  getInstallForFile: function(aFile, aCallback, aMimetype) {
    return promiseOrCallback(
      AddonManagerInternal.getInstallForFile(aFile, aMimetype),
      aCallback);
  },

  /**
   * Gets an array of add-on IDs that changed during the most recent startup.
   *
   * @param  aType
   *         The type of startup change to get
   * @return An array of add-on IDs
   */
  getStartupChanges: function(aType) {
    if (!(aType in AddonManagerInternal.startupChanges))
      return [];
    return AddonManagerInternal.startupChanges[aType].slice(0);
  },

  getAddonByID: function(aID, aCallback) {
    return promiseOrCallback(
      AddonManagerInternal.getAddonByID(aID),
      aCallback);
  },

  getAddonBySyncGUID: function(aGUID, aCallback) {
    return promiseOrCallback(
      AddonManagerInternal.getAddonBySyncGUID(aGUID),
      aCallback);
  },

  getAddonsByIDs: function(aIDs, aCallback) {
    return promiseOrCallback(
      AddonManagerInternal.getAddonsByIDs(aIDs),
      aCallback);
  },

  getAddonsWithOperationsByTypes: function(aTypes, aCallback) {
    return promiseOrCallback(
      AddonManagerInternal.getAddonsWithOperationsByTypes(aTypes),
      aCallback);
  },

  getAddonsByTypes: function(aTypes, aCallback) {
    return promiseOrCallback(
      AddonManagerInternal.getAddonsByTypes(aTypes),
      aCallback);
  },

  getAllAddons: function(aCallback) {
    return promiseOrCallback(
      AddonManagerInternal.getAllAddons(),
      aCallback);
  },

  getInstallsByTypes: function(aTypes, aCallback) {
    return promiseOrCallback(
      AddonManagerInternal.getInstallsByTypes(aTypes),
      aCallback);
  },

  getAllInstalls: function(aCallback) {
    return promiseOrCallback(
      AddonManagerInternal.getAllInstalls(),
      aCallback);
  },

  mapURIToAddonID: function(aURI) {
    return AddonManagerInternal.mapURIToAddonID(aURI);
  },

  isInstallEnabled: function(aType) {
    return AddonManagerInternal.isInstallEnabled(aType);
  },

  isInstallAllowed: function(aType, aInstallingPrincipal) {
    return AddonManagerInternal.isInstallAllowed(aType, aInstallingPrincipal);
  },

  installAddonsFromWebpage: function(aType, aBrowser, aInstallingPrincipal,
                                     aInstalls) {
    AddonManagerInternal.installAddonsFromWebpage(aType, aBrowser,
                                                  aInstallingPrincipal,
                                                  aInstalls);
  },

  installTemporaryAddon: function(aDirectory) {
    return AddonManagerInternal.installTemporaryAddon(aDirectory);
  },

  installAddonFromSources: function(aDirectory) {
    return AddonManagerInternal.installAddonFromSources(aDirectory);
  },

  getAddonByInstanceID: function(aInstanceID) {
    return AddonManagerInternal.getAddonByInstanceID(aInstanceID);
  },

  addManagerListener: function(aListener) {
    AddonManagerInternal.addManagerListener(aListener);
  },

  removeManagerListener: function(aListener) {
    AddonManagerInternal.removeManagerListener(aListener);
  },

  addInstallListener: function(aListener) {
    AddonManagerInternal.addInstallListener(aListener);
  },

  removeInstallListener: function(aListener) {
    AddonManagerInternal.removeInstallListener(aListener);
  },

  getUpgradeListener: function(aId) {
    return AddonManagerInternal.upgradeListeners.get(aId);
  },

  addUpgradeListener: function(aInstanceID, aCallback) {
    AddonManagerInternal.addUpgradeListener(aInstanceID, aCallback);
  },

  removeUpgradeListener: function(aInstanceID) {
    AddonManagerInternal.removeUpgradeListener(aInstanceID);
  },

  addAddonListener: function(aListener) {
    AddonManagerInternal.addAddonListener(aListener);
  },

  removeAddonListener: function(aListener) {
    AddonManagerInternal.removeAddonListener(aListener);
  },

  addTypeListener: function(aListener) {
    AddonManagerInternal.addTypeListener(aListener);
  },

  removeTypeListener: function(aListener) {
    AddonManagerInternal.removeTypeListener(aListener);
  },

  get addonTypes() {
    return AddonManagerInternal.addonTypes;
  },

  /**
   * Determines whether an Addon should auto-update or not.
   *
   * @param  aAddon
   *         The Addon representing the add-on
   * @return true if the addon should auto-update, false otherwise.
   */
  shouldAutoUpdate: function(aAddon) {
    if (!aAddon || typeof aAddon != "object")
      throw Components.Exception("aAddon must be specified",
                                 Cr.NS_ERROR_INVALID_ARG);

    if (!("applyBackgroundUpdates" in aAddon))
      return false;
    if (aAddon.applyBackgroundUpdates == AddonManager.AUTOUPDATE_ENABLE)
      return true;
    if (aAddon.applyBackgroundUpdates == AddonManager.AUTOUPDATE_DISABLE)
      return false;
    return this.autoUpdateDefault;
  },

  get checkCompatibility() {
    return AddonManagerInternal.checkCompatibility;
  },

  set checkCompatibility(aValue) {
    AddonManagerInternal.checkCompatibility = aValue;
  },

  get strictCompatibility() {
    return AddonManagerInternal.strictCompatibility;
  },

  set strictCompatibility(aValue) {
    AddonManagerInternal.strictCompatibility = aValue;
  },

  get checkUpdateSecurityDefault() {
    return AddonManagerInternal.checkUpdateSecurityDefault;
  },

  get checkUpdateSecurity() {
    return AddonManagerInternal.checkUpdateSecurity;
  },

  set checkUpdateSecurity(aValue) {
    AddonManagerInternal.checkUpdateSecurity = aValue;
  },

  get updateEnabled() {
    return AddonManagerInternal.updateEnabled;
  },

  set updateEnabled(aValue) {
    AddonManagerInternal.updateEnabled = aValue;
  },

  get autoUpdateDefault() {
    return AddonManagerInternal.autoUpdateDefault;
  },

  set autoUpdateDefault(aValue) {
    AddonManagerInternal.autoUpdateDefault = aValue;
  },

  get hotfixID() {
    return AddonManagerInternal.hotfixID;
  },

  escapeAddonURI: function(aAddon, aUri, aAppVersion) {
    return AddonManagerInternal.escapeAddonURI(aAddon, aUri, aAppVersion);
  },

  getPreferredIconURL: function(aAddon, aSize, aWindow = undefined) {
    return AddonManagerInternal.getPreferredIconURL(aAddon, aSize, aWindow);
  },

  get webAPI() {
    return AddonManagerInternal.webAPI;
  },

  get shutdown() {
    return gShutdownBarrier.client;
  },
};

this.AddonManager.init();

// load the timestamps module into AddonManagerInternal
Cu.import("resource://gre/modules/TelemetryTimestamps.jsm", AddonManagerInternal);
Object.freeze(AddonManagerInternal);
Object.freeze(AddonManagerPrivate);
Object.freeze(AddonManager);
