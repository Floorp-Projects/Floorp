/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let {AddonTestUtils} = Components.utils.import("resource://testing-common/AddonManagerTesting.jsm", {});
let {HttpServer} = Components.utils.import("resource://testing-common/httpd.js", {});

let gManagerWindow;
let gCategoryUtilities;
let gExperiments;
let gHttpServer;

let gSavedManifestURI;

function getExperimentAddons() {
  let deferred = Promise.defer();
  AddonManager.getAddonsByTypes(["experiment"], (addons) => {
    deferred.resolve(addons);
  });
  return deferred.promise;
}

add_task(function* initializeState() {
  gManagerWindow = yield open_manager();
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("experiments.enabled");
    if (gHttpServer) {
      gHttpServer.stop(() => {});
      Services.prefs.clearUserPref("experiments.manifest.cert.checkAttributes");
      if (gSavedManifestURI !== undefined) {
        Services.prefs.setCharPref("experments.manifest.uri", gSavedManifestURI);
      }
    }
    if (gExperiments) {
      gExperiments._policy.ignoreHashes = false;
    }
  });

  // The Experiments Manager will interfere with us by preventing installs
  // of experiments it doesn't know about. We remove it from the equation
  // because here we are only concerned with core Addon Manager operation,
  // not the superset Experiments Manager has imposed.
  if ("@mozilla.org/browser/experiments-service;1" in Components.classes) {
    let tmp = {};
    Components.utils.import("resource:///modules/experiments/Experiments.jsm", tmp);
    // There is a race condition between XPCOM service initialization and
    // this test running. We have to initialize the instance first, then
    // uninitialize it to prevent this.
    gExperiments = tmp.Experiments.instance();
    yield gExperiments._mainTask;
    yield gExperiments.uninit();
  }
});

// On an empty profile with no experiments, the experiment category
// should be hidden.
add_task(function* testInitialState() {
  Assert.ok(gCategoryUtilities.get("experiment", false), "Experiment tab is defined.");
  Assert.ok(!gCategoryUtilities.isTypeVisible("experiment"), "Experiment tab hidden by default.");
});

add_task(function* testExperimentInfoNotVisible() {
  yield gCategoryUtilities.openType("extension");
  let el = gManagerWindow.document.getElementsByClassName("experiment-info-container")[0];
  is_element_hidden(el, "Experiment info not visible on other types.");
});

// If we have an active experiment, we should see the experiments tab
// and that tab should have some messages.
add_task(function* testActiveExperiment() {
  let addon = yield install_addon("addons/browser_experiment1.xpi");

  Assert.ok(addon.userDisabled, "Add-on is disabled upon initial install.");
  Assert.equal(addon.isActive, false, "Add-on is not active.");

  Assert.ok(gCategoryUtilities.isTypeVisible("experiment"), "Experiment tab visible.");

  yield gCategoryUtilities.openType("experiment");
  let el = gManagerWindow.document.getElementsByClassName("experiment-info-container")[0];
  is_element_visible(el, "Experiment info is visible on experiment tab.");
});

add_task(function* testExperimentLearnMore() {
  // Actual URL is irrelevant.
  Services.prefs.setCharPref("toolkit.telemetry.infoURL",
                             "http://mochi.test:8888/server.js");

  yield gCategoryUtilities.openType("experiment");
  let btn = gManagerWindow.document.getElementById("experiments-learn-more");

  if (!gUseInContentUI) {
    is_element_hidden(btn, "Learn more button hidden if not using in-content UI.");
    Services.prefs.clearUserPref("toolkit.telemetry.infoURL");

    return;
  }

  is_element_visible(btn, "Learn more button visible.");

  let deferred = Promise.defer();
  window.addEventListener("DOMContentLoaded", function onLoad(event) {
    info("Telemetry privacy policy window opened.");
    window.removeEventListener("DOMContentLoaded", onLoad, false);

    let browser = gBrowser.selectedTab.linkedBrowser;
    let expected = Services.prefs.getCharPref("toolkit.telemetry.infoURL");
    Assert.equal(browser.currentURI.spec, expected, "New tab should have loaded privacy policy.");
    browser.contentWindow.close();

    Services.prefs.clearUserPref("toolkit.telemetry.infoURL");

    deferred.resolve();
  }, false);

  info("Opening telemetry privacy policy.");
  EventUtils.synthesizeMouseAtCenter(btn, {}, gManagerWindow);

  yield deferred.promise;
});

add_task(function* testOpenPreferences() {
  yield gCategoryUtilities.openType("experiment");
  let btn = gManagerWindow.document.getElementById("experiments-change-telemetry");
  if (!gUseInContentUI) {
    is_element_hidden(btn, "Change telemetry button not enabled in out of window UI.");
    info("Skipping preferences open test because not using in-content UI.");
    return;
  }

  is_element_visible(btn, "Change telemetry button visible in in-content UI.");

  let deferred = Promise.defer();
  Services.obs.addObserver(function observer(prefWin, topic, data) {
    Services.obs.removeObserver(observer, "advanced-pane-loaded");

    info("Advanced preference pane opened.");

    // We want this test to fail if the preferences pane changes.
    let el = prefWin.document.getElementById("dataChoicesPanel");
    is_element_visible(el);

    prefWin.close();
    info("Closed preferences pane.");

    deferred.resolve();
  }, "advanced-pane-loaded", false);

  info("Loading preferences pane.");
  EventUtils.synthesizeMouseAtCenter(btn, {}, gManagerWindow);

  yield deferred.promise;
});

add_task(function* testButtonPresence() {
  yield gCategoryUtilities.openType("experiment");
  let item = get_addon_element(gManagerWindow, "test-experiment1@experiments.mozilla.org");
  Assert.ok(item, "Got add-on element.");

  let el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
  // Corresponds to the uninstall permission.
  is_element_visible(el, "Remove button is visible.");
  // Corresponds to lack of disable permission.
  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "disable-btn");
  is_element_hidden(el, "Disable button not visible.");
  // Corresponds to lack of enable permission.
  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "enable-btn");
  is_element_hidden(el, "Enable button not visible.");
});

// Remove the add-on we've been testing with.
add_task(function* testCleanup() {
  yield AddonTestUtils.uninstallAddonByID("test-experiment1@experiments.mozilla.org");
  // Verify some conditions, just in case.
  let addons = yield getExperimentAddons();
  Assert.equal(addons.length, 0, "No experiment add-ons are installed.");
});

// The following tests should ideally live in browser/experiments/. However,
// they rely on some of the helper functions from head.js, which can't easily
// be consumed from other directories. So, they live here.

add_task(function* testActivateExperiment() {
  if (!gExperiments) {
    info("Skipping experiments test because that feature isn't available.");
    return;
  }

  gHttpServer = new HttpServer();
  gHttpServer.start(-1);
  let root = "http://localhost:" + gHttpServer.identity.primaryPort + "/";
  gHttpServer.registerPathHandler("/manifest", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.write(JSON.stringify({
      "version": 1,
      "experiments": [
        {
          id: "experiment-1",
          xpiURL: TESTROOT + "addons/browser_experiment1.xpi",
          xpiHash: "IRRELEVANT",
          startTime: Date.now() / 1000 - 3600,
          endTime: Date.now() / 1000 + 3600,
          maxActiveSeconds: 600,
          appName: [Services.appinfo.name],
          channel: [gExperiments._policy.updatechannel()],
        },
      ],
    }));
    response.processAsync();
    response.finish();
  });

  Services.prefs.setBoolPref("experiments.manifest.cert.checkAttributes", false);
  gSavedManifestURI = Services.prefs.getCharPref("experiments.manifest.uri");
  Services.prefs.setCharPref("experiments.manifest.uri", root + "manifest");

  // We need to remove the cache file to help ensure consistent state.
  yield OS.File.remove(gExperiments._cacheFilePath);

  Services.prefs.setBoolPref("experiments.enabled", true);

  info("Initializing experiments service.");
  yield gExperiments.init();
  info("Experiments service finished first run.");

  // Check conditions, just to be sure.
  let experiments = yield gExperiments.getExperiments();
  Assert.equal(experiments.length, 0, "No experiments known to the service.");

  // This makes testing easier.
  gExperiments._policy.ignoreHashes = true;

  info("Manually updating experiments manifest.");
  yield gExperiments.updateManifest();
  info("Experiments update complete.");

  let deferred = Promise.defer();
  gHttpServer.stop(() => {
    gHttpServer = null;

    info("getting experiment by ID");
    AddonManager.getAddonByID("test-experiment1@experiments.mozilla.org", (addon) => {
      Assert.ok(addon, "Add-on installed via Experiments manager.");

      deferred.resolve();
    });
  });

  yield deferred.promise;

  Assert.ok(gCategoryUtilities.isTypeVisible, "experiment", "Experiment tab visible.");
  yield gCategoryUtilities.openType("experiment");
  let el = gManagerWindow.document.getElementsByClassName("experiment-info-container")[0];
  is_element_visible(el, "Experiment info is visible on experiment tab.");
});

add_task(function testDeactivateExperiment() {
  if (!gExperiments) {
    return;
  }

  // Fake an empty manifest to purge data from previous manifest.
  yield gExperiments._updateExperiments({
    "version": 1,
    "experiments": [],
  });

  yield gExperiments.disableExperiment("testing");

  // We should have a record of the previously-active experiment.
  let experiments = yield gExperiments.getExperiments();
  Assert.equal(experiments.length, 1, "1 experiment is known.");
  Assert.equal(experiments[0].active, false, "Experiment is not active.");

  // We should have a previous experiment in the add-ons manager.
  let deferred = Promise.defer();
  AddonManager.getAddonsByTypes(["experiment"], (addons) => {
    deferred.resolve(addons);
  });
  let addons = yield deferred.promise;
  Assert.equal(addons.length, 1, "1 experiment add-on known.");
  Assert.ok(addons[0].appDisabled, "It is a previous experiment.");
  Assert.equal(addons[0].id, "experiment-1", "Add-on ID matches expected.");

  // Verify the UI looks sane.
  // TODO remove the pane cycle once the UI refreshes automatically.
  yield gCategoryUtilities.openType("extension");

  Assert.ok(gCategoryUtilities.isTypeVisible("experiment"), "Experiment tab visible.");
  yield gCategoryUtilities.openType("experiment");

  let item = get_addon_element(gManagerWindow, "experiment-1");
  Assert.ok(item, "Got add-on element.");
  Assert.ok(!item.active, "Element should not be active.");

  // User control buttons should not be present because previous experiments
  // should have no permissions.
  let el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "remove-btn");
  is_element_hidden(el, "Remove button is not visible.");
  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "disable-btn");
  is_element_hidden(el, "Disable button is not visible.");
  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "enable-btn");
  is_element_hidden(el, "Enable button is not visible.");
  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "preferences-btn");
  is_element_hidden(el, "Preferences button is not visible.");
});

add_task(function* testCleanup() {
  if (gExperiments) {
    Services.prefs.clearUserPref("experiments.enabled");
    Services.prefs.clearUserPref("experiments.manifest.cert.checkAttributes");
    Services.prefs.setCharPref("experiments.manifest.uri", gSavedManifestURI);

    // We perform the uninit/init cycle to purge any leftover state.
    yield OS.File.remove(gExperiments._cacheFilePath);
    yield gExperiments.uninit();
    yield gExperiments.init();
  }

  // Check post-conditions.
  let addons = yield getExperimentAddons();
  Assert.equal(addons.length, 0, "No experiment add-ons are installed.");

  yield close_manager(gManagerWindow);
});
