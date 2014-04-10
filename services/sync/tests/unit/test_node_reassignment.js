/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

_("Test that node reassignment responses are respected on all kinds of " +
  "requests.");

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/rest.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/rotaryengine.js");
Cu.import("resource://testing-common/services/sync/utils.js");

Service.engineManager.clear();

function run_test() {
  Log.repository.getLogger("Sync.AsyncResource").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.ErrorHandler").level  = Log.Level.Trace;
  Log.repository.getLogger("Sync.Resource").level      = Log.Level.Trace;
  Log.repository.getLogger("Sync.RESTRequest").level   = Log.Level.Trace;
  Log.repository.getLogger("Sync.Service").level       = Log.Level.Trace;
  Log.repository.getLogger("Sync.SyncScheduler").level = Log.Level.Trace;
  initTestLogging();

  ensureLegacyIdentityManager();

  Service.engineManager.register(RotaryEngine);

  // None of the failures in this file should result in a UI error.
  function onUIError() {
    do_throw("Errors should not be presented in the UI.");
  }
  Svc.Obs.add("weave:ui:login:error", onUIError);
  Svc.Obs.add("weave:ui:sync:error", onUIError);

  run_next_test();
}

/**
 * Emulate the following Zeus config:
 * $draining = data.get($prefix . $host . " draining");
 * if ($draining == "drain.") {
 *   log.warn($log_host_db_status . " migrating=1 (node-reassignment)" .
 *            $log_suffix);
 *   http.sendResponse("401 Node reassignment", $content_type,
 *                     '"server request: node reassignment"', "");
 * }
 */
const reassignBody = "\"server request: node reassignment\"";

// API-compatible with SyncServer handler. Bind `handler` to something to use
// as a ServerCollection handler.
function handleReassign(handler, req, resp) {
  resp.setStatusLine(req.httpVersion, 401, "Node reassignment");
  resp.setHeader("Content-Type", "application/json");
  resp.bodyOutputStream.write(reassignBody, reassignBody.length);
}

/**
 * A node assignment handler.
 */
function installNodeHandler(server, next) {
  let newNodeBody = server.baseURI;
  function handleNodeRequest(req, resp) {
    _("Client made a request for a node reassignment.");
    resp.setStatusLine(req.httpVersion, 200, "OK");
    resp.setHeader("Content-Type", "text/plain");
    resp.bodyOutputStream.write(newNodeBody, newNodeBody.length);
    Utils.nextTick(next);
  }
  let nodePath = "/user/1.0/johndoe/node/weave";
  server.server.registerPathHandler(nodePath, handleNodeRequest);
  _("Registered node handler at " + nodePath);
}

function prepareServer() {
  let deferred = Promise.defer();
  configureIdentity({username: "johndoe"}).then(() => {
    let server = new SyncServer();
    server.registerUser("johndoe");
    server.start();
    Service.serverURL = server.baseURI;
    Service.clusterURL = server.baseURI;
    do_check_eq(Service.userAPIURI, server.baseURI + "user/1.0/");
    deferred.resolve(server);
  });
  return deferred.promise;
}

function getReassigned() {
  try {
    return Services.prefs.getBoolPref("services.sync.lastSyncReassigned");
  } catch (ex if (ex.result == Cr.NS_ERROR_UNEXPECTED)) {
    return false;
  } catch (ex) {
    do_throw("Got exception retrieving lastSyncReassigned: " +
             Utils.exceptionStr(ex));
  }
}

/**
 * Make a test request to `url`, then watch the result of two syncs
 * to ensure that a node request was made.
 * Runs `between` between the two. This can be used to undo deliberate failure
 * setup, detach observers, etc.
 */
function syncAndExpectNodeReassignment(server, firstNotification, between,
                                       secondNotification, url) {
  let deferred = Promise.defer();
  function onwards() {
    let nodeFetched = false;
    function onFirstSync() {
      _("First sync completed.");
      Svc.Obs.remove(firstNotification, onFirstSync);
      Svc.Obs.add(secondNotification, onSecondSync);

      do_check_eq(Service.clusterURL, "");

      // Track whether we fetched node/weave. We want to wait for the second
      // sync to finish so that we're cleaned up for the next test, so don't
      // run_next_test in the node handler.
      nodeFetched = false;

      // Verify that the client requests a node reassignment.
      // Install a node handler to watch for these requests.
      installNodeHandler(server, function () {
        nodeFetched = true;
      });

      // Allow for tests to clean up error conditions.
      between();
    }
    function onSecondSync() {
      _("Second sync completed.");
      Svc.Obs.remove(secondNotification, onSecondSync);
      Service.scheduler.clearSyncTriggers();

      // Make absolutely sure that any event listeners are done with their work
      // before we proceed.
      waitForZeroTimer(function () {
        _("Second sync nextTick.");
        do_check_true(nodeFetched);
        Service.startOver();
        server.stop(deferred.resolve);
      });
    }

    Svc.Obs.add(firstNotification, onFirstSync);
    Service.sync();
  }

  // Make sure that it works!
  let request = new RESTRequest(url);
  request.get(function () {
    do_check_eq(request.response.status, 401);
    Utils.nextTick(onwards);
  });
  yield deferred.promise;
}

add_task(function test_momentary_401_engine() {
  _("Test a failure for engine URLs that's resolved by reassignment.");
  let server = yield prepareServer();
  let john   = server.user("johndoe");

  _("Enabling the Rotary engine.");
  let engine = Service.engineManager.get("rotary");
  engine.enabled = true;

  // We need the server to be correctly set up prior to experimenting. Do this
  // through a sync.
  let global = {syncID: Service.syncID,
                storageVersion: STORAGE_VERSION,
                rotary: {version: engine.version,
                         syncID:  engine.syncID}}
  john.createCollection("meta").insert("global", global);

  _("First sync to prepare server contents.");
  Service.sync();

  _("Setting up Rotary collection to 401.");
  let rotary = john.createCollection("rotary");
  let oldHandler = rotary.collectionHandler;
  rotary.collectionHandler = handleReassign.bind(this, undefined);

  // We want to verify that the clusterURL pref has been cleared after a 401
  // inside a sync. Flag the Rotary engine to need syncing.
  john.collection("rotary").timestamp += 1000;

  function between() {
    _("Undoing test changes.");
    rotary.collectionHandler = oldHandler;

    function onLoginStart() {
      // lastSyncReassigned shouldn't be cleared until a sync has succeeded.
      _("Ensuring that lastSyncReassigned is still set at next sync start.");
      Svc.Obs.remove("weave:service:login:start", onLoginStart);
      do_check_true(getReassigned());
    }

    _("Adding observer that lastSyncReassigned is still set on login.");
    Svc.Obs.add("weave:service:login:start", onLoginStart);
  }

  yield syncAndExpectNodeReassignment(server,
                                      "weave:service:sync:finish",
                                      between,
                                      "weave:service:sync:finish",
                                      Service.storageURL + "rotary");
});

// This test ends up being a failing fetch *after we're already logged in*.
add_task(function test_momentary_401_info_collections() {
  _("Test a failure for info/collections that's resolved by reassignment.");
  let server = yield prepareServer();

  _("First sync to prepare server contents.");
  Service.sync();

  // Return a 401 for info requests, particularly info/collections.
  let oldHandler = server.toplevelHandlers.info;
  server.toplevelHandlers.info = handleReassign;

  function undo() {
    _("Undoing test changes.");
    server.toplevelHandlers.info = oldHandler;
  }

  yield syncAndExpectNodeReassignment(server,
                                      "weave:service:sync:error",
                                      undo,
                                      "weave:service:sync:finish",
                                      Service.infoURL);
});

add_task(function test_momentary_401_storage_loggedin() {
  _("Test a failure for any storage URL, not just engine parts. " +
    "Resolved by reassignment.");
  let server = yield prepareServer();

  _("Performing initial sync to ensure we are logged in.")
  Service.sync();

  // Return a 401 for all storage requests.
  let oldHandler = server.toplevelHandlers.storage;
  server.toplevelHandlers.storage = handleReassign;

  function undo() {
    _("Undoing test changes.");
    server.toplevelHandlers.storage = oldHandler;
  }

  do_check_true(Service.isLoggedIn, "already logged in");
  yield syncAndExpectNodeReassignment(server,
                                      "weave:service:sync:error",
                                      undo,
                                      "weave:service:sync:finish",
                                      Service.storageURL + "meta/global");
});

add_task(function test_momentary_401_storage_loggedout() {
  _("Test a failure for any storage URL, not just engine parts. " +
    "Resolved by reassignment.");
  let server = yield prepareServer();

  // Return a 401 for all storage requests.
  let oldHandler = server.toplevelHandlers.storage;
  server.toplevelHandlers.storage = handleReassign;

  function undo() {
    _("Undoing test changes.");
    server.toplevelHandlers.storage = oldHandler;
  }

  do_check_false(Service.isLoggedIn, "not already logged in");
  yield syncAndExpectNodeReassignment(server,
                                      "weave:service:login:error",
                                      undo,
                                      "weave:service:sync:finish",
                                      Service.storageURL + "meta/global");
});

add_task(function test_loop_avoidance_storage() {
  _("Test that a repeated failure doesn't result in a sync loop " +
    "if node reassignment cannot resolve the failure.");

  let server = yield prepareServer();

  // Return a 401 for all storage requests.
  let oldHandler = server.toplevelHandlers.storage;
  server.toplevelHandlers.storage = handleReassign;

  let firstNotification  = "weave:service:login:error";
  let secondNotification = "weave:service:login:error";
  let thirdNotification  = "weave:service:sync:finish";

  let nodeFetched = false;
  let deferred = Promise.defer();

  // Track the time. We want to make sure the duration between the first and
  // second sync is small, and then that the duration between second and third
  // is set to be large.
  let now;

  function onFirstSync() {
    _("First sync completed.");
    Svc.Obs.remove(firstNotification, onFirstSync);
    Svc.Obs.add(secondNotification, onSecondSync);

    do_check_eq(Service.clusterURL, "");

    // We got a 401 mid-sync, and set the pref accordingly.
    do_check_true(Services.prefs.getBoolPref("services.sync.lastSyncReassigned"));

    // Track whether we fetched node/weave. We want to wait for the second
    // sync to finish so that we're cleaned up for the next test, so don't
    // run_next_test in the node handler.
    nodeFetched = false;

    // Verify that the client requests a node reassignment.
    // Install a node handler to watch for these requests.
    installNodeHandler(server, function () {
      nodeFetched = true;
    });

    // Update the timestamp.
    now = Date.now();
  }

  function onSecondSync() {
    _("Second sync completed.");
    Svc.Obs.remove(secondNotification, onSecondSync);
    Svc.Obs.add(thirdNotification, onThirdSync);

    // This sync occurred within the backoff interval.
    let elapsedTime = Date.now() - now;
    do_check_true(elapsedTime < MINIMUM_BACKOFF_INTERVAL);

    // This pref will be true until a sync completes successfully.
    do_check_true(getReassigned());

    // The timer will be set for some distant time.
    // We store nextSync in prefs, which offers us only limited resolution.
    // Include that logic here.
    let expectedNextSync = 1000 * Math.floor((now + MINIMUM_BACKOFF_INTERVAL) / 1000);
    _("Next sync scheduled for " + Service.scheduler.nextSync);
    _("Expected to be slightly greater than " + expectedNextSync);

    do_check_true(Service.scheduler.nextSync >= expectedNextSync);
    do_check_true(!!Service.scheduler.syncTimer);

    // Undo our evil scheme.
    server.toplevelHandlers.storage = oldHandler;

    // Bring the timer forward to kick off a successful sync, so we can watch
    // the pref get cleared.
    Service.scheduler.scheduleNextSync(0);
  }
  function onThirdSync() {
    Svc.Obs.remove(thirdNotification, onThirdSync);

    // That'll do for now; no more syncs.
    Service.scheduler.clearSyncTriggers();

    // Make absolutely sure that any event listeners are done with their work
    // before we proceed.
    waitForZeroTimer(function () {
      _("Third sync nextTick.");
      do_check_false(getReassigned());
      do_check_true(nodeFetched);
      Service.startOver();
      server.stop(deferred.resolve);
    });
  }

  Svc.Obs.add(firstNotification, onFirstSync);

  now = Date.now();
  Service.sync();
  yield deferred.promise;
});

add_task(function test_loop_avoidance_engine() {
  _("Test that a repeated 401 in an engine doesn't result in a sync loop " +
    "if node reassignment cannot resolve the failure.");
  let server = yield prepareServer();
  let john   = server.user("johndoe");

  _("Enabling the Rotary engine.");
  let engine = Service.engineManager.get("rotary");
  engine.enabled = true;
  let deferred = Promise.defer();

  // We need the server to be correctly set up prior to experimenting. Do this
  // through a sync.
  let global = {syncID: Service.syncID,
                storageVersion: STORAGE_VERSION,
                rotary: {version: engine.version,
                         syncID:  engine.syncID}}
  john.createCollection("meta").insert("global", global);

  _("First sync to prepare server contents.");
  Service.sync();

  _("Setting up Rotary collection to 401.");
  let rotary = john.createCollection("rotary");
  let oldHandler = rotary.collectionHandler;
  rotary.collectionHandler = handleReassign.bind(this, undefined);

  // Flag the Rotary engine to need syncing.
  john.collection("rotary").timestamp += 1000;

  function onLoginStart() {
    // lastSyncReassigned shouldn't be cleared until a sync has succeeded.
    _("Ensuring that lastSyncReassigned is still set at next sync start.");
    do_check_true(getReassigned());
  }

  function beforeSuccessfulSync() {
    _("Undoing test changes.");
    rotary.collectionHandler = oldHandler;
  }

  function afterSuccessfulSync() {
    Svc.Obs.remove("weave:service:login:start", onLoginStart);
    Service.startOver();
    server.stop(deferred.resolve);
  }

  let firstNotification  = "weave:service:sync:finish";
  let secondNotification = "weave:service:sync:finish";
  let thirdNotification  = "weave:service:sync:finish";

  let nodeFetched = false;

  // Track the time. We want to make sure the duration between the first and
  // second sync is small, and then that the duration between second and third
  // is set to be large.
  let now;

  function onFirstSync() {
    _("First sync completed.");
    Svc.Obs.remove(firstNotification, onFirstSync);
    Svc.Obs.add(secondNotification, onSecondSync);

    do_check_eq(Service.clusterURL, "");

    _("Adding observer that lastSyncReassigned is still set on login.");
    Svc.Obs.add("weave:service:login:start", onLoginStart);

    // We got a 401 mid-sync, and set the pref accordingly.
    do_check_true(Services.prefs.getBoolPref("services.sync.lastSyncReassigned"));

    // Track whether we fetched node/weave. We want to wait for the second
    // sync to finish so that we're cleaned up for the next test, so don't
    // run_next_test in the node handler.
    nodeFetched = false;

    // Verify that the client requests a node reassignment.
    // Install a node handler to watch for these requests.
    installNodeHandler(server, function () {
      nodeFetched = true;
    });

    // Update the timestamp.
    now = Date.now();
  }

  function onSecondSync() {
    _("Second sync completed.");
    Svc.Obs.remove(secondNotification, onSecondSync);
    Svc.Obs.add(thirdNotification, onThirdSync);

    // This sync occurred within the backoff interval.
    let elapsedTime = Date.now() - now;
    do_check_true(elapsedTime < MINIMUM_BACKOFF_INTERVAL);

    // This pref will be true until a sync completes successfully.
    do_check_true(getReassigned());

    // The timer will be set for some distant time.
    // We store nextSync in prefs, which offers us only limited resolution.
    // Include that logic here.
    let expectedNextSync = 1000 * Math.floor((now + MINIMUM_BACKOFF_INTERVAL) / 1000);
    _("Next sync scheduled for " + Service.scheduler.nextSync);
    _("Expected to be slightly greater than " + expectedNextSync);

    do_check_true(Service.scheduler.nextSync >= expectedNextSync);
    do_check_true(!!Service.scheduler.syncTimer);

    // Undo our evil scheme.
    beforeSuccessfulSync();

    // Bring the timer forward to kick off a successful sync, so we can watch
    // the pref get cleared.
    Service.scheduler.scheduleNextSync(0);
  }

  function onThirdSync() {
    Svc.Obs.remove(thirdNotification, onThirdSync);

    // That'll do for now; no more syncs.
    Service.scheduler.clearSyncTriggers();

    // Make absolutely sure that any event listeners are done with their work
    // before we proceed.
    waitForZeroTimer(function () {
      _("Third sync nextTick.");
      do_check_false(getReassigned());
      do_check_true(nodeFetched);
      afterSuccessfulSync();
    });
  }

  Svc.Obs.add(firstNotification, onFirstSync);

  now = Date.now();
  Service.sync();
  yield deferred.promise;
});
