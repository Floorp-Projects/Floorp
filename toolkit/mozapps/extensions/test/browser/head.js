/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const RELATIVE_DIR = "browser/toolkit/mozapps/extensions/test/browser/";

const TESTROOT = "http://example.com/" + RELATIVE_DIR;
const TESTROOT2 = "http://example.org/" + RELATIVE_DIR;
const CHROMEROOT = "chrome://mochikit/content/" + RELATIVE_DIR;

const MANAGER_URI = "about:addons";
const INSTALL_URI = "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul";
const PREF_LOGGING_ENABLED = "extensions.logging.enabled";

var gPendingTests = [];
var gTestsRun = 0;

// Turn logging on for all tests
Services.prefs.setBoolPref(PREF_LOGGING_ENABLED, true);
registerCleanupFunction(function() {
  Services.prefs.clearUserPref(PREF_LOGGING_ENABLED);
});

function add_test(test) {
  gPendingTests.push(test);
}

function run_next_test() {
  if (gPendingTests.length == 0) {
    end_test();
    return;
  }

  gTestsRun++;
  info("Running test " + gTestsRun);

  gPendingTests.shift()();
}

function get_addon_file_url(aFilename) {
  var cr = Cc["@mozilla.org/chrome/chrome-registry;1"].
           getService(Ci.nsIChromeRegistry);
  var fileurl = cr.convertChromeURL(makeURI(CHROMEROOT + "addons/" + aFilename));
  return fileurl.QueryInterface(Ci.nsIFileURL);
}

function wait_for_view_load(aManagerWindow, aCallback) {
  if (!aManagerWindow.gViewController.isLoading) {
    aCallback(aManagerWindow);
    return;
  }

  aManagerWindow.document.addEventListener("ViewChanged", function() {
    aManagerWindow.document.removeEventListener("ViewChanged", arguments.callee, false);
    aCallback(aManagerWindow);
  }, false);
}

function open_manager(aView, aCallback) {
  function setup_manager(aManagerWindow) {
    if (aView)
      aManagerWindow.loadView(aView);

    ok(aManagerWindow != null, "Should have an add-ons manager window");
    is(aManagerWindow.location, MANAGER_URI, "Should be displaying the correct UI");

    if (!aManagerWindow.gIsInitializing) {
      wait_for_view_load(aManagerWindow, aCallback);
      return;
    }

    aManagerWindow.document.addEventListener("Initialized", function() {
      aManagerWindow.document.removeEventListener("Initialized", arguments.callee, false);
      wait_for_view_load(aManagerWindow, aCallback);
    }, false);
  }

  if ("switchToTabHavingURI" in window) {
    switchToTabHavingURI(MANAGER_URI, true, function(aBrowser) {
      setup_manager(aBrowser.contentWindow.wrappedJSObject);
    });
    return;
  }

  openDialog(MANAGER_URI).addEventListener("load", function() {
    this.removeEventListener("load", arguments.callee, false);
    setup_manager(this);
  }, false);
}

function close_manager(aManagerWindow, aCallback) {
  ok(aManagerWindow != null, "Should have an add-ons manager window to close");
  is(aManagerWindow.location, MANAGER_URI, "Should be closing window with correct URI");

  aManagerWindow.addEventListener("unload", function() {
    this.removeEventListener("unload", arguments.callee, false);
    aCallback();
  }, false);

  aManagerWindow.close();
}

function restart_manager(aManagerWindow, aView, aCallback) {
  close_manager(aManagerWindow, function() { open_manager(aView, aCallback); });
}

function CertOverrideListener(host, bits) {
  this.host = host;
  this.bits = bits;
}

CertOverrideListener.prototype = {
  host: null,
  bits: null,

  getInterface: function (aIID) {
    return this.QueryInterface(aIID);
  },

  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIBadCertListener2) ||
        aIID.equals(Ci.nsIInterfaceRequestor) ||
        aIID.equals(Ci.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  notifyCertProblem: function (socketInfo, sslStatus, targetHost) {
    cert = sslStatus.QueryInterface(Components.interfaces.nsISSLStatus)
                    .serverCert;
    var cos = Cc["@mozilla.org/security/certoverride;1"].
              getService(Ci.nsICertOverrideService);
    cos.rememberValidityOverride(this.host, -1, cert, this.bits, false);
    return true;
  }
}

// Add overrides for the bad certificates
function addCertOverride(host, bits) {
  var req = new XMLHttpRequest();
  try {
    req.open("GET", "https://" + host + "/", false);
    req.channel.notificationCallbacks = new CertOverrideListener(host, bits);
    req.send(null);
  }
  catch (e) {
    // This request will fail since the SSL server is not trusted yet
  }
}

/***** Mock Provider *****/

function MockProvider(aUseAsyncCallbacks) {
  this.addons = [];
  this.installs = [];
  this.callbackTimers = [];
  this.useAsyncCallbacks = (aUseAsyncCallbacks === undefined) ? true : aUseAsyncCallbacks;

  var self = this;
  registerCleanupFunction(function() {
    if (self.started)
      self.unregister();
  });

  this.register();
}

MockProvider.prototype = {
  addons: null,
  installs: null,
  started: null,
  apiDelay: 100,
  callbackTimers: null,
  useAsyncCallbacks: null,

  /***** Utility functions *****/

  /**
   * Register this provider with the AddonManager
   */
  register: function MP_register() {
    AddonManagerPrivate.registerProvider(this);
  },

  /**
   * Unregister this provider with the AddonManager
   */
  unregister: function MP_unregister() {
    AddonManagerPrivate.unregisterProvider(this);
  },

  /**
   * Adds an add-on to the list of add-ons that this provider exposes to the
   * AddonManager, dispatching appropriate events in the process.
   *
   * @param  aAddon
   *         The add-on to add
   */
  addAddon: function MP_addAddon(aAddon) {
    this.addons.push(aAddon);

    if (!this.started)
      return;

    AddonManagerPrivate.callInstallListeners("onExternalInstall", null, aAddon,
                                             null, false)
  },

  /**
   * Adds an add-on install to the list of installs that this provider exposes
   * to the AddonManager, dispatching appropriate events in the process.
   *
   * @param  aInstall
   *         The add-on install to add
   */
  addInstall: function MP_addInstall(aInstall) {
    this.installs.push(aInstall);

    if (!this.started)
      return;

    aInstall.callListeners("onNewInstall");
  },

  /**
   * Creates a set of mock add-on objects and adds them to the list of add-ons
   * managed by this provider.
   *
   * @param  aAddonProperties
   *         An array of objects containing properties describing the add-ons
   * @return Array of the new MockAddons
   */
  createAddons: function MP_createAddons(aAddonProperties) {
    var newAddons = [];
    aAddonProperties.forEach(function(aAddonProp) {
      var addon = new MockAddon(aAddonProp.id);
      for (var prop in aAddonProp) {
        if (prop == "id")
          continue;
        addon[prop] = aAddonProp[prop];
      }
      this.addAddon(addon);
      newAddons.push(addon);
    }, this);

    return newAddons;
  },

  /**
   * Creates a set of mock add-on install objects and adds them to the list
   * of installs managed by this provider.
   *
   * @param  aInstallProperties
   *         An array of objects containing properties describing the installs
   * @return Array of the new MockInstalls
   */
  createInstalls: function MP_createInstalls(aInstallProperties) {
    var newInstalls = [];
    aInstallProperties.forEach(function(aInstallProp) {
      var install = new MockInstall();
      for (var prop in aInstallProp) {
        install[prop] = aInstallProp[prop];
      }
      this.addInstall(install);
      newInstalls.push(install);
    }, this);

    return newInstalls;
  },

  /***** AddonProvider implementation *****/

  /**
   * Called to initialize the provider.
   */
  startup: function MP_startup() {
    this.started = true;
  },

  /**
   * Called when the provider should shutdown.
   */
  shutdown: function MP_shutdown() {
    this.callbackTimers.forEach(function(aTimer) {
      aTimer.cancel();
    });
    this.callbackTimers = [];

    this.started = false;
  },

  /**
   * Called to get an Addon with a particular ID.
   *
   * @param  aId
   *         The ID of the add-on to retrieve
   * @param  aCallback
   *         A callback to pass the Addon to
   */
  getAddonByID: function MP_getAddon(aId, aCallback) {
    for (let i = 0; i < this.addons.length; i++) {
      if (this.addons[i].id == aId) {
        this._delayCallback(aCallback, this.addons[i]);
        return;
      }
    }
  },

  /**
   * Called to get Addons of a particular type.
   *
   * @param  aTypes
   *         An array of types to fetch. Can be null to get all types.
   * @param  callback
   *         A callback to pass an array of Addons to
   */
  getAddonsByTypes: function MP_getAddonsByTypes(aTypes, aCallback) {
    var addons = this.addons.filter(function(aAddon) {
      if (aTypes && aTypes.length > 0 && aTypes.indexOf(aAddon.type) == -1)
        return false;
      return true;
    });
    this._delayCallback(aCallback, addons);
  },

  /**
   * Called to get Addons that have pending operations.
   *
   * @param  aTypes
   *         An array of types to fetch. Can be null to get all types
   * @param  aCallback
   *         A callback to pass an array of Addons to
   */
  getAddonsWithOperationsByTypes: function MP_getAddonsWithOperationsByTypes(aTypes, aCallback) {
    var addons = this.addons.filter(function(aAddon) {
      if (aTypes && aTypes.length > 0 && aTypes.indexOf(aAddon.type) == -1)
        return false;
      return aAddon.pendingOperations != 0;
    });
    this._delayCallback(aCallback, addons);
  },

  /**
   * Called to get the current AddonInstalls, optionally restricting by type.
   *
   * @param  aTypes
   *         An array of types or null to get all types
   * @param  aCallback
   *         A callback to pass the array of AddonInstalls to
   */
  getInstallsByTypes: function MP_getInstallsByTypes(aTypes, aCallback) {
    var installs = this.installs.filter(function(aInstall) {
      // Appear to have actually removed cancelled installs from the provider
      if (aInstall.state == AddonManager.STATE_CANCELLED)
        return false;

      if (aTypes && aTypes.length > 0 && aTypes.indexOf(aInstall.type) == -1)
        return false;

      return true;
    });
    this._delayCallback(aCallback, installs);
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
  addonChanged: function MP_addonChanged(aId, aType, aPendingRestart) {
    // Not implemented
  },

  /**
   * Update the appDisabled property for all add-ons.
   */
  updateAddonAppDisabledStates: function MP_updateAddonAppDisabledStates() {
    // Not needed
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
  getInstallForURL: function MP_getInstallForURL(aUrl, aHash, aName, aIconURL,
                                                  aVersion, aLoadGroup, aCallback) {
    // Not yet implemented
  },

  /**
   * Called to get an AddonInstall to install an add-on from a local file.
   *
   * @param  aFile
   *         The file to be installed
   * @param  aCallback
   *         A callback to pass the AddonInstall to
   */
  getInstallForFile: function MP_getInstallForFile(aFile, aCallback) {
    // Not yet implemented
  },

  /**
   * Called to test whether installing add-ons is enabled.
   *
   * @return true if installing is enabled
   */
  isInstallEnabled: function MP_isInstallEnabled() {
    return false;
  },

  /**
   * Called to test whether this provider supports installing a particular
   * mimetype.
   *
   * @param  aMimetype
   *         The mimetype to check for
   * @return true if the mimetype is supported
   */
  supportsMimetype: function MP_supportsMimetype(aMimetype) {
    return false;
  },

  /**
   * Called to test whether installing add-ons from a URI is allowed.
   *
   * @param  aUri
   *         The URI being installed from
   * @return true if installing is allowed
   */
  isInstallAllowed: function MP_isInstallAllowed(aUri) {
    return false;
  },


  /***** Internal functions *****/

  /**
   * Delay calling a callback to fake a time-consuming async operation.
   * The delay is specified by the apiDelay property, in milliseconds.
   * Parameters to send to the callback should be specified as arguments after
   * the aCallback argument.
   *
   * @param aCallback Callback to eventually call
   */
  _delayCallback: function MP_delayCallback(aCallback) {
    var params = Array.splice(arguments, 1);

    if (!this.useAsyncCallbacks) {
      aCallback.apply(null, params);
      return;
    }

    var timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    // Need to keep a reference to the timer, so it doesn't get GC'ed
    var pos = this.callbackTimers.length;
    this.callbackTimers.push(timer);
    var self = this;
    timer.initWithCallback(function() {
      self.callbackTimers.splice(pos, 1);
      aCallback.apply(null, params);
    }, this.apiDelay, timer.TYPE_ONE_SHOT);
  }
};

/***** Mock Addon object for the Mock Provider *****/

function MockAddon(aId, aName, aType, aRestartless) {
  // Only set required attributes
  this.id = aId || "";
  this.name = aName || "";
  this.type = aType || "extension";
  this.version = "";
  this.isCompatible = true;
  this.providesUpdatesSecurely = true;
  this.blocklistState = 0;
  this.appDisabled = false;
  this.userDisabled = false;
  this.scope = AddonManager.SCOPE_PROFILE;
  this.isActive = true;
  this.creator = "";
  this.pendingOperations = 0;
  this.permissions = 0;

  this._restartless = aRestartless || false;
}

MockAddon.prototype = {
  isCompatibleWith: function(aAppVersion, aPlatformVersion) {
    return true;
  },

  findUpdates: function(aListener, aReason, aAppVersion, aPlatformVersion) {
    // Tests can implement this if they need to
  },

  uninstall: function() {
    // To be implemented when needed
  },

  cancelUninstall: function() {
    // To be implemented when needed
  }
};

/***** Mock AddonInstall object for the Mock Provider *****/

function MockInstall(aName, aType) {
  this.name = aName || "";
  this.type = aType || "extension";
  this.version = "1.0";
  this.iconURL = "";
  this.infoURL = "";
  this.state = AddonManager.STATE_AVAILABLE;
  this.error = 0;
  this.sourceURI = null;
  this.file = null;
  this.progress = 0;
  this.maxProgress = -1;
  this.certificate = null;
  this.certName = "";
  this.existingAddon = null;
  this.addon = null;
  this.listeners = [];

  // Another type of install listener for tests that want to check the results
  // of code run from standard install listeners
  this.testListeners = [];
}

MockInstall.prototype = {
  install: function() {
    switch (this.state) {
      case AddonManager.STATE_AVAILABLE:
      case AddonManager.STATE_DOWNLOADED:
        // Downloading to be implemented when needed

        this.state = AddonManager.STATE_INSTALLING;
        if (!this.callListeners("onInstallStarted")) {
          // Reverting to STATE_DOWNLOADED instead to be implemented when needed
          this.state = AddonManager.STATE_CANCELLED;
          this.callListeners("onInstallCancelled");
          return;
        }

        // Adding addon to MockProvider to be implemented when needed
        this.addon = new MockAddon("", this.name, this.type);
        this.state = AddonManager.STATE_INSTALLED;
        this.callListeners("onInstallEnded");
        break;
      case AddonManager.STATE_DOWNLOADING:
      case AddonManager.STATE_CHECKING:
      case AddonManger.STATE_INSTALLING:
        // Installation is already running
        return;
      default:
        ok(false, "Cannot start installing when state = " + this.state);
    }
  },

  cancel: function() {
    switch (this.state) {
      case AddonManager.STATE_AVAILABLE:
        this.state = AddonManager.STATE_CANCELLED;
        break;
      case AddonManager.STATE_INSTALLED:
        this.state = AddonManager.STATE_CANCELLED;
        this.callListeners("onInstallCancelled");
        break;
      default:
        // Handling cancelling when downloading to be implemented when needed
        ok(false, "Cannot cancel when state = " + this.state);
    }
  },


  addListener: function(aListener) {
    if (!this.listeners.some(function(i) i == aListener))
      this.listeners.push(aListener);
  },

  removeListener: function(aListener) {
    this.listeners = this.listeners.filter(function(i) i != aListener);
  },

  addTestListener: function(aListener) {
    if (!this.testListeners.some(function(i) i == aListener))
      this.testListeners.push(aListener);
  },

  removeTestListener: function(aListener) {
    this.testListeners = this.testListeners.filter(function(i) i != aListener);
  },

  callListeners: function(aMethod) {
    var result = AddonManagerPrivate.callInstallListeners(aMethod, this.listeners,
                                                          this, this.addon);

    // Call test listeners after standard listeners to remove race condition
    // between standard and test listeners
    this.testListeners.forEach(function(aListener) {
      if (aMethod in aListener)
        if (aListener[aMethod].call(aListener, this, this.addon) === false)
          result = false;
    });

    return result;
  }
};

