Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

function BlaEngine() {
  SyncEngine.call(this, "Bla");
}
BlaEngine.prototype = {
  __proto__: SyncEngine.prototype,

  removed: false,
  removeClientData: function() {
    this.removed = true;
  }

};
Engines.register(BlaEngine);


function test_removeClientData() {
  let engine = Engines.get("bla");
  do_check_false(engine.removed);
  Service.startOver();
  do_check_true(engine.removed);
}


function run_test() {
  test_removeClientData();
}
