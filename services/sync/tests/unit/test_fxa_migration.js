// Test the FxAMigration module
Cu.import("resource://services-sync/FxaMigrator.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://services-sync/browserid_identity.js");

// Set our username pref early so sync initializes with the legacy provider.
Services.prefs.setCharPref("services.sync.username", "foo");
// And ensure all debug messages end up being printed.
Services.prefs.setCharPref("services.sync.log.appender.dump", "Debug");

// Now import sync
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/util.js");

// And reset the username.
Services.prefs.clearUserPref("services.sync.username");

Cu.import("resource://testing-common/services/sync/utils.js");
Cu.import("resource://testing-common/services/common/logging.js");
Cu.import("resource://testing-common/services/sync/rotaryengine.js");

const FXA_USERNAME = "someone@somewhere";

// Utilities
function promiseOneObserver(topic) {
  return new Promise((resolve, reject) => {
    let observer = function(subject, topic, data) {
      Services.obs.removeObserver(observer, topic);
      resolve({ subject: subject, data: data });
    }
    Services.obs.addObserver(observer, topic, false);
  });
}

function promiseStopServer(server) {
  return new Promise((resolve, reject) => {
    server.stop(resolve);
  });
}


// Helpers
function configureLegacySync() {
  let engine = new RotaryEngine(Service);
  engine.enabled = true;
  Svc.Prefs.set("registerEngines", engine.name);
  Svc.Prefs.set("log.logger.engine.rotary", "Trace");

  let contents = {
    meta: {global: {engines: {rotary: {version: engine.version,
                                       syncID:  engine.syncID}}}},
    crypto: {},
    rotary: {}
  };

  const USER = "foo";
  const PASSPHRASE = "abcdeabcdeabcdeabcdeabcdea";

  setBasicCredentials(USER, "password", PASSPHRASE);

  let onRequest = function(request, response) {
    // ideally we'd only do this while a legacy user is configured, but WTH.
    response.setHeader("x-weave-alert", JSON.stringify({code: "soft-eol"}));
  }
  let server = new SyncServer({onRequest: onRequest});
  server.registerUser(USER, "password");
  server.createContents(USER, contents);
  server.start();

  Service.serverURL = server.baseURI;
  Service.clusterURL = server.baseURI;
  Service.identity.username = USER;
  Service._updateCachedURLs();

  Service.engineManager._engines[engine.name] = engine;

  return [engine, server];
}

function configureFxa() {
  Services.prefs.setCharPref("identity.fxaccounts.auth.uri", "http://localhost");
}

add_task(function *testMigration() {
  configureFxa();

  // when we do a .startOver we want the new provider.
  let oldValue = Services.prefs.getBoolPref("services.sync-testing.startOverKeepIdentity");
  Services.prefs.setBoolPref("services.sync-testing.startOverKeepIdentity", false);

  // disable the addons engine - this engine choice is arbitrary, but we
  // want to check it remains disabled after migration.
  Services.prefs.setBoolPref("services.sync.engine.addons", false);

  do_register_cleanup(() => {
    Services.prefs.setBoolPref("services.sync-testing.startOverKeepIdentity", oldValue)
    Services.prefs.setBoolPref("services.sync.engine.addons", true);
  });

  // No sync user - that should report no user-action necessary.
  Assert.deepEqual((yield fxaMigrator._queueCurrentUserState()), null,
                   "no user state when complete");

  // Arrange for a legacy sync user and manually bump the migrator
  let [engine, server] = configureLegacySync();

  // Check our disabling of the "addons" engine worked, and for good measure,
  // that the "passwords" engine is enabled.
  Assert.ok(!Service.engineManager.get("addons").enabled, "addons is disabled");
  Assert.ok(Service.engineManager.get("passwords").enabled, "passwords is enabled");

  // monkey-patch the migration sentinel code so we know it was called.
  let haveStartedSentinel = false;
  let origSetFxAMigrationSentinel = Service.setFxAMigrationSentinel;
  let promiseSentinelWritten = new Promise((resolve, reject) => {
    Service.setFxAMigrationSentinel = function(arg) {
      haveStartedSentinel = true;
      return origSetFxAMigrationSentinel.call(Service, arg).then(result => {
        Service.setFxAMigrationSentinel = origSetFxAMigrationSentinel;
        resolve(result);
        return result;
      });
    }
  });

  // We are now configured for legacy sync, but we aren't in an EOL state yet,
  // so should still be not waiting for a user.
  Assert.deepEqual((yield fxaMigrator._queueCurrentUserState()), null,
                   "no user state before server EOL");

  // Start a sync - this will cause an EOL notification which the migrator's
  // observer will notice.
  let promise = promiseOneObserver("fxa-migration:state-changed");
  _("Starting sync");
  Service.sync();
  _("Finished sync");

  // We should have seen the observer, so be waiting for an FxA user.
  Assert.equal((yield promise).data, fxaMigrator.STATE_USER_FXA, "now waiting for FxA.")

  // Re-calling our user-state promise should also reflect the same state.
  Assert.equal((yield fxaMigrator._queueCurrentUserState()),
               fxaMigrator.STATE_USER_FXA,
               "still waiting for FxA.");

  // arrange for an unverified FxA user.
  let config = makeIdentityConfig({username: FXA_USERNAME});
  let fxa = new FxAccounts({});
  config.fxaccount.user.email = config.username;
  delete config.fxaccount.user.verified;
  // *sob* - shouldn't need this boilerplate
  fxa.internal.currentAccountState.getCertificate = function(data, keyPair, mustBeValidUntil) {
    this.cert = {
      validUntil: fxa.internal.now() + CERT_LIFETIME,
      cert: "certificate",
    };
    return Promise.resolve(this.cert.cert);
  };

  // As soon as we set the FxA user the observers should fire and magically
  // transition.
  promise = promiseOneObserver("fxa-migration:state-changed");
  fxAccounts.setSignedInUser(config.fxaccount.user);

  let observerInfo = yield promise;
  Assert.equal(observerInfo.data,
               fxaMigrator.STATE_USER_FXA_VERIFIED,
               "now waiting for verification");
  Assert.ok(observerInfo.subject instanceof Ci.nsISupportsString,
            "email was passed to observer");
  Assert.equal(observerInfo.subject.data,
               FXA_USERNAME,
               "email passed to observer is correct");

  // should have seen the user set, so state should automatically update.
  Assert.equal((yield fxaMigrator._queueCurrentUserState()),
               fxaMigrator.STATE_USER_FXA_VERIFIED,
               "now waiting for verification");

  // Before we verify the user, fire off a sync that calls us back during
  // the sync and before it completes - this way we can ensure we do the right
  // thing in terms of blocking sync and waiting for it to complete.

  let wasWaiting = false;
  // This is a PITA as sync is pseudo-blocking.
  engine._syncFinish = function () {
    // We aren't in a generator here, so use a helper to block on promises.
    function getState() {
      let cb = Async.makeSpinningCallback();
      fxaMigrator._queueCurrentUserState().then(state => cb(null, state));
      return cb.wait();
    }
    // should still be waiting for verification.
    Assert.equal(getState(), fxaMigrator.STATE_USER_FXA_VERIFIED,
                 "still waiting for verification");

    // arrange for the user to be verified.  The fxAccount's mock story is
    // broken, so go behind its back.
    config.fxaccount.user.verified = true;
    fxAccounts.setSignedInUser(config.fxaccount.user);
    Services.obs.notifyObservers(null, ONVERIFIED_NOTIFICATION, null);

    // spinningly wait for the migrator to catch up - sync is running so
    // we should be in a 'null' user-state as there is no user-action
    // necessary.
    let cb = Async.makeSpinningCallback();
    promiseOneObserver("fxa-migration:state-changed").then(({ data: state }) => cb(null, state));
    Assert.equal(cb.wait(), null, "no user action necessary while sync completes.");

    // We must not have started writing the sentinel yet.
    Assert.ok(!haveStartedSentinel, "haven't written a sentinel yet");

    // sync should be blocked from continuing
    Assert.ok(Service.scheduler.isBlocked, "sync is blocked.")

    wasWaiting = true;
    throw ex;
  };

  _("Starting sync");
  Service.sync();
  _("Finished sync");

  // mock sync so we can ensure the final sync is scheduled with the FxA user.
  // (letting a "normal" sync complete is a PITA without mocking huge amounts
  // of FxA infra)
  let promiseFinalSync = new Promise((resolve, reject) => {
    let oldSync = Service.sync;
    Service.sync = function() {
      Service.sync = oldSync;
      resolve();
    }
  });

  Assert.ok(wasWaiting, "everything was good while sync was running.")

  // The migration is now going to run to completion.
  // sync should still be "blocked"
  Assert.ok(Service.scheduler.isBlocked, "sync is blocked.");

  // We should see the migration sentinel written and it should return true.
  Assert.ok((yield promiseSentinelWritten), "wrote the sentinel");

  // And we should see a new sync start
  yield promiseFinalSync;

  // and we should be configured for FxA
  let WeaveService = Cc["@mozilla.org/weave/service;1"]
         .getService(Components.interfaces.nsISupports)
         .wrappedJSObject;
  Assert.ok(WeaveService.fxAccountsEnabled, "FxA is enabled");
  Assert.ok(Service.identity instanceof BrowserIDManager,
            "sync is configured with the browserid_identity provider.");
  Assert.equal(Service.identity.username, config.username, "correct user configured")
  Assert.ok(!Service.scheduler.isBlocked, "sync is not blocked.")
  // and the user state should remain null.
  Assert.deepEqual((yield fxaMigrator._queueCurrentUserState()),
                   null,
                   "still no user action necessary");
  // and our engines should be in the same enabled/disabled state as before.
  Assert.ok(!Service.engineManager.get("addons").enabled, "addons is still disabled");
  Assert.ok(Service.engineManager.get("passwords").enabled, "passwords is still enabled");

  // aaaand, we are done - clean up.
  yield promiseStopServer(server);
});

// Test our tokenServer URL is set correctly given we've changed the prefname
// it uses.
add_task(function* testTokenServerOldPrefName() {
  let value = "http://custom-token-server/";
  // Set the pref we used in the past...
  Services.prefs.setCharPref("services.sync.tokenServerURI", value);
  // And make sure the new pref the value will be written to has a different
  // value.
  Assert.notEqual(Services.prefs.getCharPref("identity.sync.tokenserver.uri"), value);

  let prefs = fxaMigrator._getSentinelPrefs();
  Assert.equal(prefs["services.sync.tokenServerURI"], value);
  // check it applies correctly.
  Services.prefs.clearUserPref("services.sync.tokenServerURI");
  Assert.ok(!Services.prefs.prefHasUserValue("services.sync.tokenServerURI"));
  fxaMigrator._applySentinelPrefs(prefs);
  // We should have written the pref value to the *new* pref name.
  Assert.equal(Services.prefs.getCharPref("identity.sync.tokenserver.uri"), value);
  // And the old pref name should remain untouched.
  Assert.ok(!Services.prefs.prefHasUserValue("services.sync.tokenServerURI"));
});

add_task(function* testTokenServerNewPrefName() {
  let value = "http://token-server/";
  // Set the new pref name we now use.
  Services.prefs.setCharPref("identity.sync.tokenserver.uri", value);

  let prefs = fxaMigrator._getSentinelPrefs();
  // It should be written to the sentinel with the *old* pref name.
  Assert.equal(prefs["services.sync.tokenServerURI"], value);
  // check it applies correctly.
  Services.prefs.clearUserPref("services.sync.tokenServerURI");
  Assert.ok(!Services.prefs.prefHasUserValue("services.sync.tokenServerURI"));
  fxaMigrator._applySentinelPrefs(prefs);
  // We should have written the pref value to the new pref name.
  Assert.equal(Services.prefs.getCharPref("identity.sync.tokenserver.uri"), value);
  // And the old pref name should remain untouched.
  Assert.ok(!Services.prefs.prefHasUserValue("services.sync.tokenServerURI"));
});

function run_test() {
  initTestLogging();
  do_register_cleanup(() => {
    fxaMigrator.finalize();
    Svc.Prefs.resetBranch("");
  });
  run_next_test();
}
