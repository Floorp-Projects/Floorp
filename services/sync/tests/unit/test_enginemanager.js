/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/service.js");

function run_test() {
  _("We start out with a clean slate");

  let manager = new EngineManager(Service);

  let engines = manager.getAll();
  do_check_eq(engines.length, 0);
  do_check_eq(manager.get('dummy'), undefined);

  _("Register an engine");
  function DummyEngine() {}
  DummyEngine.prototype.name = "dummy";
  manager.register(DummyEngine);
  let dummy = manager.get('dummy');
  do_check_true(dummy instanceof DummyEngine);

  engines = manager.getAll();
  do_check_eq(engines.length, 1);
  do_check_eq(engines[0], dummy);

  _("Register an already registered engine is ignored");
  manager.register(DummyEngine);
  do_check_eq(manager.get('dummy'), dummy);

  _("Register multiple engines in one go");
  function PetrolEngine() {}
  PetrolEngine.prototype.name = "petrol";
  function DieselEngine() {}
  DieselEngine.prototype.name = "diesel";

  manager.register([PetrolEngine, DieselEngine]);
  let petrol = manager.get('petrol');
  let diesel = manager.get('diesel');
  do_check_true(petrol instanceof PetrolEngine);
  do_check_true(diesel instanceof DieselEngine);

  engines = manager.getAll();
  do_check_eq(engines.length, 3);
  do_check_neq(engines.indexOf(petrol), -1);
  do_check_neq(engines.indexOf(diesel), -1);

  _("Retrieve multiple engines in one go");
  engines = manager.get(["dummy", "diesel"]);
  do_check_eq(engines.length, 2);
  do_check_neq(engines.indexOf(dummy), -1);
  do_check_neq(engines.indexOf(diesel), -1);

  _("getEnabled() only returns enabled engines");
  engines = manager.getEnabled();
  do_check_eq(engines.length, 0);

  petrol.enabled = true;
  engines = manager.getEnabled();
  do_check_eq(engines.length, 1);
  do_check_eq(engines[0], petrol);

  dummy.enabled = true;
  diesel.enabled = true;
  engines = manager.getEnabled();
  do_check_eq(engines.length, 3);

  _("Unregister an engine by name");
  manager.unregister('dummy');
  do_check_eq(manager.get('dummy'), undefined);
  engines = manager.getAll();
  do_check_eq(engines.length, 2);
  do_check_eq(engines.indexOf(dummy), -1);

  _("Unregister an engine by value");
  // manager.unregister() checks for instanceof Engine, so let's make one:
  function ActualEngine() {}
  ActualEngine.prototype = {__proto__: Engine.prototype,
                            name: 'actual'};
  manager.register(ActualEngine);
  let actual = manager.get('actual');
  do_check_true(actual instanceof ActualEngine);
  do_check_true(actual instanceof Engine);

  manager.unregister(actual);
  do_check_eq(manager.get('actual'), undefined);
}
