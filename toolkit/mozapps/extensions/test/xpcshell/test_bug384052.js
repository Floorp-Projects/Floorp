const Cc = Components.classes;
const Ci = Components.interfaces;

const CLASS_ID = Components.ID("{12345678-1234-1234-1234-123456789abc}");
const CONTRACT_ID = "@mozilla.org/test-parameter-source;1";

var gTestURL = "http://127.0.0.1:4444/?itemID=%ITEM_ID%&custom1=%CUSTOM1%&custom2=%CUSTOM2%";
var gExpectedURL = null;
var gSeenExpectedURL = false;

var gComponentRegistrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
var gObserverService = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
var gCategoryManager = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);

// Factory for our parameter handler
var paramHandlerFactory = {
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIFactory) || iid.equals(Ci.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  createInstance: function(outer, iid) {
    var bag = Cc["@mozilla.org/hash-property-bag;1"]
                .createInstance(Ci.nsIWritablePropertyBag);
    bag.setProperty("CUSTOM1", "custom_parameter_1");
    bag.setProperty("CUSTOM2", "custom_parameter_2");
    return bag.QueryInterface(iid);
  }
};

// Observer that should recognize when extension manager requests our URL
var observer = {
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIObserver) || iid.equals(Ci.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  observe: function(subject, topic, data) {
    if (topic == "http-on-modify-request") {
      var channel = subject.QueryInterface(Ci.nsIChannel);
      if (channel.originalURI.spec == gExpectedURL)
        gSeenExpectedURL = true;
    }
  }
}

function initTest()
{
  do_test_pending();
  // Setup extension manager
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  // Register our parameter handlers
  gComponentRegistrar.registerFactory(CLASS_ID, "Test component", CONTRACT_ID, paramHandlerFactory);
  gCategoryManager.addCategoryEntry("extension-update-params", "CUSTOM1", CONTRACT_ID, false, false);
  gCategoryManager.addCategoryEntry("extension-update-params", "CUSTOM2", CONTRACT_ID, false, false);

  // Register observer to get notified when URLs are requested
  gObserverService.addObserver(observer, "http-on-modify-request", false);

  // Install a test extension into the profile
  let dir = gProfD.clone();
  dir.append("extensions");
  dir.append("test@mozilla.org");
  writeInstallRDFToDir({
    id: "test@mozilla.org",
    version: "1.0",
    name: "Test extension",
    updateURL: gTestURL,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
  }, dir);

  startupManager();
}

function shutdownTest()
{
  shutdownManager();

  gComponentRegistrar.unregisterFactory(CLASS_ID, paramHandlerFactory);
  gCategoryManager.deleteCategoryEntry("extension-update-params", "CUSTOM1", false);
  gCategoryManager.deleteCategoryEntry("extension-update-params", "CUSTOM2", false);

  gObserverService.removeObserver(observer, "http-on-modify-request");

  do_test_finished();
}

function run_test()
{
  initTest();

  gExpectedURL = gTestURL.replace(/%ITEM_ID%/, "test@mozilla.org")
                         .replace(/%CUSTOM1%/, "custom_parameter_1")
                         .replace(/%CUSTOM2%/, "custom_parameter_2");

  AddonManager.getAddon("test@mozilla.org", function(item) {
    // Initiate update
    item.findUpdates({
      onNoCompatibilityUpdateAvailable: function(addon) {
        do_throw("Should not have seen no compatibility update");
      },

      onUpdateAvailable: function(addon, install) {
        ensure_test_completed();

        do_check_eq(addon, a1);
        do_check_eq(install.name, addon.name);
        do_check_eq(install.version, "2.0");
        do_check_eq(install.state, AddonManager.STATE_AVAILABLE);
        do_check_eq(install.existingAddon, addon);

        prepare_test({}, [
          "onDownloadStarted",
          "onDownloadEnded",
        ], check_test_1);
        install.install();
      },

      onUpdateFinished: function(addon) {
        do_check_true(gSeenExpectedURL);
        shutdownTest();
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}
