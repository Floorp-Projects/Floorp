/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

const RELATIVE_DIR = "browser/toolkit/mozapps/extensions/test/browser/";

const TESTROOT = "http://example.com/" + RELATIVE_DIR;
const TESTROOT2 = "http://example.org/" + RELATIVE_DIR;

const MANAGER_URI = "about:addons";
const INSTALL_URI = "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul";
const PREF_LOGGING_ENABLED = "extensions.logging.enabled";
const PREF_SEARCH_MAXRESULTS = "extensions.getAddons.maxResults";
const CHROME_NAME = "mochikit";

function getChromeRoot(path) {
  if (path === undefined) {
    return "chrome://" + CHROME_NAME + "/content/" + RELATIVE_DIR;
  }
  return getRootDirectory(path);
}

var gPendingTests = [];
var gTestsRun = 0;
var gTestStart = null;

var gUseInContentUI = ("switchToTabHavingURI" in window);

// Turn logging on for all tests
Services.prefs.setBoolPref(PREF_LOGGING_ENABLED, true);
// Turn off remote results in searches
Services.prefs.setIntPref(PREF_SEARCH_MAXRESULTS, 0);
registerCleanupFunction(function() {
  Services.prefs.clearUserPref(PREF_LOGGING_ENABLED);
  try {
    Services.prefs.clearUserPref(PREF_SEARCH_MAXRESULTS);
  }
  catch (e) {
  }
});

function log_exceptions(aCallback) {
  try {
    var args = Array.slice(arguments, 1);
    return aCallback.apply(null, args);
  }
  catch (e) {
    info("Exception thrown: " + e);
    throw e;
  }
}

function add_test(test) {
  gPendingTests.push(test);
}

function run_next_test() {
  if (gTestsRun > 0)
    info("Test " + gTestsRun + " took " + (Date.now() - gTestStart) + "ms");

  if (gPendingTests.length == 0) {
    end_test();
    return;
  }

  gTestsRun++;
  var test = gPendingTests.shift();
  if (test.name)
    info("Running test " + gTestsRun + " (" + test.name + ")");
  else
    info("Running test " + gTestsRun);

  gTestStart = Date.now();
  log_exceptions(test);
}

function get_addon_file_url(aFilename) {
  var chromeroot = getChromeRoot(gTestPath);
  try {
    var cr = Cc["@mozilla.org/chrome/chrome-registry;1"].
             getService(Ci.nsIChromeRegistry);
    var fileurl = cr.convertChromeURL(makeURI(chromeroot + "addons/" + aFilename));
    return fileurl.QueryInterface(Ci.nsIFileURL);
  } catch(ex) {
    var jar = getJar(chromeroot + "addons/" + aFilename);
    var tmpDir = extractJarToTmp(jar);
    tmpDir.append(aFilename);

    var ios = Components.classes["@mozilla.org/network/io-service;1"].
                getService(Components.interfaces.nsIIOService);
    return ios.newFileURI(tmpDir).QueryInterface(Ci.nsIFileURL);
  }
}

function check_all_in_list(aManager, aIds, aIgnoreExtras) {
  var doc = aManager.document;
  var view = doc.getElementById("view-port").selectedPanel;
  var listid = view.id == "search-view" ? "search-list" : "addon-list";
  var list = doc.getElementById(listid);

  var inlist = [];
  var node = list.firstChild;
  while (node) {
    if (node.value)
      inlist.push(node.value);
    node = node.nextSibling;
  }

  for (var i = 0; i < aIds.length; i++) {
    if (inlist.indexOf(aIds[i]) == -1)
      ok(false, "Should find " + aIds[i] + " in the list");
  }

  if (aIgnoreExtras)
    return;

  for (i = 0; i < inlist.length; i++) {
    if (aIds.indexOf(inlist[i]) == -1)
      ok(false, "Shouldn't have seen " + inlist[i] + " in the list");
  }
}

function get_addon_element(aManager, aId) {
  var doc = aManager.document;
  var view = doc.getElementById("view-port").selectedPanel;
  var listid = "addon-list";
  if (view.id == "search-view")
    listid = "search-list";
  else if (view.id == "updates-view")
    listid = "updates-list";
  var list = doc.getElementById(listid);

  var node = list.firstChild;
  while (node) {
    if (node.value == aId)
      return node;
    node = node.nextSibling;
  }
  return null;
}

function wait_for_view_load(aManagerWindow, aCallback, aForceWait) {
  if (!aForceWait && !aManagerWindow.gViewController.isLoading) {
    log_exceptions(aCallback, aManagerWindow);
    return;
  }

  aManagerWindow.document.addEventListener("ViewChanged", function() {
    aManagerWindow.document.removeEventListener("ViewChanged", arguments.callee, false);
    log_exceptions(aCallback, aManagerWindow);
  }, false);
}

function wait_for_manager_load(aManagerWindow, aCallback) {
  if (!aManagerWindow.gIsInitializing) {
    log_exceptions(aCallback, aManagerWindow);
    return;
  }

  info("Waiting for initialization");
  aManagerWindow.document.addEventListener("Initialized", function() {
    aManagerWindow.document.removeEventListener("Initialized", arguments.callee, false);
    log_exceptions(aCallback, aManagerWindow);
  }, false);
}

function open_manager(aView, aCallback, aLoadCallback) {
  function setup_manager(aManagerWindow) {
    if (aLoadCallback)
      log_exceptions(aLoadCallback, aManagerWindow);

    if (aView)
      aManagerWindow.loadView(aView);

    ok(aManagerWindow != null, "Should have an add-ons manager window");
    is(aManagerWindow.location, MANAGER_URI, "Should be displaying the correct UI");

    wait_for_manager_load(aManagerWindow, function() {
      wait_for_view_load(aManagerWindow, aCallback);
    });
  }

  if (gUseInContentUI) {
    gBrowser.selectedTab = gBrowser.addTab();
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
    log_exceptions(aCallback);
  }, false);

  aManagerWindow.close();
}

function restart_manager(aManagerWindow, aView, aCallback, aLoadCallback) {
  if (!aManagerWindow) {
    open_manager(aView, aCallback, aLoadCallback);
    return;
  }

  close_manager(aManagerWindow, function() {
    open_manager(aView, aCallback, aLoadCallback);
  });
}

function is_hidden(aElement) {
  var style = aElement.ownerDocument.defaultView.getComputedStyle(aElement, "");
  if (style.display == "none")
    return true;
  if (style.visibility != "visible")
    return true;

  // Hiding a parent element will hide all its children
  if (aElement.parentNode != aElement.ownerDocument)
    return is_hidden(aElement.parentNode);

  return false;
}

function is_element_visible(aElement, aMsg) {
  isnot(aElement, null, "Element should not be null, when checking visibility");
  ok(!is_hidden(aElement), aMsg);
}

function is_element_hidden(aElement, aMsg) {
  isnot(aElement, null, "Element should not be null, when checking visibility");
  ok(is_hidden(aElement), aMsg);
}

function CategoryUtilities(aManagerWindow) {
  this.window = aManagerWindow;

  var self = this;
  this.window.addEventListener("unload", function() {
    self.window.removeEventListener("unload", arguments.callee, false);
    self.window = null;
  }, false);
}

CategoryUtilities.prototype = {
  window: null,

  get selectedCategory() {
    isnot(this.window, null, "Should not get selected category when manager window is not loaded");
    var selectedItem = this.window.document.getElementById("categories").selectedItem;
    isnot(selectedItem, null, "A category should be selected");
    var view = this.window.gViewController.parseViewId(selectedItem.value);
    return (view.type == "list") ? view.param : view.type;
  },

  get: function(aCategoryType) {
    isnot(this.window, null, "Should not get category when manager window is not loaded");
    var categories = this.window.document.getElementById("categories");

    var viewId = "addons://list/" + aCategoryType;
    var items = categories.getElementsByAttribute("value", viewId);
    if (items.length)
      return items[0];

    viewId = "addons://" + aCategoryType + "/";
    items = categories.getElementsByAttribute("value", viewId);
    if (items.length)
      return items[0];

    ok(false, "Should have found a category with type " + aCategoryType);
    return null;
  },

  getViewId: function(aCategoryType) {
    isnot(this.window, null, "Should not get view id when manager window is not loaded");
    return this.get(aCategoryType).value;
  },

  isVisible: function(aCategory) {
    isnot(this.window, null, "Should not check visible state when manager window is not loaded");
    if (aCategory.hasAttribute("disabled") &&
        aCategory.getAttribute("disabled") == "true")
      return false;

    return !is_hidden(aCategory);
  },

  isTypeVisible: function(aCategoryType) {
    return this.isVisible(this.get(aCategoryType));
  },

  open: function(aCategory, aCallback) {
    isnot(this.window, null, "Should not open category when manager window is not loaded");
    ok(this.isVisible(aCategory), "Category should be visible if attempting to open it");

    EventUtils.synthesizeMouse(aCategory, 2, 2, { }, this.window);

    if (aCallback)
      wait_for_view_load(this.window, aCallback);
  },

  openType: function(aCategoryType, aCallback) {
    this.open(this.get(aCategoryType), aCallback);
  }
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
    aAddon._provider = this;

    if (!this.started)
      return;

    let requiresRestart = (aAddon.operationsRequiringRestart &
                           AddonManager.OP_NEEDS_RESTART_INSTALL) != 0;
    AddonManagerPrivate.callInstallListeners("onExternalInstall", null, aAddon,
                                             null, requiresRestart)
  },

  /**
   * Removes an add-on from the list of add-ons that this provider exposes to
   * the AddonManager, dispatching the onUninstalled event in the process.
   *
   * @param  aAddon
   *         The add-on to add
   */
  removeAddon: function MP_removeAddon(aAddon) {
    var pos = this.addons.indexOf(aAddon);
    if (pos == -1) {
      ok(false, "Tried to remove an add-on that wasn't registered with the mock provider");
      return;
    }

    this.addons.splice(pos, 1);

    if (!this.started)
      return;

    AddonManagerPrivate.callAddonListeners("onUninstalled", aAddon);
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
        if (prop == "applyBackgroundUpdates") {
          addon._applyBackgroundUpdates = aAddonProp[prop];
          continue;
        }
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
        if (prop == "sourceURI") {
          install[prop] = NetUtil.newURI(aInstallProp[prop]);
          continue;
        }

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

    aCallback(null);
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

function MockAddon(aId, aName, aType, aOperationsRequiringRestart) {
  // Only set required attributes.
  this.id = aId || "";
  this.name = aName || "";
  this.type = aType || "extension";
  this.version = "";
  this.isCompatible = true;
  this.providesUpdatesSecurely = true;
  this.blocklistState = 0;
  this.appDisabled = false;
  this._userDisabled = false;
  this._applyBackgroundUpdates = AddonManager.AUTOUPDATE_ENABLE;
  this.scope = AddonManager.SCOPE_PROFILE;
  this.isActive = true;
  this.creator = "";
  this.pendingOperations = 0;
  this._permissions = AddonManager.PERM_CAN_UNINSTALL |
                      AddonManager.PERM_CAN_ENABLE |
                      AddonManager.PERM_CAN_DISABLE |
                      AddonManager.PERM_CAN_UPGRADE;
  this.operationsRequiringRestart = aOperationsRequiringRestart ||
    (AddonManager.OP_NEEDS_RESTART_INSTALL |
     AddonManager.OP_NEEDS_RESTART_UNINSTALL |
     AddonManager.OP_NEEDS_RESTART_ENABLE |
     AddonManager.OP_NEEDS_RESTART_DISABLE);
}

MockAddon.prototype = {
  get shouldBeActive() {
    return !this.appDisabled && !this._userDisabled;
  },

  get userDisabled() {
    return this._userDisabled;
  },

  set userDisabled(val) {
    if (val == this._userDisabled)
      return val;

    var currentActive = this.shouldBeActive;
    this._userDisabled = val;
    var newActive = this.shouldBeActive;
    this._updateActiveState(currentActive, newActive);

    return val;
  },

  get permissions() {
    let permissions = this._permissions;
    if (this.appDisabled || !this._userDisabled)
      permissions &= ~AddonManager.PERM_CAN_ENABLE;
    if (this.appDisabled || this._userDisabled)
      permissions &= ~AddonManager.PERM_CAN_DISABLE;
    return permissions;
  },

  set permissions(val) {
    return this._permissions = val;
  },

  get applyBackgroundUpdates() {
    return this._applyBackgroundUpdates;
  },
  
  set applyBackgroundUpdates(val) {
    if (val != AddonManager.AUTOUPDATE_DEFAULT &&
        val != AddonManager.AUTOUPDATE_DISABLE &&
        val != AddonManager.AUTOUPDATE_ENABLE) {
      ok(false, "addon.applyBackgroundUpdates set to an invalid value: " + val);
    }
    this._applyBackgroundUpdates = val;
    AddonManagerPrivate.callAddonListeners("onPropertyChanged", this, ["applyBackgroundUpdates"]);
  },

  isCompatibleWith: function(aAppVersion, aPlatformVersion) {
    return true;
  },

  findUpdates: function(aListener, aReason, aAppVersion, aPlatformVersion) {
    // Tests can implement this if they need to
  },

  uninstall: function() {
    if (this.pendingOperations & AddonManager.PENDING_UNINSTALL)
      throw new Error("Add-on is already pending uninstall");

    var needsRestart = !!(this.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_UNINSTALL);
    this.pendingOperations |= AddonManager.PENDING_UNINSTALL;
    AddonManagerPrivate.callAddonListeners("onUninstalling", this, needsRestart);
    if (!needsRestart) {
      this.pendingOperations -= AddonManager.PENDING_UNINSTALL;
      this._provider.removeAddon(this);
    }
  },

  cancelUninstall: function() {
    if (!(this.pendingOperations & AddonManager.PENDING_UNINSTALL))
      throw new Error("Add-on is not pending uninstall");

    this.pendingOperations -= AddonManager.PENDING_UNINSTALL;
    AddonManagerPrivate.callAddonListeners("onOperationCancelled", this);
  },

  _updateActiveState: function(currentActive, newActive) {
    if (currentActive == newActive)
      return;

    if (newActive == this.isActive) {
      this.pendingOperations -= (newActive ? AddonManager.PENDING_DISABLE : AddonManager.PENDING_ENABLE);
      AddonManagerPrivate.callAddonListeners("onOperationCancelled", this);
    }
    else if (newActive) {
      var needsRestart = !!(this.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_ENABLE);
      this.pendingOperations |= AddonManager.PENDING_ENABLE;
      AddonManagerPrivate.callAddonListeners("onEnabling", this, needsRestart);
      if (!needsRestart) {
        this.isActive = newActive;
        this.pendingOperations -= AddonManager.PENDING_ENABLE;
        AddonManagerPrivate.callAddonListeners("onEnabled", this);
      }
    }
    else {
      var needsRestart = !!(this.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_DISABLE);
      this.pendingOperations |= AddonManager.PENDING_DISABLE;
      AddonManagerPrivate.callAddonListeners("onDisabling", this, needsRestart);
      if (!needsRestart) {
        this.isActive = newActive;
        this.pendingOperations -= AddonManager.PENDING_DISABLE;
        AddonManagerPrivate.callAddonListeners("onDisabled", this);
      }
    }
  }
};

/***** Mock AddonInstall object for the Mock Provider *****/

function MockInstall(aName, aType, aAddonToInstall) {
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
  this._addonToInstall = aAddonToInstall;
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
        if (this._addonToInstall)
          this.addon = this._addonToInstall;
        else {
          this.addon = new MockAddon("", this.name, this.type);
          this.addon.pendingOperations = AddonManager.PENDING_INSTALL;
        }

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

