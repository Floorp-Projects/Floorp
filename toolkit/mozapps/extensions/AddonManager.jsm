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

const PREF_BLOCKLIST_PINGCOUNT = "extensions.blocklist.pingCount";
const PREF_EM_UPDATE_ENABLED   = "extensions.update.enabled";
const PREF_EM_LAST_APP_VERSION = "extensions.lastAppVersion";
const PREF_EM_AUTOUPDATE_DEFAULT = "extensions.update.autoUpdateDefault";

Components.utils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = [ "AddonManager", "AddonManagerPrivate" ];

const CATEGORY_PROVIDER_MODULE = "addon-provider-module";

// A list of providers to load by default
const DEFAULT_PROVIDERS = [
  "resource://gre/modules/XPIProvider.jsm",
  "resource://gre/modules/LightweightThemeManager.jsm"
];

["LOG", "WARN", "ERROR"].forEach(function(aName) {
  this.__defineGetter__(aName, function() {
    Components.utils.import("resource://gre/modules/AddonLogging.jsm");

    LogManager.getLogger("addons.manager", this);
    return this[aName];
  });
}, this);

/**
 * Calls a callback method consuming any thrown exception. Any parameters after
 * the callback parameter will be passed to the callback.
 *
 * @param  aCallback
 *         The callback method to call
 */
function safeCall(aCallback) {
  var args = Array.slice(arguments, 1);

  try {
    aCallback.apply(null, args);
  }
  catch (e) {
    WARN("Exception calling callback", e);
  }
}

/**
 * Calls a method on a provider if it exists and consumes any thrown exception.
 * Any parameters after the dflt parameter are passed to the provider's method.
 *
 * @param  aProvider
 *         The provider to call
 * @param  aMethod
 *         The method name to call
 * @param  aDefault
 *         A default return value if the provider does not implement the named
 *         method or throws an error.
 * @return the return value from the provider or dflt if the provider does not
 *         implement method or throws an error
 */
function callProvider(aProvider, aMethod, aDefault) {
  if (!(aMethod in aProvider))
    return aDefault;

  var args = Array.slice(arguments, 3);

  try {
    return aProvider[aMethod].apply(aProvider, args);
  }
  catch (e) {
    ERROR("Exception calling provider" + aMethod, e);
    return aDefault;
  }
}

/**
 * A helper class to repeatedly call a listener with each object in an array
 * optionally checking whether the object has a method in it.
 *
 * @param  aObjects
 *         The array of objects to iterate through
 * @param  aMethod
 *         An optional method name, if not null any objects without this method
 *         will not be passed to the listener
 * @param  aListener
 *         A listener implementing nextObject and noMoreObjects methods. The
 *         former will be called with the AsyncObjectCaller as the first
 *         parameter and the object as the second. noMoreObjects will be passed
 *         just the AsyncObjectCaller
 */
function AsyncObjectCaller(aObjects, aMethod, aListener) {
  this.objects = aObjects.slice(0);
  this.method = aMethod;
  this.listener = aListener;

  this.callNext();
}

AsyncObjectCaller.prototype = {
  objects: null,
  method: null,
  listener: null,

  /**
   * Passes the next object to the listener or calls noMoreObjects if there
   * are none left.
   */
  callNext: function AOC_callNext() {
    if (this.objects.length == 0) {
      this.listener.noMoreObjects(this);
      return;
    }

    let object = this.objects.shift();
    if (!this.method || this.method in object)
      this.listener.nextObject(this, object);
    else
      this.callNext();
  }
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
 * @param  aThumbnailURL
 *         The URL to the thumbnail version of the screenshot
 * @param  aCaption
 *         The caption of the screenshot
 */
function AddonScreenshot(aURL, aThumbnailURL, aCaption) {
  this.url = aURL;
  this.thumbnailURL = aThumbnailURL;
  this.caption = aCaption;
}

AddonScreenshot.prototype = {
  url: null,
  thumbnailURL: null,
  caption: null,

  // Returns the screenshot URL, defaulting to the empty string
  toString: function() {
    return this.url || "";
  }
}

var gStarted = false;

/**
 * This is the real manager, kept here rather than in AddonManager to keep its
 * contents hidden from API users.
 */
var AddonManagerInternal = {
  installListeners: [],
  addonListeners: [],
  providers: [],

  /**
   * Initializes the AddonManager, loading any known providers and initializing
   * them.
   */
  startup: function AMI_startup() {
    if (gStarted)
      return;

    let appChanged = undefined;

    try {
      appChanged = Services.appinfo.version !=
                   Services.prefs.getCharPref(PREF_EM_LAST_APP_VERSION);
    }
    catch (e) { }

    if (appChanged !== false) {
      LOG("Application has been upgraded");
      Services.prefs.setCharPref(PREF_EM_LAST_APP_VERSION,
                                 Services.appinfo.version);
      Services.prefs.setIntPref(PREF_BLOCKLIST_PINGCOUNT,
                                (appChanged === undefined ? 0 : 1));
    }

    // Ensure all default providers have had a chance to register themselves
    DEFAULT_PROVIDERS.forEach(function(url) {
      try {
        Components.utils.import(url, {});
      }
      catch (e) {
        ERROR("Exception loading default provider \"" + url + "\"", e);
      }
    });

    // Load any providers registered in the category manager
    let catman = Cc["@mozilla.org/categorymanager;1"].
                 getService(Ci.nsICategoryManager);
    let entries = catman.enumerateCategory(CATEGORY_PROVIDER_MODULE);
    while (entries.hasMoreElements()) {
      let entry = entries.getNext().QueryInterface(Ci.nsISupportsCString).data;
      let url = catman.getCategoryEntry(CATEGORY_PROVIDER_MODULE, entry);

      try {
        Components.utils.import(url, {});
      }
      catch (e) {
        ERROR("Exception loading provider " + entry + " from category \"" +
              url + "\"", e);
      }
    }

    this.providers.forEach(function(provider) {
      callProvider(provider, "startup", null, appChanged);
    });
    gStarted = true;
  },

  /**
   * Registers a new AddonProvider.
   *
   * @param  aProvider
   *         The provider to register
   */
  registerProvider: function AMI_registerProvider(aProvider) {
    this.providers.push(aProvider);

    // If we're registering after startup call this provider's startup.
    if (gStarted)
      callProvider(aProvider, "startup");
  },

  /**
   * Unregisters an AddonProvider.
   *
   * @param  aProvider
   *         The provider to unregister
   */
  unregisterProvider: function AMI_unregisterProvider(aProvider) {
    let pos = 0;
    while (pos < this.providers.length) {
      if (this.providers[pos] == aProvider)
        this.providers.splice(pos, 1);
      else
        pos++;
    }

    // If we're unregistering after startup call this provider's shutdown.
    if (gStarted)
      callProvider(aProvider, "shutdown");
  },

  /**
   * Shuts down the addon manager and all registered providers, this must clean
   * up everything in order for automated tests to fake restarts.
   */
  shutdown: function AM_shutdown() {
    this.providers.forEach(function(provider) {
      callProvider(provider, "shutdown");
    });

    this.installListeners.splice(0);
    this.addonListeners.splice(0);
    gStarted = false;
  },

  /**
   * Performs a background update check by starting an update for all add-ons
   * that can be updated.
   */
  backgroundUpdateCheck: function AMI_backgroundUpdateCheck() {
    if (!Services.prefs.getBoolPref(PREF_EM_UPDATE_ENABLED))
      return;

    Services.obs.notifyObservers(null, "addons-background-update-start", null);
    let pendingUpdates = 1;

    function notifyComplete() {
      if (--pendingUpdates == 0)
        Services.obs.notifyObservers(null, "addons-background-update-complete", null);
    }

    let scope = {};
    Components.utils.import("resource://gre/modules/AddonRepository.jsm", scope);
    Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm", scope);
    scope.LightweightThemeManager.updateCurrentTheme();

    this.getAllAddons(function getAddonsCallback(aAddons) {
      if ("getCachedAddonByID" in scope.AddonRepository) {
        pendingUpdates++;
        var ids = [a.id for each (a in aAddons)];
        scope.AddonRepository.repopulateCache(ids, notifyComplete);
      }

      pendingUpdates += aAddons.length;
      var autoUpdateDefault = AddonManager.autoUpdateDefault;

      function shouldAutoUpdate(aAddon) {
        if (!("applyBackgroundUpdates" in aAddon))
          return false;
        if (aAddon.applyBackgroundUpdates == AddonManager.AUTOUPDATE_ENABLE)
          return true;
        if (aAddon.applyBackgroundUpdates == AddonManager.AUTOUPDATE_DISABLE)
          return false;
        return autoUpdateDefault;
      }

      aAddons.forEach(function BUC_forEachCallback(aAddon) {
        // Check all add-ons for updates so that any compatibility updates will
        // be applied
        aAddon.findUpdates({
          onUpdateAvailable: function BUC_onUpdateAvailable(aAddon, aInstall) {
            // Start installing updates when the add-on can be updated and
            // background updates should be applied.
            if (aAddon.permissions & AddonManager.PERM_CAN_UPGRADE &&
                shouldAutoUpdate(aAddon)) {
              aInstall.install();
            }
          },

          onUpdateFinished: notifyComplete
        }, AddonManager.UPDATE_WHEN_PERIODIC_UPDATE);
      });

      notifyComplete();
    });
  },

  /**
   * Calls all registered InstallListeners with an event. Any parameters after
   * the extraListeners parameter are passed to the listener.
   *
   * @param  aMethod
   *         The method on the listeners to call
   * @param  aExtraListeners
   *         An array of extra InstallListeners to also call
   * @return false if any of the listeners returned false, true otherwise
   */
  callInstallListeners: function AMI_callInstallListeners(aMethod, aExtraListeners) {
    let result = true;
    let listeners = this.installListeners;
    if (aExtraListeners)
      listeners = aExtraListeners.concat(listeners);
    let args = Array.slice(arguments, 2);

    listeners.forEach(function(listener) {
      try {
        if (aMethod in listener) {
          if (listener[aMethod].apply(listener, args) === false)
            result = false;
        }
      }
      catch (e) {
        WARN("InstallListener threw exception when calling " + aMethod, e);
      }
    });
    return result;
  },

  /**
   * Calls all registered AddonListeners with an event. Any parameters after
   * the method parameter are passed to the listener.
   *
   * @param  aMethod
   *         The method on the listeners to call
   */
  callAddonListeners: function AMI_callAddonListeners(aMethod) {
    var args = Array.slice(arguments, 1);
    this.addonListeners.forEach(function(listener) {
      try {
        if (aMethod in listener)
          listener[aMethod].apply(listener, args);
      }
      catch (e) {
        WARN("AddonListener threw exception when calling " + aMethod, e);
      }
    });
  },

  /**
   * Notifies all providers that an add-on has been enabled when that type of
   * add-on only supports a single add-on being enabled at a time. This allows
   * the providers to disable theirs if necessary.
   *
   * @param  aId
   *         The id of the enabled add-on
   * @param  aType
   *         The type of the enabled add-on
   * @param  aPendingRestart
   *         A boolean indicating if the change will only take place the next
   *         time the application is restarted
   */
  notifyAddonChanged: function AMI_notifyAddonChanged(aId, aType, aPendingRestart) {
    this.providers.forEach(function(provider) {
      callProvider(provider, "addonChanged", null, aId, aType, aPendingRestart);
    });
  },

  /**
   * Notifies all providers they need to update the appDisabled property for
   * their add-ons in response to an application change such as a blocklist
   * update.
   */
  updateAddonAppDisabledStates: function AMI_updateAddonAppDisabledStates() {
    this.providers.forEach(function(provider) {
      callProvider(provider, "updateAddonAppDisabledStates");
    });
  },

  /**
   * Asynchronously gets an AddonInstall for a URL.
   *
   * @param  aUrl
   *         The url the add-on is located at
   * @param  aCallback
   *         A callback to pass the AddonInstall to
   * @param  aMimetype
   *         The mimetype of the add-on
   * @param  aHash
   *         An optional hash of the add-on
   * @param  aName
   *         An optional placeholder name while the add-on is being downloaded
   * @param  aIconURL
   *         An optional placeholder icon URL while the add-on is being downloaded
   * @param  aVersion
   *         An optional placeholder version while the add-on is being downloaded
   * @param  aLoadGroup
   *         An optional nsILoadGroup to associate any network requests with
   * @throws if the aUrl, aCallback or aMimetype arguments are not specified
   */
  getInstallForURL: function AMI_getInstallForURL(aUrl, aCallback, aMimetype,
                                                  aHash, aName, aIconURL,
                                                  aVersion, aLoadGroup) {
    if (!aUrl || !aMimetype || !aCallback)
      throw new TypeError("Invalid arguments");

    for (let i = 0; i < this.providers.length; i++) {
      if (callProvider(this.providers[i], "supportsMimetype", false, aMimetype)) {
        callProvider(this.providers[i], "getInstallForURL", null,
                     aUrl, aHash, aName, aIconURL, aVersion, aLoadGroup,
                     function(aInstall) {
          safeCall(aCallback, aInstall);
        });
        return;
      }
    }
    safeCall(aCallback, null);
  },

  /**
   * Asynchronously gets an AddonInstall for an nsIFile.
   *
   * @param  aFile
   *         the nsIFile where the add-on is located
   * @param  aCallback
   *         A callback to pass the AddonInstall to
   * @param  aMimetype
   *         An optional mimetype hint for the add-on
   * @throws if the aFile or aCallback arguments are not specified
   */
  getInstallForFile: function AMI_getInstallForFile(aFile, aCallback, aMimetype) {
    if (!aFile || !aCallback)
      throw Cr.NS_ERROR_INVALID_ARG;

    new AsyncObjectCaller(this.providers, "getInstallForFile", {
      nextObject: function(aCaller, aProvider) {
        callProvider(aProvider, "getInstallForFile", null, aFile,
                     function(aInstall) {
          if (aInstall)
            safeCall(aCallback, aInstall);
          else
            aCaller.callNext();
        });
      },

      noMoreObjects: function(aCaller) {
        safeCall(aCallback, null);
      }
    });
  },

  /**
   * Asynchronously gets all current AddonInstalls optionally limiting to a list
   * of types.
   *
   * @param  aTypes
   *         An optional array of types to retrieve. Each type is a string name
   * @param  aCallback
   *         A callback which will be passed an array of AddonInstalls
   * @throws if the aCallback argument is not specified
   */
  getInstallsByTypes: function AMI_getInstallsByTypes(aTypes, aCallback) {
    if (!aCallback)
      throw Cr.NS_ERROR_INVALID_ARG;

    let installs = [];

    new AsyncObjectCaller(this.providers, "getInstallsByTypes", {
      nextObject: function(aCaller, aProvider) {
        callProvider(aProvider, "getInstallsByTypes", null, aTypes,
                     function(aProviderInstalls) {
          installs = installs.concat(aProviderInstalls);
          aCaller.callNext();
        });
      },

      noMoreObjects: function(aCaller) {
        safeCall(aCallback, installs);
      }
    });
  },

  /**
   * Asynchronously gets all current AddonInstalls.
   *
   * @param  aCallback
   *         A callback which will be passed an array of AddonInstalls
   */
  getAllInstalls: function AMI_getAllInstalls(aCallback) {
    this.getInstallsByTypes(null, aCallback);
  },

  /**
   * Checks whether installation is enabled for a particular mimetype.
   *
   * @param  aMimetype
   *         The mimetype to check
   * @return true if installation is enabled for the mimetype
   */
  isInstallEnabled: function AMI_isInstallEnabled(aMimetype) {
    for (let i = 0; i < this.providers.length; i++) {
      if (callProvider(this.providers[i], "supportsMimetype", false, aMimetype) &&
          callProvider(this.providers[i], "isInstallEnabled"))
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
   * @param  aURI
   *         The uri of the source, may be null
   * @return true if the source is allowed to install this mimetype
   */
  isInstallAllowed: function AMI_isInstallAllowed(aMimetype, aURI) {
    for (let i = 0; i < this.providers.length; i++) {
      if (callProvider(this.providers[i], "supportsMimetype", false, aMimetype) &&
          callProvider(this.providers[i], "isInstallAllowed", null, aURI))
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
   * @param  aSource
   *         The nsIDOMWindowInternal that started the installs
   * @param  aURI
   *         the nsIURI that started the installs
   * @param  aInstalls
   *         The array of AddonInstalls to be installed
   */
  installAddonsFromWebpage: function AMI_installAddonsFromWebpage(aMimetype,
                                                                  aSource,
                                                                  aURI,
                                                                  aInstalls) {
    if (!("@mozilla.org/addons/web-install-listener;1" in Cc)) {
      WARN("No web installer available, cancelling all installs");
      aInstalls.forEach(function(aInstall) {
        aInstall.cancel();
      });
      return;
    }

    try {
      let weblistener = Cc["@mozilla.org/addons/web-install-listener;1"].
                        getService(Ci.amIWebInstallListener);

      if (!this.isInstallAllowed(aMimetype, aURI)) {
        if (weblistener.onWebInstallBlocked(aSource, aURI, aInstalls,
                                            aInstalls.length)) {
          aInstalls.forEach(function(aInstall) {
            aInstall.install();
          });
        }
      }
      else if (weblistener.onWebInstallRequested(aSource, aURI, aInstalls,
                                                   aInstalls.length)) {
        aInstalls.forEach(function(aInstall) {
          aInstall.install();
        });
      }
    }
    catch (e) {
      // In the event that the weblistener throws during instatiation or when
      // calling onWebInstallBlocked or onWebInstallRequested all of the
      // installs should get cancelled.
      WARN("Failure calling web installer", e);
      aInstalls.forEach(function(aInstall) {
        aInstall.cancel();
      });
    }
  },

  /**
   * Adds a new InstallListener if the listener is not already registered.
   *
   * @param  aListener
   *         The InstallListener to add
   */
  addInstallListener: function AMI_addInstallListener(aListener) {
    if (!this.installListeners.some(function(i) { return i == aListener; }))
      this.installListeners.push(aListener);
  },

  /**
   * Removes an InstallListener if the listener is registered.
   *
   * @param  aListener
   *         The InstallListener to remove
   */
  removeInstallListener: function AMI_removeInstallListener(aListener) {
    let pos = 0;
    while (pos < this.installListeners.length) {
      if (this.installListeners[pos] == aListener)
        this.installListeners.splice(pos, 1);
      else
        pos++;
    }
  },

  /**
   * Asynchronously gets an add-on with a specific ID.
   *
   * @param  aId
   *         The ID of the add-on to retrieve
   * @param  aCallback
   *         The callback to pass the retrieved add-on to
   * @throws if the aId or aCallback arguments are not specified
   */
  getAddonByID: function AMI_getAddonByID(aId, aCallback) {
    if (!aId || !aCallback)
      throw Cr.NS_ERROR_INVALID_ARG;

    new AsyncObjectCaller(this.providers, "getAddonByID", {
      nextObject: function(aCaller, aProvider) {
        callProvider(aProvider, "getAddonByID", null, aId, function(aAddon) {
          if (aAddon)
            safeCall(aCallback, aAddon);
          else
            aCaller.callNext();
        });
      },

      noMoreObjects: function(aCaller) {
        safeCall(aCallback, null);
      }
    });
  },

  /**
   * Asynchronously gets an array of add-ons.
   *
   * @param  aIds
   *         The array of IDs to retrieve
   * @param  aCallback
   *         The callback to pass an array of Addons to
   * @throws if the aId or aCallback arguments are not specified
   */
  getAddonsByIDs: function AMI_getAddonsByIDs(aIds, aCallback) {
    if (!aIds || !aCallback)
      throw Cr.NS_ERROR_INVALID_ARG;

    let addons = [];

    new AsyncObjectCaller(aIds, null, {
      nextObject: function(aCaller, aId) {
        AddonManagerInternal.getAddonByID(aId, function(aAddon) {
          addons.push(aAddon);
          aCaller.callNext();
        });
      },

      noMoreObjects: function(aCaller) {
        safeCall(aCallback, addons);
      }
    });
  },

  /**
   * Asynchronously gets add-ons of specific types.
   *
   * @param  aTypes
   *         An optional array of types to retrieve. Each type is a string name
   * @param  aCallback
   *         The callback to pass an array of Addons to.
   * @throws if the aCallback argument is not specified
   */
  getAddonsByTypes: function AMI_getAddonsByTypes(aTypes, aCallback) {
    if (!aCallback)
      throw Cr.NS_ERROR_INVALID_ARG;

    let addons = [];

    new AsyncObjectCaller(this.providers, "getAddonsByTypes", {
      nextObject: function(aCaller, aProvider) {
        callProvider(aProvider, "getAddonsByTypes", null, aTypes,
                     function(aProviderAddons) {
          addons = addons.concat(aProviderAddons);
          aCaller.callNext();
        });
      },

      noMoreObjects: function(aCaller) {
        safeCall(aCallback, addons);
      }
    });
  },

  /**
   * Asynchronously gets all installed add-ons.
   *
   * @param  aCallback
   *         A callback which will be passed an array of Addons
   */
  getAllAddons: function AMI_getAllAddons(aCallback) {
    this.getAddonsByTypes(null, aCallback);
  },

  /**
   * Asynchronously gets add-ons that have operations waiting for an application
   * restart to complete.
   *
   * @param  aTypes
   *         An optional array of types to retrieve. Each type is a string name
   * @param  aCallback
   *         The callback to pass the array of Addons to
   * @throws if the aCallback argument is not specified
   */
  getAddonsWithOperationsByTypes:
  function AMI_getAddonsWithOperationsByTypes(aTypes, aCallback) {
    if (!aCallback)
      throw Cr.NS_ERROR_INVALID_ARG;

    let addons = [];

    new AsyncObjectCaller(this.providers, "getAddonsWithOperationsByTypes", {
      nextObject: function(aCaller, aProvider) {
        callProvider(aProvider, "getAddonsWithOperationsByTypes", null, aTypes,
                     function(aProviderAddons) {
          addons = addons.concat(aProviderAddons);
          aCaller.callNext();
        });
      },

      noMoreObjects: function(caller) {
        safeCall(aCallback, addons);
      }
    });
  },

  /**
   * Adds a new AddonListener if the listener is not already registered.
   *
   * @param  aListener
   *         The listener to add
   */
  addAddonListener: function AMI_addAddonListener(aListener) {
    if (!this.addonListeners.some(function(i) { return i == aListener; }))
      this.addonListeners.push(aListener);
  },

  /**
   * Removes an AddonListener if the listener is registered.
   *
   * @param  aListener
   *         The listener to remove
   */
  removeAddonListener: function AMI_removeAddonListener(aListener) {
    let pos = 0;
    while (pos < this.addonListeners.length) {
      if (this.addonListeners[pos] == aListener)
        this.addonListeners.splice(pos, 1);
      else
        pos++;
    }
  },
  
  get autoUpdateDefault() {
    try {
      return Services.prefs.getBoolPref(PREF_EM_AUTOUPDATE_DEFAULT);
    } catch(e) { }
    return true;
  }
};

/**
 * Should not be used outside of core Mozilla code. This is a private API for
 * the startup and platform integration code to use. Refer to the methods on
 * AddonManagerInternal for documentation however note that these methods are
 * subject to change at any time.
 */
var AddonManagerPrivate = {
  startup: function AMP_startup() {
    AddonManagerInternal.startup();
  },

  registerProvider: function AMP_registerProvider(aProvider) {
    AddonManagerInternal.registerProvider(aProvider);
  },

  unregisterProvider: function AMP_unregisterProvider(aProvider) {
    AddonManagerInternal.unregisterProvider(aProvider);
  },

  shutdown: function AMP_shutdown() {
    AddonManagerInternal.shutdown();
  },

  backgroundUpdateCheck: function AMP_backgroundUpdateCheck() {
    AddonManagerInternal.backgroundUpdateCheck();
  },

  notifyAddonChanged: function AMP_notifyAddonChanged(aId, aType, aPendingRestart) {
    AddonManagerInternal.notifyAddonChanged(aId, aType, aPendingRestart);
  },

  updateAddonAppDisabledStates: function AMP_updateAddonAppDisabledStates() {
    AddonManagerInternal.updateAddonAppDisabledStates();
  },

  callInstallListeners: function AMP_callInstallListeners(aMethod) {
    return AddonManagerInternal.callInstallListeners.apply(AddonManagerInternal,
                                                           arguments);
  },

  callAddonListeners: function AMP_callAddonListeners(aMethod) {
    AddonManagerInternal.callAddonListeners.apply(AddonManagerInternal, arguments);
  },

  AddonAuthor: AddonAuthor,

  AddonScreenshot: AddonScreenshot
};

/**
 * This is the public API that UI and developers should be calling. All methods
 * just forward to AddonManagerInternal.
 */
var AddonManager = {
  // Constants for the AddonInstall.state property
  // The install is available for download.
  STATE_AVAILABLE: 0,
  // The install is being downloaded.
  STATE_DOWNLOADING: 1,
  // The install is checking for compatibility information.
  STATE_CHECKING: 2,
  // The install is downloaded and ready to install.
  STATE_DOWNLOADED: 3,
  // The download failed.
  STATE_DOWNLOAD_FAILED: 4,
  // The add-on is being installed.
  STATE_INSTALLING: 5,
  // The add-on has been installed.
  STATE_INSTALLED: 6,
  // The install failed.
  STATE_INSTALL_FAILED: 7,
  // The install has been cancelled.
  STATE_CANCELLED: 8,

  // Constants representing different types of errors while downloading an
  // add-on.
  // The download failed due to network problems.
  ERROR_NETWORK_FAILURE: -1,
  // The downloaded file did not match the provided hash.
  ERROR_INCORRECT_HASH: -2,
  // The downloaded file seems to be corrupted in some way.
  ERROR_CORRUPT_FILE: -3,
  // An error occured trying to write to the filesystem.
  ERROR_FILE_ACCESS: -4,

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

  // General descriptions of where items are installed.
  // Installed in this profile.
  SCOPE_PROFILE: 1,
  // Installed for all of this user's profiles.
  SCOPE_USER: 2,
  // Installed and owned by the application.
  SCOPE_APPLICATION: 4,
  // Installed for all users of the computer.
  SCOPE_SYSTEM: 8,
  // The combination of all scopes.
  SCOPE_ALL: 15,
  
  // Constants for Addon.applyBackgroundUpdates.
  // Indicates that the Addon should not update automatically.
  AUTOUPDATE_DISABLE: 0,
  // Indicates that the Addon should update automatically only if
  // that's the global default.
  AUTOUPDATE_DEFAULT: 1,
  // Indicates that the Addon should update automatically.
  AUTOUPDATE_ENABLE: 2,

  getInstallForURL: function AM_getInstallForURL(aUrl, aCallback, aMimetype,
                                                 aHash, aName, aIconURL,
                                                 aVersion, aLoadGroup) {
    AddonManagerInternal.getInstallForURL(aUrl, aCallback, aMimetype, aHash,
                                          aName, aIconURL, aVersion, aLoadGroup);
  },

  getInstallForFile: function AM_getInstallForFile(aFile, aCallback, aMimetype) {
    AddonManagerInternal.getInstallForFile(aFile, aCallback, aMimetype);
  },

  getAddonByID: function AM_getAddonByID(aId, aCallback) {
    AddonManagerInternal.getAddonByID(aId, aCallback);
  },

  getAddonsByIDs: function AM_getAddonsByIDs(aIds, aCallback) {
    AddonManagerInternal.getAddonsByIDs(aIds, aCallback);
  },

  getAddonsWithOperationsByTypes:
  function AM_getAddonsWithOperationsByTypes(aTypes, aCallback) {
    AddonManagerInternal.getAddonsWithOperationsByTypes(aTypes, aCallback);
  },

  getAddonsByTypes: function AM_getAddonsByTypes(aTypes, aCallback) {
    AddonManagerInternal.getAddonsByTypes(aTypes, aCallback);
  },

  getAllAddons: function AM_getAllAddons(aCallback) {
    AddonManagerInternal.getAllAddons(aCallback);
  },

  getInstallsByTypes: function AM_getInstallsByTypes(aTypes, aCallback) {
    AddonManagerInternal.getInstallsByTypes(aTypes, aCallback);
  },

  getAllInstalls: function AM_getAllInstalls(aCallback) {
    AddonManagerInternal.getAllInstalls(aCallback);
  },

  isInstallEnabled: function AM_isInstallEnabled(aType) {
    return AddonManagerInternal.isInstallEnabled(aType);
  },

  isInstallAllowed: function AM_isInstallAllowed(aType, aUri) {
    return AddonManagerInternal.isInstallAllowed(aType, aUri);
  },

  installAddonsFromWebpage: function AM_installAddonsFromWebpage(aType, aSource,
                                                                 aUri, aInstalls) {
    AddonManagerInternal.installAddonsFromWebpage(aType, aSource, aUri, aInstalls);
  },

  addInstallListener: function AM_addInstallListener(aListener) {
    AddonManagerInternal.addInstallListener(aListener);
  },

  removeInstallListener: function AM_removeInstallListener(aListener) {
    AddonManagerInternal.removeInstallListener(aListener);
  },

  addAddonListener: function AM_addAddonListener(aListener) {
    AddonManagerInternal.addAddonListener(aListener);
  },

  removeAddonListener: function AM_removeAddonListener(aListener) {
    AddonManagerInternal.removeAddonListener(aListener);
  },
  
  get autoUpdateDefault() {
    return AddonManagerInternal.autoUpdateDefault;
  }
};

Object.freeze(AddonManagerInternal);
Object.freeze(AddonManagerPrivate);
Object.freeze(AddonManager);
