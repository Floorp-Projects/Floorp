Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/util.js");

add_test(function test_processIncoming_abort() {
  _("An abort exception, raised in applyIncoming, will abort _processIncoming.");
  new SyncTestingInfrastructure();
  generateNewKeys();

  let engine = new RotaryEngine();

  _("Create some server data.");
  let meta_global = Records.set(engine.metaURL, new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                          syncID: engine.syncID}};

  let collection = new ServerCollection();
  let id = Utils.makeGUID();
  let payload = encryptPayload({id: id, denomination: "Record No. " + id});
  collection.insert(id, payload);

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  _("Fake applyIncoming to abort.");
  engine._store.applyIncoming = function (record) {
    let ex = {code: Engine.prototype.eEngineAbortApplyIncoming,
              cause: "Nooo"};
    _("Throwing: " + JSON.stringify(ex));
    throw ex;
  };
  
  _("Trying _processIncoming. It will throw after aborting.");
  let err;
  try {
    engine._syncStartup();
    engine._processIncoming();
  } catch (ex) {
    err = ex;
  }

  do_check_eq(err, "Nooo");
  err = undefined;

  _("Trying engine.sync(). It will abort without error.");
  try {
    // This will quietly fail.
    engine.sync();
  } catch (ex) {
    err = ex;
  }

  do_check_eq(err, undefined);

  server.stop(run_next_test);
  Svc.Prefs.resetBranch("");
  Records.clearCache();
});

function run_test() {
  run_next_test();
}
