const CLASS_ID = Components.ID("{12345678-1234-1234-1234-123456789abc}");
const CONTRACT_ID = "@mozilla.org/test-parameter-source;1";

// Get and create the HTTP server.
Components.utils.import("resource://testing-common/httpd.js");
var testserver = new HttpServer();
testserver.start(-1);
gPort = testserver.identity.primaryPort;

var gTestURL = "http://127.0.0.1:" + gPort + "/update.rdf?itemID=%ITEM_ID%&custom1=%CUSTOM1%&custom2=%CUSTOM2%";
var gExpectedQuery = "itemID=test@mozilla.org&custom1=custom_parameter_1&custom2=custom_parameter_2";
var gSeenExpectedURL = false;

var gComponentRegistrar = Components.manager.QueryInterface(AM_Ci.nsIComponentRegistrar);
var gCategoryManager = AM_Cc["@mozilla.org/categorymanager;1"].getService(AM_Ci.nsICategoryManager);

// Factory for our parameter handler
var paramHandlerFactory = {
  QueryInterface: function(iid) {
    if (iid.equals(AM_Ci.nsIFactory) || iid.equals(AM_Ci.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  createInstance: function(outer, iid) {
    var bag = AM_Cc["@mozilla.org/hash-property-bag;1"].
              createInstance(AM_Ci.nsIWritablePropertyBag);
    bag.setProperty("CUSTOM1", "custom_parameter_1");
    bag.setProperty("CUSTOM2", "custom_parameter_2");
    return bag.QueryInterface(iid);
  }
};

function initTest()
{
  do_test_pending();
  // Setup extension manager
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  // Configure the HTTP server.
  testserver.registerPathHandler("/update.rdf", function(aRequest, aResponse) {
    gSeenExpectedURL = aRequest.queryString == gExpectedQuery;
    aResponse.setStatusLine(null, 404, "Not Found");
  });

  // Register our parameter handlers
  gComponentRegistrar.registerFactory(CLASS_ID, "Test component", CONTRACT_ID, paramHandlerFactory);
  gCategoryManager.addCategoryEntry("extension-update-params", "CUSTOM1", CONTRACT_ID, false, false);
  gCategoryManager.addCategoryEntry("extension-update-params", "CUSTOM2", CONTRACT_ID, false, false);

  // Install a test extension into the profile
  let dir = gProfD.clone();
  dir.append("extensions");
  writeInstallRDFForExtension({
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

  do_test_finished();
}

function run_test()
{
  initTest();

  AddonManager.getAddonByID("test@mozilla.org", function(item) {
    // Initiate update
    item.findUpdates({
      onCompatibilityUpdateAvailable: function(addon) {
        do_throw("Should not have seen a compatibility update");
      },

      onUpdateAvailable: function(addon, install) {
        do_throw("Should not have seen an available update");
      },

      onUpdateFinished: function(addon, error) {
        do_check_eq(error, AddonManager.UPDATE_STATUS_DOWNLOAD_ERROR);
        do_check_true(gSeenExpectedURL);
        do_execute_soon(shutdownTest);
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}
