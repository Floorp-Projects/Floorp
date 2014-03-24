/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/stages/enginesync.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/engines/passwords.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://testing-common/services/sync/utils.js");

function run_test() {
  initTestLogging("Trace");
  run_next_test();
}

add_test(function test_simple() {
  ensureLegacyIdentityManager();
  // Stub fxAccountsEnabled
  let xpcs = Cc["@mozilla.org/weave/service;1"]
             .getService(Components.interfaces.nsISupports)
             .wrappedJSObject;
  let fxaEnabledGetter = xpcs.__lookupGetter__("fxAccountsEnabled");
  xpcs.__defineGetter__("fxAccountsEnabled", () => true);

  // Stub mpEnabled.
  let mpEnabledF = Utils.mpEnabled;
  let mpEnabled = false;
  Utils.mpEnabled = function() mpEnabled;

  let manager = Service.engineManager;

  Service.engineManager.register(PasswordEngine);
  let engine = Service.engineManager.get("passwords");
  let wipeCount = 0;
  let engineWipeServerF = engine.wipeServer;
  engine.wipeServer = function() {
    ++wipeCount;
  }

  // A server for the metadata.
  let server  = new SyncServer();
  let johndoe = server.registerUser("johndoe", "password");
  johndoe.createContents({
    meta: {global: {engines: {passwords: {version: engine.version,
                                          syncID: engine.syncID}}}},
    crypto: {},
    clients: {}
  });
  server.start();
  setBasicCredentials("johndoe", "password", "abcdeabcdeabcdeabcdeabcdea");
  Service.serverURL = server.baseURI;
  Service.clusterURL = server.baseURI;

  let engineSync = new EngineSynchronizer(Service);
  engineSync._log.level = Log.Level.Trace;

  function assertEnabled(expected, message) {
    Assert.strictEqual(engine.enabled, expected, message);
    // The preference *must* reflect the actual state.
    Assert.strictEqual(Svc.Prefs.get("engine." + engine.prefName), expected,
                       message + " (pref should match enabled state)");
  }

  try {
    assertEnabled(true, "password engine should be enabled by default")
    let engineMeta = Service.recordManager.get(engine.metaURL);
    // This engine should be in the meta/global
    Assert.notStrictEqual(engineMeta.payload.engines[engine.name], undefined,
                          "The engine should appear in the metadata");
    Assert.ok(!engineMeta.changed, "the metadata for the password engine hasn't changed");

    // (pretend to) enable a master-password
    mpEnabled = true;
    // The password engine should be locally disabled...
    assertEnabled(false, "if mp is locked the engine should be disabled");
    // ...but not declined.
    Assert.ok(!manager.isDeclined("passwords"), "password engine is not declined");
    // Next time a sync would happen, we call _updateEnabledEngines(), which
    // would remove the engine from the metadata - call that now.
    engineSync._updateEnabledEngines();
    // The global meta should no longer list the engine.
    engineMeta = Service.recordManager.get(engine.metaURL);
    Assert.strictEqual(engineMeta.payload.engines[engine.name], undefined,
                       "The engine should have vanished");
    // And we should have wiped the server data.
    Assert.strictEqual(wipeCount, 1, "wipeServer should have been called");

    // Now simulate an incoming meta/global indicating the engine should be
    // enabled.  We should fail to actually enable it - the pref should remain
    // false and we wipe the server for anything another device might have
    // stored.
    let meta = {
      payload: {
        engines: {
          "passwords": {"version":1,"syncID":"yfBi2v7PpFO2"},
        },
      },
    };
    engineSync._updateEnabledFromMeta(meta, 3, manager);
    Assert.strictEqual(wipeCount, 2, "wipeServer should have been called");
    Assert.ok(!manager.isDeclined("passwords"), "password engine is not declined");
    assertEnabled(false, "engine still not enabled locally");

    // Let's turn the MP off - but *not* re-enable it locally.
    mpEnabled = false;
    // Just disabling the MP isn't enough to force it back to enabled.
    assertEnabled(false, "engine still not enabled locally");
    // Another incoming metadata record with the engine enabled should cause
    // it to be enabled locally.
    meta = {
      payload: {
        engines: {
          "passwords": 1,
        },
      },
    };
    engineSync._updateEnabledFromMeta(meta, 3, manager);
    Assert.strictEqual(wipeCount, 2, "wipeServer should *not* have been called again");
    Assert.ok(!manager.isDeclined("passwords"), "password engine is not declined");
    // It should be enabled locally.
    assertEnabled(true, "engine now enabled locally");
    // Next time a sync starts it should magically re-appear in our meta/global
    engine._syncStartup();
    //engineSync._updateEnabledEngines();
    engineMeta = Service.recordManager.get(engine.metaURL);
    Assert.equal(engineMeta.payload.engines[engine.name].version, engine.version,
                 "The engine should re-appear in the metadata");
  } finally {
    // restore the damage we did above...
    engine.wipeServer = engineWipeServerF;
    engine._store.wipe();
    // Un-stub mpEnabled and fxAccountsEnabled
    Utils.mpEnabled = mpEnabledF;
    xpcs.__defineGetter__("fxAccountsEnabled", fxaEnabledGetter);
    server.stop(run_next_test);
  }
});
