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

const PREF_EM_UPDATE_ENABLED = "extensions.update.enabled";

Components.utils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = [ "AddonManager", "AddonManagerPrivate" ];

// A list of providers to load by default
const PROVIDERS = [
  "resource://gre/modules/XPIProvider.jsm",
  "resource://gre/modules/LightweightThemeManager.jsm"
];

/**
 * Logs a debugging message.
 *
 * @param   str
 *          The string to log
 */
function LOG(str) {
  dump("*** addons.manager: " + str + "\n");
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
 * Calls a callback method consuming any thrown exception. Any parameters after
 * the callback parameter will be passed to the callback.
 *
 * @param   callback
 *          The callback method to call
 */
function safeCall(callback) {
  var args = Array.slice(arguments, 1);

  try {
    callback.apply(null, args);
  }
  catch (e) {
    WARN("Exception calling callback: " + e);
  }
}

/**
 * Calls a method on a provider if it exists and consumes any thrown exception.
 * Any parameters after the dflt parameter are passed to the provider's method.
 *
 * @param   provider
 *          The provider to call
 * @param   method
 *          The method name to call
 * @param   dflt
 *          A default return value if the provider does not implement the named
 *          method or throws an error.
 * @return  the return value from the provider or dflt if the provider does not
 *          implement method or throws an error
 */
function callProvider(provider, method, dflt) {
  if (!(method in provider))
    return dflt;

  var args = Array.slice(arguments, 3);

  try {
    return provider[method].apply(provider, args);
  }
  catch (e) {
    ERROR("Exception calling provider." + method + ": " + e);
    return dflt;
  }
}

/**
 * A helper class to repeatedly call a listener with each object in an array
 * optionally checking whether the object has a method in it.
 *
 * @param   objects
 *          The array of objects to iterate through
 * @param   method
 *          An optional method name, if not null any objects without this method
 *          will not be passed to the listener
 * @param   listener
 *          A listener implementing nextObject and noMoreObjects methods. The
 *          former will be called with the AsyncObjectCaller as the first
 *          parameter and the object as the second. noMoreObjects will be passed
 *          just the AsyncObjectCaller
 */
function AsyncObjectCaller(objects, method, listener) {
  this.objects = objects.slice(0);
  this.method = method;
  this.listener = listener;

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
 * This is the real manager, kept here rather than in AddonManager to keep its
 * contents hidden from API users.
 */
var AddonManagerInternal = {
  installListeners: null,
  addonListeners: null,
  providers: [],
  started: false,

  /**
   * Initializes the AddonManager, loading any known providers and initializing
   * them. 
   */
  startup: function AMI_startup() {
    if (this.started)
      return;

    this.installListeners = [];
    this.addonListeners = [];

    let appChanged = true;

    try {
      appChanged = Services.appinfo.version !=
                   Services.prefs.getCharPref("extensions.lastAppVersion");
    }
    catch (e) { }

    if (appChanged) {
      LOG("Application has been upgraded");
      Services.prefs.setCharPref("extensions.lastAppVersion",
                                 Services.appinfo.version);
    }

    // Ensure all default providers have had a chance to register themselves
    PROVIDERS.forEach(function(url) {
      Components.utils.import(url, {});
    });

    let needsRestart = false;
    this.providers.forEach(function(provider) {
      callProvider(provider, "startup");
      if (callProvider(provider, "checkForChanges", false, appChanged))
        needsRestart = true;
    });
    this.started = true;

    // Flag to the platform that a restart is necessary
    if (needsRestart) {
      let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].
                       getService(Ci.nsIAppStartup2);
      appStartup.needsRestart = needsRestart;
    }
  },

  /**
   * Registers a new AddonProvider.
   *
   * @param   provider
   *          The provider to register
   */
  registerProvider: function AMI_registerProvider(provider) {
    this.providers.push(provider);

    // If we're registering after startup call this provider's startup.
    if (this.started)
      callProvider(provider, "startup");
  },

  /**
   * Shuts down the addon manager and all registered providers, this must clean
   * up everything in order for automated tests to fake restarts.
   */
  shutdown: function AM_shutdown() {
    this.providers.forEach(function(provider) {
      callProvider(provider, "shutdown");
    });

    this.installListeners = null;
    this.addonListeners = null;
    this.started = false;
  },

  /**
   * Performs a background update check by starting an update for all add-ons
   * that can be updated.
   */
  backgroundUpdateCheck: function AMI_backgroundUpdateCheck() {
    if (!Services.prefs.getBoolPref(PREF_EM_UPDATE_ENABLED))
      return;

    this.getAddonsByTypes(null, function getAddonsCallback(addons) {
      addons.forEach(function BUC_forEachCallback(addon) {
        if (addon.permissions & AddonManager.PERM_CAN_UPGRADE) {
          addon.findUpdates({
            onUpdateAvailable: function BUC_onUpdateAvailable(addon, install) {
              install.install();
            }
          }, AddonManager.UPDATE_WHEN_PERIODIC_UPDATE);
        }
      });
    });
  },

  /**
   * Calls all registered InstallListeners with an event. Any parameters after
   * the extraListeners parameter are passed to the listener.
   *
   * @param   method
   *          The method on the listeners to call
   * @param   extraListeners
   *          An array of extra InstallListeners to also call
   * @return  false if any of the listeners returned false, true otherwise
   */
  callInstallListeners: function AMI_callInstallListeners(method, extraListeners) {
    let result = true;
    let listeners = this.installListeners;
    if (extraListeners)
      listeners = extraListeners.concat(listeners);
    let args = Array.slice(arguments, 2);

    listeners.forEach(function(listener) {
      try {
        if (method in listener) {
          if (listener[method].apply(listener, args) === false)
            result = false;
        }
      }
      catch (e) {
        WARN("InstallListener threw exception when calling " + method + ": " + e);
      }
    });
    return result;
  },

  /**
   * Calls all registered AddonListeners with an event. Any parameters after
   * the method parameter are passed to the listener.
   *
   * @param   method
   *          The method on the listeners to call
   */
  callAddonListeners: function AMI_callAddonListeners(method) {
    var args = Array.slice(arguments, 1);
    this.addonListeners.forEach(function(listener) {
      try {
        if (method in listener)
          listener[method].apply(listener, args);
      }
      catch (e) {
        WARN("AddonListener threw exception when calling " + method + ": " + e);
      }
    });
  },

  /**
   * Notifies all providers that an add-on has been enabled when that type of
   * add-on only supports a single add-on being enabled at a time. This allows
   * the providers to disable theirs if necessary.
   *
   * @param   id
   *          The id of the enabled add-on
   * @param   type
   *          The type of the enabled add-on
   * @param   pendingRestart
   *          A boolean indicating if the change will only take place the next
   *          time the application is restarted
   */
  notifyAddonChanged: function AMI_notifyAddonChanged(id, type, pendingRestart) {
    this.providers.forEach(function(provider) {
      callProvider(provider, "addonChanged", null, id, type, pendingRestart);
    });
  },

  /**
   * Asynchronously gets an AddonInstall for a URL.
   *
   * @param   url
   *          The url the add-on is located at
   * @param   callback
   *          A callback to pass the AddonInstall to
   * @param   mimetype
   *          The mimetype of the add-on
   * @param   hash
   *          An optional hash of the add-on
   * @param   name
   *          An optional placeholder name while the add-on is being downloaded
   * @param   iconURL
   *          An optional placeholder icon URL while the add-on is being downloaded
   * @param   version
   *          An optional placeholder version while the add-on is being downloaded
   * @param   loadgroup
   *          An optional nsILoadGroup to associate any network requests with
   * @throws  if the url, callback or mimetype arguments are not specified
   */
  getInstallForURL: function AMI_getInstallForURL(url, callback, mimetype, hash,
                                                  name, iconURL, version,
                                                  loadgroup) {
    if (!url || !mimetype || !callback)
      throw new TypeError("Invalid arguments");

    for (let i = 0; i < this.providers.length; i++) {
      if (callProvider(this.providers[i], "supportsMimetype", false, mimetype)) {
        callProvider(this.providers[i], "getInstallForURL", null,
                     url, hash, name, iconURL, version, loadgroup,
                     function(install) {
          safeCall(callback, install);
        });
        return;
      }
    }
    safeCall(callback, null);
  },

  /**
   * Asynchronously gets an AddonInstall for an nsIFile.
   *
   * @param   file
   *          the nsIFile where the add-on is located
   * @param   callback
   *          A callback to pass the AddonInstall to
   * @param   mimetype
   *          An optional mimetype hint for the add-on
   * @throws  if the file or callback arguments are not specified
   */
  getInstallForFile: function AMI_getInstallForFile(file, callback, mimetype) {
    if (!file || !callback)
      throw Cr.NS_ERROR_INVALID_ARG;

    new AsyncObjectCaller(this.providers, "getInstallForFile", {
      nextObject: function(caller, provider) {
        callProvider(provider, "getInstallForFile", null, file,
                     function(install) {
          if (install)
            safeCall(callback, install);
          else
            caller.callNext();
        });
      },

      noMoreObjects: function(caller) {
        safeCall(callback, null);
      }
    });
  },

  /**
   * Asynchronously gets all current AddonInstalls optionally limiting to a list
   * of types.
   *
   * @param   types
   *          An optional array of types to retrieve. Each type is a string name
   * @param   callback
   *          A callback which will be passed an array of AddonInstalls
   * @throws  if the callback argument is not specified
   */
  getInstalls: function AMI_getInstalls(types, callback) {
    if (!callback)
      throw Cr.NS_ERROR_INVALID_ARG;

    let installs = [];

    new AsyncObjectCaller(this.providers, "getInstalls", {
      nextObject: function(caller, provider) {
        callProvider(provider, "getInstalls", null, types,
                     function(providerInstalls) {
          installs = installs.concat(providerInstalls);
          caller.callNext();
        });
      },

      noMoreObjects: function(caller) {
        safeCall(callback, installs);
      }
    });
  },

  /**
   * Checks whether installation is enabled for a particular mimetype.
   *
   * @param   mimetype
   *          The mimetype to check
   * @return  true if installation is enabled for the mimetype
   */
  isInstallEnabled: function AMI_isInstallEnabled(mimetype) {
    for (let i = 0; i < this.providers.length; i++) {
      if (callProvider(this.providers[i], "supportsMimetype", false, mimetype) &&
          callProvider(this.providers[i], "isInstallEnabled"))
        return true;
    }
    return false;
  },

  /**
   * Checks whether a particular source is allowed to install add-ons of a
   * given mimetype.
   *
   * @param   mimetype
   *          The mimetype of the add-on
   * @param   uri
   *          The uri of the source, may be null
   * @return  true if the source is allowed to install this mimetype
   */
  isInstallAllowed: function AMI_isInstallAllowed(mimetype, uri) {
    for (let i = 0; i < this.providers.length; i++) {
      if (callProvider(this.providers[i], "supportsMimetype", false, mimetype) &&
          callProvider(this.providers[i], "isInstallAllowed", null, uri))
        return true;
    }
  },

  /**
   * Starts installation of an array of AddonInstalls notifying the registered
   * web install listener of blocked or started installs.
   *
   * @param   mimetype
   *          The mimetype of add-ons being installed
   * @param   source
   *          The nsIDOMWindowInternal that started the installs
   * @param   uri
   *          the nsIURI that started the installs
   * @param   installs
   *          The array of AddonInstalls to be installed
   */
  installAddonsFromWebpage: function AMI_installAddonsFromWebpage(mimetype,
                                                                  source,
                                                                  uri,
                                                                  installs) {
    if (!("@mozilla.org/addons/web-install-listener;1" in Cc)) {
      WARN("No web installer available, cancelling all installs");
      installs.forEach(function(install) {
        install.cancel();
      });
      return;
    }

    try {
      let weblistener = Cc["@mozilla.org/addons/web-install-listener;1"].
                        getService(Ci.amIWebInstallListener);

      if (!this.isInstallAllowed(mimetype, uri)) {
        if (weblistener.onWebInstallBlocked(source, uri, installs,
                                            installs.length)) {
          installs.forEach(function(install) {
            install.install();
          });
        }
      }
      else if (weblistener.onWebInstallRequested(source, uri, installs,
                                                 installs.length)) {
        installs.forEach(function(install) {
          install.install();
        });
      }
    }
    catch (e) {
      // In the event that the weblistener throws during instatiation or when
      // calling onWebInstallBlocked or onWebInstallRequested all of the
      // installs should get cancelled.
      WARN("Failure calling web installer: " + e);
      installs.forEach(function(install) {
        install.cancel();
      });
    }
  },

  /**
   * Adds a new InstallListener if the listener is not already registered.
   *
   * @param   listener
   *          The InstallListener to add
   */
  addInstallListener: function AMI_addInstallListener(listener) {
    if (!this.installListeners.some(function(i) { return i == listener; }))
      this.installListeners.push(listener);
  },

  /**
   * Removes an InstallListener if the listener is registered.
   *
   * @param   listener
   *          The InstallListener to remove
   */
  removeInstallListener: function AMI_removeInstallListener(listener) {
    this.installListeners = this.installListeners.filter(function(i) {
      return i != listener;
    });
  },

  /**
   * Asynchronously gets an add-on with a specific ID.
   *
   * @param   id
   *          The ID of the add-on to retrieve
   * @param   callback
   *          The callback to pass the retrieved add-on to
   * @throws  if the id or callback arguments are not specified
   */
  getAddon: function AMI_getAddon(id, callback) {
    if (!id || !callback)
      throw Cr.NS_ERROR_INVALID_ARG;

    new AsyncObjectCaller(this.providers, "getAddon", {
      nextObject: function(caller, provider) {
        callProvider(provider, "getAddon", null, id, function(addon) {
          if (addon)
            safeCall(callback, addon);
          else
            caller.callNext();
        });
      },

      noMoreObjects: function(caller) {
        safeCall(callback, null);
      }
    });
  },

  /**
   * Asynchronously gets an array of add-ons.
   *
   * @param   ids
   *          The array of IDs to retrieve
   * @param   callback
   *          The callback to pass an array of Addons to
   * @throws  if the id or callback arguments are not specified
   */
  getAddons: function AMI_getAddons(ids, callback) {
    if (!ids || !callback)
      throw Cr.NS_ERROR_INVALID_ARG;

    let addons = [];

    new AsyncObjectCaller(ids, null, {
      nextObject: function(caller, id) {
        AddonManagerInternal.getAddon(id, function(addon) {
          addons.push(addon);
          caller.callNext();
        });
      },

      noMoreObjects: function(caller) {
        safeCall(callback, addons);
      }
    });
  },

  /**
   * Asynchronously gets add-ons of specific types.
   *
   * @param   types
   *          An optional array of types to retrieve. Each type is a string name
   * @param   callback
   *          The callback to pass an array of Addons to.
   * @throws  if the callback argument is not specified
   */
  getAddonsByTypes: function AMI_getAddonsByTypes(types, callback) {
    if (!callback)
      throw Cr.NS_ERROR_INVALID_ARG;

    let addons = [];

    new AsyncObjectCaller(this.providers, "getAddonsByTypes", {
      nextObject: function(caller, provider) {
        callProvider(provider, "getAddonsByTypes", null, types,
                     function(providerAddons) {
          addons = addons.concat(providerAddons);
          caller.callNext();
        });
      },

      noMoreObjects: function(caller) {
        safeCall(callback, addons);
      }
    });
  },

  /**
   * Asynchronously gets add-ons that have operations waiting for an application
   * restart to complete.
   *
   * @param   types
   *          An optional array of types to retrieve. Each type is a string name
   * @param   callback
   *          The callback to pass the array of Addons to
   * @throws  if the callback argument is not specified
   */
  getAddonsWithPendingOperations:
  function AMI_getAddonsWithPendingOperations(types, callback) {
    if (!callback)
      throw Cr.NS_ERROR_INVALID_ARG;

    let addons = [];

    new AsyncObjectCaller(this.providers, "getAddonsWithPendingOperations", {
      nextObject: function(caller, provider) {
        callProvider(provider, "getAddonsWithPendingOperations", null, types,
                     function(providerAddons) {
          addons = addons.concat(providerAddons);
          caller.callNext();
        });
      },

      noMoreObjects: function(caller) {
        safeCall(callback, addons);
      }
    });
  },

  /**
   * Adds a new AddonListener if the listener is not already registered.
   *
   * @param   listener
   *          The listener to add
   */
  addAddonListener: function AMI_addAddonListener(listener) {
    if (!this.addonListeners.some(function(i) { return i == listener; }))
      this.addonListeners.push(listener);
  },

  /**
   * Removes an AddonListener if the listener is registered.
   *
   * @param   listener
   *          The listener to remove
   */
  removeAddonListener: function AMI_removeAddonListener(listener) {
    this.addonListeners = this.addonListeners.filter(function(i) {
      return i != listener;
    });
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

  registerProvider: function AMP_registerProvider(provider) {
    AddonManagerInternal.registerProvider(provider);
  },

  shutdown: function AMP_shutdown() {
    AddonManagerInternal.shutdown();
  },

  backgroundUpdateCheck: function AMP_backgroundUpdateCheck() {
    AddonManagerInternal.backgroundUpdateCheck();
  },

  notifyAddonChanged: function AMP_notifyAddonChanged(id, type, pendingRestart) {
    AddonManagerInternal.notifyAddonChanged(id, type, pendingRestart);
  },

  callInstallListeners: function AMP_callInstallListeners(method) {
    return AddonManagerInternal.callInstallListeners.apply(AddonManagerInternal,
                                                          arguments);
  },

  callAddonListeners: function AMP_callAddonListeners(method) {
    AddonManagerInternal.callAddonListeners.apply(AddonManagerInternal, arguments);
  }
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
  // Indicates that the Addon will be enabled after the application restarts.
  PENDING_ENABLE: 1,
  // Indicates that the Addon will be disabled after the application restarts.
  PENDING_DISABLE: 2,
  // Indicates that the Addon will be uninstalled after the application restarts.
  PENDING_UNINSTALL: 4,
  // Indicates that the Addon will be installed after the application restarts.
  PENDING_INSTALL: 8,

  // Constants for permissions in Addon.permissions.
  // Indicates that the Addon can be uninstalled.
  PERM_CAN_UNINSTALL: 1,
  // Indicates that the Addon can be enabled by the user.
  PERM_CAN_ENABLE: 2,
  // Indicates that the Addon can be disabled by the user.
  PERM_CAN_DISABLE: 4,
  // Indicates that the Addon can be upgraded.
  PERM_CAN_UPGRADE: 8,

  getInstallForURL: function AM_getInstallForURL(url, callback, mimetype, hash,
                                                 name, iconURL, version,
                                                 loadgroup) {
    AddonManagerInternal.getInstallForURL(url, callback, mimetype, hash, name,
                                         iconURL, version, loadgroup);
  },

  getInstallForFile: function AM_getInstallForFile(file, callback, mimetype) {
    AddonManagerInternal.getInstallForFile(file, callback, mimetype);
  },

  getAddon: function AM_getAddon(id, callback) {
    AddonManagerInternal.getAddon(id, callback);
  },

  getAddons: function AM_getAddons(ids, callback) {
    AddonManagerInternal.getAddons(ids, callback);
  },

  getAddonsWithPendingOperations:
  function AM_getAddonsWithPendingOperations(types, callback) {
    AddonManagerInternal.getAddonsWithPendingOperations(types, callback);
  },

  getAddonsByTypes: function AM_getAddonsByTypes(types, callback) {
    AddonManagerInternal.getAddonsByTypes(types, callback);
  },

  getInstalls: function AM_getInstalls(types, callback) {
    AddonManagerInternal.getInstalls(types, callback);
  },

  isInstallEnabled: function AM_isInstallEnabled(type) {
    return AddonManagerInternal.isInstallEnabled(type);
  },

  isInstallAllowed: function AM_isInstallAllowed(type, uri) {
    return AddonManagerInternal.isInstallAllowed(type, uri);
  },

  installAddonsFromWebpage: function AM_installAddonsFromWebpage(type, source,
                                                                 uri, installs) {
    AddonManagerInternal.installAddonsFromWebpage(type, source, uri, installs);
  },

  addInstallListener: function AM_addInstallListener(listener) {
    AddonManagerInternal.addInstallListener(listener);
  },

  removeInstallListener: function AM_removeInstallListener(listener) {
    AddonManagerInternal.removeInstallListener(listener);
  },

  addAddonListener: function AM_addAddonListener(listener) {
    AddonManagerInternal.addAddonListener(listener);
  },

  removeAddonListener: function AM_removeAddonListener(listener) {
    AddonManagerInternal.removeAddonListener(listener);
  }
};
