/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/Promise.jsm", this);

var {AddonManagerTesting} = Components.utils.import("resource://testing-common/AddonManagerTesting.jsm", {});
var {HttpServer} = Components.utils.import("resource://testing-common/httpd.js", {});

var gManagerWindow;
var gCategoryUtilities;
var gExperiments;
var gHttpServer;

var gSavedManifestURI;
var gIsEnUsLocale;

const SEC_IN_ONE_DAY = 24 * 60 * 60;
const MS_IN_ONE_DAY  = SEC_IN_ONE_DAY * 1000;

function getExperimentAddons() {
  return new Promise(resolve => {
    AddonManager.getAddonsByTypes(["experiment"], (addons) => {
      resolve(addons);
    });
  });
}

function patchPolicy(policy, data) {
  for (let key of Object.keys(data)) {
    Object.defineProperty(policy, key, {
      value: data[key],
      writable: true,
    });
  }
}

function defineNow(policy, time) {
  patchPolicy(policy, { now: () => new Date(time) });
}

function openDetailsView(aId) {
  let item = get_addon_element(gManagerWindow, aId);
  Assert.ok(item, "Should have got add-on element.");
  is_element_visible(item, "Add-on element should be visible.");

  EventUtils.synthesizeMouseAtCenter(item, { clickCount: 1 }, gManagerWindow);
  EventUtils.synthesizeMouseAtCenter(item, { clickCount: 2 }, gManagerWindow);

  return new Promise(resolve => {
    wait_for_view_load(gManagerWindow, resolve);
  });
}

function clickRemoveButton(addonElement) {
  let btn = gManagerWindow.document.getAnonymousElementByAttribute(addonElement, "anonid", "remove-btn");
  if (!btn) {
    return Promise.reject();
  }

  EventUtils.synthesizeMouseAtCenter(btn, { clickCount: 1 }, gManagerWindow);
  let deferred = Promise.defer();
  setTimeout(deferred.resolve, 0);
  return deferred;
}

function clickUndoButton(addonElement) {
  let btn = gManagerWindow.document.getAnonymousElementByAttribute(addonElement, "anonid", "undo-btn");
  if (!btn) {
    return Promise.reject();
  }

  EventUtils.synthesizeMouseAtCenter(btn, { clickCount: 1 }, gManagerWindow);
  let deferred = Promise.defer();
  setTimeout(deferred.resolve, 0);
  return deferred;
}

add_task(async function initializeState() {
  gManagerWindow = await open_manager();
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("experiments.enabled");
    Services.prefs.clearUserPref("toolkit.telemetry.enabled");
    if (gHttpServer) {
      gHttpServer.stop(() => {});
      if (gSavedManifestURI !== undefined) {
        Services.prefs.setCharPref("experments.manifest.uri", gSavedManifestURI);
      }
    }
    if (gExperiments) {
      let tmp = {};
      Cu.import("resource:///modules/experiments/Experiments.jsm", tmp);
      gExperiments._policy = new tmp.Experiments.Policy();
    }
  });

  gIsEnUsLocale = Services.locale.getAppLocaleAsLangTag() == "en-US";

  // The Experiments Manager will interfere with us by preventing installs
  // of experiments it doesn't know about. We remove it from the equation
  // because here we are only concerned with core Addon Manager operation,
  // not the superset Experiments Manager has imposed.
  if ("@mozilla.org/browser/experiments-service;1" in Components.classes) {
    let tmp = {};
    Cu.import("resource:///modules/experiments/Experiments.jsm", tmp);
    // There is a race condition between XPCOM service initialization and
    // this test running. We have to initialize the instance first, then
    // uninitialize it to prevent this.
    gExperiments = tmp.Experiments.instance();
    await gExperiments._mainTask;
    await gExperiments.uninit();
  }
});

// On an empty profile with no experiments, the experiment category
// should be hidden.
add_task(async function testInitialState() {
  Assert.ok(gCategoryUtilities.get("experiment", false), "Experiment tab is defined.");
  Assert.ok(!gCategoryUtilities.isTypeVisible("experiment"), "Experiment tab hidden by default.");
});

add_task(async function testExperimentInfoNotVisible() {
  await gCategoryUtilities.openType("extension");
  let el = gManagerWindow.document.getElementsByClassName("experiment-info-container")[0];
  is_element_hidden(el, "Experiment info not visible on other types.");
});

// If we have an active experiment, we should see the experiments tab
// and that tab should have some messages.
add_task(async function testActiveExperiment() {
  let addon = await install_addon("addons/browser_experiment1.xpi");

  Assert.ok(addon.userDisabled, "Add-on is disabled upon initial install.");
  Assert.equal(addon.isActive, false, "Add-on is not active.");

  Assert.ok(gCategoryUtilities.isTypeVisible("experiment"), "Experiment tab visible.");

  await gCategoryUtilities.openType("experiment");
  let el = gManagerWindow.document.getElementsByClassName("experiment-info-container")[0];
  is_element_visible(el, "Experiment info is visible on experiment tab.");
});

add_task(async function testExperimentLearnMore() {
  await gCategoryUtilities.openType("experiment");
  let btn = gManagerWindow.document.getElementById("experiments-learn-more");

  is_element_visible(btn, "Learn more button visible.");

  // Actual URL is irrelevant.
  let expected = "http://mochi.test:8888/server.js";
  Services.prefs.setCharPref("toolkit.telemetry.infoURL", expected);

  info("Opening telemetry privacy policy.");
  let loadPromise = BrowserTestUtils.waitForNewTab(gBrowser, expected);
  EventUtils.synthesizeMouseAtCenter(btn, {}, gManagerWindow);
  await loadPromise;

  Services.prefs.clearUserPref("toolkit.telemetry.infoURL");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testOpenPreferences() {
  await gCategoryUtilities.openType("experiment");
  let btn = gManagerWindow.document.getElementById("experiments-change-telemetry");

  is_element_visible(btn, "Change telemetry button visible in in-content UI.");

  let deferred = Promise.defer();
  Services.obs.addObserver(function observer(prefWin, topic, data) {
    Services.obs.removeObserver(observer, "privacy-pane-loaded");
    info("Privacy preference pane opened.");
    executeSoon(function() {
      // We want this test to fail if the preferences pane changes,
      // but we can't check if the data-choices button is visible
      // since it is only in the DOM when MOZ_TELEMETRY_REPORTING=1.
      let el = prefWin.document.getElementById("dataCollectionCategory");
      is_element_visible(el);

      prefWin.close();
      info("Closed preferences pane.");

      deferred.resolve();
    });
  }, "privacy-pane-loaded");

  info("Loading preferences pane.");
  // We need to focus before synthesizing the mouse event (bug 1240052) as
  // synthesizeMouseAtCenter currently only synthesizes the mouse in the child process.
  // This can cause some subtle differences if the child isn't focused.
  await SimpleTest.promiseFocus();
  await BrowserTestUtils.synthesizeMouseAtCenter("#experiments-change-telemetry", {},
                                                 gBrowser.selectedBrowser);

  await deferred.promise;
});

add_task(async function testButtonPresence() {
  await gCategoryUtilities.openType("experiment");
  let item = get_addon_element(gManagerWindow, "test-experiment1@experiments.mozilla.org");
  Assert.ok(item, "Got add-on element.");
  item.parentNode.ensureElementIsVisible(item);

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
add_task(async function testCleanup() {
  await AddonManagerTesting.uninstallAddonByID("test-experiment1@experiments.mozilla.org");
  // Verify some conditions, just in case.
  let addons = await getExperimentAddons();
  Assert.equal(addons.length, 0, "No experiment add-ons are installed.");
});

// The following tests should ideally live in browser/experiments/. However,
// they rely on some of the helper functions from head.js, which can't easily
// be consumed from other directories. So, they live here.

add_task(async function testActivateExperiment() {
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

  gSavedManifestURI = Services.prefs.getCharPref("experiments.manifest.uri");
  Services.prefs.setCharPref("experiments.manifest.uri", root + "manifest");

  // We need to remove the cache file to help ensure consistent state.
  await OS.File.remove(gExperiments._cacheFilePath);

  Services.prefs.setBoolPref("toolkit.telemetry.enabled", true);
  Services.prefs.setBoolPref("experiments.enabled", true);

  info("Initializing experiments service.");
  await gExperiments.init();
  info("Experiments service finished first run.");

  // Check conditions, just to be sure.
  let experiments = await gExperiments.getExperiments();
  Assert.equal(experiments.length, 0, "No experiments known to the service.");

  // This makes testing easier.
  gExperiments._policy.ignoreHashes = true;

  info("Manually updating experiments manifest.");
  await gExperiments.updateManifest();
  info("Experiments update complete.");

  await new Promise(resolve => {
    gHttpServer.stop(() => {
      gHttpServer = null;

      info("getting experiment by ID");
      AddonManager.getAddonByID("test-experiment1@experiments.mozilla.org", (addon) => {
        Assert.ok(addon, "Add-on installed via Experiments manager.");

        resolve();
      });
    });

  });

  Assert.ok(gCategoryUtilities.isTypeVisible, "experiment", "Experiment tab visible.");
  await gCategoryUtilities.openType("experiment");
  let el = gManagerWindow.document.getElementsByClassName("experiment-info-container")[0];
  is_element_visible(el, "Experiment info is visible on experiment tab.");
});

add_task(async function testDeactivateExperiment() {
  if (!gExperiments) {
    return;
  }

  // Fake an empty manifest to purge data from previous manifest.
  await gExperiments._updateExperiments({
    "version": 1,
    "experiments": [],
  });

  await gExperiments.disableExperiment("testing");

  // We should have a record of the previously-active experiment.
  let experiments = await gExperiments.getExperiments();
  Assert.equal(experiments.length, 1, "1 experiment is known.");
  Assert.equal(experiments[0].active, false, "Experiment is not active.");

  // We should have a previous experiment in the add-ons manager.
  let addons = await new Promise(resolve => {
    AddonManager.getAddonsByTypes(["experiment"], (addons) => {
      resolve(addons);
    });
  });
  Assert.equal(addons.length, 1, "1 experiment add-on known.");
  Assert.ok(addons[0].appDisabled, "It is a previous experiment.");
  Assert.equal(addons[0].id, "experiment-1", "Add-on ID matches expected.");

  // Verify the UI looks sane.

  Assert.ok(gCategoryUtilities.isTypeVisible("experiment"), "Experiment tab visible.");
  let item = get_addon_element(gManagerWindow, "experiment-1");
  Assert.ok(item, "Got add-on element.");
  Assert.ok(!item.active, "Element should not be active.");
  item.parentNode.ensureElementIsVisible(item);

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

add_task(async function testActivateRealExperiments() {
  if (!gExperiments) {
    info("Skipping experiments test because that feature isn't available.");
    return;
  }

  await gExperiments._updateExperiments({
    "version": 1,
    "experiments": [
      {
        id: "experiment-2",
        xpiURL: TESTROOT + "addons/browser_experiment1.xpi",
        xpiHash: "IRRELEVANT",
        startTime: Date.now() / 1000 - 3600,
        endTime: Date.now() / 1000 + 3600,
        maxActiveSeconds: 600,
        appName: [Services.appinfo.name],
        channel: [gExperiments._policy.updatechannel()],
      },
    ],
  });
  await gExperiments._run();

  // Check the active experiment.

  let item = get_addon_element(gManagerWindow, "test-experiment1@experiments.mozilla.org");
  Assert.ok(item, "Got add-on element.");
  item.parentNode.ensureElementIsVisible(item);

  let el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "experiment-state");
  is_element_visible(el, "Experiment state label should be visible.");
  if (gIsEnUsLocale) {
    Assert.equal(el.value, "Active");
  }

  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "experiment-time");
  is_element_visible(el, "Experiment time label should be visible.");
  if (gIsEnUsLocale) {
    Assert.equal(el.value, "Less than a day remaining");
  }

  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "error-container");
  is_element_hidden(el, "error-container should be hidden.");
  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "warning-container");
  is_element_hidden(el, "warning-container should be hidden.");
  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "pending-container");
  is_element_hidden(el, "pending-container should be hidden.");
  let { version } = await get_tooltip_info(item);
  Assert.equal(version, undefined, "version should be hidden.");
  el = item.ownerDocument.getAnonymousElementByAttribute(item, "class", "disabled-postfix");
  is_element_hidden(el, "disabled-postfix should be hidden.");
  el = item.ownerDocument.getAnonymousElementByAttribute(item, "class", "update-postfix");
  is_element_hidden(el, "update-postfix should be hidden.");
  el = item.ownerDocument.getAnonymousElementByAttribute(item, "class", "experiment-bullet");
  is_element_visible(el, "experiment-bullet should be visible.");

  // Check the previous experiment.

  item = get_addon_element(gManagerWindow, "experiment-1");
  Assert.ok(item, "Got add-on element.");
  item.parentNode.ensureElementIsVisible(item);

  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "experiment-state");
  is_element_visible(el, "Experiment state label should be visible.");
  if (gIsEnUsLocale) {
    Assert.equal(el.value, "Complete");
  }

  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "experiment-time");
  is_element_visible(el, "Experiment time label should be visible.");
  if (gIsEnUsLocale) {
    Assert.equal(el.value, "Less than a day ago");
  }

  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "error-container");
  is_element_hidden(el, "error-container should be hidden.");
  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "warning-container");
  is_element_hidden(el, "warning-container should be hidden.");
  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "pending-container");
  is_element_hidden(el, "pending-container should be hidden.");
  ({ version } = await get_tooltip_info(item));
  Assert.equal(version, undefined, "version should be hidden.");
  el = item.ownerDocument.getAnonymousElementByAttribute(item, "class", "disabled-postfix");
  is_element_hidden(el, "disabled-postfix should be hidden.");
  el = item.ownerDocument.getAnonymousElementByAttribute(item, "class", "update-postfix");
  is_element_hidden(el, "update-postfix should be hidden.");
  el = item.ownerDocument.getAnonymousElementByAttribute(item, "class", "experiment-bullet");
  is_element_visible(el, "experiment-bullet should be visible.");

  // Install an "older" experiment.

  await gExperiments.disableExperiment("experiment-2");

  let now = Date.now();
  let fakeNow = now - 5 * MS_IN_ONE_DAY;
  defineNow(gExperiments._policy, fakeNow);

  await gExperiments._updateExperiments({
    "version": 1,
    "experiments": [
      {
        id: "experiment-3",
        xpiURL: TESTROOT + "addons/browser_experiment1.xpi",
        xpiHash: "IRRELEVANT",
        startTime: fakeNow / 1000 - SEC_IN_ONE_DAY,
        endTime: now / 1000 + 10 * SEC_IN_ONE_DAY,
        maxActiveSeconds: 100 * SEC_IN_ONE_DAY,
        appName: [Services.appinfo.name],
        channel: [gExperiments._policy.updatechannel()],
      },
    ],
  });
  await gExperiments._run();

  // Check the active experiment.

  item = get_addon_element(gManagerWindow, "test-experiment1@experiments.mozilla.org");
  Assert.ok(item, "Got add-on element.");
  item.parentNode.ensureElementIsVisible(item);

  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "experiment-state");
  is_element_visible(el, "Experiment state label should be visible.");
  if (gIsEnUsLocale) {
    Assert.equal(el.value, "Active");
  }

  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "experiment-time");
  is_element_visible(el, "Experiment time label should be visible.");
  if (gIsEnUsLocale) {
    Assert.equal(el.value, "10 days remaining");
  }

  // Disable it and check it's previous experiment entry.

  await gExperiments.disableExperiment("experiment-3");

  item = get_addon_element(gManagerWindow, "experiment-3");
  Assert.ok(item, "Got add-on element.");
  item.parentNode.ensureElementIsVisible(item);

  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "experiment-state");
  is_element_visible(el, "Experiment state label should be visible.");
  if (gIsEnUsLocale) {
    Assert.equal(el.value, "Complete");
  }

  el = item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "experiment-time");
  is_element_visible(el, "Experiment time label should be visible.");
  if (gIsEnUsLocale) {
    Assert.equal(el.value, "5 days ago");
  }
});

add_task(async function testDetailView() {
  if (!gExperiments) {
    info("Skipping experiments test because that feature isn't available.");
    return;
  }

  defineNow(gExperiments._policy, Date.now());
  await gExperiments._updateExperiments({
    "version": 1,
    "experiments": [
      {
        id: "experiment-4",
        xpiURL: TESTROOT + "addons/browser_experiment1.xpi",
        xpiHash: "IRRELEVANT",
        startTime: Date.now() / 1000 - 3600,
        endTime: Date.now() / 1000 + 3600,
        maxActiveSeconds: 600,
        appName: [Services.appinfo.name],
        channel: [gExperiments._policy.updatechannel()],
      },
    ],
  });
  await gExperiments._run();

  // Check active experiment.

  await openDetailsView("test-experiment1@experiments.mozilla.org");

  let el = gManagerWindow.document.getElementById("detail-experiment-state");
  is_element_visible(el, "Experiment state label should be visible.");
  if (gIsEnUsLocale) {
    Assert.equal(el.value, "Active");
  }

  el = gManagerWindow.document.getElementById("detail-experiment-time");
  is_element_visible(el, "Experiment time label should be visible.");
  if (gIsEnUsLocale) {
    Assert.equal(el.value, "Less than a day remaining");
  }

  el = gManagerWindow.document.getElementById("detail-version");
  is_element_hidden(el, "detail-version should be hidden.");
  el = gManagerWindow.document.getElementById("detail-creator");
  is_element_hidden(el, "detail-creator should be hidden.");
  el = gManagerWindow.document.getElementById("detail-experiment-bullet");
  is_element_visible(el, "experiment-bullet should be visible.");

  // Check previous experiment.

  await gCategoryUtilities.openType("experiment");
  await openDetailsView("experiment-3");

  el = gManagerWindow.document.getElementById("detail-experiment-state");
  is_element_visible(el, "Experiment state label should be visible.");
  if (gIsEnUsLocale) {
    Assert.equal(el.value, "Complete");
  }

  el = gManagerWindow.document.getElementById("detail-experiment-time");
  is_element_visible(el, "Experiment time label should be visible.");
  if (gIsEnUsLocale) {
    Assert.equal(el.value, "5 days ago");
  }

  el = gManagerWindow.document.getElementById("detail-version");
  is_element_hidden(el, "detail-version should be hidden.");
  el = gManagerWindow.document.getElementById("detail-creator");
  is_element_hidden(el, "detail-creator should be hidden.");
  el = gManagerWindow.document.getElementById("detail-experiment-bullet");
  is_element_visible(el, "experiment-bullet should be visible.");
});

add_task(async function testRemoveAndUndo() {
  if (!gExperiments) {
    info("Skipping experiments test because that feature isn't available.");
    return;
  }

  await gCategoryUtilities.openType("experiment");

  let addon = get_addon_element(gManagerWindow, "test-experiment1@experiments.mozilla.org");
  Assert.ok(addon, "Got add-on element.");

  await clickRemoveButton(addon);
  addon.parentNode.ensureElementIsVisible(addon);

  let el = gManagerWindow.document.getAnonymousElementByAttribute(addon, "class", "pending");
  is_element_visible(el, "Uninstall undo information should be visible.");

  await clickUndoButton(addon);
  addon = get_addon_element(gManagerWindow, "test-experiment1@experiments.mozilla.org");
  Assert.ok(addon, "Got add-on element.");
});

add_task(async function testCleanup() {
  if (gExperiments) {
    Services.prefs.clearUserPref("experiments.enabled");
    Services.prefs.setCharPref("experiments.manifest.uri", gSavedManifestURI);

    // We perform the uninit/init cycle to purge any leftover state.
    await OS.File.remove(gExperiments._cacheFilePath);
    await gExperiments.uninit();
    await gExperiments.init();

    Services.prefs.clearUserPref("toolkit.telemetry.enabled");
  }

  // Check post-conditions.
  let addons = await getExperimentAddons();
  Assert.equal(addons.length, 0, "No experiment add-ons are installed.");

  await close_manager(gManagerWindow);
});
