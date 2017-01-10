/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/async.js");
Cu.import("resource://testing-common/services/common/utils.js");
Cu.import("resource://testing-common/PlacesTestUtils.jsm");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, 'SyncPingSchema', function() {
  let ns = {};
  Cu.import("resource://gre/modules/FileUtils.jsm", ns);
  let stream = Cc["@mozilla.org/network/file-input-stream;1"]
               .createInstance(Ci.nsIFileInputStream);
  let jsonReader = Cc["@mozilla.org/dom/json;1"]
                   .createInstance(Components.interfaces.nsIJSON);
  let schema;
  try {
    let schemaFile = do_get_file("sync_ping_schema.json");
    stream.init(schemaFile, ns.FileUtils.MODE_RDONLY, ns.FileUtils.PERMS_FILE, 0);
    schema = jsonReader.decodeFromStream(stream, stream.available());
  } finally {
    stream.close();
  }

  // Allow tests to make whatever engines they want, this shouldn't cause
  // validation failure.
  schema.definitions.engine.properties.name = { type: "string" };
  return schema;
});

XPCOMUtils.defineLazyGetter(this, 'SyncPingValidator', function() {
  let ns = {};
  Cu.import("resource://testing-common/ajv-4.1.1.js", ns);
  let ajv = new ns.Ajv({ async: "co*" });
  return ajv.compile(SyncPingSchema);
});

var provider = {
  getFile(prop, persistent) {
    persistent.value = true;
    switch (prop) {
      case "ExtPrefDL":
        return [Services.dirsvc.get("CurProcD", Ci.nsIFile)];
      default:
        throw Cr.NS_ERROR_FAILURE;
    }
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDirectoryServiceProvider])
};
Services.dirsvc.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);

// This is needed for loadAddonTestFunctions().
var gGlobalScope = this;

function ExtensionsTestPath(path) {
  if (path[0] != "/") {
    throw Error("Path must begin with '/': " + path);
  }

  return "../../../../toolkit/mozapps/extensions/test/xpcshell" + path;
}

/**
 * Loads the AddonManager test functions by importing its test file.
 *
 * This should be called in the global scope of any test file needing to
 * interface with the AddonManager. It should only be called once, or the
 * universe will end.
 */
function loadAddonTestFunctions() {
  const path = ExtensionsTestPath("/head_addons.js");
  let file = do_get_file(path);
  let uri = Services.io.newFileURI(file);
  Services.scriptloader.loadSubScript(uri.spec, gGlobalScope);
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
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
  const path = webExtensionsTestPath("/head_sync.js");
  let file = do_get_file(path);
  let uri = Services.io.newFileURI(file);
  Services.scriptloader.loadSubScript(uri.spec, gGlobalScope);
}

function getAddonInstall(name) {
  let f = do_get_file(ExtensionsTestPath("/addons/" + name + ".xpi"));
  let cb = Async.makeSyncCallback();
  AddonManager.getInstallForFile(f, cb);

  return Async.waitForSyncCallback(cb);
}

/**
 * Obtains an addon from the add-on manager by id.
 *
 * This is merely a synchronous wrapper.
 *
 * @param  id
 *         ID of add-on to fetch
 * @return addon object on success or undefined or null on failure
 */
function getAddonFromAddonManagerByID(id) {
   let cb = Async.makeSyncCallback();
   AddonManager.getAddonByID(id, cb);
   return Async.waitForSyncCallback(cb);
}

/**
 * Installs an add-on synchronously from an addonInstall
 *
 * @param  install addonInstall instance to install
 */
function installAddonFromInstall(install) {
  let cb = Async.makeSyncCallback();
  let listener = {onInstallEnded: cb};
  AddonManager.addInstallListener(listener);
  install.install();
  Async.waitForSyncCallback(cb);
  AddonManager.removeAddonListener(listener);

  do_check_neq(null, install.addon);
  do_check_neq(null, install.addon.syncGUID);

  return install.addon;
}

/**
 * Convenience function to install an add-on from the extensions unit tests.
 *
 * @param  name
 *         String name of add-on to install. e.g. test_install1
 * @return addon object that was installed
 */
function installAddon(name) {
  let install = getAddonInstall(name);
  do_check_neq(null, install);
  return installAddonFromInstall(install);
}

/**
 * Convenience function to uninstall an add-on synchronously.
 *
 * @param addon
 *        Addon instance to uninstall
 */
function uninstallAddon(addon) {
  let cb = Async.makeSyncCallback();
  let listener = {onUninstalled(uninstalled) {
    if (uninstalled.id == addon.id) {
      AddonManager.removeAddonListener(listener);
      cb(uninstalled);
    }
  }};

  AddonManager.addAddonListener(listener);
  addon.uninstall();
  Async.waitForSyncCallback(cb);
}

function generateNewKeys(collectionKeys, collections = null) {
  let wbo = collectionKeys.generateNewKeysWBO(collections);
  let modified = new_timestamp();
  collectionKeys.setContents(wbo.cleartext, modified);
}

// Helpers for testing open tabs.
// These reflect part of the internal structure of TabEngine,
// and stub part of Service.wm.

function mockShouldSkipWindow(win) {
  return win.closed ||
         win.mockIsPrivate;
}

function mockGetTabState(tab) {
  return tab;
}

function mockGetWindowEnumerator(url, numWindows, numTabs, indexes, moreURLs) {
  let elements = [];

  function url2entry(url) {
    return {
      url: ((typeof url == "function") ? url() : url),
      title: "title"
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
      tabs.push(TestingUtils.deepCopy({
        index: indexes ? indexes() : 1,
        entries: (moreURLs ? [url].concat(moreURLs()) : [url]).map(url2entry),
        attributes: {
          image: "image"
        },
        lastAccessed: 1499
      }));
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

  return {
    hasMoreElements() {
      return elements.length;
    },
    getNext() {
      return elements.shift();
    },
  };
}

// Helper that allows checking array equality.
function do_check_array_eq(a1, a2) {
  do_check_eq(a1.length, a2.length);
  for (let i = 0; i < a1.length; ++i) {
    do_check_eq(a1[i], a2[i]);
  }
}

// Helper function to get the sync telemetry and add the typically used test
// engine names to its list of allowed engines.
function get_sync_test_telemetry() {
  let ns = {};
  Cu.import("resource://services-sync/telemetry.js", ns);
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
        do_print("telemetry ping validation failed");
        do_print("the ping data is: " + JSON.stringify(record, undefined, 2));
        do_print("the validation failures: " + JSON.stringify(SyncPingValidator.errors, undefined, 2));
        ok(false, "Sync telemetry ping validation failed - see output above for details");
      }
    }
    equal(record.version, 1);
    record.syncs.forEach(p => {
      lessOrEqual(p.when, Date.now());
      if (p.devices) {
        ok(!p.devices.some(device => device.id == record.deviceID));
        equal(new Set(p.devices.map(device => device.id)).size,
              p.devices.length, "Duplicate device ids in ping devices list");
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

function wait_for_ping(callback, allowErrorPings, getFullPing = false) {
  return new Promise(resolve => {
    let telem = get_sync_test_telemetry();
    let oldSubmit = telem.submit;
    telem.submit = function(record) {
      telem.submit = oldSubmit;
      if (allowErrorPings) {
        assert_valid_ping(record);
      } else {
        assert_success_ping(record);
      }
      if (getFullPing) {
        resolve(record);
      } else {
        equal(record.syncs.length, 1);
        resolve(record.syncs[0]);
      }
    };
    callback();
  });
}

// Short helper for wait_for_ping
function sync_and_validate_telem(allowErrorPings, getFullPing = false) {
  return wait_for_ping(() => Service.sync(), allowErrorPings, getFullPing);
}

// Used for the (many) cases where we do a 'partial' sync, where only a single
// engine is actually synced, but we still want to ensure we're generating a
// valid ping. Returns a promise that resolves to the ping, or rejects with the
// thrown error after calling an optional callback.
function sync_engine_and_validate_telem(engine, allowErrorPings, onError) {
  return new Promise((resolve, reject) => {
    let telem = get_sync_test_telemetry();
    let caughtError = null;
    // Clear out status, so failures from previous syncs won't show up in the
    // telemetry ping.
    let ns = {};
    Cu.import("resource://services-sync/status.js", ns);
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
    telem.submit = function(ping) {
      telem.submit = oldSubmit;
      ping.syncs.forEach(record => {
        if (record && record.status) {
          // did we see anything to lead us to believe that something bad actually happened
          let realProblem = record.failureReason || record.engines.some(e => {
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
            if (record.status.service && record.status.service == initialServiceStatus) {
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
    }
    Svc.Obs.notify("weave:service:sync:start");
    try {
      engine.sync();
    } catch (e) {
      caughtError = e;
    }
    if (caughtError) {
      Svc.Obs.notify("weave:service:sync:error", caughtError);
    } else {
      Svc.Obs.notify("weave:service:sync:finish");
    }
  });
}

// Returns a promise that resolves once the specified observer notification
// has fired.
function promiseOneObserver(topic, callback) {
  return new Promise((resolve, reject) => {
    let observer = function(subject, data) {
      Svc.Obs.remove(topic, observer);
      resolve({ subject, data });
    }
    Svc.Obs.add(topic, observer)
  });
}

function promiseStopServer(server) {
  return new Promise(resolve => server.stop(resolve));
}

function promiseNextTick() {
  return new Promise(resolve => {
    Utils.nextTick(resolve);
  });
}
// Avoid an issue where `client.name2` containing unicode characters causes
// a number of tests to fail, due to them assuming that we do not need to utf-8
// encode or decode data sent through the mocked server (see bug 1268912).
Utils.getDefaultDeviceName = function() {
  return "Test device name";
};


