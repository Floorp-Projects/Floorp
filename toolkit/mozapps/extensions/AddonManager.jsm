/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Cannot use Services.appinfo here, or else xpcshell-tests will blow up, as
// most tests later register different nsIAppInfo implementations, which
// wouldn't be reflected in Services.appinfo anymore, as the lazy getter
// underlying it would have been initialized if we used it here.
if ("@mozilla.org/xre/app-info;1" in Cc) {
  // eslint-disable-next-line mozilla/use-services
  let runtime = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);
  if (runtime.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
    // Refuse to run in child processes.
    throw new Error("You cannot use the AddonManager in child processes!");
  }
}

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const MOZ_COMPATIBILITY_NIGHTLY = ![
  "aurora",
  "beta",
  "release",
  "esr",
].includes(AppConstants.MOZ_UPDATE_CHANNEL);

const INTL_LOCALES_CHANGED = "intl:app-locales-changed";

const PREF_AMO_ABUSEREPORT = "extensions.abuseReport.amWebAPI.enabled";
const PREF_BLOCKLIST_PINGCOUNTVERSION = "extensions.blocklist.pingCountVersion";
const PREF_EM_UPDATE_ENABLED = "extensions.update.enabled";
const PREF_EM_LAST_APP_VERSION = "extensions.lastAppVersion";
const PREF_EM_LAST_PLATFORM_VERSION = "extensions.lastPlatformVersion";
const PREF_EM_AUTOUPDATE_DEFAULT = "extensions.update.autoUpdateDefault";
const PREF_EM_STRICT_COMPATIBILITY = "extensions.strictCompatibility";
const PREF_EM_CHECK_UPDATE_SECURITY = "extensions.checkUpdateSecurity";
const PREF_SYS_ADDON_UPDATE_ENABLED = "extensions.systemAddon.update.enabled";

const PREF_MIN_WEBEXT_PLATFORM_VERSION =
  "extensions.webExtensionsMinPlatformVersion";
const PREF_WEBAPI_TESTING = "extensions.webapi.testing";
const PREF_EM_POSTDOWNLOAD_THIRD_PARTY =
  "extensions.postDownloadThirdPartyPrompt";

const UPDATE_REQUEST_VERSION = 2;

const BRANCH_REGEXP = /^([^\.]+\.[0-9]+[a-z]*).*/gi;
const PREF_EM_CHECK_COMPATIBILITY_BASE = "extensions.checkCompatibility";
var PREF_EM_CHECK_COMPATIBILITY = MOZ_COMPATIBILITY_NIGHTLY
  ? PREF_EM_CHECK_COMPATIBILITY_BASE + ".nightly"
  : undefined;

const VALID_TYPES_REGEXP = /^[\w\-]+$/;

const WEBAPI_INSTALL_HOSTS = ["addons.mozilla.org"];
const WEBAPI_TEST_INSTALL_HOSTS = [
  "addons.allizom.org",
  "addons-dev.allizom.org",
  "example.com",
];

const AMO_ATTRIBUTION_ALLOWED_SOURCES = ["amo", "disco"];
const AMO_ATTRIBUTION_DATA_KEYS = [
  "utm_campaign",
  "utm_content",
  "utm_medium",
  "utm_source",
];
const AMO_ATTRIBUTION_DATA_MAX_LENGTH = 40;

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
// This global is overridden by xpcshell tests, and therefore cannot be
// a const.
var { AsyncShutdown } = ChromeUtils.import(
  "resource://gre/modules/AsyncShutdown.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["Element"]);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonRepository: "resource://gre/modules/addons/AddonRepository.jsm",
  AbuseReporter: "resource://gre/modules/AbuseReporter.jsm",
  Extension: "resource://gre/modules/Extension.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "WEBEXT_POSTDOWNLOAD_THIRD_PARTY",
  PREF_EM_POSTDOWNLOAD_THIRD_PARTY,
  false
);

// Initialize the WebExtension process script service as early as possible,
// since it needs to be able to track things like new frameLoader globals that
// are created before other framework code has been initialized.
Services.ppmm.loadProcessScript(
  "resource://gre/modules/extensionProcessScriptLoader.js",
  true
);

const INTEGER = /^[1-9]\d*$/;

var EXPORTED_SYMBOLS = ["AddonManager", "AddonManagerPrivate", "AMTelemetry"];

const CATEGORY_PROVIDER_MODULE = "addon-provider-module";

// A list of providers to load by default
const DEFAULT_PROVIDERS = ["resource://gre/modules/addons/XPIProvider.jsm"];

const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
// Configure a logger at the parent 'addons' level to format
// messages for all the modules under addons.*
const PARENT_LOGGER_ID = "addons";
var parentLogger = Log.repository.getLogger(PARENT_LOGGER_ID);
parentLogger.level = Log.Level.Warn;
var formatter = new Log.BasicFormatter();
// Set parent logger (and its children) to append to
// the Javascript section of the Browser Console
parentLogger.addAppender(new Log.ConsoleAppender(formatter));

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
  init() {
    Services.prefs.addObserver(PREF_LOGGING_ENABLED, this);
    Services.obs.addObserver(this, "xpcom-shutdown");
    this.observe(null, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, PREF_LOGGING_ENABLED);
  },

  observe(aSubject, aTopic, aData) {
    if (aTopic == "xpcom-shutdown") {
      Services.prefs.removeObserver(PREF_LOGGING_ENABLED, this);
      Services.obs.removeObserver(this, "xpcom-shutdown");
    } else if (aTopic == NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) {
      let debugLogEnabled = Services.prefs.getBoolPref(
        PREF_LOGGING_ENABLED,
        false
      );
      if (debugLogEnabled) {
        parentLogger.level = Log.Level.Debug;
      } else {
        parentLogger.level = Log.Level.Warn;
      }
    }
  },
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
  } catch (e) {
    logger.warn("Exception calling callback", e);
  }
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
  if (!(aMethod in aProvider)) {
    return aDefault;
  }

  try {
    return aProvider[aMethod].apply(aProvider, aArgs);
  } catch (e) {
    reportProviderError(aProvider, aMethod, e);
    return aDefault;
  }
}

/**
 * Calls a method on a provider if it exists and consumes any thrown exception.
 * Parameters after aMethod are passed to aProvider.aMethod().
 * If the provider does not implement the method, or the method throws, calls
 * the callback with 'undefined'.
 *
 * @param  aProvider
 *         The provider to call
 * @param  aMethod
 *         The method name to call
 */
async function promiseCallProvider(aProvider, aMethod, ...aArgs) {
  if (!(aMethod in aProvider)) {
    return undefined;
  }
  try {
    return aProvider[aMethod].apply(aProvider, aArgs);
  } catch (e) {
    reportProviderError(aProvider, aMethod, e);
    return undefined;
  }
}

/**
 * Gets the currently selected locale for display.
 * @return  the selected locale or "en-US" if none is selected
 */
function getLocale() {
  return Services.locale.requestedLocale || "en-US";
}

const WEB_EXPOSED_ADDON_PROPERTIES = [
  "id",
  "version",
  "type",
  "name",
  "description",
  "isActive",
];

function webAPIForAddon(addon) {
  if (!addon) {
    return null;
  }

  // These web-exposed Addon properties (see AddonManager.webidl)
  // just come directly from an Addon object.
  let result = {};
  for (let prop of WEB_EXPOSED_ADDON_PROPERTIES) {
    result[prop] = addon[prop];
  }

  // These properties are computed.
  result.isEnabled = !addon.userDisabled;
  result.canUninstall = Boolean(
    addon.permissions & AddonManager.PERM_CAN_UNINSTALL
  );

  return result;
}

/**
 * Listens for a browser changing origin and cancels the installs that were
 * started by it.
 */
function BrowserListener(aBrowser, aInstallingPrincipal, aInstall) {
  this.browser = aBrowser;
  this.messageManager = this.browser.messageManager;
  this.principal = aInstallingPrincipal;
  this.install = aInstall;

  aBrowser.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_LOCATION);
  Services.obs.addObserver(this, "message-manager-close", true);

  aInstall.addListener(this);

  this.registered = true;
}

BrowserListener.prototype = {
  browser: null,
  install: null,
  registered: false,

  unregister() {
    if (!this.registered) {
      return;
    }
    this.registered = false;

    Services.obs.removeObserver(this, "message-manager-close");
    // The browser may have already been detached
    if (this.browser.removeProgressListener) {
      this.browser.removeProgressListener(this);
    }

    this.install.removeListener(this);
    this.install = null;
  },

  cancelInstall() {
    try {
      this.install.cancel();
    } catch (e) {
      // install may have already failed or been cancelled, ignore these
    }
  },

  observe(subject, topic, data) {
    if (subject != this.messageManager) {
      return;
    }

    // The browser's message manager has closed and so the browser is
    // going away, cancel the install
    this.cancelInstall();
  },

  onLocationChange(webProgress, request, location) {
    if (
      this.browser.contentPrincipal &&
      this.principal.subsumes(this.browser.contentPrincipal)
    ) {
      return;
    }

    // The browser has navigated to a new origin so cancel the install
    this.cancelInstall();
  },

  onDownloadCancelled(install) {
    this.unregister();
  },

  onDownloadFailed(install) {
    this.unregister();
  },

  onInstallFailed(install) {
    this.unregister();
  },

  onInstallEnded(install) {
    this.unregister();
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsISupportsWeakReference",
    "nsIWebProgressListener",
    "nsIObserver",
  ]),
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
  toString() {
    return this.name || "";
  },
};

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
function AddonScreenshot(
  aURL,
  aWidth,
  aHeight,
  aThumbnailURL,
  aThumbnailWidth,
  aThumbnailHeight,
  aCaption
) {
  this.url = aURL;
  if (aWidth) {
    this.width = aWidth;
  }
  if (aHeight) {
    this.height = aHeight;
  }
  if (aThumbnailURL) {
    this.thumbnailURL = aThumbnailURL;
  }
  if (aThumbnailWidth) {
    this.thumbnailWidth = aThumbnailWidth;
  }
  if (aThumbnailHeight) {
    this.thumbnailHeight = aThumbnailHeight;
  }
  if (aCaption) {
    this.caption = aCaption;
  }
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
  toString() {
    return this.url || "";
  },
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
 *         The key for the string in the properties file.
 * @param  aViewType
 *         The optional type of view to use in the UI
 * @param  aUIPriority
 *         The priority is used by the UI to list the types in order. Lower
 *         values push the type higher in the list.
 */
function AddonType(aID, aLocaleURI, aLocaleKey, aViewType, aUIPriority) {
  if (!aID) {
    throw Components.Exception(
      "An AddonType must have an ID",
      Cr.NS_ERROR_INVALID_ARG
    );
  }

  if (aViewType && aUIPriority === undefined) {
    throw Components.Exception(
      "An AddonType with a defined view must have a set UI priority",
      Cr.NS_ERROR_INVALID_ARG
    );
  }

  if (!aLocaleKey) {
    throw Components.Exception(
      "An AddonType must have a displayable name",
      Cr.NS_ERROR_INVALID_ARG
    );
  }

  this.id = aID;
  this.uiPriority = aUIPriority;
  this.viewType = aViewType;

  if (aLocaleURI) {
    XPCOMUtils.defineLazyGetter(this, "name", () => {
      let bundle = Services.strings.createBundle(aLocaleURI);
      return bundle.GetStringFromName(aLocaleKey);
    });
  } else {
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
var gWebExtensionsMinPlatformVersion = "";
var gFinalShutdownBarrier = null;
var gBeforeShutdownBarrier = null;
var gRepoShutdownState = "";
var gShutdownInProgress = false;
var gBrowserUpdated = null;

var AMTelemetry;

/**
 * This is the real manager, kept here rather than in AddonManager to keep its
 * contents hidden from API users.
 * @class
 * @lends AddonManager
 */
var AddonManagerInternal = {
  managerListeners: new Set(),
  installListeners: new Set(),
  addonListeners: new Set(),
  typeListeners: new Set(),
  pendingProviders: new Set(),
  providers: new Set(),
  providerShutdowns: new Map(),
  types: {},
  startupChanges: {},
  // Store telemetry details per addon provider
  telemetryDetails: {},
  upgradeListeners: new Map(),
  externalExtensionLoaders: new Map(),

  recordTimestamp(name, value) {
    this.TelemetryTimestamps.add(name, value);
  },

  /**
   * Start up a provider, and register its shutdown hook if it has one
   *
   * @param {string} aProvider - An add-on provider.
   * @param {boolean} aAppChanged - Whether or not the app version has changed since last session.
   * @param {string} aOldAppVersion - Previous application version, if changed.
   * @param {string} aOldPlatformVersion - Previous platform version, if changed.
   *
   * @private
   */
  _startProvider(aProvider, aAppChanged, aOldAppVersion, aOldPlatformVersion) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    logger.debug(`Starting provider: ${providerName(aProvider)}`);
    callProvider(
      aProvider,
      "startup",
      null,
      aAppChanged,
      aOldAppVersion,
      aOldPlatformVersion
    );
    if ("shutdown" in aProvider) {
      let name = providerName(aProvider);
      let AMProviderShutdown = () => {
        // If the provider has been unregistered, it will have been removed from
        // this.providers. If it hasn't been unregistered, then this is a normal
        // shutdown - and we move it to this.pendingProviders in case we're
        // running in a test that will start AddonManager again.
        if (this.providers.has(aProvider)) {
          this.providers.delete(aProvider);
          this.pendingProviders.add(aProvider);
        }

        return new Promise((resolve, reject) => {
          logger.debug("Calling shutdown blocker for " + name);
          resolve(aProvider.shutdown());
        }).catch(err => {
          logger.warn("Failure during shutdown of " + name, err);
          AddonManagerPrivate.recordException(
            "AMI",
            "Async shutdown of " + name,
            err
          );
        });
      };
      logger.debug("Registering shutdown blocker for " + name);
      this.providerShutdowns.set(aProvider, AMProviderShutdown);
      AddonManagerPrivate.finalShutdown.addBlocker(name, AMProviderShutdown);
    }

    this.pendingProviders.delete(aProvider);
    this.providers.add(aProvider);
    logger.debug(`Provider finished startup: ${providerName(aProvider)}`);
  },

  _getProviderByName(aName) {
    for (let provider of this.providers) {
      if (providerName(provider) == aName) {
        return provider;
      }
    }
    return undefined;
  },

  /**
   * Initializes the AddonManager, loading any known providers and initializing
   * them.
   */
  startup() {
    try {
      if (gStarted) {
        return;
      }

      this.recordTimestamp("AMI_startup_begin");

      // Enable the addonsManager telemetry event category.
      AMTelemetry.init();

      // clear this for xpcshell test restarts
      for (let provider in this.telemetryDetails) {
        delete this.telemetryDetails[provider];
      }

      let appChanged = undefined;

      let oldAppVersion = null;
      try {
        oldAppVersion = Services.prefs.getCharPref(PREF_EM_LAST_APP_VERSION);
        appChanged = Services.appinfo.version != oldAppVersion;
      } catch (e) {}

      gBrowserUpdated = appChanged;

      let oldPlatformVersion = Services.prefs.getCharPref(
        PREF_EM_LAST_PLATFORM_VERSION,
        ""
      );

      if (appChanged !== false) {
        logger.debug("Application has been upgraded");
        Services.prefs.setCharPref(
          PREF_EM_LAST_APP_VERSION,
          Services.appinfo.version
        );
        Services.prefs.setCharPref(
          PREF_EM_LAST_PLATFORM_VERSION,
          Services.appinfo.platformVersion
        );
        Services.prefs.setIntPref(
          PREF_BLOCKLIST_PINGCOUNTVERSION,
          appChanged === undefined ? 0 : -1
        );
      }

      if (!MOZ_COMPATIBILITY_NIGHTLY) {
        PREF_EM_CHECK_COMPATIBILITY =
          PREF_EM_CHECK_COMPATIBILITY_BASE +
          "." +
          Services.appinfo.version.replace(BRANCH_REGEXP, "$1");
      }

      gCheckCompatibility = Services.prefs.getBoolPref(
        PREF_EM_CHECK_COMPATIBILITY,
        gCheckCompatibility
      );
      Services.prefs.addObserver(PREF_EM_CHECK_COMPATIBILITY, this);

      gStrictCompatibility = Services.prefs.getBoolPref(
        PREF_EM_STRICT_COMPATIBILITY,
        gStrictCompatibility
      );
      Services.prefs.addObserver(PREF_EM_STRICT_COMPATIBILITY, this);

      let defaultBranch = Services.prefs.getDefaultBranch("");
      gCheckUpdateSecurityDefault = defaultBranch.getBoolPref(
        PREF_EM_CHECK_UPDATE_SECURITY,
        gCheckUpdateSecurityDefault
      );

      gCheckUpdateSecurity = Services.prefs.getBoolPref(
        PREF_EM_CHECK_UPDATE_SECURITY,
        gCheckUpdateSecurity
      );
      Services.prefs.addObserver(PREF_EM_CHECK_UPDATE_SECURITY, this);

      gUpdateEnabled = Services.prefs.getBoolPref(
        PREF_EM_UPDATE_ENABLED,
        gUpdateEnabled
      );
      Services.prefs.addObserver(PREF_EM_UPDATE_ENABLED, this);

      gAutoUpdateDefault = Services.prefs.getBoolPref(
        PREF_EM_AUTOUPDATE_DEFAULT,
        gAutoUpdateDefault
      );
      Services.prefs.addObserver(PREF_EM_AUTOUPDATE_DEFAULT, this);

      gWebExtensionsMinPlatformVersion = Services.prefs.getCharPref(
        PREF_MIN_WEBEXT_PLATFORM_VERSION,
        gWebExtensionsMinPlatformVersion
      );
      Services.prefs.addObserver(PREF_MIN_WEBEXT_PLATFORM_VERSION, this);

      // Watch for language changes, refresh the addon cache when it changes.
      Services.obs.addObserver(this, INTL_LOCALES_CHANGED);

      // Ensure all default providers have had a chance to register themselves
      for (let url of DEFAULT_PROVIDERS) {
        try {
          let scope = {};
          ChromeUtils.import(url, scope);
          // Sanity check - make sure the provider exports a symbol that
          // has a 'startup' method
          let syms = Object.keys(scope);
          if (syms.length < 1 || typeof scope[syms[0]].startup != "function") {
            logger.warn("Provider " + url + " has no startup()");
            AddonManagerPrivate.recordException(
              "AMI",
              "provider " + url,
              "no startup()"
            );
          }
          logger.debug(
            "Loaded provider scope for " +
              url +
              ": " +
              Object.keys(scope).toSource()
          );
        } catch (e) {
          AddonManagerPrivate.recordException(
            "AMI",
            "provider " + url + " load failed",
            e
          );
          logger.error('Exception loading default provider "' + url + '"', e);
        }
      }

      // Load any providers registered in the category manager
      for (let { entry, value: url } of Services.catMan.enumerateCategory(
        CATEGORY_PROVIDER_MODULE
      )) {
        try {
          ChromeUtils.import(url, {});
          logger.debug(`Loaded provider scope for ${url}`);
        } catch (e) {
          AddonManagerPrivate.recordException(
            "AMI",
            "provider " + url + " load failed",
            e
          );
          logger.error(
            "Exception loading provider " +
              entry +
              ' from category "' +
              url +
              '"',
            e
          );
        }
      }

      // Register our shutdown handler with the AsyncShutdown manager
      gBeforeShutdownBarrier = new AsyncShutdown.Barrier(
        "AddonManager: Waiting to start provider shutdown."
      );
      gFinalShutdownBarrier = new AsyncShutdown.Barrier(
        "AddonManager: Waiting for providers to shut down."
      );
      AsyncShutdown.profileBeforeChange.addBlocker(
        "AddonManager: shutting down.",
        this.shutdownManager.bind(this),
        { fetchState: this.shutdownState.bind(this) }
      );

      // Once we start calling providers we must allow all normal methods to work.
      gStarted = true;

      for (let provider of this.pendingProviders) {
        this._startProvider(
          provider,
          appChanged,
          oldAppVersion,
          oldPlatformVersion
        );
      }

      // If this is a new profile just pretend that there were no changes
      if (appChanged === undefined) {
        for (let type in this.startupChanges) {
          delete this.startupChanges[type];
        }
      }

      gStartupComplete = true;
      this.recordTimestamp("AMI_startup_end");
    } catch (e) {
      logger.error("startup failed", e);
      AddonManagerPrivate.recordException("AMI", "startup failed", e);
    }

    logger.debug("Completed startup sequence");
    this.callManagerListeners("onStartup");
  },

  /**
   * Registers a new AddonProvider.
   *
   * @param {string} aProvider -The provider to register
   * @param {string[]} [aTypes] - An optional array of add-on types
   */
  registerProvider(aProvider, aTypes) {
    if (!aProvider || typeof aProvider != "object") {
      throw Components.Exception(
        "aProvider must be specified",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (aTypes && !Array.isArray(aTypes)) {
      throw Components.Exception(
        "aTypes must be an array or null",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    this.pendingProviders.add(aProvider);

    if (aTypes) {
      for (let type of aTypes) {
        if (!(type.id in this.types)) {
          if (!VALID_TYPES_REGEXP.test(type.id)) {
            logger.warn("Ignoring invalid type " + type.id);
            return;
          }

          this.types[type.id] = {
            type,
            providers: [aProvider],
          };

          let typeListeners = new Set(this.typeListeners);
          for (let listener of typeListeners) {
            safeCall(() => listener.onTypeAdded(type));
          }
        } else {
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
  unregisterProvider(aProvider) {
    if (!aProvider || typeof aProvider != "object") {
      throw Components.Exception(
        "aProvider must be specified",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    this.providers.delete(aProvider);
    // The test harness will unregister XPIProvider *after* shutdown, which is
    // after the provider will have been moved from providers to
    // pendingProviders.
    this.pendingProviders.delete(aProvider);

    for (let type in this.types) {
      this.types[type].providers = this.types[type].providers.filter(
        p => p != aProvider
      );
      if (!this.types[type].providers.length) {
        let oldType = this.types[type].type;
        delete this.types[type];

        let typeListeners = new Set(this.typeListeners);
        for (let listener of typeListeners) {
          safeCall(() => listener.onTypeRemoved(oldType));
        }
      }
    }

    // If we're unregistering after startup but before shutting down,
    // remove the blocker for this provider's shutdown and call it.
    // If we're already shutting down, just let gFinalShutdownBarrier
    // call it to avoid races.
    if (gStarted && !gShutdownInProgress) {
      logger.debug(
        "Unregistering shutdown blocker for " + providerName(aProvider)
      );
      let shutter = this.providerShutdowns.get(aProvider);
      if (shutter) {
        this.providerShutdowns.delete(aProvider);
        gFinalShutdownBarrier.client.removeBlocker(shutter);
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
  markProviderSafe(aProvider) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (!aProvider || typeof aProvider != "object") {
      throw Components.Exception(
        "aProvider must be specified",
        Cr.NS_ERROR_INVALID_ARG
      );
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
  callProviders(aMethod, ...aArgs) {
    if (!aMethod || typeof aMethod != "string") {
      throw Components.Exception(
        "aMethod must be a non-empty string",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let providers = [...this.providers];
    for (let provider of providers) {
      try {
        if (aMethod in provider) {
          provider[aMethod].apply(provider, aArgs);
        }
      } catch (e) {
        reportProviderError(provider, aMethod, e);
      }
    }
  },

  /**
   * Report the current state of asynchronous shutdown
   */
  shutdownState() {
    let state = [];
    for (let barrier of [gBeforeShutdownBarrier, gFinalShutdownBarrier]) {
      if (barrier) {
        state.push({ name: barrier.client.name, state: barrier.state });
      }
    }
    state.push({
      name: "AddonRepository: async shutdown",
      state: gRepoShutdownState,
    });
    return state;
  },

  /**
   * Shuts down the addon manager and all registered providers, this must clean
   * up everything in order for automated tests to fake restarts.
   * @return Promise{null} that resolves when all providers and dependent modules
   *                       have finished shutting down
   */
  async shutdownManager() {
    logger.debug("before shutdown");
    try {
      await gBeforeShutdownBarrier.wait();
    } catch (e) {
      Cu.reportError(e);
    }

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

    Services.obs.removeObserver(this, INTL_LOCALES_CHANGED);

    let savedError = null;
    // Only shut down providers if they've been started.
    if (gStarted) {
      try {
        await gFinalShutdownBarrier.wait();
      } catch (err) {
        savedError = err;
        logger.error("Failure during wait for shutdown barrier", err);
        AddonManagerPrivate.recordException(
          "AMI",
          "Async shutdown of AddonManager providers",
          err
        );
      }
    }

    // Shut down AddonRepository after providers (if any).
    try {
      gRepoShutdownState = "in progress";
      await AddonRepository.shutdown();
      gRepoShutdownState = "done";
    } catch (err) {
      savedError = err;
      logger.error("Failure during AddonRepository shutdown", err);
      AddonManagerPrivate.recordException(
        "AMI",
        "Async shutdown of AddonRepository",
        err
      );
    }

    logger.debug("Async provider shutdown done");
    this.managerListeners.clear();
    this.installListeners.clear();
    this.addonListeners.clear();
    this.typeListeners.clear();
    this.providerShutdowns.clear();
    for (let type in this.startupChanges) {
      delete this.startupChanges[type];
    }
    gStarted = false;
    gStartupComplete = false;
    gFinalShutdownBarrier = null;
    gBeforeShutdownBarrier = null;
    gShutdownInProgress = false;
    if (savedError) {
      throw savedError;
    }
  },

  /**
   * Notified when a preference we're interested in has changed.
   *
   * @see nsIObserver
   */
  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case INTL_LOCALES_CHANGED: {
        // Asynchronously fetch and update the addons cache.
        AddonRepository.backgroundUpdateCheck();
        return;
      }
    }

    switch (aData) {
      case PREF_EM_CHECK_COMPATIBILITY: {
        let oldValue = gCheckCompatibility;
        gCheckCompatibility = Services.prefs.getBoolPref(
          PREF_EM_CHECK_COMPATIBILITY,
          true
        );

        this.callManagerListeners("onCompatibilityModeChanged");

        if (gCheckCompatibility != oldValue) {
          this.updateAddonAppDisabledStates();
        }

        break;
      }
      case PREF_EM_STRICT_COMPATIBILITY: {
        let oldValue = gStrictCompatibility;
        gStrictCompatibility = Services.prefs.getBoolPref(
          PREF_EM_STRICT_COMPATIBILITY,
          true
        );

        this.callManagerListeners("onCompatibilityModeChanged");

        if (gStrictCompatibility != oldValue) {
          this.updateAddonAppDisabledStates();
        }

        break;
      }
      case PREF_EM_CHECK_UPDATE_SECURITY: {
        let oldValue = gCheckUpdateSecurity;
        gCheckUpdateSecurity = Services.prefs.getBoolPref(
          PREF_EM_CHECK_UPDATE_SECURITY,
          true
        );

        this.callManagerListeners("onCheckUpdateSecurityChanged");

        if (gCheckUpdateSecurity != oldValue) {
          this.updateAddonAppDisabledStates();
        }

        break;
      }
      case PREF_EM_UPDATE_ENABLED: {
        gUpdateEnabled = Services.prefs.getBoolPref(
          PREF_EM_UPDATE_ENABLED,
          true
        );

        this.callManagerListeners("onUpdateModeChanged");
        break;
      }
      case PREF_EM_AUTOUPDATE_DEFAULT: {
        gAutoUpdateDefault = Services.prefs.getBoolPref(
          PREF_EM_AUTOUPDATE_DEFAULT,
          true
        );

        this.callManagerListeners("onUpdateModeChanged");
        break;
      }
      case PREF_MIN_WEBEXT_PLATFORM_VERSION: {
        gWebExtensionsMinPlatformVersion = Services.prefs.getCharPref(
          PREF_MIN_WEBEXT_PLATFORM_VERSION
        );
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
  escapeAddonURI(aAddon, aUri, aAppVersion) {
    if (!aAddon || typeof aAddon != "object") {
      throw Components.Exception(
        "aAddon must be an Addon object",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (!aUri || typeof aUri != "string") {
      throw Components.Exception(
        "aUri must be a non-empty string",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (aAppVersion && typeof aAppVersion != "string") {
      throw Components.Exception(
        "aAppVersion must be a string or null",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    var addonStatus =
      aAddon.userDisabled || aAddon.softDisabled
        ? "userDisabled"
        : "userEnabled";

    if (!aAddon.isCompatible) {
      addonStatus += ",incompatible";
    }

    let { blocklistState } = aAddon;
    if (blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED) {
      addonStatus += ",blocklisted";
    }
    if (blocklistState == Ci.nsIBlocklistService.STATE_SOFTBLOCKED) {
      addonStatus += ",softblocked";
    }

    let params = new Map(
      Object.entries({
        ITEM_ID: aAddon.id,
        ITEM_VERSION: aAddon.version,
        ITEM_STATUS: addonStatus,
        APP_ID: Services.appinfo.ID,
        APP_VERSION: aAppVersion ? aAppVersion : Services.appinfo.version,
        REQ_VERSION: UPDATE_REQUEST_VERSION,
        APP_OS: Services.appinfo.OS,
        APP_ABI: Services.appinfo.XPCOMABI,
        APP_LOCALE: getLocale(),
        CURRENT_APP_VERSION: Services.appinfo.version,
      })
    );

    let uri = aUri.replace(/%([A-Z_]+)%/g, (m0, m1) => params.get(m1) || m0);

    // escape() does not properly encode + symbols in any embedded FVF strings.
    return uri.replace(/\+/g, "%2B");
  },

  _updatePromptHandler(info) {
    let oldPerms = info.existingAddon.userPermissions;
    if (!oldPerms) {
      // Updating from a legacy add-on, just let it proceed
      return Promise.resolve();
    }

    let newPerms = info.addon.userPermissions;

    let difference = Extension.comparePermissions(oldPerms, newPerms);

    // If there are no new permissions, just go ahead with the update
    if (!difference.origins.length && !difference.permissions.length) {
      return Promise.resolve();
    }

    return new Promise((resolve, reject) => {
      let subject = {
        wrappedJSObject: {
          addon: info.addon,
          permissions: difference,
          resolve,
          reject,
          // Reference to the related AddonInstall object (used in AMTelemetry to
          // link the recorded event to the other events from the same install flow).
          install: info.install,
        },
      };
      Services.obs.notifyObservers(subject, "webextension-update-permissions");
    });
  },

  // Returns true if System Addons should be updated
  systemUpdateEnabled() {
    if (!Services.prefs.getBoolPref(PREF_SYS_ADDON_UPDATE_ENABLED)) {
      return false;
    }
    if (Services.policies && !Services.policies.isAllowed("SysAddonUpdate")) {
      return false;
    }
    return true;
  },

  /**
   * Performs a background update check by starting an update for all add-ons
   * that can be updated.
   * @return Promise{null} Resolves when the background update check is complete
   *                       (the resulting addon installations may still be in progress).
   */
  backgroundUpdateCheck() {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    let buPromise = (async () => {
      logger.debug("Background update check beginning");

      Services.obs.notifyObservers(null, "addons-background-update-start");

      if (this.updateEnabled) {
        // Keep track of all the async add-on updates happening in parallel
        let updates = [];

        let allAddons = await this.getAllAddons();

        // Repopulate repository cache first, to ensure compatibility overrides
        // are up to date before checking for addon updates.
        await AddonRepository.backgroundUpdateCheck();

        for (let addon of allAddons) {
          // Check all add-ons for updates so that any compatibility updates will
          // be applied

          if (!(addon.permissions & AddonManager.PERM_CAN_UPGRADE)) {
            continue;
          }

          updates.push(
            new Promise((resolve, reject) => {
              addon.findUpdates(
                {
                  onUpdateAvailable(aAddon, aInstall) {
                    // Start installing updates when the add-on can be updated and
                    // background updates should be applied.
                    logger.debug("Found update for add-on ${id}", aAddon);
                    if (AddonManager.shouldAutoUpdate(aAddon)) {
                      // XXX we really should resolve when this install is done,
                      // not when update-available check completes, no?
                      logger.debug(`Starting upgrade install of ${aAddon.id}`);
                      aInstall.promptHandler = (...args) =>
                        AddonManagerInternal._updatePromptHandler(...args);
                      aInstall.install();
                    }
                  },

                  onUpdateFinished: aAddon => {
                    logger.debug("onUpdateFinished for ${id}", aAddon);
                    resolve();
                  },
                },
                AddonManager.UPDATE_WHEN_PERIODIC_UPDATE
              );
            })
          );
        }
        await Promise.all(updates);
      }

      if (AddonManagerInternal.systemUpdateEnabled()) {
        try {
          await AddonManagerInternal._getProviderByName(
            "XPIProvider"
          ).updateSystemAddons();
        } catch (e) {
          logger.warn("Failed to update system addons", e);
        }
      }

      logger.debug("Background update check complete");
      Services.obs.notifyObservers(null, "addons-background-update-complete");
    })();
    // Fork the promise chain so we can log the error and let our caller see it too.
    buPromise.catch(e => logger.warn("Error in background update", e));
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
  addStartupChange(aType, aID) {
    if (!aType || typeof aType != "string") {
      throw Components.Exception(
        "aType must be a non-empty string",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (!aID || typeof aID != "string") {
      throw Components.Exception(
        "aID must be a non-empty string",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (gStartupComplete) {
      return;
    }
    logger.debug("Registering startup change '" + aType + "' for " + aID);

    // Ensure that an ID is only listed in one type of change
    for (let type in this.startupChanges) {
      this.removeStartupChange(type, aID);
    }

    if (!(aType in this.startupChanges)) {
      this.startupChanges[aType] = [];
    }
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
  removeStartupChange(aType, aID) {
    if (!aType || typeof aType != "string") {
      throw Components.Exception(
        "aType must be a non-empty string",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (!aID || typeof aID != "string") {
      throw Components.Exception(
        "aID must be a non-empty string",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (gStartupComplete) {
      return;
    }

    if (!(aType in this.startupChanges)) {
      return;
    }

    this.startupChanges[aType] = this.startupChanges[aType].filter(
      aItem => aItem != aID
    );
  },

  /**
   * Calls all registered AddonManagerListeners with an event. Any parameters
   * after the method parameter are passed to the listener.
   *
   * @param  aMethod
   *         The method on the listeners to call
   */
  callManagerListeners(aMethod, ...aArgs) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (!aMethod || typeof aMethod != "string") {
      throw Components.Exception(
        "aMethod must be a non-empty string",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let managerListeners = new Set(this.managerListeners);
    for (let listener of managerListeners) {
      try {
        if (aMethod in listener) {
          listener[aMethod].apply(listener, aArgs);
        }
      } catch (e) {
        logger.warn(
          "AddonManagerListener threw exception when calling " + aMethod,
          e
        );
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
  callInstallListeners(aMethod, aExtraListeners, ...aArgs) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (!aMethod || typeof aMethod != "string") {
      throw Components.Exception(
        "aMethod must be a non-empty string",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (aExtraListeners && !Array.isArray(aExtraListeners)) {
      throw Components.Exception(
        "aExtraListeners must be an array or null",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let result = true;
    let listeners;
    if (aExtraListeners) {
      listeners = new Set(
        aExtraListeners.concat(Array.from(this.installListeners))
      );
    } else {
      listeners = new Set(this.installListeners);
    }

    for (let listener of listeners) {
      try {
        if (aMethod in listener) {
          if (listener[aMethod].apply(listener, aArgs) === false) {
            result = false;
          }
        }
      } catch (e) {
        logger.warn(
          "InstallListener threw exception when calling " + aMethod,
          e
        );
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
  callAddonListeners(aMethod, ...aArgs) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (!aMethod || typeof aMethod != "string") {
      throw Components.Exception(
        "aMethod must be a non-empty string",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let addonListeners = new Set(this.addonListeners);
    for (let listener of addonListeners) {
      try {
        if (aMethod in listener) {
          listener[aMethod].apply(listener, aArgs);
        }
      } catch (e) {
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
  async notifyAddonChanged(aID, aType, aPendingRestart) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (aID && typeof aID != "string") {
      throw Components.Exception(
        "aID must be a string or null",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (!aType || typeof aType != "string") {
      throw Components.Exception(
        "aType must be a non-empty string",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    // Temporary hack until bug 520124 lands.
    // We can get here during synchronous startup, at which point it's
    // considered unsafe (and therefore disallowed by AddonManager.jsm) to
    // access providers that haven't been initialized yet. Since this is when
    // XPIProvider is starting up, XPIProvider can't access itself via APIs
    // going through AddonManager.jsm. Thankfully, this is the only use
    // of this API, and we know it's safe to use this API with both
    // providers; so we have this hack to allow bypassing the normal
    // safetey guard.
    // The notifyAddonChanged/addonChanged API will be unneeded and therefore
    // removed by bug 520124, so this is a temporary quick'n'dirty hack.
    let providers = [...this.providers, ...this.pendingProviders];
    for (let provider of providers) {
      let result = callProvider(
        provider,
        "addonChanged",
        null,
        aID,
        aType,
        aPendingRestart
      );
      if (result) {
        await result;
      }
    }
  },

  /**
   * Notifies all providers they need to update the appDisabled property for
   * their add-ons in response to an application change such as a blocklist
   * update.
   */
  updateAddonAppDisabledStates() {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    this.callProviders("updateAddonAppDisabledStates");
  },

  /**
   * Notifies all providers that the repository has updated its data for
   * installed add-ons.
   */
  updateAddonRepositoryData() {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    return (async () => {
      for (let provider of this.providers) {
        await promiseCallProvider(provider, "updateAddonRepositoryData");
      }

      // only tests should care about this
      Services.obs.notifyObservers(null, "TEST:addon-repository-data-updated");
    })();
  },

  /**
   * Asynchronously gets an AddonInstall for a URL.
   *
   * @param  aUrl
   *         The string represenation of the URL where the add-on is located
   * @param  {Object} [aOptions = {}]
   *         Additional options for this install
   * @param  {string} [aOptions.hash]
   *         An optional hash of the add-on
   * @param  {string} [aOptions.name]
   *         An optional placeholder name while the add-on is being downloaded
   * @param  {string|Object} [aOptions.icons]
   *         Optional placeholder icons while the add-on is being downloaded
   * @param  {string} [aOptions.version]
   *         An optional placeholder version while the add-on is being downloaded
   * @param  {XULElement} [aOptions.browser]
   *         An optional <browser> element for download permissions prompts.
   * @param  {nsIPrincipal} [aOptions.triggeringPrincipal]
   *         The principal which is attempting to install the add-on.
   * @param  {Object} [aOptions.telemetryInfo]
   *         An optional object which provides details about the installation source
   *         included in the addon manager telemetry events.
   * @throws if aUrl is not specified or if an optional argument of
   *         an improper type is passed.
   */
  async getInstallForURL(aUrl, aOptions = {}) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (!aUrl || typeof aUrl != "string") {
      throw Components.Exception(
        "aURL must be a non-empty string",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (aOptions.hash && typeof aOptions.hash != "string") {
      throw Components.Exception(
        "hash must be a string or null",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (aOptions.name && typeof aOptions.name != "string") {
      throw Components.Exception(
        "name must be a string or null",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (aOptions.icons) {
      if (typeof aOptions.icons == "string") {
        aOptions.icons = { "32": aOptions.icons };
      } else if (typeof aOptions.icons != "object") {
        throw Components.Exception(
          "icons must be a string, an object or null",
          Cr.NS_ERROR_INVALID_ARG
        );
      }
    } else {
      aOptions.icons = {};
    }

    if (aOptions.version && typeof aOptions.version != "string") {
      throw Components.Exception(
        "version must be a string or null",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (aOptions.browser && !Element.isInstance(aOptions.browser)) {
      throw Components.Exception(
        "aOptions.browser must be an Element or null",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    for (let provider of this.providers) {
      let install = await promiseCallProvider(
        provider,
        "getInstallForURL",
        aUrl,
        aOptions
      );
      if (install) {
        return install;
      }
    }

    return null;
  },

  /**
   * Asynchronously gets an AddonInstall for an nsIFile.
   *
   * @param  aFile
   *         The nsIFile where the add-on is located
   * @param  aMimetype
   *         An optional mimetype hint for the add-on
   * @param  aTelemetryInfo
   *         An optional object which provides details about the installation source
   *         included in the addon manager telemetry events.
   * @param  aUseSystemLocation
   *         If true the addon is installed into the system profile location.
   * @throws if the aFile or aCallback arguments are not specified
   */
  getInstallForFile(aFile, aMimetype, aTelemetryInfo, aUseSystemLocation) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (!(aFile instanceof Ci.nsIFile)) {
      throw Components.Exception(
        "aFile must be a nsIFile",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (aMimetype && typeof aMimetype != "string") {
      throw Components.Exception(
        "aMimetype must be a string or null",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    return (async () => {
      for (let provider of this.providers) {
        let install = await promiseCallProvider(
          provider,
          "getInstallForFile",
          aFile,
          aTelemetryInfo,
          aUseSystemLocation
        );

        if (install) {
          return install;
        }
      }

      return null;
    })();
  },

  /**
   * Uninstall an addon from the system profile location.
   *
   * @param {string} aID
   *         The ID of the addon to remove.
   * @returns A promise that resolves when the addon is uninstalled.
   */
  uninstallSystemProfileAddon(aID) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }
    return AddonManagerInternal._getProviderByName(
      "XPIProvider"
    ).uninstallSystemProfileAddon(aID);
  },

  /**
   * Asynchronously gets all current AddonInstalls optionally limiting to a list
   * of types.
   *
   * @param  aTypes
   *         An optional array of types to retrieve. Each type is a string name
   * @throws If the aCallback argument is not specified
   */
  getInstallsByTypes(aTypes) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (aTypes && !Array.isArray(aTypes)) {
      throw Components.Exception(
        "aTypes must be an array or null",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    return (async () => {
      let installs = [];

      for (let provider of this.providers) {
        let providerInstalls = await promiseCallProvider(
          provider,
          "getInstallsByTypes",
          aTypes
        );

        if (providerInstalls) {
          installs.push(...providerInstalls);
        }
      }

      return installs;
    })();
  },

  /**
   * Asynchronously gets all current AddonInstalls.
   */
  getAllInstalls() {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    return this.getInstallsByTypes(null);
  },

  /**
   * Checks whether installation is enabled for a particular mimetype.
   *
   * @param  aMimetype
   *         The mimetype to check
   * @return true if installation is enabled for the mimetype
   */
  isInstallEnabled(aMimetype) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (!aMimetype || typeof aMimetype != "string") {
      throw Components.Exception(
        "aMimetype must be a non-empty string",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let providers = [...this.providers];
    for (let provider of providers) {
      if (
        callProvider(provider, "supportsMimetype", false, aMimetype) &&
        callProvider(provider, "isInstallEnabled")
      ) {
        return true;
      }
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
  isInstallAllowed(aMimetype, aInstallingPrincipal) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (!aMimetype || typeof aMimetype != "string") {
      throw Components.Exception(
        "aMimetype must be a non-empty string",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (
      !aInstallingPrincipal ||
      !(aInstallingPrincipal instanceof Ci.nsIPrincipal)
    ) {
      throw Components.Exception(
        "aInstallingPrincipal must be a nsIPrincipal",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (
      this.isInstallAllowedByPolicy(
        aInstallingPrincipal,
        null,
        true /* explicit */
      )
    ) {
      return true;
    }

    let providers = [...this.providers];
    for (let provider of providers) {
      if (
        callProvider(provider, "supportsMimetype", false, aMimetype) &&
        callProvider(provider, "isInstallAllowed", null, aInstallingPrincipal)
      ) {
        return true;
      }
    }
    return false;
  },

  /**
   * Checks whether a particular source is allowed to install add-ons based
   * on policy.
   *
   * @param  aInstallingPrincipal
   *         The nsIPrincipal that initiated the install
   * @param  aInstall
   *         The AddonInstall to be installed
   * @param  explicit
   *         If this is set, we only return true if the source is explicitly
   *         blocked via policy.
   *
   * @return boolean
   *         By default, returns true if the source is blocked by policy
   *         or there is no policy.
   *         If explicit is set, only returns true of the source is
   *         blocked by policy, false otherwise. This is needed for
   *         handling inverse cases.
   */
  isInstallAllowedByPolicy(aInstallingPrincipal, aInstall, explicit) {
    if (Services.policies) {
      let extensionSettings = Services.policies.getExtensionSettings("*");
      if (extensionSettings && extensionSettings.install_sources) {
        if (
          (!aInstall ||
            Services.policies.allowedInstallSource(aInstall.sourceURI)) &&
          (!aInstallingPrincipal ||
            !aInstallingPrincipal.URI ||
            Services.policies.allowedInstallSource(aInstallingPrincipal.URI))
        ) {
          return true;
        }
        return false;
      }
    }
    return !explicit;
  },

  installNotifyObservers(
    aTopic,
    aBrowser,
    aUri,
    aInstall,
    aInstallFn,
    aCancelFn
  ) {
    let info = {
      wrappedJSObject: {
        browser: aBrowser,
        originatingURI: aUri,
        installs: [aInstall],
        install: aInstallFn,
        cancel: aCancelFn,
      },
    };
    Services.obs.notifyObservers(info, aTopic);
  },

  startInstall(browser, url, install) {
    this.installNotifyObservers("addon-install-started", browser, url, install);

    // Local installs may already be in a failed state in which case
    // we won't get any further events, detect those cases now.
    if (
      install.state == AddonManager.STATE_DOWNLOADED &&
      install.addon.appDisabled
    ) {
      install.cancel();
      this.installNotifyObservers(
        "addon-install-failed",
        browser,
        url,
        install
      );
      return;
    }

    let self = this;
    let listener = {
      onDownloadCancelled() {
        install.removeListener(listener);
      },

      onDownloadFailed() {
        install.removeListener(listener);
        self.installNotifyObservers(
          "addon-install-failed",
          browser,
          url,
          install
        );
      },

      onDownloadEnded() {
        if (install.addon.appDisabled) {
          // App disabled items are not compatible and so fail to install.
          install.removeListener(listener);
          install.cancel();
          self.installNotifyObservers(
            "addon-install-failed",
            browser,
            url,
            install
          );
        }
      },

      onInstallCancelled() {
        install.removeListener(listener);
      },

      onInstallFailed() {
        install.removeListener(listener);
        self.installNotifyObservers(
          "addon-install-failed",
          browser,
          url,
          install
        );
      },

      onInstallEnded() {
        install.removeListener(listener);

        // If installing a theme that is disabled and can be enabled
        // then enable it
        if (
          install.addon.type == "theme" &&
          !!install.addon.userDisabled &&
          !install.addon.appDisabled
        ) {
          install.addon.enable();
        }

        let needsRestart =
          install.addon.pendingOperations != AddonManager.PENDING_NONE;

        if (!needsRestart) {
          let subject = {
            wrappedJSObject: { target: browser, addon: install.addon },
          };
          Services.obs.notifyObservers(subject, "webextension-install-notify");
        } else {
          self.installNotifyObservers(
            "addon-install-complete",
            browser,
            url,
            install
          );
        }
      },
    };

    install.addListener(listener);

    // Start downloading if it hasn't already begun
    install.install();
  },

  /**
   * Starts installation of an AddonInstall notifying the registered
   * web install listener of a blocked or started install.
   *
   * @param  aMimetype
   *         The mimetype of the add-on being installed
   * @param  aBrowser
   *         The optional browser element that started the install
   * @param  aInstallingPrincipal
   *         The nsIPrincipal that initiated the install
   * @param  aInstall
   *         The AddonInstall to be installed
   */
  installAddonFromWebpage(aMimetype, aBrowser, aInstallingPrincipal, aInstall) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (!aMimetype || typeof aMimetype != "string") {
      throw Components.Exception(
        "aMimetype must be a non-empty string",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (aBrowser && !Element.isInstance(aBrowser)) {
      throw Components.Exception(
        "aSource must be an Element, or null",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (
      !aInstallingPrincipal ||
      !(aInstallingPrincipal instanceof Ci.nsIPrincipal)
    ) {
      throw Components.Exception(
        "aInstallingPrincipal must be a nsIPrincipal",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    // When a chrome in-content UI has loaded a <browser> inside to host a
    // website we want to do our security checks on the inner-browser but
    // notify front-end that install events came from the outer-browser (the
    // main tab's browser). Check this by seeing if the browser we've been
    // passed is in a content type docshell and if so get the outer-browser.
    let topBrowser = aBrowser;
    // GeckoView does not pass a browser.
    if (aBrowser) {
      let docShell = aBrowser.ownerGlobal.docShell;
      if (docShell.itemType == Ci.nsIDocShellTreeItem.typeContent) {
        topBrowser = docShell.chromeEventHandler;
      }
    }

    try {
      // Use fullscreenElement to check for DOM fullscreen, while still allowing
      // macOS fullscreen, which still has a browser chrome.
      if (topBrowser && topBrowser.ownerDocument.fullscreenElement) {
        // Addon installation and the resulting notifications should be
        // blocked in DOM fullscreen for security and usability reasons.
        // Installation prompts in fullscreen can trick the user into
        // installing unwanted addons.
        // In fullscreen the notification box does not have a clear
        // visual association with its parent anymore.
        aInstall.cancel();

        this.installNotifyObservers(
          "addon-install-fullscreen-blocked",
          topBrowser,
          aInstallingPrincipal.URI,
          aInstall
        );
        return;
      } else if (!this.isInstallEnabled(aMimetype)) {
        aInstall.cancel();

        this.installNotifyObservers(
          "addon-install-disabled",
          topBrowser,
          aInstallingPrincipal.URI,
          aInstall
        );
        return;
      } else if (
        aInstallingPrincipal.isNullPrincipal ||
        (aBrowser &&
          (!aBrowser.contentPrincipal ||
            // When we attempt to handle an XPI load immediately after a
            // process switch, the DocShell it's being loaded into will have
            // a null principal, since it won't have been initialized yet.
            // Allowing installs in this case is relatively safe, since
            // there isn't much to gain by spoofing an install request from
            // a null principal in any case. This exception can be removed
            // once content handlers are triggered by DocumentChannel in the
            // parent process.
            !(
              aBrowser.contentPrincipal.isNullPrincipal ||
              aInstallingPrincipal.subsumes(aBrowser.contentPrincipal)
            ))) ||
        !this.isInstallAllowedByPolicy(
          aInstallingPrincipal,
          aInstall,
          false /* explicit */
        )
      ) {
        aInstall.cancel();

        this.installNotifyObservers(
          "addon-install-origin-blocked",
          topBrowser,
          aInstallingPrincipal.URI,
          aInstall
        );
        return;
      }

      if (aBrowser) {
        // The install may start now depending on the web install listener,
        // listen for the browser navigating to a new origin and cancel the
        // install in that case.
        new BrowserListener(aBrowser, aInstallingPrincipal, aInstall);
      }

      let startInstall = source => {
        AddonManagerInternal.setupPromptHandler(
          aBrowser,
          aInstallingPrincipal.URI,
          aInstall,
          true,
          source
        );

        AddonManagerInternal.startInstall(
          aBrowser,
          aInstallingPrincipal.URI,
          aInstall
        );
      };

      let installAllowed = this.isInstallAllowed(
        aMimetype,
        aInstallingPrincipal
      );
      let installPerm = Services.perms.testPermissionFromPrincipal(
        aInstallingPrincipal,
        "install"
      );

      if (installAllowed) {
        startInstall("AMO");
      } else if (installPerm === Ci.nsIPermissionManager.DENY_ACTION) {
        // Block without prompt
        aInstall.cancel();
        this.installNotifyObservers(
          "addon-install-blocked-silent",
          topBrowser,
          aInstallingPrincipal.URI,
          aInstall
        );
      } else if (!WEBEXT_POSTDOWNLOAD_THIRD_PARTY) {
        // Block with prompt
        this.installNotifyObservers(
          "addon-install-blocked",
          topBrowser,
          aInstallingPrincipal.URI,
          aInstall,
          () => startInstall("other")
        );
      } else {
        // We download the addon and validate whether a 3rd party
        // install prompt should be shown using e.g. recommended
        // state and install_origins.
        logger.info(`Addon download before validation.`);
        startInstall("other");
      }
    } catch (e) {
      // In the event that the weblistener throws during instantiation or when
      // calling onWebInstallBlocked or onWebInstallRequested the
      // install should get cancelled.
      logger.warn("Failure calling web installer", e);
      aInstall.cancel();
    }
  },

  /**
   * Starts installation of an AddonInstall created from add-ons manager
   * front-end code (e.g., drag-and-drop of xpis or "Install Add-on from File"
   *
   * @param  browser
   *         The browser element where the installation was initiated
   * @param  uri
   *         The URI of the page where the installation was initiated
   * @param  install
   *         The AddonInstall to be installed
   */
  installAddonFromAOM(browser, uri, install) {
    if (!this.isInstallAllowedByPolicy(null, install)) {
      install.cancel();

      this.installNotifyObservers(
        "addon-install-origin-blocked",
        browser,
        install.sourceURI,
        install
      );
      return;
    }
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    AddonManagerInternal.setupPromptHandler(
      browser,
      uri,
      install,
      true,
      "local"
    );
    AddonManagerInternal.startInstall(browser, uri, install);
  },

  /**
   * Adds a new InstallListener if the listener is not already registered.
   *
   * @param  aListener
   *         The InstallListener to add
   */
  addInstallListener(aListener) {
    if (!aListener || typeof aListener != "object") {
      throw Components.Exception(
        "aListener must be a InstallListener object",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    this.installListeners.add(aListener);
  },

  /**
   * Removes an InstallListener if the listener is registered.
   *
   * @param  aListener
   *         The InstallListener to remove
   */
  removeInstallListener(aListener) {
    if (!aListener || typeof aListener != "object") {
      throw Components.Exception(
        "aListener must be a InstallListener object",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    this.installListeners.delete(aListener);
  },
  /**
   * Adds new or overrides existing UpgradeListener.
   *
   * @param  aInstanceID
   *         The instance ID of an addon to register a listener for.
   * @param  aCallback
   *         The callback to invoke when updates are available for this addon.
   * @throws if there is no addon matching the instanceID
   */
  addUpgradeListener(aInstanceID, aCallback) {
    if (!aInstanceID || typeof aInstanceID != "symbol") {
      throw Components.Exception(
        "aInstanceID must be a symbol",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (!aCallback || typeof aCallback != "function") {
      throw Components.Exception(
        "aCallback must be a function",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let addonId = this.syncGetAddonIDByInstanceID(aInstanceID);
    if (!addonId) {
      throw Error(`No addon matching instanceID: ${String(aInstanceID)}`);
    }
    logger.debug(`Registering upgrade listener for ${addonId}`);
    this.upgradeListeners.set(addonId, aCallback);
  },

  /**
   * Removes an UpgradeListener if the listener is registered.
   *
   * @param  aInstanceID
   *         The instance ID of the addon to remove
   */
  removeUpgradeListener(aInstanceID) {
    if (!aInstanceID || typeof aInstanceID != "symbol") {
      throw Components.Exception(
        "aInstanceID must be a symbol",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let addonId = this.syncGetAddonIDByInstanceID(aInstanceID);
    if (!addonId) {
      throw Error(`No addon for instanceID: ${aInstanceID}`);
    }
    if (this.upgradeListeners.has(addonId)) {
      this.upgradeListeners.delete(addonId);
    } else {
      throw Error(`No upgrade listener registered for addon ID: ${addonId}`);
    }
  },

  addExternalExtensionLoader(loader) {
    this.externalExtensionLoaders.set(loader.name, loader);
  },

  /**
   * Installs a temporary add-on from a local file or directory.
   *
   * @param  aFile
   *         An nsIFile for the file or directory of the add-on to be
   *         temporarily installed.
   * @returns a Promise that rejects if the add-on is not a valid restartless
   *          add-on or if the same ID is already temporarily installed.
   */
  installTemporaryAddon(aFile) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (!(aFile instanceof Ci.nsIFile)) {
      throw Components.Exception(
        "aFile must be a nsIFile",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    return AddonManagerInternal._getProviderByName(
      "XPIProvider"
    ).installTemporaryAddon(aFile);
  },

  /**
   * Installs an add-on from a built-in location
   *  (ie a resource: url referencing assets shipped with the application)
   *
   * @param  aBase
   *         A string containing the base URL.  Must be a resource: URL.
   * @returns a Promise that resolves when the addon is installed.
   */
  installBuiltinAddon(aBase) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    return AddonManagerInternal._getProviderByName(
      "XPIProvider"
    ).installBuiltinAddon(aBase);
  },

  /**
   * Like `installBuiltinAddon`, but only installs the addon at `aBase`
   * if an existing built-in addon with the ID `aID` and version doesn't
   * already exist.
   *
   * @param {string} aID
   *        The ID of the add-on being registered.
   * @param {string} aVersion
   *        The version of the add-on being registered.
   * @param {string} aBase
   *        A string containing the base URL.  Must be a resource: URL.
   * @returns a Promise that resolves when the addon is installed.
   */
  maybeInstallBuiltinAddon(aID, aVersion, aBase) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    return AddonManagerInternal._getProviderByName(
      "XPIProvider"
    ).maybeInstallBuiltinAddon(aID, aVersion, aBase);
  },

  syncGetAddonIDByInstanceID(aInstanceID) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (!aInstanceID || typeof aInstanceID != "symbol") {
      throw Components.Exception(
        "aInstanceID must be a Symbol()",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    return AddonManagerInternal._getProviderByName(
      "XPIProvider"
    ).getAddonIDByInstanceID(aInstanceID);
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
   *         An object with either an icons property that is a key-value list
   *         of icon size and icon URL, or an object having an iconURL property.
   * @param  aSize
   *         Ideal icon size in pixels
   * @param  aWindow
   *         Optional window object for determining the correct scale.
   * @return {String} The absolute URL of the icon or null if the addon doesn't have icons
   */
  getPreferredIconURL(aAddon, aSize, aWindow = undefined) {
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
    }

    // quick return if the exact size was found
    if (icons[aSize]) {
      return icons[aSize];
    }

    let bestSize = null;

    for (let size of Object.keys(icons)) {
      if (!INTEGER.test(size)) {
        throw Components.Exception(
          "Invalid icon size, must be an integer",
          Cr.NS_ERROR_ILLEGAL_VALUE
        );
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
      } else {
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
   * @type {function}
   * @param  {string} aID
   *         The ID of the add-on to retrieve
   * @returns {Promise} resolves with the found Addon or null if no such add-on exists. Never rejects.
   * @throws if the aID argument is not specified
   */
  getAddonByID(aID) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (!aID || typeof aID != "string") {
      throw Components.Exception(
        "aID must be a non-empty string",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let promises = Array.from(this.providers, p =>
      promiseCallProvider(p, "getAddonByID", aID)
    );
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
  getAddonBySyncGUID(aGUID) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (!aGUID || typeof aGUID != "string") {
      throw Components.Exception(
        "aGUID must be a non-empty string",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    return (async () => {
      for (let provider of this.providers) {
        let addon = await promiseCallProvider(
          provider,
          "getAddonBySyncGUID",
          aGUID
        );

        if (addon) {
          return addon;
        }
      }

      return null;
    })();
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
  getAddonsByIDs(aIDs) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (!Array.isArray(aIDs)) {
      throw Components.Exception(
        "aIDs must be an array",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let promises = aIDs.map(a => AddonManagerInternal.getAddonByID(a));
    return Promise.all(promises);
  },

  /**
   * Asynchronously gets add-ons of specific types.
   *
   * @param  aTypes
   *         An optional array of types to retrieve. Each type is a string name
   */
  getAddonsByTypes(aTypes) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (aTypes && !Array.isArray(aTypes)) {
      throw Components.Exception(
        "aTypes must be an array or null",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    return (async () => {
      let addons = [];

      for (let provider of this.providers) {
        let providerAddons = await promiseCallProvider(
          provider,
          "getAddonsByTypes",
          aTypes
        );

        if (providerAddons) {
          addons.push(...providerAddons);
        }
      }

      return addons;
    })();
  },

  /**
   * Gets active add-ons of specific types.
   *
   * This is similar to getAddonsByTypes() but it may return a limited
   * amount of information about only active addons.  Consequently, it
   * can be implemented by providers using only immediately available
   * data as opposed to getAddonsByTypes which may require I/O).
   *
   * @param  aTypes
   *         An optional array of types to retrieve. Each type is a string name
   *
   * @resolve {addons: Array, fullData: bool}
   *          fullData is true if addons contains all the data we have on those
   *          addons. It is false if addons only contains partial data.
   */
  async getActiveAddons(aTypes) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (aTypes && !Array.isArray(aTypes)) {
      throw Components.Exception(
        "aTypes must be an array or null",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let addons = [],
      fullData = true;

    for (let provider of this.providers) {
      let providerAddons, providerFullData;
      if ("getActiveAddons" in provider) {
        ({
          addons: providerAddons,
          fullData: providerFullData,
        } = await callProvider(provider, "getActiveAddons", null, aTypes));
      } else {
        providerAddons = await promiseCallProvider(
          provider,
          "getAddonsByTypes",
          aTypes
        );
        providerAddons = providerAddons.filter(a => a.isActive);
        providerFullData = true;
      }

      if (providerAddons) {
        addons.push(...providerAddons);
        fullData = fullData && providerFullData;
      }
    }

    return { addons, fullData };
  },

  /**
   * Asynchronously gets all installed add-ons.
   */
  getAllAddons() {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    return this.getAddonsByTypes(null);
  },

  /**
   * Adds a new AddonManagerListener if the listener is not already registered.
   *
   * @param {AddonManagerListener} aListener
   *         The listener to add
   */
  addManagerListener(aListener) {
    if (!aListener || typeof aListener != "object") {
      throw Components.Exception(
        "aListener must be an AddonManagerListener object",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    this.managerListeners.add(aListener);
  },

  /**
   * Removes an AddonManagerListener if the listener is registered.
   *
   * @param {AddonManagerListener} aListener
   *         The listener to remove
   */
  removeManagerListener(aListener) {
    if (!aListener || typeof aListener != "object") {
      throw Components.Exception(
        "aListener must be an AddonManagerListener object",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    this.managerListeners.delete(aListener);
  },

  /**
   * Adds a new AddonListener if the listener is not already registered.
   *
   * @param {AddonManagerListener} aListener
   *        The AddonListener to add.
   */
  addAddonListener(aListener) {
    if (!aListener || typeof aListener != "object") {
      throw Components.Exception(
        "aListener must be an AddonListener object",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    this.addonListeners.add(aListener);
  },

  /**
   * Removes an AddonListener if the listener is registered.
   *
   * @param {object}  aListener
   *         The AddonListener to remove
   */
  removeAddonListener(aListener) {
    if (!aListener || typeof aListener != "object") {
      throw Components.Exception(
        "aListener must be an AddonListener object",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    this.addonListeners.delete(aListener);
  },

  /**
   * Adds a new TypeListener if the listener is not already registered.
   *
   * @param {TypeListener} aListener
   *         The TypeListener to add
   */
  addTypeListener(aListener) {
    if (!aListener || typeof aListener != "object") {
      throw Components.Exception(
        "aListener must be a TypeListener object",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    this.typeListeners.add(aListener);
  },

  /**
   * Removes an TypeListener if the listener is registered.
   *
   * @param  aListener
   *         The TypeListener to remove
   */
  removeTypeListener(aListener) {
    if (!aListener || typeof aListener != "object") {
      throw Components.Exception(
        "aListener must be a TypeListener object",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    this.typeListeners.delete(aListener);
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
        if (!target.hasOwnProperty(property)) {
          return undefined;
        }

        return target[property].type;
      },

      getOwnPropertyDescriptor(target, property) {
        if (!target.hasOwnProperty(property)) {
          return undefined;
        }

        return {
          value: target[property].type,
          writable: false,
          // Claim configurability to maintain the proxy invariants.
          configurable: true,
          enumerable: true,
        };
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
      },
    });
  },

  get autoUpdateDefault() {
    return gAutoUpdateDefault;
  },

  set autoUpdateDefault(aValue) {
    aValue = !!aValue;
    if (aValue != gAutoUpdateDefault) {
      Services.prefs.setBoolPref(PREF_EM_AUTOUPDATE_DEFAULT, aValue);
    }
  },

  get checkCompatibility() {
    return gCheckCompatibility;
  },

  set checkCompatibility(aValue) {
    aValue = !!aValue;
    if (aValue != gCheckCompatibility) {
      if (!aValue) {
        Services.prefs.setBoolPref(PREF_EM_CHECK_COMPATIBILITY, false);
      } else {
        Services.prefs.clearUserPref(PREF_EM_CHECK_COMPATIBILITY);
      }
    }
  },

  get strictCompatibility() {
    return gStrictCompatibility;
  },

  set strictCompatibility(aValue) {
    aValue = !!aValue;
    if (aValue != gStrictCompatibility) {
      Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, aValue);
    }
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
      if (aValue != gCheckUpdateSecurityDefault) {
        Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, aValue);
      } else {
        Services.prefs.clearUserPref(PREF_EM_CHECK_UPDATE_SECURITY);
      }
    }
  },

  get updateEnabled() {
    return gUpdateEnabled;
  },

  set updateEnabled(aValue) {
    aValue = !!aValue;
    if (aValue != gUpdateEnabled) {
      Services.prefs.setBoolPref(PREF_EM_UPDATE_ENABLED, aValue);
    }
  },

  /**
   * Verify whether we need to show the 3rd party install prompt.
   *
   * Bypass the third party install prompt if this is an install:
   *   - is an install from a recognized source
   *   - is a an addon that can bypass the panel, such as a recommended addon
   *
   * @param {browser}      browser browser user is installing from
   * @param {nsIURI}       url     URI for the principal of the installing source
   * @param {AddonInstallWrapper} install
   * @param {Object}       info    information such as addon wrapper
   * @param {AddonWrapper} info.addon
   * @param {string}       source  simplified string describing source of install and is
   *                               generated based on the installing principal and checking
   *                               against site permissions and enterprise policy.
   *                               It may be one of "AMO", "local" or "other".
   * @returns {Promise}            Rejected when the installation should not proceed.
   */
  _verifyThirdPartyInstall(browser, url, install, info, source) {
    // If we are not post-download processing, this panel was already shown.
    // Otherwise, if this is from AMO or local, bypass the prompt.
    if (!WEBEXT_POSTDOWNLOAD_THIRD_PARTY || ["AMO", "local"].includes(source)) {
      return Promise.resolve();
    }

    // verify both the installing source and the xpi url are allowed.
    if (
      !info.addon.validInstallOrigins({
        installFrom: url,
        source: install.sourceURI,
      })
    ) {
      install.error = AddonManager.ERROR_INVALID_DOMAIN;
      return Promise.reject();
    }

    // Some addons such as recommended addons do not result in this prompt.
    if (info.addon.canBypassThirdParyInstallPrompt) {
      return Promise.resolve();
    }

    return new Promise((resolve, reject) => {
      this.installNotifyObservers(
        "addon-install-blocked",
        browser,
        url,
        install,
        resolve,
        reject
      );
    });
  },

  setupPromptHandler(browser, url, install, requireConfirm, source) {
    install.promptHandler = info =>
      new Promise((resolve, reject) => {
        this._verifyThirdPartyInstall(browser, url, install, info, source)
          .then(() => {
            // All installs end up in this callback when the add-on is available
            // for installation.  There are numerous different things that can
            // happen from here though.  For webextensions, if the application
            // implements webextension permission prompts, those always take
            // precedence.
            // If this add-on is not a webextension or if the application does not
            // implement permission prompts, no confirmation is displayed for
            // installs created from about:addons (in which case requireConfirm
            // is false).
            // In the remaining cases, a confirmation prompt is displayed but the
            // application may override it either by implementing the
            // "@mozilla.org/addons/web-install-prompt;1" contract or by setting
            // the customConfirmationUI preference and responding to the
            // "addon-install-confirmation" notification.  If the application
            // does not implement its own prompt, use the built-in xul dialog.
            if (info.addon.userPermissions) {
              let subject = {
                wrappedJSObject: {
                  target: browser,
                  info: Object.assign({ resolve, reject, source }, info),
                },
              };
              subject.wrappedJSObject.info.permissions =
                info.addon.userPermissions;
              Services.obs.notifyObservers(
                subject,
                "webextension-permission-prompt"
              );
            } else if (requireConfirm) {
              // The methods below all want to call the install() or cancel()
              // method on the provided AddonInstall object to either accept
              // or reject the confirmation.  Fit that into our promise-based
              // control flow by wrapping the install object.  However,
              // xpInstallConfirm.xul matches the install object it is passed
              // with the argument passed to an InstallListener, so give it
              // access to the underlying object through the .wrapped property.
              let proxy = new Proxy(install, {
                get(target, property) {
                  if (property == "install") {
                    return resolve;
                  } else if (property == "cancel") {
                    return reject;
                  } else if (property == "wrapped") {
                    return target;
                  }
                  let result = target[property];
                  return typeof result == "function"
                    ? result.bind(target)
                    : result;
                },
              });

              // Check for a custom installation prompt that may be provided by the
              // applicaton
              if ("@mozilla.org/addons/web-install-prompt;1" in Cc) {
                try {
                  let prompt = Cc[
                    "@mozilla.org/addons/web-install-prompt;1"
                  ].getService(Ci.amIWebInstallPrompt);
                  prompt.confirm(browser, url, [proxy]);
                  return;
                } catch (e) {}
              }

              this.installNotifyObservers(
                "addon-install-confirmation",
                browser,
                url,
                proxy
              );
            } else {
              resolve();
            }
          })
          .catch(e => {
            // Error is undefined if the promise was rejected.
            if (e) {
              Cu.reportError(`Install prompt handler error: ${e}`);
            }
            reject();
          });
      });
  },

  webAPI: {
    // installs maps integer ids to AddonInstall instances.
    installs: new Map(),
    nextInstall: 0,

    sendEvent: null,
    setEventHandler(fn) {
      this.sendEvent = fn;
    },

    async getAddonByID(target, id) {
      return webAPIForAddon(await AddonManager.getAddonByID(id));
    },

    // helper to copy (and convert) the properties we care about
    copyProps(install, obj) {
      obj.state = AddonManager.stateToString(install.state);
      obj.error = AddonManager.errorToString(install.error);
      obj.progress = install.progress;
      obj.maxProgress = install.maxProgress;
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
        let host = Services.io.newURI(options.url).host;
        if (WEBAPI_INSTALL_HOSTS.includes(host)) {
          return;
        }
        if (
          Services.prefs.getBoolPref(PREF_WEBAPI_TESTING) &&
          WEBAPI_TEST_INSTALL_HOSTS.includes(host)
        ) {
          return;
        }

        throw new Error(`Install from ${host} not permitted`);
      }

      const makeListener = (id, mm) => {
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
        let installPromise = new Promise((resolve, reject) => {
          events.forEach(event => {
            listener[event] = (install, addon) => {
              let data = { event, id };
              AddonManager.webAPI.copyProps(install, data);
              this.sendEvent(mm, data);
              if (event == "onInstallEnded") {
                resolve(addon);
              } else if (
                event == "onDownloadFailed" ||
                event == "onInstallFailed"
              ) {
                reject({ message: "install failed" });
              } else if (
                event == "onDownloadCancelled" ||
                event == "onInstallCancelled"
              ) {
                reject({ message: "install cancelled" });
              } else if (event == "onDownloadEnded") {
                if (install.addon.appDisabled) {
                  // App disabled items are not compatible and so fail to install
                  install.cancel();
                  AddonManagerInternal.installNotifyObservers(
                    "addon-install-failed",
                    target,
                    Services.io.newURI(options.url),
                    install
                  );
                }
              }
            };
          });
        });

        // We create the promise here since this is where we're setting
        // up the InstallListener, but if the install is never started,
        // no handlers will be attached so make sure we terminate errors.
        installPromise.catch(() => {});

        return { listener, installPromise };
      };

      try {
        checkInstallUrl(options.url);
      } catch (err) {
        return Promise.reject({ message: err.message });
      }

      return AddonManagerInternal.getInstallForURL(options.url, {
        browser: target,
        triggeringPrincipal: options.triggeringPrincipal,
        hash: options.hash,
        telemetryInfo: {
          source: AddonManager.getInstallSourceFromHost(options.sourceHost),
          sourceURL: options.sourceURL,
          method: "amWebAPI",
        },
      }).then(install => {
        let requireConfirm = true;
        if (
          target.contentDocument &&
          target.contentDocument.nodePrincipal.isSystemPrincipal
        ) {
          requireConfirm = false;
        }
        AddonManagerInternal.setupPromptHandler(
          target,
          null,
          install,
          requireConfirm,
          "AMO"
        );

        let id = this.nextInstall++;
        let { listener, installPromise } = makeListener(
          id,
          target.messageManager
        );
        install.addListener(listener);

        this.installs.set(id, {
          install,
          target,
          listener,
          installPromise,
          messageManager: target.messageManager,
        });

        let result = { id };
        this.copyProps(install, result);
        return result;
      });
    },

    async addonUninstall(target, id) {
      let addon = await AddonManager.getAddonByID(id);
      if (!addon) {
        return false;
      }

      if (!(addon.permissions & AddonManager.PERM_CAN_UNINSTALL)) {
        return Promise.reject({ message: "Addon cannot be uninstalled" });
      }

      try {
        addon.uninstall();
        return true;
      } catch (err) {
        Cu.reportError(err);
        return false;
      }
    },

    async addonSetEnabled(target, id, value) {
      let addon = await AddonManager.getAddonByID(id);
      if (!addon) {
        throw new Error(`No such addon ${id}`);
      }

      if (value) {
        await addon.enable();
      } else {
        await addon.disable();
      }
    },

    async addonInstallDoInstall(target, id) {
      let state = this.installs.get(id);
      if (!state) {
        throw new Error(`invalid id ${id}`);
      }

      let addon = await state.install.install();

      if (addon.type == "theme" && !addon.appDisabled) {
        await addon.enable();
      }

      await new Promise(resolve => {
        let subject = {
          wrappedJSObject: { target, addon, callback: resolve },
        };
        Services.obs.notifyObservers(subject, "webextension-install-notify");
      });
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
        if (info.messageManager == mm) {
          this.forgetInstall(id);
        }
      }
    },

    async addonReportAbuse(target, id) {
      if (!Services.prefs.getBoolPref(PREF_AMO_ABUSEREPORT, false)) {
        return Promise.reject({
          message: "amWebAPI reportAbuse not supported",
        });
      }

      let existingDialog = AbuseReporter.getOpenDialog();
      if (existingDialog) {
        existingDialog.close();
      }

      const dialog = await AbuseReporter.openDialog(id, "amo", target).catch(
        err => {
          Cu.reportError(err);
          return Promise.reject({
            message: "Error creating abuse report",
          });
        }
      );

      return dialog.promiseReport.then(
        async report => {
          if (!report) {
            return false;
          }

          await report.submit().catch(err => {
            Cu.reportError(err);
            return Promise.reject({
              message: "Error submitting abuse report",
            });
          });

          return true;
        },
        err => {
          Cu.reportError(err);
          dialog.close();
          return Promise.reject({
            message: "Error creating abuse report",
          });
        }
      );
    },
  },
};

/**
 * Should not be used outside of core Mozilla code. This is a private API for
 * the startup and platform integration code to use. Refer to the methods on
 * AddonManagerInternal for documentation however note that these methods are
 * subject to change at any time.
 */
var AddonManagerPrivate = {
  startup() {
    AddonManagerInternal.startup();
  },

  addonIsActive(addonId) {
    return AddonManagerInternal._getProviderByName("XPIProvider").addonIsActive(
      addonId
    );
  },

  /**
   * Gets an array of add-ons which were side-loaded prior to the last
   * startup, and are currently disabled.
   *
   * @returns {Promise<Array<Addon>>}
   */
  getNewSideloads() {
    return AddonManagerInternal._getProviderByName(
      "XPIProvider"
    ).getNewSideloads();
  },

  get browserUpdated() {
    return gBrowserUpdated;
  },

  registerProvider(aProvider, aTypes) {
    AddonManagerInternal.registerProvider(aProvider, aTypes);
  },

  unregisterProvider(aProvider) {
    AddonManagerInternal.unregisterProvider(aProvider);
  },

  markProviderSafe(aProvider) {
    AddonManagerInternal.markProviderSafe(aProvider);
  },

  backgroundUpdateCheck() {
    return AddonManagerInternal.backgroundUpdateCheck();
  },

  backgroundUpdateTimerHandler() {
    // Don't return the promise here, since the caller doesn't care.
    AddonManagerInternal.backgroundUpdateCheck();
  },

  addStartupChange(aType, aID) {
    AddonManagerInternal.addStartupChange(aType, aID);
  },

  removeStartupChange(aType, aID) {
    AddonManagerInternal.removeStartupChange(aType, aID);
  },

  notifyAddonChanged(aID, aType, aPendingRestart) {
    return AddonManagerInternal.notifyAddonChanged(aID, aType, aPendingRestart);
  },

  updateAddonAppDisabledStates() {
    AddonManagerInternal.updateAddonAppDisabledStates();
  },

  updateAddonRepositoryData() {
    return AddonManagerInternal.updateAddonRepositoryData();
  },

  callInstallListeners(...aArgs) {
    return AddonManagerInternal.callInstallListeners.apply(
      AddonManagerInternal,
      aArgs
    );
  },

  callAddonListeners(...aArgs) {
    AddonManagerInternal.callAddonListeners.apply(AddonManagerInternal, aArgs);
  },

  AddonAuthor,

  AddonScreenshot,

  AddonType,

  get BOOTSTRAP_REASONS() {
    return AddonManagerInternal._getProviderByName("XPIProvider")
      .BOOTSTRAP_REASONS;
  },

  recordTimestamp(name, value) {
    AddonManagerInternal.recordTimestamp(name, value);
  },

  _simpleMeasures: {},
  recordSimpleMeasure(name, value) {
    this._simpleMeasures[name] = value;
  },

  recordException(aModule, aContext, aException) {
    let report = {
      module: aModule,
      context: aContext,
    };

    if (typeof aException == "number") {
      report.message = Components.Exception("", aException).name;
    } else {
      report.message = aException.toString();
      if (aException.fileName) {
        report.file = aException.fileName;
        report.line = aException.lineNumber;
      }
    }

    this._simpleMeasures.exception = report;
  },

  getSimpleMeasures() {
    return this._simpleMeasures;
  },

  getTelemetryDetails() {
    return AddonManagerInternal.telemetryDetails;
  },

  setTelemetryDetails(aProvider, aDetails) {
    AddonManagerInternal.telemetryDetails[aProvider] = aDetails;
  },

  // Start a timer, record a simple measure of the time interval when
  // timer.done() is called
  simpleTimer(aName) {
    let startTime = Cu.now();
    return {
      done: () =>
        this.recordSimpleMeasure(aName, Math.round(Cu.now() - startTime)),
    };
  },

  async recordTiming(name, task) {
    let timer = this.simpleTimer(name);
    try {
      return await task();
    } finally {
      timer.done();
    }
  },

  /**
   * Helper to call update listeners when no update is available.
   *
   * This can be used as an implementation for Addon.findUpdates() when
   * no update mechanism is available.
   */
  callNoUpdateListeners(addon, listener, reason, appVersion, platformVersion) {
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

  hasUpgradeListener(aId) {
    return AddonManagerInternal.upgradeListeners.has(aId);
  },

  getUpgradeListener(aId) {
    return AddonManagerInternal.upgradeListeners.get(aId);
  },

  get externalExtensionLoaders() {
    return AddonManagerInternal.externalExtensionLoaders;
  },

  /**
   * Predicate that returns true if we think the given extension ID
   * might have been generated by XPIProvider.
   */
  isTemporaryInstallID(extensionId) {
    if (!gStarted) {
      throw Components.Exception(
        "AddonManager is not initialized",
        Cr.NS_ERROR_NOT_INITIALIZED
      );
    }

    if (!extensionId || typeof extensionId != "string") {
      throw Components.Exception(
        "extensionId must be a string",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    return AddonManagerInternal._getProviderByName(
      "XPIProvider"
    ).isTemporaryInstallID(extensionId);
  },

  isDBLoaded() {
    let provider = AddonManagerInternal._getProviderByName("XPIProvider");
    return provider ? provider.isDBLoaded : false;
  },

  get databaseReady() {
    let provider = AddonManagerInternal._getProviderByName("XPIProvider");
    return provider ? provider.databaseReady : new Promise(() => {});
  },

  /**
   * Async shutdown barrier which blocks the completion of add-on
   * manager shutdown. This should generally only be used by add-on
   * providers (i.e., XPIProvider) to complete their final shutdown
   * tasks.
   */
  get finalShutdown() {
    return gFinalShutdownBarrier.client;
  },
};

/**
 * This is the public API that UI and developers should be calling. All methods
 * just forward to AddonManagerInternal.
 * @class
 */
var AddonManager = {
  // Map used to convert the known install source hostnames into the value to set into the
  // telemetry events.
  _installHostSource: new Map([
    ["addons.mozilla.org", "amo"],
    ["discovery.addons.mozilla.org", "disco"],
  ]),

  // Constants for the AddonInstall.state property
  // These will show up as AddonManager.STATE_* (eg, STATE_AVAILABLE)
  _states: new Map([
    // The install is available for download.
    ["STATE_AVAILABLE", 0],
    // The install is being downloaded.
    ["STATE_DOWNLOADING", 1],
    // The install is checking the update for compatibility information.
    ["STATE_CHECKING_UPDATE", 2],
    // The install is downloaded and ready to install.
    ["STATE_DOWNLOADED", 3],
    // The download failed.
    ["STATE_DOWNLOAD_FAILED", 4],
    // The install may not proceed until the user accepts a prompt
    ["STATE_AWAITING_PROMPT", 5],
    // Any prompts are done
    ["STATE_PROMPTS_DONE", 6],
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
  // The _errors codes are translated to text for a panel in browser-addons.js.
  // The text is located in browser.properties.
  _errors: new Map([
    // The download failed due to network problems.
    ["ERROR_NETWORK_FAILURE", -1],
    // The downloaded file did not match the provided hash.
    ["ERROR_INCORRECT_HASH", -2],
    // The downloaded file seems to be corrupted in some way.
    ["ERROR_CORRUPT_FILE", -3],
    // An error occurred trying to write to the filesystem.
    ["ERROR_FILE_ACCESS", -4],
    // The add-on must be signed and isn't.
    ["ERROR_SIGNEDSTATE_REQUIRED", -5],
    // The downloaded add-on had a different type than expected.
    // TODO Bug 1740792
    ["ERROR_UNEXPECTED_ADDON_TYPE", -6],
    // The addon did not have the expected ID
    // TODO Bug 1740792
    ["ERROR_INCORRECT_ID", -7],
    // The addon install_origins does not list the 3rd party domain.
    ["ERROR_INVALID_DOMAIN", -8],
  ]),
  // The update check timed out
  ERROR_TIMEOUT: -1,
  // There was an error while downloading the update information.
  ERROR_DOWNLOAD_ERROR: -2,
  // The update information was malformed in some way.
  ERROR_PARSE_ERROR: -3,
  // The update information was not in any known format.
  ERROR_UNKNOWN_FORMAT: -4,
  // The update information was not correctly signed or there was an SSL error.
  ERROR_SECURITY_ERROR: -5,
  // The update was cancelled
  ERROR_CANCELLED: -6,
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
  // Indicates that the Addon can be set to be allowed/disallowed
  // in private browsing windows.
  PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS: 32,
  // Indicates that internal APIs can uninstall the add-on, even if the
  // front-end cannot.
  PERM_API_CAN_UNINSTALL: 64,

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

  // Constants for Addon.applyBackgroundUpdates.
  // Indicates that the Addon should not update automatically.
  AUTOUPDATE_DISABLE: 0,
  // Indicates that the Addon should update automatically only if
  // that's the global default.
  AUTOUPDATE_DEFAULT: 1,
  // Indicates that the Addon should update automatically.
  AUTOUPDATE_ENABLE: 2,

  // Constants for how Addon options should be shown.
  // Options will be displayed in a new tab, if possible
  OPTIONS_TYPE_TAB: 3,
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
  // Add-on is signed with a "Mozilla Extensions" certificate
  SIGNEDSTATE_PRIVILEGED: 4,

  get __AddonManagerInternal__() {
    return AppConstants.DEBUG ? AddonManagerInternal : undefined;
  },

  /** Boolean indicating whether AddonManager startup has completed. */
  get isReady() {
    return gStartupComplete && !gShutdownInProgress;
  },

  /** @constructor */
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

  getInstallSourceFromHost(host) {
    if (this._installHostSource.has(host)) {
      return this._installHostSource.get(host);
    }

    if (WEBAPI_TEST_INSTALL_HOSTS.includes(host)) {
      return "test-host";
    }

    return "unknown";
  },

  getInstallForURL(aUrl, aOptions) {
    return AddonManagerInternal.getInstallForURL(aUrl, aOptions);
  },

  getInstallForFile(
    aFile,
    aMimetype,
    aTelemetryInfo,
    aUseSystemLocation = false
  ) {
    return AddonManagerInternal.getInstallForFile(
      aFile,
      aMimetype,
      aTelemetryInfo,
      aUseSystemLocation
    );
  },

  uninstallSystemProfileAddon(aID) {
    return AddonManagerInternal.uninstallSystemProfileAddon(aID);
  },

  stageLangpacksForAppUpdate(appVersion, platformVersion) {
    return AddonManagerInternal._getProviderByName(
      "XPIProvider"
    ).stageLangpacksForAppUpdate(appVersion, platformVersion);
  },

  /**
   * Gets an array of add-on IDs that changed during the most recent startup.
   *
   * @param  aType
   *         The type of startup change to get
   * @return An array of add-on IDs
   */
  getStartupChanges(aType) {
    if (!(aType in AddonManagerInternal.startupChanges)) {
      return [];
    }
    return AddonManagerInternal.startupChanges[aType].slice(0);
  },

  getAddonByID(aID) {
    return AddonManagerInternal.getAddonByID(aID);
  },

  getAddonBySyncGUID(aGUID) {
    return AddonManagerInternal.getAddonBySyncGUID(aGUID);
  },

  getAddonsByIDs(aIDs) {
    return AddonManagerInternal.getAddonsByIDs(aIDs);
  },

  getAddonsByTypes(aTypes) {
    return AddonManagerInternal.getAddonsByTypes(aTypes);
  },

  getActiveAddons(aTypes) {
    return AddonManagerInternal.getActiveAddons(aTypes);
  },

  getAllAddons() {
    return AddonManagerInternal.getAllAddons();
  },

  getInstallsByTypes(aTypes) {
    return AddonManagerInternal.getInstallsByTypes(aTypes);
  },

  getAllInstalls() {
    return AddonManagerInternal.getAllInstalls();
  },

  isInstallEnabled(aType) {
    return AddonManagerInternal.isInstallEnabled(aType);
  },

  isInstallAllowed(aType, aInstallingPrincipal) {
    return AddonManagerInternal.isInstallAllowed(aType, aInstallingPrincipal);
  },

  installAddonFromWebpage(aType, aBrowser, aInstallingPrincipal, aInstall) {
    AddonManagerInternal.installAddonFromWebpage(
      aType,
      aBrowser,
      aInstallingPrincipal,
      aInstall
    );
  },

  installAddonFromAOM(aBrowser, aUri, aInstall) {
    AddonManagerInternal.installAddonFromAOM(aBrowser, aUri, aInstall);
  },

  installTemporaryAddon(aDirectory) {
    return AddonManagerInternal.installTemporaryAddon(aDirectory);
  },

  installBuiltinAddon(aBase) {
    return AddonManagerInternal.installBuiltinAddon(aBase);
  },

  maybeInstallBuiltinAddon(aID, aVersion, aBase) {
    return AddonManagerInternal.maybeInstallBuiltinAddon(aID, aVersion, aBase);
  },

  addManagerListener(aListener) {
    AddonManagerInternal.addManagerListener(aListener);
  },

  removeManagerListener(aListener) {
    AddonManagerInternal.removeManagerListener(aListener);
  },

  addInstallListener(aListener) {
    AddonManagerInternal.addInstallListener(aListener);
  },

  removeInstallListener(aListener) {
    AddonManagerInternal.removeInstallListener(aListener);
  },

  getUpgradeListener(aId) {
    return AddonManagerInternal.upgradeListeners.get(aId);
  },

  addUpgradeListener(aInstanceID, aCallback) {
    AddonManagerInternal.addUpgradeListener(aInstanceID, aCallback);
  },

  removeUpgradeListener(aInstanceID) {
    return AddonManagerInternal.removeUpgradeListener(aInstanceID);
  },

  addExternalExtensionLoader(types, loader) {
    return AddonManagerInternal.addExternalExtensionLoader(types, loader);
  },

  addAddonListener(aListener) {
    AddonManagerInternal.addAddonListener(aListener);
  },

  removeAddonListener(aListener) {
    AddonManagerInternal.removeAddonListener(aListener);
  },

  addTypeListener(aListener) {
    AddonManagerInternal.addTypeListener(aListener);
  },

  removeTypeListener(aListener) {
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
  shouldAutoUpdate(aAddon) {
    if (!aAddon || typeof aAddon != "object") {
      throw Components.Exception(
        "aAddon must be specified",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (!("applyBackgroundUpdates" in aAddon)) {
      return false;
    }
    if (!(aAddon.permissions & AddonManager.PERM_CAN_UPGRADE)) {
      return false;
    }
    if (aAddon.applyBackgroundUpdates == AddonManager.AUTOUPDATE_ENABLE) {
      return true;
    }
    if (aAddon.applyBackgroundUpdates == AddonManager.AUTOUPDATE_DISABLE) {
      return false;
    }
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

  escapeAddonURI(aAddon, aUri, aAppVersion) {
    return AddonManagerInternal.escapeAddonURI(aAddon, aUri, aAppVersion);
  },

  getPreferredIconURL(aAddon, aSize, aWindow = undefined) {
    return AddonManagerInternal.getPreferredIconURL(aAddon, aSize, aWindow);
  },

  get webAPI() {
    return AddonManagerInternal.webAPI;
  },

  /**
   * Async shutdown barrier which blocks the start of AddonManager
   * shutdown. Callers should add blockers to this barrier if they need
   * to complete add-on manager operations before it shuts down.
   */
  get beforeShutdown() {
    return gBeforeShutdownBarrier.client;
  },
};

/**
 * Listens to the AddonManager install and addon events and send telemetry events.
 */
AMTelemetry = {
  telemetrySetupDone: false,

  init() {
    // Enable the addonsManager telemetry event category before the AddonManager
    // has completed its startup, otherwise telemetry events recorded during the
    // AddonManager/XPIProvider startup will not be recorded.
    Services.telemetry.setEventRecordingEnabled("addonsManager", true);
  },

  // This method is called by the AddonManager, once it has been started, so that we can
  // init the telemetry event category and start listening for the events related to the
  // addons installation and management.
  onStartup() {
    if (this.telemetrySetupDone) {
      return;
    }

    this.telemetrySetupDone = true;

    Services.obs.addObserver(this, "addon-install-origin-blocked");
    Services.obs.addObserver(this, "addon-install-disabled");
    Services.obs.addObserver(this, "addon-install-blocked");

    AddonManager.addInstallListener(this);
    AddonManager.addAddonListener(this);
  },

  // Observer Service notification callback.

  observe(subject, topic, data) {
    switch (topic) {
      case "addon-install-blocked": {
        const { installs } = subject.wrappedJSObject;
        this.recordInstallEvent(installs[0], { step: "site_warning" });
        break;
      }
      case "addon-install-origin-blocked": {
        const { installs } = subject.wrappedJSObject;
        this.recordInstallEvent(installs[0], { step: "site_blocked" });
        break;
      }
      case "addon-install-disabled": {
        const { installs } = subject.wrappedJSObject;
        this.recordInstallEvent(installs[0], {
          step: "install_disabled_warning",
        });
        break;
      }
    }
  },

  // AddonManager install listener callbacks.

  onNewInstall(install) {
    this.recordInstallEvent(install, { step: "started" });
  },

  onInstallCancelled(install) {
    this.recordInstallEvent(install, { step: "cancelled" });
  },

  onInstallPostponed(install) {
    this.recordInstallEvent(install, { step: "postponed" });
  },

  onInstallFailed(install) {
    this.recordInstallEvent(install, { step: "failed" });
  },

  onInstallEnded(install) {
    this.recordInstallEvent(install, { step: "completed" });
    // Skip install_stats events for install objects related to.
    // add-on updates.
    if (!install.existingAddon) {
      this.recordInstallStatsEvent(install);
    }
  },

  onDownloadStarted(install) {
    this.recordInstallEvent(install, { step: "download_started" });
  },

  onDownloadCancelled(install) {
    this.recordInstallEvent(install, { step: "cancelled" });
  },

  onDownloadEnded(install) {
    let download_time = Math.round(Cu.now() - install.downloadStartedAt);
    this.recordInstallEvent(install, {
      step: "download_completed",
      download_time,
    });
  },

  onDownloadFailed(install) {
    let download_time = Math.round(Cu.now() - install.downloadStartedAt);
    this.recordInstallEvent(install, {
      step: "download_failed",
      download_time,
    });
  },

  // Addon listeners callbacks.

  onUninstalled(addon) {
    this.recordManageEvent(addon, "uninstall");
  },

  onEnabled(addon) {
    this.recordManageEvent(addon, "enable");
  },

  onDisabled(addon) {
    this.recordManageEvent(addon, "disable");
  },

  // Internal helpers methods.

  /**
   * Get a trimmed version of the given string if it is longer than 80 chars.
   *
   * @param {string} str
   *        The original string content.
   *
   * @returns {string}
   *          The trimmed version of the string when longer than 80 chars, or the given string
   *          unmodified otherwise.
   */
  getTrimmedString(str) {
    if (str.length <= 80) {
      return str;
    }

    const length = str.length;

    // Trim the string to prevent a flood of warnings messages logged internally by recordEvent,
    // the trimmed version is going to be composed by the first 40 chars and the last 37 and 3 dots
    // that joins the two parts, to visually indicate that the string has been trimmed.
    return `${str.slice(0, 40)}...${str.slice(length - 37, length)}`;
  },

  /**
   * Retrieve the addonId for the given AddonInstall instance.
   *
   * @param {AddonInstall} install
   *        The AddonInstall instance to retrieve the addonId from.
   *
   * @returns {string | null}
   *          The addonId for the given AddonInstall instance (if any).
   */
  getAddonIdFromInstall(install) {
    // Returns the id of the extension that is being installed, as soon as the
    // addon is available in the AddonInstall instance (after being downloaded
    // and validated successfully).
    if (install.addon) {
      return install.addon.id;
    }

    // While updating an addon, the existing addon can be
    // used to retrieve the addon id since the first update event.
    if (install.existingAddon) {
      return install.existingAddon.id;
    }

    return null;
  },

  /**
   * Retrieve the telemetry event's object property value for the given
   * AddonInstall instance.
   *
   * @param {AddonInstall} install
   *        The AddonInstall instance to retrieve the event object from.
   *
   * @returns {string}
   *          The object for the given AddonInstall instance.
   */
  getEventObjectFromInstall(install) {
    let addonType;

    if (install.type) {
      // The AddonInstall wrapper already provides a type (if it was known when the
      // install object has been created).
      addonType = install.type;
    } else if (install.addon) {
      // The install flow has reached a step that has an addon instance which we can
      // check to know the extension type (e.g. after download for the DownloadAddonInstall).
      addonType = install.addon.type;
    } else if (install.existingAddon) {
      // The install flow is an update and we can look the existingAddon to check which was
      // the add-on type that is being installed.
      addonType = install.existingAddon.type;
    }

    return this.getEventObjectFromAddonType(addonType);
  },

  /**
   * Retrieve the telemetry event source for the given AddonInstall instance.
   *
   * @param {AddonInstall} install
   *        The AddonInstall instance to retrieve the source from.
   *
   * @returns {Object | null}
   *          The telemetry infor ({source, method}) from the given AddonInstall instance.
   */
  getInstallTelemetryInfo(install) {
    if (install.installTelemetryInfo) {
      return install.installTelemetryInfo;
    } else if (
      install.existingAddon &&
      install.existingAddon.installTelemetryInfo
    ) {
      // Get the install source from the existing addon (e.g. for an extension update).
      return install.existingAddon.installTelemetryInfo;
    }

    return null;
  },

  /**
   * Get the telemetry event's object property for the given addon type
   *
   * @param {string} addonType
   *        The addon type to convert into the related telemetry event object.
   *
   * @returns {string}
   *          The object for the given addon type.
   */
  getEventObjectFromAddonType(addonType) {
    switch (addonType) {
      case undefined:
        return "unknown";
      case "extension":
      case "theme":
      case "locale":
      case "dictionary":
        return addonType;
      default:
        // Currently this should only include plugins and gmp-plugins
        return "other";
    }
  },

  convertToString(value) {
    if (value == null) {
      // Convert null and undefined to empty strings.
      return "";
    }
    switch (typeof value) {
      case "string":
        return value;
      case "boolean":
        return value ? "1" : "0";
    }
    return String(value);
  },

  /**
   * Return the UTM parameters found in `sourceURL` for AMO attribution data.
   *
   * @param {string} sourceURL
   *        The source URL from where the add-on has been installed.
   *
   * @returns {object}
   *          An object containing the attribution data for AMO if any. Keys
   *          are defined in `AMO_ATTRIBUTION_DATA_KEYS`. Values are strings.
   */
  parseAttributionDataForAMO(sourceURL) {
    let searchParams;

    try {
      searchParams = new URL(sourceURL).searchParams;
    } catch {
      return {};
    }

    const utmKeys = [...searchParams.keys()].filter(key =>
      AMO_ATTRIBUTION_DATA_KEYS.includes(key)
    );

    return utmKeys.reduce((params, key) => {
      let value = searchParams.get(key);
      if (typeof value === "string") {
        value = value.slice(0, AMO_ATTRIBUTION_DATA_MAX_LENGTH);
      }

      return { ...params, [key]: value };
    }, {});
  },

  /**
   * Record an "install stats" event when the source is included in
   * `AMO_ATTRIBUTION_ALLOWED_SOURCES`.
   *
   * @param {AddonInstall} install
   *        The AddonInstall instance to record an install_stats event for.
   */
  recordInstallStatsEvent(install) {
    const telemetryInfo = this.getInstallTelemetryInfo(install);

    if (!AMO_ATTRIBUTION_ALLOWED_SOURCES.includes(telemetryInfo?.source)) {
      return;
    }

    const method = "install_stats";
    const object = this.getEventObjectFromInstall(install);
    const addonId = this.getAddonIdFromInstall(install);

    if (!addonId) {
      Cu.reportError(
        "Missing addonId when trying to record an install_stats event"
      );
      return;
    }

    let extra = {
      addon_id: this.getTrimmedString(addonId),
    };

    if (
      telemetryInfo?.source === "amo" &&
      typeof telemetryInfo?.sourceURL === "string"
    ) {
      extra = {
        ...extra,
        ...this.parseAttributionDataForAMO(telemetryInfo.sourceURL),
      };
    }

    if (
      telemetryInfo?.source === "disco" &&
      typeof telemetryInfo?.taarRecommended === "boolean"
    ) {
      extra = {
        ...extra,
        taar_based: this.convertToString(telemetryInfo.taarRecommended),
      };
    }

    this.recordEvent({ method, object, value: install.hashedAddonId, extra });
  },

  /**
   * Convert all the telemetry event's extra_vars into strings, if needed.
   *
   * @param {object} extraVars
   * @returns {object} The formatted extra vars.
   */
  formatExtraVars({ addon, ...extraVars }) {
    if (addon) {
      extraVars.addonId = addon.id;
      extraVars.type = addon.type;
    }

    // All the extra_vars in a telemetry event have to be strings.
    for (var [key, value] of Object.entries(extraVars)) {
      if (value == undefined) {
        delete extraVars[key];
      } else {
        extraVars[key] = this.convertToString(value);
      }
    }

    if (extraVars.addonId) {
      extraVars.addonId = this.getTrimmedString(extraVars.addonId);
    }

    return extraVars;
  },

  /**
   * Record an install or update event for the given AddonInstall instance.
   *
   * @param {AddonInstall} install
   *        The AddonInstall instance to record an install or update event for.
   * @param {object} extraVars
   *        The additional extra_vars to include in the recorded event.
   * @param {string} extraVars.step
   *        The current step in the install or update flow.
   * @param {string} extraVars.download_time
   *        The number of ms needed to download the extension.
   * @param {string} extraVars.num_strings
   *        The number of permission description string for the extension
   *        permission doorhanger.
   */
  recordInstallEvent(install, extraVars) {
    // Early exit if AMTelemetry's telemetry setup has not been done yet.
    if (!this.telemetrySetupDone) {
      return;
    }

    let extra = {};

    let telemetryInfo = this.getInstallTelemetryInfo(install);
    if (telemetryInfo && typeof telemetryInfo.source === "string") {
      extra.source = telemetryInfo.source;
    }

    if (extra.source === "internal") {
      // Do not record the telemetry event for installation sources
      // that are marked as "internal".
      return;
    }

    // Also include the install source's method when applicable (e.g. install events with
    // source "about:addons" may have "install-from-file" or "url" as their source method).
    if (telemetryInfo && typeof telemetryInfo.method === "string") {
      extra.method = telemetryInfo.method;
    }

    let addonId = this.getAddonIdFromInstall(install);
    let object = this.getEventObjectFromInstall(install);

    let installId = String(install.installId);
    let eventMethod = install.existingAddon ? "update" : "install";

    if (addonId) {
      extra.addon_id = this.getTrimmedString(addonId);
    }

    if (install.error) {
      extra.error = AddonManager.errorToString(install.error);
    }

    if (
      eventMethod === "install" &&
      Services.prefs.getBoolPref("extensions.install_origins.enabled", true)
    ) {
      // This is converted to "1" / "0".
      extra.install_origins = Array.isArray(install.addon?.installOrigins);
    }

    if (eventMethod === "update") {
      // For "update" telemetry events, also include an extra var which determine
      // if the update has been requested by the user.
      extra.updated_from = install.isUserRequestedUpdate ? "user" : "app";
    }

    // All the extra vars in a telemetry event have to be strings.
    extra = this.formatExtraVars({ ...extraVars, ...extra });

    this.recordEvent({ method: eventMethod, object, value: installId, extra });
  },

  /**
   * Record a manage event for the given addon.
   *
   * @param {AddonWrapper} addon
   *        The AddonWrapper instance.
   * @param {object} extraVars
   *        The additional extra_vars to include in the recorded event.
   * @param {string} extraVars.num_strings
   *        The number of permission description string for the extension
   *        permission doorhanger.
   */
  recordManageEvent(addon, method, extraVars) {
    // Early exit if AMTelemetry's telemetry setup has not been done yet.
    if (!this.telemetrySetupDone) {
      return;
    }

    let extra = {};

    if (addon.installTelemetryInfo) {
      if ("source" in addon.installTelemetryInfo) {
        extra.source = addon.installTelemetryInfo.source;
      }

      // Also include the install source's method when applicable (e.g. install events with
      // source "about:addons" may have "install-from-file" or "url" as their source method).
      if ("method" in addon.installTelemetryInfo) {
        extra.method = addon.installTelemetryInfo.method;
      }
    }

    if (extra.source === "internal") {
      // Do not record the telemetry event for installation sources
      // that are marked as "internal".
      return;
    }

    let object = this.getEventObjectFromAddonType(addon.type);
    let value = this.getTrimmedString(addon.id);

    extra = { ...extraVars, ...extra };

    let hasExtraVars = !!Object.keys(extra).length;
    extra = this.formatExtraVars(extra);

    this.recordEvent({
      method,
      object,
      value,
      extra: hasExtraVars ? extra : null,
    });
  },

  /**
   * Record an event for when a link is clicked.
   *
   * @param {object} opts
   * @param {string} opts.object
   *        The object of the event, should be an identifier for where the link
   *        is located. The accepted values are listed in the
   *        addonsManager.link object of the Events.yaml file.
   * @param {string} opts.value The identifier for the link destination.
   * @param {object} opts.extra
   *        The extra data to be sent, all keys must be registered in the
   *        extra_keys section of addonsManager.link in Events.yaml.
   */
  recordLinkEvent({ object, value, extra = null }) {
    this.recordEvent({ method: "link", object, value, extra });
  },

  /**
   * Record an event for an action that took place.
   *
   * @param {object} opts
   * @param {string} opts.object
   *        The object of the event, should an identifier for where the action
   *        took place. The accepted values are listed in the
   *        addonsManager.action object of the Events.yaml file.
   * @param {string} opts.action The identifier for the action.
   * @param {string} opts.value An optional value for the action.
   * @param {object} opts.addon
   *        An optional object with the "id" and "type" properties, for example
   *        an AddonWrapper object. Passing this will set some extra properties.
   * @param {string} opts.addon.id
   *        The add-on ID to assign to extra.addonId.
   * @param {string} opts.addon.type
   *        The add-on type to assign to extra.type.
   * @param {string} opts.view The current view, when object is aboutAddons.
   * @param {object} opts.extra
   *        The extra data to be sent, all keys must be registered in the
   *        extra_keys section of addonsManager.action in Events.yaml. If
   *        opts.addon is passed then it will overwrite the addonId and type
   *        properties in this object, if they are set.
   */
  recordActionEvent({ object, action, value, addon, view, extra }) {
    extra = { ...extra, action, addon, view };
    if (action === "installFromRecommendation") {
      extra.taar_based = !!addon.taarRecommended;
    }
    this.recordEvent({
      method: "action",
      object,
      // Treat null and undefined as null.
      value: value == null ? null : this.convertToString(value),
      extra: this.formatExtraVars(extra),
    });
  },

  /**
   * Record an event for a view load in about:addons.
   *
   * @param {object} opts
   * @param {string} opts.view
   *        The identifier for the view. The accepted values are listed in the
   *        object property of addonsManager.view object of the Events.yaml
   *        file.
   * @param {AddonWrapper} opts.addon
   *        An optional add-on object related to the event.
   * @param {string} opts.type
   *        An optional type for the view. If opts.addon is set it will
   *        overwrite this value with the type of the add-on.
   * @param {boolean} opts.taarEnabled
   *        Set to true if taar-based discovery was enabled when the user
   *        did switch between about:addons views.
   */
  recordViewEvent({ view, addon, type, taarEnabled }) {
    this.recordEvent({
      method: "view",
      object: "aboutAddons",
      value: view,
      extra: this.formatExtraVars({
        type,
        addon,
        taar_enabled: taarEnabled,
      }),
    });
  },

  /**
   * Record an event on abuse report submissions.
   *
   * @params {object} opts
   * @params {string} opts.addonId
   *         The id of the addon being reported.
   * @params {string} [opts.addonType]
   *         The type of the addon being reported  (only present for an existing
   *         addonId).
   * @params {string} [opts.errorType]
   *         The AbuseReport errorType for a submission failure.
   * @params {string} opts.reportEntryPoint
   *         The entry point of the abuse report.
   */
  recordReportEvent({ addonId, addonType, errorType, reportEntryPoint }) {
    this.recordEvent({
      method: "report",
      object: reportEntryPoint,
      value: addonId,
      extra: this.formatExtraVars({
        addon_type: addonType,
        error_type: errorType,
      }),
    });
  },

  recordEvent({ method, object, value, extra }) {
    if (typeof value != "string") {
      // The value must be a string or null, make sure it's valid so sending
      // the event doesn't fail.
      value = null;
    }
    try {
      Services.telemetry.recordEvent(
        "addonsManager",
        method,
        object,
        value,
        extra
      );
    } catch (err) {
      // If the telemetry throws just log the error so it doesn't break any
      // functionality.
      Cu.reportError(err);
    }
  },
};

AddonManager.init();

// Setup the AMTelemetry once the AddonManager has been started.
AddonManager.addManagerListener(AMTelemetry);

// load the timestamps module into AddonManagerInternal
ChromeUtils.import(
  "resource://gre/modules/TelemetryTimestamps.jsm",
  AddonManagerInternal
);
Object.freeze(AddonManagerInternal);
Object.freeze(AddonManagerPrivate);
Object.freeze(AddonManager);
