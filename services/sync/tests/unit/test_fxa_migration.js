// We change this pref before anything else initializes
Services.prefs.setCharPref("identity.fxaccounts.auth.uri", "http://localhost");

// Test the FxAMigration module
Cu.import("resource://services-sync/FxaMigrator.jsm");

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
  let server = new SyncServer({onRequest});
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

add_task(async function testMigrationUnlinks() {

  // when we do a .startOver we want the new provider.
  let oldValue = Services.prefs.getBoolPref("services.sync-testing.startOverKeepIdentity");
  Services.prefs.setBoolPref("services.sync-testing.startOverKeepIdentity", false);

  do_register_cleanup(() => {
    Services.prefs.setBoolPref("services.sync-testing.startOverKeepIdentity", oldValue)
  });

  // Arrange for a legacy sync user.
  let [engine, server] = configureLegacySync();

  // Start a sync - this will cause an EOL notification which the migrator's
  // observer will notice.
  let promiseMigration = promiseOneObserver("fxa-migration:state-changed");
  let promiseStartOver = promiseOneObserver("weave:service:start-over:finish");
  _("Starting sync");
  Service.sync();
  _("Finished sync");

  await promiseStartOver;
  await promiseMigration;
  // We should have seen the observer and Sync should no longer be configured.
  Assert.ok(!Services.prefs.prefHasUserValue("services.sync.username"));
});

function run_test() {
  initTestLogging();
  do_register_cleanup(() => {
    fxaMigrator.finalize();
    Svc.Prefs.resetBranch("");
  });
  run_next_test();
}
