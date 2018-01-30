/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/rotaryengine.js");

add_task(async function test_processIncoming_abort() {
  _("An abort exception, raised in applyIncoming, will abort _processIncoming.");
  let engine = new RotaryEngine(Service);

  let collection = new ServerCollection();
  let id = Utils.makeGUID();
  let payload = encryptPayload({id, denomination: "Record No. " + id});
  collection.insert(id, payload);

  let server = sync_httpd_setup({
      "/1.1/foo/storage/rotary": collection.handler()
  });

  await SyncTestingInfrastructure(server);
  await generateNewKeys(Service.collectionKeys);

  _("Create some server data.");
  let meta_global = Service.recordManager.set(engine.metaURL,
                                              new WBORecord(engine.metaURL));
  meta_global.payload.engines = {rotary: {version: engine.version,
                                          syncID: engine.syncID}};
  _("Fake applyIncoming to abort.");
  engine._store.applyIncoming = async function(record) {
    let ex = {code: Engine.prototype.eEngineAbortApplyIncoming,
              cause: "Nooo"};
    _("Throwing: " + JSON.stringify(ex));
    throw ex;
  };

  _("Trying _processIncoming. It will throw after aborting.");
  let err;
  try {
    await engine._syncStartup();
    await engine._processIncoming();
  } catch (ex) {
    err = ex;
  }

  Assert.equal(err, "Nooo");
  err = undefined;

  _("Trying engine.sync(). It will abort without error.");
  try {
    // This will quietly fail.
    await engine.sync();
  } catch (ex) {
    err = ex;
  }

  Assert.equal(err, undefined);

  await promiseStopServer(server);
  Svc.Prefs.resetBranch("");
  Service.recordManager.clearCache();

  engine._tracker.clearChangedIDs();
  await engine.finalize();
});
