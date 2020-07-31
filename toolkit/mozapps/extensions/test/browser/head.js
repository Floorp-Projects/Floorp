/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
/* globals end_test */

/* eslint no-unused-vars: ["error", {vars: "local", args: "none"}] */

var { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

var tmp = {};
ChromeUtils.import("resource://gre/modules/AddonManager.jsm", tmp);
ChromeUtils.import("resource://gre/modules/Log.jsm", tmp);
var AddonManager = tmp.AddonManager;
var AddonManagerPrivate = tmp.AddonManagerPrivate;
var Log = tmp.Log;

var pathParts = gTestPath.split("/");
// Drop the test filename
pathParts.splice(pathParts.length - 1, pathParts.length);

const RELATIVE_DIR = pathParts.slice(4).join("/") + "/";

const TESTROOT = "http://example.com/" + RELATIVE_DIR;
const SECURE_TESTROOT = "https://example.com/" + RELATIVE_DIR;
const TESTROOT2 = "http://example.org/" + RELATIVE_DIR;
const SECURE_TESTROOT2 = "https://example.org/" + RELATIVE_DIR;
const CHROMEROOT = pathParts.join("/") + "/";
const PREF_DISCOVER_ENABLED = "extensions.getAddons.showPane";
const PREF_XPI_ENABLED = "xpinstall.enabled";
const PREF_UPDATEURL = "extensions.update.url";
const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";
const PREF_UI_LASTCATEGORY = "extensions.ui.lastCategory";

const MANAGER_URI = "about:addons";
const PREF_LOGGING_ENABLED = "extensions.logging.enabled";
const PREF_STRICT_COMPAT = "extensions.strictCompatibility";

var PREF_CHECK_COMPATIBILITY;
(function() {
  var channel = Services.prefs.getCharPref("app.update.channel", "default");
  if (
    channel != "aurora" &&
    channel != "beta" &&
    channel != "release" &&
    channel != "esr"
  ) {
    var version = "nightly";
  } else {
    version = Services.appinfo.version.replace(
      /^([^\.]+\.[0-9]+[a-z]*).*/gi,
      "$1"
    );
  }
  PREF_CHECK_COMPATIBILITY = "extensions.checkCompatibility." + version;
})();

var gPendingTests = [];
var gTestsRun = 0;
var gTestStart = null;

var gRestorePrefs = [
  { name: PREF_LOGGING_ENABLED },
  { name: "extensions.webservice.discoverURL" },
  { name: "extensions.update.url" },
  { name: "extensions.update.background.url" },
  { name: "extensions.update.enabled" },
  { name: "extensions.update.autoUpdateDefault" },
  { name: "extensions.getAddons.get.url" },
  { name: "extensions.getAddons.getWithPerformance.url" },
  { name: "extensions.getAddons.cache.enabled" },
  { name: "devtools.chrome.enabled" },
  { name: PREF_STRICT_COMPAT },
  { name: PREF_CHECK_COMPATIBILITY },
];

for (let pref of gRestorePrefs) {
  if (!Services.prefs.prefHasUserValue(pref.name)) {
    pref.type = "clear";
    continue;
  }
  pref.type = Services.prefs.getPrefType(pref.name);
  if (pref.type == Services.prefs.PREF_BOOL) {
    pref.value = Services.prefs.getBoolPref(pref.name);
  } else if (pref.type == Services.prefs.PREF_INT) {
    pref.value = Services.prefs.getIntPref(pref.name);
  } else if (pref.type == Services.prefs.PREF_STRING) {
    pref.value = Services.prefs.getCharPref(pref.name);
  }
}

// Turn logging on for all tests
Services.prefs.setBoolPref(PREF_LOGGING_ENABLED, true);

function promiseFocus(window) {
  return new Promise(resolve => waitForFocus(resolve, window));
}

// Helper to register test failures and close windows if any are left open
function checkOpenWindows(aWindowID) {
  let found = false;
  for (let win of Services.wm.getEnumerator(aWindowID)) {
    if (!win.closed) {
      found = true;
      win.close();
    }
  }
  if (found) {
    ok(false, "Found unexpected " + aWindowID + " window still open");
  }
}

// Tools to disable and re-enable the background update and blocklist timers
// so that tests can protect themselves from unwanted timer events.
var gCatMan = Services.catMan;
// Default value from toolkit/mozapps/extensions/extensions.manifest, but disable*UpdateTimer()
// records the actual value so we can put it back in enable*UpdateTimer()
var backgroundUpdateConfig =
  "@mozilla.org/addons/integration;1,getService,addon-background-update-timer,extensions.update.interval,86400";

var UTIMER = "update-timer";
var AMANAGER = "addonManager";
var BLOCKLIST = "nsBlocklistService";

function disableBackgroundUpdateTimer() {
  info("Disabling " + UTIMER + " " + AMANAGER);
  backgroundUpdateConfig = gCatMan.getCategoryEntry(UTIMER, AMANAGER);
  gCatMan.deleteCategoryEntry(UTIMER, AMANAGER, true);
}

function enableBackgroundUpdateTimer() {
  info("Enabling " + UTIMER + " " + AMANAGER);
  gCatMan.addCategoryEntry(
    UTIMER,
    AMANAGER,
    backgroundUpdateConfig,
    false,
    true
  );
}

registerCleanupFunction(function() {
  // Restore prefs
  for (let pref of gRestorePrefs) {
    if (pref.type == "clear") {
      Services.prefs.clearUserPref(pref.name);
    } else if (pref.type == Services.prefs.PREF_BOOL) {
      Services.prefs.setBoolPref(pref.name, pref.value);
    } else if (pref.type == Services.prefs.PREF_INT) {
      Services.prefs.setIntPref(pref.name, pref.value);
    } else if (pref.type == Services.prefs.PREF_STRING) {
      Services.prefs.setCharPref(pref.name, pref.value);
    }
  }

  // Throw an error if the add-ons manager window is open anywhere
  checkOpenWindows("Addons:Manager");
  checkOpenWindows("Addons:Compatibility");
  checkOpenWindows("Addons:Install");

  return AddonManager.getAllInstalls().then(aInstalls => {
    for (let install of aInstalls) {
      if (install instanceof MockInstall) {
        continue;
      }

      ok(
        false,
        "Should not have seen an install of " +
          install.sourceURI.spec +
          " in state " +
          install.state
      );
      install.cancel();
    }
  });
});

function log_exceptions(aCallback, ...aArgs) {
  try {
    return aCallback.apply(null, aArgs);
  } catch (e) {
    info("Exception thrown: " + e);
    throw e;
  }
}

function log_callback(aPromise, aCallback) {
  aPromise.then(aCallback).catch(e => info("Exception thrown: " + e));
  return aPromise;
}

function add_test(test) {
  gPendingTests.push(test);
}

function run_next_test() {
  // Make sure we're not calling run_next_test from inside an add_task() test
  // We're inside the browser_test.js 'testScope' here
  if (this.__tasks) {
    throw new Error(
      "run_next_test() called from an add_task() test function. " +
        "run_next_test() should not be called from inside add_task() " +
        "under any circumstances!"
    );
  }
  if (gTestsRun > 0) {
    info("Test " + gTestsRun + " took " + (Date.now() - gTestStart) + "ms");
  }

  if (!gPendingTests.length) {
    executeSoon(end_test);
    return;
  }

  gTestsRun++;
  var test = gPendingTests.shift();
  if (test.name) {
    info("Running test " + gTestsRun + " (" + test.name + ")");
  } else {
    info("Running test " + gTestsRun);
  }

  gTestStart = Date.now();
  executeSoon(() => log_exceptions(test));
}

var get_tooltip_info = async function(addonEl, managerWindow) {
  // Extract from title attribute.
  const { addon } = addonEl;
  const name = addon.name;

  let nameWithVersion = addonEl.addonNameEl.title;
  if (addonEl.addon.userDisabled) {
    // TODO - Bug 1558077: Currently Fluent is clearing the addon title
    // when the addon is disabled, fixing it requires changes to the
    // HTML about:addons localized strings, and then remove this
    // workaround.
    nameWithVersion = `${name} ${addon.version}`;
  }

  return {
    name,
    version: nameWithVersion.substring(name.length + 1),
  };
};

function get_addon_file_url(aFilename) {
  try {
    var cr = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
      Ci.nsIChromeRegistry
    );
    var fileurl = cr.convertChromeURL(
      makeURI(CHROMEROOT + "addons/" + aFilename)
    );
    return fileurl.QueryInterface(Ci.nsIFileURL);
  } catch (ex) {
    var jar = getJar(CHROMEROOT + "addons/" + aFilename);
    var tmpDir = extractJarToTmp(jar);
    tmpDir.append(aFilename);

    return Services.io.newFileURI(tmpDir).QueryInterface(Ci.nsIFileURL);
  }
}

function get_current_view(aManager) {
  let view = aManager.document.getElementById("view-port").selectedPanel;
  if (view.id == "headered-views") {
    view = aManager.document.getElementById("headered-views-content")
      .selectedPanel;
  }
  is(
    view,
    aManager.gViewController.displayedView,
    "view controller is tracking the displayed view correctly"
  );
  return view;
}

function check_all_in_list(aManager, aIds, aIgnoreExtras) {
  var doc = aManager.document;
  var list = doc.getElementById("addon-list");

  var inlist = [];
  var node = list.firstChild;
  while (node) {
    if (node.value) {
      inlist.push(node.value);
    }
    node = node.nextSibling;
  }

  for (let id of aIds) {
    if (!inlist.includes(id)) {
      ok(false, "Should find " + id + " in the list");
    }
  }

  if (aIgnoreExtras) {
    return;
  }

  for (let inlistItem of inlist) {
    if (!aIds.includes(inlistItem)) {
      ok(false, "Shouldn't have seen " + inlistItem + " in the list");
    }
  }
}

function get_addon_element(aManager, aId) {
  const win = aManager.getHtmlBrowser().contentWindow;
  return getAddonCard(win, aId);
}

function getAddonCard(win, id) {
  return win.document.querySelector(`addon-card[addon-id="${id}"]`);
}

function wait_for_view_load(
  aManagerWindow,
  aCallback,
  aForceWait,
  aLongerTimeout
) {
  let p = new Promise(resolve => {
    requestLongerTimeout(aLongerTimeout ? aLongerTimeout : 2);

    if (!aForceWait && !aManagerWindow.gViewController.isLoading) {
      resolve(aManagerWindow);
      return;
    }

    aManagerWindow.document.addEventListener(
      "ViewChanged",
      function() {
        resolve(aManagerWindow);
      },
      { once: true }
    );
  });

  return log_callback(p, aCallback);
}

function wait_for_manager_load(aManagerWindow, aCallback) {
  let p = new Promise(resolve => {
    if (!aManagerWindow.gIsInitializing) {
      resolve(aManagerWindow);
      return;
    }

    info("Waiting for initialization");
    aManagerWindow.document.addEventListener(
      "Initialized",
      function() {
        resolve(aManagerWindow);
      },
      { once: true }
    );
  });

  return log_callback(p, aCallback);
}

function open_manager(
  aView,
  aCallback,
  aLoadCallback,
  aLongerTimeout,
  aWin = window
) {
  let p = new Promise((resolve, reject) => {
    async function setup_manager(aManagerWindow) {
      if (aLoadCallback) {
        log_exceptions(aLoadCallback, aManagerWindow);
      }

      if (aView) {
        aManagerWindow.loadView(aView);
      }

      ok(aManagerWindow != null, "Should have an add-ons manager window");
      is(
        aManagerWindow.location.href,
        MANAGER_URI,
        "Should be displaying the correct UI"
      );

      await promiseFocus(aManagerWindow);
      info("window has focus, waiting for manager load");
      await wait_for_manager_load(aManagerWindow);
      info("Manager waiting for view load");
      await wait_for_view_load(aManagerWindow, null, null, aLongerTimeout);
      resolve(aManagerWindow);
    }

    info("Loading manager window in tab");
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
      Services.obs.removeObserver(observer, aTopic);
      if (aSubject.location.href != MANAGER_URI) {
        info("Ignoring load event for " + aSubject.location.href);
        return;
      }
      setup_manager(aSubject);
    }, "EM-loaded");

    aWin.gBrowser.selectedTab = BrowserTestUtils.addTab(aWin.gBrowser);
    aWin.switchToTabHavingURI(MANAGER_URI, true, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
  });

  // The promise resolves with the manager window, so it is passed to the callback
  return log_callback(p, aCallback);
}

function close_manager(aManagerWindow, aCallback, aLongerTimeout) {
  let p = new Promise((resolve, reject) => {
    requestLongerTimeout(aLongerTimeout ? aLongerTimeout : 2);

    ok(
      aManagerWindow != null,
      "Should have an add-ons manager window to close"
    );
    is(
      aManagerWindow.location.href,
      MANAGER_URI,
      "Should be closing window with correct URI"
    );

    aManagerWindow.addEventListener("unload", function listener() {
      try {
        dump("Manager window unload handler\n");
        this.removeEventListener("unload", listener);
        resolve();
      } catch (e) {
        reject(e);
      }
    });
  });

  info("Telling manager window to close");
  aManagerWindow.close();
  info("Manager window close() call returned");

  return log_callback(p, aCallback);
}

function restart_manager(aManagerWindow, aView, aCallback, aLoadCallback) {
  if (!aManagerWindow) {
    return open_manager(aView, aCallback, aLoadCallback);
  }

  return close_manager(aManagerWindow).then(() =>
    open_manager(aView, aCallback, aLoadCallback)
  );
}

function wait_for_window_open(aCallback) {
  let p = new Promise(resolve => {
    Services.wm.addListener({
      onOpenWindow(aXulWin) {
        Services.wm.removeListener(this);

        let domwindow = aXulWin.docShell.domWindow;
        domwindow.addEventListener(
          "load",
          function() {
            executeSoon(function() {
              resolve(domwindow);
            });
          },
          { once: true }
        );
      },

      onCloseWindow(aWindow) {},
    });
  });

  return log_callback(p, aCallback);
}

function get_string(aName, ...aArgs) {
  var bundle = Services.strings.createBundle(
    "chrome://mozapps/locale/extensions/extensions.properties"
  );
  if (!aArgs.length) {
    return bundle.GetStringFromName(aName);
  }
  return bundle.formatStringFromName(aName, aArgs);
}

function formatDate(aDate) {
  const dtOptions = { year: "numeric", month: "long", day: "numeric" };
  return aDate.toLocaleDateString(undefined, dtOptions);
}

function is_hidden(aElement) {
  var style = aElement.ownerGlobal.getComputedStyle(aElement);
  if (style.display == "none") {
    return true;
  }
  if (style.visibility != "visible") {
    return true;
  }

  // Hiding a parent element will hide all its children
  if (aElement.parentNode != aElement.ownerDocument) {
    return is_hidden(aElement.parentNode);
  }

  return false;
}

function is_element_visible(aElement, aMsg) {
  isnot(aElement, null, "Element should not be null, when checking visibility");
  ok(!is_hidden(aElement), aMsg || aElement + " should be visible");
}

function is_element_hidden(aElement, aMsg) {
  isnot(aElement, null, "Element should not be null, when checking visibility");
  ok(is_hidden(aElement), aMsg || aElement + " should be hidden");
}

function promiseAddonByID(aId) {
  return AddonManager.getAddonByID(aId);
}

function promiseAddonsByIDs(aIDs) {
  return AddonManager.getAddonsByIDs(aIDs);
}
/**
 * Install an add-on and call a callback when complete.
 *
 * The callback will receive the Addon for the installed add-on.
 */
async function install_addon(path, cb, pathPrefix = TESTROOT) {
  let install = await AddonManager.getInstallForURL(pathPrefix + path);
  let p = new Promise((resolve, reject) => {
    install.addListener({
      onInstallEnded: () => resolve(install.addon),
    });

    install.install();
  });

  return log_callback(p, cb);
}

function CategoryUtilities(aManagerWindow) {
  this.window = aManagerWindow.getHtmlBrowser().contentWindow;
  this.managerWindow = aManagerWindow;

  this.window.addEventListener("unload", () => (this.window = null), {
    once: true,
  });
  this.managerWindow.addEventListener(
    "unload",
    () => (this.managerWindow = null),
    { once: true }
  );
}

CategoryUtilities.prototype = {
  window: null,
  managerWindow: null,

  get _categoriesBox() {
    return this.window.document.querySelector("categories-box");
  },

  getSelectedViewId() {
    let selectedItem = this._categoriesBox.querySelector("[selected]");
    isnot(selectedItem, null, "A category should be selected");
    return selectedItem.getAttribute("viewid");
  },

  get selectedCategory() {
    isnot(
      this.window,
      null,
      "Should not get selected category when manager window is not loaded"
    );
    let viewId = this.getSelectedViewId();
    let view = this.managerWindow.gViewController.parseViewId(viewId);
    return view.type == "list" ? view.param : view.type;
  },

  get(categoryType) {
    isnot(
      this.window,
      null,
      "Should not get category when manager window is not loaded"
    );

    let button = this._categoriesBox.querySelector(`[name="${categoryType}"]`);
    if (button) {
      return button;
    }

    ok(false, "Should have found a category with type " + categoryType);
    return null;
  },

  isVisible(categoryButton) {
    isnot(
      this.window,
      null,
      "Should not check visible state when manager window is not loaded"
    );

    // There are some tests checking this before the categories have loaded.
    if (!categoryButton) {
      return false;
    }

    if (categoryButton.disabled || categoryButton.hidden) {
      return false;
    }

    return !is_hidden(categoryButton);
  },

  isTypeVisible(categoryType) {
    return this.isVisible(this.get(categoryType));
  },

  open(categoryButton) {
    isnot(
      this.window,
      null,
      "Should not open category when manager window is not loaded"
    );
    ok(
      this.isVisible(categoryButton),
      "Category should be visible if attempting to open it"
    );

    EventUtils.synthesizeMouseAtCenter(categoryButton, {}, this.window);

    // Use wait_for_view_load until all open_manager calls are gone.
    return wait_for_view_load(this.managerWindow);
  },

  openType(categoryType) {
    return this.open(this.get(categoryType));
  },
};

// Returns a promise that will resolve when the certificate error override has been added, or reject
// if there is some failure.
function addCertOverride(host, bits) {
  return new Promise((resolve, reject) => {
    let req = new XMLHttpRequest();
    req.open("GET", "https://" + host + "/");
    req.onload = reject;
    req.onerror = () => {
      if (req.channel && req.channel.securityInfo) {
        let securityInfo = req.channel.securityInfo.QueryInterface(
          Ci.nsITransportSecurityInfo
        );
        if (securityInfo.serverCert) {
          let cos = Cc["@mozilla.org/security/certoverride;1"].getService(
            Ci.nsICertOverrideService
          );
          cos.rememberValidityOverride(
            host,
            -1,
            securityInfo.serverCert,
            bits,
            false
          );
          resolve();
          return;
        }
      }
      reject();
    };
    req.send(null);
  });
}

// Returns a promise that will resolve when the necessary certificate overrides have been added.
function addCertOverrides() {
  return Promise.all([
    addCertOverride(
      "nocert.example.com",
      Ci.nsICertOverrideService.ERROR_MISMATCH
    ),
    addCertOverride(
      "self-signed.example.com",
      Ci.nsICertOverrideService.ERROR_UNTRUSTED
    ),
    addCertOverride(
      "untrusted.example.com",
      Ci.nsICertOverrideService.ERROR_UNTRUSTED
    ),
    addCertOverride(
      "expired.example.com",
      Ci.nsICertOverrideService.ERROR_TIME
    ),
  ]);
}

/** *** Mock Provider *****/

function MockProvider() {
  this.addons = [];
  this.installs = [];
  this.types = [
    {
      id: "extension",
      name: "Extensions",
      uiPriority: 4000,
      flags:
        AddonManager.TYPE_UI_VIEW_LIST |
        AddonManager.TYPE_SUPPORTS_UNDO_RESTARTLESS_UNINSTALL,
    },
  ];

  var self = this;
  registerCleanupFunction(function() {
    if (self.started) {
      self.unregister();
    }
  });

  this.register();
}

MockProvider.prototype = {
  addons: null,
  installs: null,
  started: null,
  types: null,
  queryDelayPromise: Promise.resolve(),

  blockQueryResponses() {
    this.queryDelayPromise = new Promise(resolve => {
      this._unblockQueries = resolve;
    });
  },

  unblockQueryResponses() {
    if (this._unblockQueries) {
      this._unblockQueries();
      this._unblockQueries = null;
    } else {
      throw new Error("Queries are not blocked");
    }
  },

  /** *** Utility functions *****/

  /**
   * Register this provider with the AddonManager
   */
  register: function MP_register() {
    info("Registering mock add-on provider");
    AddonManagerPrivate.registerProvider(this, this.types);
  },

  /**
   * Unregister this provider with the AddonManager
   */
  unregister: function MP_unregister() {
    info("Unregistering mock add-on provider");
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
    var oldAddons = this.addons.filter(aOldAddon => aOldAddon.id == aAddon.id);
    var oldAddon = oldAddons.length ? oldAddons[0] : null;

    this.addons = this.addons.filter(aOldAddon => aOldAddon.id != aAddon.id);

    this.addons.push(aAddon);
    aAddon._provider = this;

    if (!this.started) {
      return;
    }

    let requiresRestart =
      (aAddon.operationsRequiringRestart &
        AddonManager.OP_NEEDS_RESTART_INSTALL) !=
      0;
    AddonManagerPrivate.callInstallListeners(
      "onExternalInstall",
      null,
      aAddon,
      oldAddon,
      requiresRestart
    );
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
      ok(
        false,
        "Tried to remove an add-on that wasn't registered with the mock provider"
      );
      return;
    }

    this.addons.splice(pos, 1);

    if (!this.started) {
      return;
    }

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
    aInstall._provider = this;

    if (!this.started) {
      return;
    }

    aInstall.callListeners("onNewInstall");
  },

  removeInstall: function MP_removeInstall(aInstall) {
    var pos = this.installs.indexOf(aInstall);
    if (pos == -1) {
      ok(
        false,
        "Tried to remove an install that wasn't registered with the mock provider"
      );
      return;
    }

    this.installs.splice(pos, 1);
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
    for (let addonProp of aAddonProperties) {
      let addon = new MockAddon(addonProp.id);
      for (let prop in addonProp) {
        if (prop == "id") {
          continue;
        }
        if (prop == "applyBackgroundUpdates") {
          addon._applyBackgroundUpdates = addonProp[prop];
        } else if (prop == "appDisabled") {
          addon._appDisabled = addonProp[prop];
        } else if (prop == "userDisabled") {
          addon.setUserDisabled(addonProp[prop]);
        } else {
          addon[prop] = addonProp[prop];
        }
      }
      if (!addon.optionsType && !!addon.optionsURL) {
        addon.optionsType = AddonManager.OPTIONS_TYPE_DIALOG;
      }

      // Make sure the active state matches the passed in properties
      addon.isActive = addon.shouldBeActive;

      this.addAddon(addon);
      newAddons.push(addon);
    }

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
    for (let installProp of aInstallProperties) {
      let install = new MockInstall(
        installProp.name || null,
        installProp.type || null,
        null
      );
      for (let prop in installProp) {
        switch (prop) {
          case "name":
          case "type":
            break;
          case "sourceURI":
            install[prop] = NetUtil.newURI(installProp[prop]);
            break;
          default:
            install[prop] = installProp[prop];
        }
      }
      this.addInstall(install);
      newInstalls.push(install);
    }

    return newInstalls;
  },

  /** *** AddonProvider implementation *****/

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
    this.started = false;
  },

  /**
   * Called to get an Addon with a particular ID.
   *
   * @param  aId
   *         The ID of the add-on to retrieve
   */
  async getAddonByID(aId) {
    await this.queryDelayPromise;

    for (let addon of this.addons) {
      if (addon.id == aId) {
        return addon;
      }
    }

    return null;
  },

  /**
   * Called to get Addons of a particular type.
   *
   * @param  aTypes
   *         An array of types to fetch. Can be null to get all types.
   */
  async getAddonsByTypes(aTypes) {
    await this.queryDelayPromise;

    var addons = this.addons.filter(function(aAddon) {
      if (aTypes && !!aTypes.length && !aTypes.includes(aAddon.type)) {
        return false;
      }
      return true;
    });
    return addons;
  },

  /**
   * Called to get the current AddonInstalls, optionally restricting by type.
   *
   * @param  aTypes
   *         An array of types or null to get all types
   */
  async getInstallsByTypes(aTypes) {
    await this.queryDelayPromise;

    var installs = this.installs.filter(function(aInstall) {
      // Appear to have actually removed cancelled installs from the provider
      if (aInstall.state == AddonManager.STATE_CANCELLED) {
        return false;
      }

      if (aTypes && !!aTypes.length && !aTypes.includes(aInstall.type)) {
        return false;
      }

      return true;
    });
    return installs;
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
   * @param  {string} aUrl
   *         The URL to be installed
   * @param  {object} aOptions
   *         Options for the install
   */
  getInstallForURL: function MP_getInstallForURL(aUrl, aOptions) {
    // Not yet implemented
  },

  /**
   * Called to get an AddonInstall to install an add-on from a local file.
   *
   * @param  aFile
   *         The file to be installed
   */
  getInstallForFile: function MP_getInstallForFile(aFile) {
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
};

/** *** Mock Addon object for the Mock Provider *****/

function MockAddon(aId, aName, aType, aOperationsRequiringRestart) {
  // Only set required attributes.
  this.id = aId || "";
  this.name = aName || "";
  this.type = aType || "extension";
  this.version = "";
  this.isCompatible = true;
  this.providesUpdatesSecurely = true;
  this.blocklistState = 0;
  this._appDisabled = false;
  this._userDisabled = false;
  this._applyBackgroundUpdates = AddonManager.AUTOUPDATE_ENABLE;
  this.scope = AddonManager.SCOPE_PROFILE;
  this.isActive = true;
  this.creator = "";
  this.pendingOperations = 0;
  this._permissions =
    AddonManager.PERM_CAN_UNINSTALL |
    AddonManager.PERM_CAN_ENABLE |
    AddonManager.PERM_CAN_DISABLE |
    AddonManager.PERM_CAN_UPGRADE |
    AddonManager.PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS;
  this.operationsRequiringRestart =
    aOperationsRequiringRestart != undefined
      ? aOperationsRequiringRestart
      : AddonManager.OP_NEEDS_RESTART_INSTALL |
        AddonManager.OP_NEEDS_RESTART_UNINSTALL |
        AddonManager.OP_NEEDS_RESTART_ENABLE |
        AddonManager.OP_NEEDS_RESTART_DISABLE;
}

MockAddon.prototype = {
  get isCorrectlySigned() {
    if (this.signedState === AddonManager.SIGNEDSTATE_NOT_REQUIRED) {
      return true;
    }
    return this.signedState > AddonManager.SIGNEDSTATE_MISSING;
  },

  get shouldBeActive() {
    return (
      !this.appDisabled &&
      !this._userDisabled &&
      !(this.pendingOperations & AddonManager.PENDING_UNINSTALL)
    );
  },

  get appDisabled() {
    return this._appDisabled;
  },

  set appDisabled(val) {
    if (val == this._appDisabled) {
      return val;
    }

    AddonManagerPrivate.callAddonListeners("onPropertyChanged", this, [
      "appDisabled",
    ]);

    var currentActive = this.shouldBeActive;
    this._appDisabled = val;
    var newActive = this.shouldBeActive;
    this._updateActiveState(currentActive, newActive);

    return val;
  },

  get userDisabled() {
    return this._userDisabled;
  },

  set userDisabled(val) {
    throw new Error("No. Bad.");
  },

  setUserDisabled(val) {
    if (val == this._userDisabled) {
      return;
    }

    var currentActive = this.shouldBeActive;
    this._userDisabled = val;
    var newActive = this.shouldBeActive;
    this._updateActiveState(currentActive, newActive);
  },

  async enable() {
    await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));

    this.setUserDisabled(false);
  },
  async disable() {
    await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));

    this.setUserDisabled(true);
  },

  get permissions() {
    let permissions = this._permissions;
    if (this.appDisabled || !this._userDisabled) {
      permissions &= ~AddonManager.PERM_CAN_ENABLE;
    }
    if (this.appDisabled || this._userDisabled) {
      permissions &= ~AddonManager.PERM_CAN_DISABLE;
    }
    return permissions;
  },

  set permissions(val) {
    return (this._permissions = val);
  },

  get applyBackgroundUpdates() {
    return this._applyBackgroundUpdates;
  },

  set applyBackgroundUpdates(val) {
    if (
      val != AddonManager.AUTOUPDATE_DEFAULT &&
      val != AddonManager.AUTOUPDATE_DISABLE &&
      val != AddonManager.AUTOUPDATE_ENABLE
    ) {
      ok(false, "addon.applyBackgroundUpdates set to an invalid value: " + val);
    }
    this._applyBackgroundUpdates = val;
    AddonManagerPrivate.callAddonListeners("onPropertyChanged", this, [
      "applyBackgroundUpdates",
    ]);
  },

  isCompatibleWith(aAppVersion, aPlatformVersion) {
    return true;
  },

  findUpdates(aListener, aReason, aAppVersion, aPlatformVersion) {
    // Tests can implement this if they need to
  },

  async getBlocklistURL() {
    return this.blocklistURL;
  },

  uninstall(aAlwaysAllowUndo = false) {
    if (
      this.operationsRequiringRestart &
        AddonManager.OP_NEED_RESTART_UNINSTALL &&
      this.pendingOperations & AddonManager.PENDING_UNINSTALL
    ) {
      throw Components.Exception("Add-on is already pending uninstall");
    }

    var needsRestart =
      aAlwaysAllowUndo ||
      !!(
        this.operationsRequiringRestart &
        AddonManager.OP_NEEDS_RESTART_UNINSTALL
      );
    this.pendingOperations |= AddonManager.PENDING_UNINSTALL;
    AddonManagerPrivate.callAddonListeners(
      "onUninstalling",
      this,
      needsRestart
    );
    if (!needsRestart) {
      this.pendingOperations -= AddonManager.PENDING_UNINSTALL;
      this._provider.removeAddon(this);
    } else if (
      !(this.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_DISABLE)
    ) {
      this.isActive = false;
    }
  },

  cancelUninstall() {
    if (!(this.pendingOperations & AddonManager.PENDING_UNINSTALL)) {
      throw Components.Exception("Add-on is not pending uninstall");
    }

    this.pendingOperations -= AddonManager.PENDING_UNINSTALL;
    this.isActive = this.shouldBeActive;
    AddonManagerPrivate.callAddonListeners("onOperationCancelled", this);
  },

  markAsSeen() {
    this.seen = true;
  },

  _updateActiveState(currentActive, newActive) {
    if (currentActive == newActive) {
      return;
    }

    if (newActive == this.isActive) {
      this.pendingOperations -= newActive
        ? AddonManager.PENDING_DISABLE
        : AddonManager.PENDING_ENABLE;
      AddonManagerPrivate.callAddonListeners("onOperationCancelled", this);
    } else if (newActive) {
      let needsRestart = !!(
        this.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_ENABLE
      );
      this.pendingOperations |= AddonManager.PENDING_ENABLE;
      AddonManagerPrivate.callAddonListeners("onEnabling", this, needsRestart);
      if (!needsRestart) {
        this.isActive = newActive;
        this.pendingOperations -= AddonManager.PENDING_ENABLE;
        AddonManagerPrivate.callAddonListeners("onEnabled", this);
      }
    } else {
      let needsRestart = !!(
        this.operationsRequiringRestart & AddonManager.OP_NEEDS_RESTART_DISABLE
      );
      this.pendingOperations |= AddonManager.PENDING_DISABLE;
      AddonManagerPrivate.callAddonListeners("onDisabling", this, needsRestart);
      if (!needsRestart) {
        this.isActive = newActive;
        this.pendingOperations -= AddonManager.PENDING_DISABLE;
        AddonManagerPrivate.callAddonListeners("onDisabled", this);
      }
    }
  },
};

/** *** Mock AddonInstall object for the Mock Provider *****/

function MockInstall(aName, aType, aAddonToInstall) {
  this.name = aName || "";
  // Don't expose type until download completed
  this._type = aType || "extension";
  this.type = null;
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
  install() {
    switch (this.state) {
      case AddonManager.STATE_AVAILABLE:
        this.state = AddonManager.STATE_DOWNLOADING;
        if (!this.callListeners("onDownloadStarted")) {
          this.state = AddonManager.STATE_CANCELLED;
          this.callListeners("onDownloadCancelled");
          return;
        }

        this.type = this._type;

        // Adding addon to MockProvider to be implemented when needed
        if (this._addonToInstall) {
          this.addon = this._addonToInstall;
        } else {
          this.addon = new MockAddon("", this.name, this.type);
          this.addon.version = this.version;
          this.addon.pendingOperations = AddonManager.PENDING_INSTALL;
        }
        this.addon.install = this;
        if (this.existingAddon) {
          if (!this.addon.id) {
            this.addon.id = this.existingAddon.id;
          }
          this.existingAddon.pendingUpgrade = this.addon;
          this.existingAddon.pendingOperations |= AddonManager.PENDING_UPGRADE;
        }

        this.state = AddonManager.STATE_DOWNLOADED;
        this.callListeners("onDownloadEnded");
      // fall through
      case AddonManager.STATE_DOWNLOADED:
        this.state = AddonManager.STATE_INSTALLING;
        if (!this.callListeners("onInstallStarted")) {
          this.state = AddonManager.STATE_CANCELLED;
          this.callListeners("onInstallCancelled");
          return;
        }

        let needsRestart =
          this.operationsRequiringRestart &
          AddonManager.OP_NEEDS_RESTART_INSTALL;
        AddonManagerPrivate.callAddonListeners(
          "onInstalling",
          this.addon,
          needsRestart
        );
        if (!needsRestart) {
          AddonManagerPrivate.callAddonListeners("onInstalled", this.addon);
        }

        this.state = AddonManager.STATE_INSTALLED;
        this.callListeners("onInstallEnded");
        break;
      case AddonManager.STATE_DOWNLOADING:
      case AddonManager.STATE_CHECKING_UPDATE:
      case AddonManager.STATE_INSTALLING:
        // Installation is already running
        return;
      default:
        ok(false, "Cannot start installing when state = " + this.state);
    }
  },

  cancel() {
    switch (this.state) {
      case AddonManager.STATE_AVAILABLE:
        this.state = AddonManager.STATE_CANCELLED;
        break;
      case AddonManager.STATE_INSTALLED:
        this.state = AddonManager.STATE_CANCELLED;
        this._provider.removeInstall(this);
        this.callListeners("onInstallCancelled");
        break;
      default:
        // Handling cancelling when downloading to be implemented when needed
        ok(false, "Cannot cancel when state = " + this.state);
    }
  },

  addListener(aListener) {
    if (!this.listeners.some(i => i == aListener)) {
      this.listeners.push(aListener);
    }
  },

  removeListener(aListener) {
    this.listeners = this.listeners.filter(i => i != aListener);
  },

  addTestListener(aListener) {
    if (!this.testListeners.some(i => i == aListener)) {
      this.testListeners.push(aListener);
    }
  },

  removeTestListener(aListener) {
    this.testListeners = this.testListeners.filter(i => i != aListener);
  },

  callListeners(aMethod) {
    var result = AddonManagerPrivate.callInstallListeners(
      aMethod,
      this.listeners,
      this,
      this.addon
    );

    // Call test listeners after standard listeners to remove race condition
    // between standard and test listeners
    for (let listener of this.testListeners) {
      try {
        if (aMethod in listener) {
          if (listener[aMethod](this, this.addon) === false) {
            result = false;
          }
        }
      } catch (e) {
        ok(false, "Test listener threw exception: " + e);
      }
    }

    return result;
  },
};

function waitForCondition(condition, nextTest, errorMsg) {
  let tries = 0;
  let interval = setInterval(function() {
    if (tries >= 30) {
      ok(false, errorMsg);
      moveOn();
    }
    var conditionPassed;
    try {
      conditionPassed = condition();
    } catch (e) {
      ok(false, e + "\n" + e.stack);
      conditionPassed = false;
    }
    if (conditionPassed) {
      moveOn();
    }
    tries++;
  }, 100);
  let moveOn = function() {
    clearInterval(interval);
    nextTest();
  };
}

function getTestPluginTag() {
  let ph = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  let tags = ph.getPluginTags();

  // Find the test plugin
  for (let i = 0; i < tags.length; i++) {
    if (tags[i].name == "Test Plug-in") {
      return tags[i];
    }
  }
  ok(false, "Unable to find plugin");
  return null;
}

// Wait for and then acknowledge (by pressing the primary button) the
// given notification.
function promiseNotification(id = "addon-webext-permissions") {
  if (
    !Services.prefs.getBoolPref("extensions.webextPermissionPrompts", false)
  ) {
    return Promise.resolve();
  }

  return new Promise(resolve => {
    function popupshown() {
      let notification = PopupNotifications.getNotification(id);
      if (notification) {
        PopupNotifications.panel.removeEventListener("popupshown", popupshown);
        PopupNotifications.panel.firstElementChild.button.click();
        resolve();
      }
    }
    PopupNotifications.panel.addEventListener("popupshown", popupshown);
  });
}

/**
 * Wait for the given PopupNotification to display
 *
 * @param {string} name
 *        The name of the notification to wait for.
 *
 * @returns {Promise}
 *          Resolves with the notification window.
 */
function promisePopupNotificationShown(name = "addon-webext-permissions") {
  return new Promise(resolve => {
    function popupshown() {
      let notification = PopupNotifications.getNotification(name);
      if (!notification) {
        return;
      }

      ok(notification, `${name} notification shown`);
      ok(PopupNotifications.isPanelOpen, "notification panel open");

      PopupNotifications.panel.removeEventListener("popupshown", popupshown);
      resolve(PopupNotifications.panel.firstChild);
    }
    PopupNotifications.panel.addEventListener("popupshown", popupshown);
  });
}

function waitAppMenuNotificationShown(
  id,
  addonId,
  accept = false,
  win = window
) {
  const { AppMenuNotifications } = ChromeUtils.import(
    "resource://gre/modules/AppMenuNotifications.jsm"
  );
  return new Promise(resolve => {
    let { document, PanelUI } = win;

    async function popupshown() {
      let notification = AppMenuNotifications.activeNotification;
      if (!notification) {
        return;
      }

      is(notification.id, id, `${id} notification shown`);
      ok(PanelUI.isNotificationPanelOpen, "notification panel open");

      PanelUI.notificationPanel.removeEventListener("popupshown", popupshown);

      if (id == "addon-installed" && addonId) {
        let addon = await AddonManager.getAddonByID(addonId);
        if (!addon) {
          ok(false, `Addon with id "${addonId}" not found`);
        }
        let hidden = !(
          addon.permissions &
          AddonManager.PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS
        );
        let checkbox = document.getElementById("addon-incognito-checkbox");
        is(checkbox.hidden, hidden, "checkbox visibility is correct");
      }
      if (accept) {
        let popupnotificationID = PanelUI._getPopupId(notification);
        let popupnotification = document.getElementById(popupnotificationID);
        popupnotification.button.click();
      }

      resolve();
    }
    // If it's already open just run the test.
    let notification = AppMenuNotifications.activeNotification;
    if (notification && PanelUI.isNotificationPanelOpen) {
      popupshown();
      return;
    }
    PanelUI.notificationPanel.addEventListener("popupshown", popupshown);
  });
}

function acceptAppMenuNotificationWhenShown(id, addonId) {
  return waitAppMenuNotificationShown(id, addonId, true);
}

const ABOUT_ADDONS_METHODS = new Set(["action", "view", "link"]);
function assertAboutAddonsTelemetryEvents(events, filters = {}) {
  TelemetryTestUtils.assertEvents(events, {
    category: "addonsManager",
    method: actual =>
      filters.methods
        ? filters.methods.includes(actual)
        : ABOUT_ADDONS_METHODS.has(actual),
    object: "aboutAddons",
  });
}

/* HTML view helpers */
async function loadInitialView(type, opts) {
  if (type) {
    // Force the first page load to be the view we want.
    let viewId;
    if (type.startsWith("addons://")) {
      viewId = type;
    } else {
      viewId =
        type == "discover" ? "addons://discover/" : `addons://list/${type}`;
    }
    Services.prefs.setCharPref(PREF_UI_LASTCATEGORY, viewId);
  }

  let loadCallback;
  let loadCallbackDone = Promise.resolve();

  if (opts && opts.loadCallback) {
    // Make sure the HTML browser is loaded and pass its window to the callback
    // function instead of the XUL window.
    loadCallback = managerWindow => {
      loadCallbackDone = managerWindow
        .promiseHtmlBrowserLoaded()
        .then(async browser => {
          let win = browser.contentWindow;
          win.managerWindow = managerWindow;
          // Wait for the test code to finish running before proceeding.
          await opts.loadCallback(win);
        });
    };
  }
  let managerWindow = await open_manager(null, null, loadCallback);

  let browser = managerWindow.document.getElementById("html-view-browser");
  let win = browser.contentWindow;
  if (!opts || !opts.withAnimations) {
    win.document.body.setAttribute("skip-animations", "");
  }
  win.managerWindow = managerWindow;

  // Let any load callback code to run before the rest of the test continues.
  await loadCallbackDone;

  return win;
}

function waitForViewLoad(win) {
  return wait_for_view_load(win.managerWindow, undefined, true);
}

function closeView(win) {
  return close_manager(win.managerWindow);
}

function switchView(win, type) {
  return new CategoryUtilities(win.managerWindow).openType(type);
}

function isCategoryVisible(win, type) {
  return new CategoryUtilities(win.managerWindow).isTypeVisible(type);
}

function mockPromptService() {
  let { prompt } = Services;
  let promptService = {
    // The prompt returns 1 for cancelled and 0 for accepted.
    _response: 1,
    QueryInterface: ChromeUtils.generateQI(["nsIPromptService"]),
    confirmEx: () => promptService._response,
  };
  Services.prompt = promptService;
  registerCleanupFunction(() => {
    Services.prompt = prompt;
  });
  return promptService;
}

function assertHasPendingUninstalls(addonList, expectedPendingUninstallsCount) {
  const pendingUninstalls = addonList.querySelector(
    "message-bar-stack.pending-uninstall"
  );
  ok(pendingUninstalls, "Got a pending-uninstall message-bar-stack");
  is(
    pendingUninstalls.childElementCount,
    expectedPendingUninstallsCount,
    "Got a message bar in the pending-uninstall message-bar-stack"
  );
}

function assertHasPendingUninstallAddon(addonList, addon) {
  const pendingUninstalls = addonList.querySelector(
    "message-bar-stack.pending-uninstall"
  );
  const addonPendingUninstall = addonList.getPendingUninstallBar(addon);
  ok(
    addonPendingUninstall,
    "Got expected message-bar for the pending uninstall test extension"
  );
  is(
    addonPendingUninstall.parentNode,
    pendingUninstalls,
    "pending uninstall bar should be part of the message-bar-stack"
  );
  is(
    addonPendingUninstall.getAttribute("addon-id"),
    addon.id,
    "Got expected addon-id attribute on the pending uninstall message-bar"
  );
}

async function testUndoPendingUninstall(addonList, addon) {
  const addonPendingUninstall = addonList.getPendingUninstallBar(addon);
  const undoButton = addonPendingUninstall.querySelector("button[action=undo]");
  ok(undoButton, "Got undo action button in the pending uninstall message-bar");

  info(
    "Clicking the pending uninstall undo button and wait for addon card rendered"
  );
  const updated = BrowserTestUtils.waitForEvent(addonList, "add");
  undoButton.click();
  await updated;

  ok(
    addon && !(addon.pendingOperations & AddonManager.PENDING_UNINSTALL),
    "The addon pending uninstall cancelled"
  );
}

function loadTestSubscript(filePath) {
  Services.scriptloader.loadSubScript(new URL(filePath, gTestPath).href, this);
}

function cleanupPendingNotifications() {
  const { ExtensionsUI } = ChromeUtils.import(
    "resource:///modules/ExtensionsUI.jsm"
  );
  info("Cleanup any pending notification before exiting the test");
  const keys = ChromeUtils.nondeterministicGetWeakSetKeys(
    ExtensionsUI.pendingNotifications
  );
  if (keys) {
    keys.forEach(key => ExtensionsUI.pendingNotifications.delete(key));
  }
}

function promisePermissionPrompt(addonId) {
  return BrowserUtils.promiseObserved(
    "webextension-permission-prompt",
    subject => {
      const { info } = subject.wrappedJSObject || {};
      return !addonId || (info.addon && info.addon.id === addonId);
    }
  ).then(({ subject }) => {
    return subject.wrappedJSObject.info;
  });
}

async function handlePermissionPrompt({
  addonId,
  reject = false,
  assertIcon = true,
} = {}) {
  const info = await promisePermissionPrompt(addonId);
  // Assert that info.addon and info.icon are defined as expected.
  is(
    info.addon && info.addon.id,
    addonId,
    "Got the AddonWrapper in the permission prompt info"
  );

  if (assertIcon) {
    ok(info.icon != null, "Got an addon icon in the permission prompt info");
  }

  if (reject) {
    info.reject();
  } else {
    info.resolve();
  }
}

async function switchToDetailView({ id, win }) {
  let card = getAddonCard(win, id);
  ok(card, `Addon card found for ${id}`);
  ok(!card.querySelector("addon-details"), "The card doesn't have details");
  let loaded = waitForViewLoad(win);
  EventUtils.synthesizeMouseAtCenter(card, { clickCount: 1 }, win);
  await loaded;
  card = getAddonCard(win, id);
  ok(card.querySelector("addon-details"), "The card does have details");
  return card;
}
