/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from head_appinfo.js */
/* import-globals-from ../../../common/tests/unit/head_helpers.js */
/* import-globals-from head_errorhandler_common.js */
/* import-globals-from head_http_server.js */

// This file expects Service to be defined in the global scope when EHTestsCommon
// is used (from service.js).
/* global Service */

var { AddonTestUtils, MockAsyncShutdown } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
var { Async } = ChromeUtils.import("resource://services-common/async.js");
var { CommonUtils } = ChromeUtils.import("resource://services-common/utils.js");
var { PlacesTestUtils } = ChromeUtils.import(
  "resource://testing-common/PlacesTestUtils.jsm"
);
var { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
var { SerializableSet, Svc, Utils, getChromeWindow } = ChromeUtils.import(
  "resource://services-sync/util.js"
);
var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
var { PlacesUtils } = ChromeUtils.import(
  "resource://gre/modules/PlacesUtils.jsm"
);
var { PlacesSyncUtils } = ChromeUtils.import(
  "resource://gre/modules/PlacesSyncUtils.jsm"
);
var { ObjectUtils } = ChromeUtils.import(
  "resource://gre/modules/ObjectUtils.jsm"
);
var {
  AccountState,
  MockFxaStorageManager,
  SyncTestingInfrastructure,
  configureFxAccountIdentity,
  configureIdentity,
  encryptPayload,
  getLoginTelemetryScalar,
  makeFxAccountsInternalMock,
  makeIdentityConfig,
  promiseNamedTimer,
  promiseZeroTimer,
  sumHistogram,
  syncTestLogging,
  waitForZeroTimer,
} = ChromeUtils.import("resource://testing-common/services/sync/utils.js");
ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);

add_task(async function head_setup() {
  // Initialize logging. This will sometimes be reset by a pref reset,
  // so it's also called as part of SyncTestingInfrastructure().
  syncTestLogging();
  // If a test imports Service, make sure it is initialized first.
  if (typeof Service !== "undefined") {
    await Service.promiseInitialized;
  }
});

XPCOMUtils.defineLazyGetter(this, "SyncPingSchema", function() {
  let ns = {};
  ChromeUtils.import("resource://gre/modules/FileUtils.jsm", ns);
  ChromeUtils.import("resource://gre/modules/NetUtil.jsm", ns);
  let stream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  let schema;
  try {
    let schemaFile = do_get_file("sync_ping_schema.json");
    stream.init(
      schemaFile,
      ns.FileUtils.MODE_RDONLY,
      ns.FileUtils.PERMS_FILE,
      0
    );

    let bytes = ns.NetUtil.readInputStream(stream, stream.available());
    schema = JSON.parse(new TextDecoder().decode(bytes));
  } finally {
    stream.close();
  }

  // Allow tests to make whatever engines they want, this shouldn't cause
  // validation failure.
  schema.definitions.engine.properties.name = { type: "string" };
  return schema;
});

XPCOMUtils.defineLazyGetter(this, "SyncPingValidator", function() {
  let ns = {};
  ChromeUtils.import("resource://testing-common/ajv-4.1.1.js", ns);
  let ajv = new ns.Ajv({ async: "co*" });
  return ajv.compile(SyncPingSchema);
});

// This is needed for loadAddonTestFunctions().
var gGlobalScope = this;

function ExtensionsTestPath(path) {
  if (path[0] != "/") {
    throw Error("Path must begin with '/': " + path);
  }

  return "../../../../toolkit/mozapps/extensions/test/xpcshell" + path;
}

function webExtensionsTestPath(path) {
  if (path[0] != "/") {
    throw Error("Path must begin with '/': " + path);
  }

  return "../../../../toolkit/components/extensions/test/xpcshell" + path;
}

/**
 * Loads the WebExtension test functions by importing its test file.
 */
function loadWebExtensionTestFunctions() {
  /* import-globals-from ../../../../toolkit/components/extensions/test/xpcshell/head_sync.js */
  const path = webExtensionsTestPath("/head_sync.js");
  let file = do_get_file(path);
  let uri = Services.io.newFileURI(file);
  Services.scriptloader.loadSubScript(uri.spec, gGlobalScope);
}

/**
 * Installs an add-on from an addonInstall
 *
 * @param  install addonInstall instance to install
 */
async function installAddonFromInstall(install) {
  await install.install();

  Assert.notEqual(null, install.addon);
  Assert.notEqual(null, install.addon.syncGUID);

  return install.addon;
}

/**
 * Convenience function to install an add-on from the extensions unit tests.
 *
 * @param  file
 *         Add-on file to install.
 * @param  reconciler
 *         addons reconciler, if passed we will wait on the events to be
 *         processed before resolving
 * @return addon object that was installed
 */
async function installAddon(file, reconciler = null) {
  let install = await AddonManager.getInstallForFile(file);
  Assert.notEqual(null, install);
  const addon = await installAddonFromInstall(install);
  if (reconciler) {
    await reconciler.queueCaller.promiseCallsComplete();
  }
  return addon;
}

/**
 * Convenience function to uninstall an add-on.
 *
 * @param addon
 *        Addon instance to uninstall
 * @param reconciler
 *        addons reconciler, if passed we will wait on the events to be
 *        processed before resolving
 */
async function uninstallAddon(addon, reconciler = null) {
  const uninstallPromise = new Promise(res => {
    let listener = {
      onUninstalled(uninstalled) {
        if (uninstalled.id == addon.id) {
          AddonManager.removeAddonListener(listener);
          res(uninstalled);
        }
      },
    };
    AddonManager.addAddonListener(listener);
  });
  addon.uninstall();
  await uninstallPromise;
  if (reconciler) {
    await reconciler.queueCaller.promiseCallsComplete();
  }
}

async function generateNewKeys(collectionKeys, collections = null) {
  let wbo = await collectionKeys.generateNewKeysWBO(collections);
  let modified = new_timestamp();
  collectionKeys.setContents(wbo.cleartext, modified);
}

// Helpers for testing open tabs.
// These reflect part of the internal structure of TabEngine,
// and stub part of Service.wm.

function mockShouldSkipWindow(win) {
  return win.closed || win.mockIsPrivate;
}

function mockGetTabState(tab) {
  return tab;
}

function mockGetWindowEnumerator(url, numWindows, numTabs, indexes, moreURLs) {
  let elements = [];

  function url2entry(urlToConvert) {
    return {
      url: typeof urlToConvert == "function" ? urlToConvert() : urlToConvert,
      title: "title",
    };
  }

  for (let w = 0; w < numWindows; ++w) {
    let tabs = [];
    let win = {
      closed: false,
      mockIsPrivate: false,
      gBrowser: {
        tabs,
      },
    };
    elements.push(win);

    for (let t = 0; t < numTabs; ++t) {
      tabs.push(
        Cu.cloneInto(
          {
            index: indexes ? indexes() : 1,
            entries: (moreURLs ? [url].concat(moreURLs()) : [url]).map(
              url2entry
            ),
            attributes: {
              image: "image",
            },
            lastAccessed: 1499,
          },
          {}
        )
      );
    }
  }

  // Always include a closed window and a private window.
  elements.push({
    closed: true,
    mockIsPrivate: false,
    gBrowser: {
      tabs: [],
    },
  });

  elements.push({
    closed: false,
    mockIsPrivate: true,
    gBrowser: {
      tabs: [],
    },
  });

  return elements.values();
}

// Helper function to get the sync telemetry and add the typically used test
// engine names to its list of allowed engines.
function get_sync_test_telemetry() {
  let ns = {};
  ChromeUtils.import("resource://services-sync/telemetry.js", ns);
  let testEngines = ["rotary", "steam", "sterling", "catapult"];
  for (let engineName of testEngines) {
    ns.SyncTelemetry.allowedEngines.add(engineName);
  }
  ns.SyncTelemetry.submissionInterval = -1;
  return ns.SyncTelemetry;
}

function assert_valid_ping(record) {
  // This is called as the test harness tears down due to shutdown. This
  // will typically have no recorded syncs, and the validator complains about
  // it. So ignore such records (but only ignore when *both* shutdown and
  // no Syncs - either of them not being true might be an actual problem)
  if (record && (record.why != "shutdown" || record.syncs.length != 0)) {
    if (!SyncPingValidator(record)) {
      if (SyncPingValidator.errors.length) {
        // validation failed - using a simple |deepEqual([], errors)| tends to
        // truncate the validation errors in the output and doesn't show that
        // the ping actually was - so be helpful.
        info("telemetry ping validation failed");
        info("the ping data is: " + JSON.stringify(record, undefined, 2));
        info(
          "the validation failures: " +
            JSON.stringify(SyncPingValidator.errors, undefined, 2)
        );
        ok(
          false,
          "Sync telemetry ping validation failed - see output above for details"
        );
      }
    }
    equal(record.version, 1);
    record.syncs.forEach(p => {
      lessOrEqual(p.when, Date.now());
      if (p.devices) {
        ok(!p.devices.some(device => device.id == record.deviceID));
        equal(
          new Set(p.devices.map(device => device.id)).size,
          p.devices.length,
          "Duplicate device ids in ping devices list"
        );
      }
    });
  }
}

// Asserts that `ping` is a ping that doesn't contain any failure information
function assert_success_ping(ping) {
  ok(!!ping);
  assert_valid_ping(ping);
  ping.syncs.forEach(record => {
    ok(!record.failureReason, JSON.stringify(record.failureReason));
    equal(undefined, record.status);
    greater(record.engines.length, 0);
    for (let e of record.engines) {
      ok(!e.failureReason);
      equal(undefined, e.status);
      if (e.validation) {
        equal(undefined, e.validation.problems);
        equal(undefined, e.validation.failureReason);
      }
      if (e.outgoing) {
        for (let o of e.outgoing) {
          equal(undefined, o.failed);
          notEqual(undefined, o.sent);
        }
      }
      if (e.incoming) {
        equal(undefined, e.incoming.failed);
        equal(undefined, e.incoming.newFailed);
        notEqual(undefined, e.incoming.applied || e.incoming.reconciled);
      }
    }
  });
}

// Hooks into telemetry to validate all pings after calling.
function validate_all_future_pings() {
  let telem = get_sync_test_telemetry();
  telem.submit = assert_valid_ping;
}

function wait_for_pings(expectedPings) {
  return new Promise(resolve => {
    let telem = get_sync_test_telemetry();
    let oldSubmit = telem.submit;
    let pings = [];
    telem.submit = function(record) {
      pings.push(record);
      if (pings.length == expectedPings) {
        telem.submit = oldSubmit;
        resolve(pings);
      }
    };
  });
}

async function wait_for_ping(callback, allowErrorPings, getFullPing = false) {
  let pingsPromise = wait_for_pings(1);
  await callback();
  let [record] = await pingsPromise;
  if (allowErrorPings) {
    assert_valid_ping(record);
  } else {
    assert_success_ping(record);
  }
  if (getFullPing) {
    return record;
  }
  equal(record.syncs.length, 1);
  return record.syncs[0];
}

// Short helper for wait_for_ping
function sync_and_validate_telem(allowErrorPings, getFullPing = false) {
  return wait_for_ping(() => Service.sync(), allowErrorPings, getFullPing);
}

// Used for the (many) cases where we do a 'partial' sync, where only a single
// engine is actually synced, but we still want to ensure we're generating a
// valid ping. Returns a promise that resolves to the ping, or rejects with the
// thrown error after calling an optional callback.
async function sync_engine_and_validate_telem(
  engine,
  allowErrorPings,
  onError
) {
  let telem = get_sync_test_telemetry();
  let caughtError = null;
  // Clear out status, so failures from previous syncs won't show up in the
  // telemetry ping.
  let ns = {};
  ChromeUtils.import("resource://services-sync/status.js", ns);
  ns.Status._engines = {};
  ns.Status.partial = false;
  // Ideally we'd clear these out like we do with engines, (probably via
  // Status.resetSync()), but this causes *numerous* tests to fail, so we just
  // assume that if no failureReason or engine failures are set, and the
  // status properties are the same as they were initially, that it's just
  // a leftover.
  // This is only an issue since we're triggering the sync of just one engine,
  // without doing any other parts of the sync.
  let initialServiceStatus = ns.Status._service;
  let initialSyncStatus = ns.Status._sync;

  let oldSubmit = telem.submit;
  let submitPromise = new Promise((resolve, reject) => {
    telem.submit = function(ping) {
      telem.submit = oldSubmit;
      ping.syncs.forEach(record => {
        if (record && record.status) {
          // did we see anything to lead us to believe that something bad actually happened
          let realProblem =
            record.failureReason ||
            record.engines.some(e => {
              if (e.failureReason || e.status) {
                return true;
              }
              if (e.outgoing && e.outgoing.some(o => o.failed > 0)) {
                return true;
              }
              return e.incoming && e.incoming.failed;
            });
          if (!realProblem) {
            // no, so if the status is the same as it was initially, just assume
            // that its leftover and that we can ignore it.
            if (record.status.sync && record.status.sync == initialSyncStatus) {
              delete record.status.sync;
            }
            if (
              record.status.service &&
              record.status.service == initialServiceStatus
            ) {
              delete record.status.service;
            }
            if (!record.status.sync && !record.status.service) {
              delete record.status;
            }
          }
        }
      });
      if (allowErrorPings) {
        assert_valid_ping(ping);
      } else {
        assert_success_ping(ping);
      }
      equal(ping.syncs.length, 1);
      if (caughtError) {
        if (onError) {
          onError(ping.syncs[0], ping);
        }
        reject(caughtError);
      } else {
        resolve(ping.syncs[0]);
      }
    };
  });
  // neuter the scheduler as it interacts badly with some of the tests - the
  // engine being synced usually isn't the registered engine, so we see
  // scored incremented and not removed, which schedules unexpected syncs.
  let oldObserve = Service.scheduler.observe;
  Service.scheduler.observe = () => {};
  try {
    Svc.Obs.notify("weave:service:sync:start");
    try {
      await engine.sync();
    } catch (e) {
      caughtError = e;
    }
    if (caughtError) {
      Svc.Obs.notify("weave:service:sync:error", caughtError);
    } else {
      Svc.Obs.notify("weave:service:sync:finish");
    }
  } finally {
    Service.scheduler.observe = oldObserve;
  }
  return submitPromise;
}

// Returns a promise that resolves once the specified observer notification
// has fired.
function promiseOneObserver(topic, callback) {
  return new Promise((resolve, reject) => {
    let observer = function(subject, data) {
      Svc.Obs.remove(topic, observer);
      resolve({ subject, data });
    };
    Svc.Obs.add(topic, observer);
  });
}

async function registerRotaryEngine() {
  let { RotaryEngine } = ChromeUtils.import(
    "resource://testing-common/services/sync/rotaryengine.js"
  );
  await Service.engineManager.clear();

  await Service.engineManager.register(RotaryEngine);
  let engine = Service.engineManager.get("rotary");
  let syncID = await engine.resetLocalSyncID();
  engine.enabled = true;

  return { engine, syncID, tracker: engine._tracker };
}

// Set the validation prefs to attempt validation every time to avoid non-determinism.
function enableValidationPrefs(engines = ["bookmarks"]) {
  for (let engine of engines) {
    Svc.Prefs.set(`engine.${engine}.validation.interval`, 0);
    Svc.Prefs.set(`engine.${engine}.validation.percentageChance`, 100);
    Svc.Prefs.set(`engine.${engine}.validation.maxRecords`, -1);
    Svc.Prefs.set(`engine.${engine}.validation.enabled`, true);
  }
}

async function serverForEnginesWithKeys(users, engines, callback) {
  // Generate and store a fake default key bundle to avoid resetting the client
  // before the first sync.
  let wbo = await Service.collectionKeys.generateNewKeysWBO();
  let modified = new_timestamp();
  Service.collectionKeys.setContents(wbo.cleartext, modified);

  let allEngines = [Service.clientsEngine].concat(engines);

  let globalEngines = {};
  for (let engine of allEngines) {
    let syncID = await engine.resetLocalSyncID();
    globalEngines[engine.name] = { version: engine.version, syncID };
  }

  let contents = {
    meta: {
      global: {
        syncID: Service.syncID,
        storageVersion: STORAGE_VERSION,
        engines: globalEngines,
      },
    },
    crypto: {
      keys: encryptPayload(wbo.cleartext),
    },
  };
  for (let engine of allEngines) {
    contents[engine.name] = {};
  }

  return serverForUsers(users, contents, callback);
}

async function serverForFoo(engine, callback) {
  // The bookmarks engine *always* tracks changes, meaning we might try
  // and sync due to the bookmarks we ourselves create! Worse, because we
  // do an engine sync only, there's no locking - so we end up with multiple
  // syncs running. Neuter that by making the threshold very large.
  Service.scheduler.syncThreshold = 10000000;
  return serverForEnginesWithKeys({ foo: "password" }, engine, callback);
}

// Places notifies history observers asynchronously, so `addVisits` might return
// before the tracker receives the notification. This helper registers an
// observer that resolves once the expected notification fires.
async function promiseVisit(expectedType, expectedURI) {
  return new Promise(resolve => {
    function done(type, uri) {
      if (uri == expectedURI.spec && type == expectedType) {
        PlacesUtils.history.removeObserver(observer);
        PlacesObservers.removeListener(
          ["page-visited"],
          observer.handlePlacesEvents
        );
        resolve();
      }
    }
    let observer = {
      handlePlacesEvents(events) {
        Assert.equal(events.length, 1);
        Assert.equal(events[0].type, "page-visited");
        done("added", events[0].url);
      },
      onBeginUpdateBatch() {},
      onEndUpdateBatch() {},
      onTitleChanged() {},
      onFrecencyChanged() {},
      onManyFrecenciesChanged() {},
      onDeleteURI(uri) {
        done("removed", uri.spec);
      },
      onClearHistory() {},
      onPageChanged() {},
      onDeleteVisits() {},
    };
    PlacesUtils.history.addObserver(observer, false);
    PlacesObservers.addListener(["page-visited"], observer.handlePlacesEvents);
  });
}

async function addVisit(
  suffix,
  referrer = null,
  transition = PlacesUtils.history.TRANSITION_LINK
) {
  let uriString = "http://getfirefox.com/" + suffix;
  let uri = CommonUtils.makeURI(uriString);
  _("Adding visit for URI " + uriString);

  let visitAddedPromise = promiseVisit("added", uri);
  await PlacesTestUtils.addVisits({
    uri,
    visitDate: Date.now() * 1000,
    transition,
    referrer,
  });
  await visitAddedPromise;

  return uri;
}

function bookmarkNodesToInfos(nodes) {
  return nodes.map(node => {
    let info = {
      guid: node.guid,
      index: node.index,
    };
    if (node.children) {
      info.children = bookmarkNodesToInfos(node.children);
    }
    // Check orphan parent anno.
    if (PlacesUtils.annotations.itemHasAnnotation(node.id, "sync/parent")) {
      info.requestedParent = PlacesUtils.annotations.getItemAnnotation(
        node.id,
        "sync/parent"
      );
    }
    return info;
  });
}

async function assertBookmarksTreeMatches(rootGuid, expected, message) {
  let root = await PlacesUtils.promiseBookmarksTree(rootGuid, {
    includeItemIds: true,
  });
  let actual = bookmarkNodesToInfos(root.children);

  if (!ObjectUtils.deepEqual(actual, expected)) {
    _(`Expected structure for ${rootGuid}`, JSON.stringify(expected));
    _(`Actual structure for ${rootGuid}`, JSON.stringify(actual));
    throw new Assert.constructor.AssertionError({ actual, expected, message });
  }
}

function bufferedBookmarksEnabled() {
  return Services.prefs.getBoolPref(
    "services.sync.engine.bookmarks.buffer",
    false
  );
}
