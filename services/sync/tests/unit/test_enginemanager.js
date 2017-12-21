/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/service.js");

function PetrolEngine() {}
PetrolEngine.prototype.name = "petrol";
PetrolEngine.prototype.finalize = async function() {};

function DieselEngine() {}
DieselEngine.prototype.name = "diesel";
DieselEngine.prototype.finalize = async function() {};

function DummyEngine() {}
DummyEngine.prototype.name = "dummy";
DummyEngine.prototype.finalize = async function() {};

function ActualEngine() {}
ActualEngine.prototype = {__proto__: Engine.prototype,
                          name: "actual"};

add_task(async function test_basics() {
  _("We start out with a clean slate");

  let manager = new EngineManager(Service);

  let engines = await manager.getAll();
  Assert.equal(engines.length, 0);
  Assert.equal((await manager.get("dummy")), undefined);

  _("Register an engine");
  await manager.register(DummyEngine);
  let dummy = await manager.get("dummy");
  Assert.ok(dummy instanceof DummyEngine);

  engines = await manager.getAll();
  Assert.equal(engines.length, 1);
  Assert.equal(engines[0], dummy);

  _("Register an already registered engine is ignored");
  await manager.register(DummyEngine);
  Assert.equal((await manager.get("dummy")), dummy);

  _("Register multiple engines in one go");
  await manager.register([PetrolEngine, DieselEngine]);
  let petrol = await manager.get("petrol");
  let diesel = await manager.get("diesel");
  Assert.ok(petrol instanceof PetrolEngine);
  Assert.ok(diesel instanceof DieselEngine);

  engines = await manager.getAll();
  Assert.equal(engines.length, 3);
  Assert.notEqual(engines.indexOf(petrol), -1);
  Assert.notEqual(engines.indexOf(diesel), -1);

  _("Retrieve multiple engines in one go");
  engines = await manager.get(["dummy", "diesel"]);
  Assert.equal(engines.length, 2);
  Assert.notEqual(engines.indexOf(dummy), -1);
  Assert.notEqual(engines.indexOf(diesel), -1);

  _("getEnabled() only returns enabled engines");
  engines = await manager.getEnabled();
  Assert.equal(engines.length, 0);

  petrol.enabled = true;
  engines = await manager.getEnabled();
  Assert.equal(engines.length, 1);
  Assert.equal(engines[0], petrol);

  dummy.enabled = true;
  diesel.enabled = true;
  engines = await manager.getEnabled();
  Assert.equal(engines.length, 3);

  _("getEnabled() returns enabled engines in sorted order");
  petrol.syncPriority = 1;
  dummy.syncPriority = 2;
  diesel.syncPriority = 3;

  engines = await manager.getEnabled();

  do_check_array_eq(engines, [petrol, dummy, diesel]);

  _("Changing the priorities should change the order in getEnabled()");

  dummy.syncPriority = 4;

  engines = await manager.getEnabled();

  do_check_array_eq(engines, [petrol, diesel, dummy]);

  _("Unregister an engine by name");
  manager.unregister("dummy");
  Assert.equal((await manager.get("dummy")), undefined);
  engines = await manager.getAll();
  Assert.equal(engines.length, 2);
  Assert.equal(engines.indexOf(dummy), -1);

  _("Unregister an engine by value");
  // manager.unregister() checks for instanceof Engine, so let's make one:
  await manager.register(ActualEngine);
  let actual = await manager.get("actual");
  Assert.ok(actual instanceof ActualEngine);
  Assert.ok(actual instanceof Engine);

  manager.unregister(actual);
  Assert.equal((await manager.get("actual")), undefined);
});

