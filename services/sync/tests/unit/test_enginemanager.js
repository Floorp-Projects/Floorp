Cu.import("resource://services-sync/engines.js");

function run_test() {
  _("We start out with a clean slate");
  let engines = Engines.getAll();
  do_check_eq(engines.length, 0);
  do_check_eq(Engines.get('dummy'), undefined);

  _("Register an engine");
  function DummyEngine() {}
  DummyEngine.prototype.name = "dummy";
  Engines.register(DummyEngine);
  let dummy = Engines.get('dummy');
  do_check_true(dummy instanceof DummyEngine);

  engines = Engines.getAll();
  do_check_eq(engines.length, 1);
  do_check_eq(engines[0], dummy);

  _("Register an already registered engine is ignored");
  Engines.register(DummyEngine);
  do_check_eq(Engines.get('dummy'), dummy);

  _("Register multiple engines in one go");
  function PetrolEngine() {}
  PetrolEngine.prototype.name = "petrol";
  function DieselEngine() {}
  DieselEngine.prototype.name = "diesel";

  Engines.register([PetrolEngine, DieselEngine]);
  let petrol = Engines.get('petrol');
  let diesel = Engines.get('diesel');
  do_check_true(petrol instanceof PetrolEngine);
  do_check_true(diesel instanceof DieselEngine);

  engines = Engines.getAll();
  do_check_eq(engines.length, 3);
  do_check_neq(engines.indexOf(petrol), -1);
  do_check_neq(engines.indexOf(diesel), -1);

  _("Retrieve multiple engines in one go");
  engines = Engines.get(["dummy", "diesel"]);
  do_check_eq(engines.length, 2);
  do_check_neq(engines.indexOf(dummy), -1);
  do_check_neq(engines.indexOf(diesel), -1);

  _("getEnabled() only returns enabled engines");
  engines = Engines.getEnabled();
  do_check_eq(engines.length, 0);

  petrol.enabled = true;
  engines = Engines.getEnabled();
  do_check_eq(engines.length, 1);
  do_check_eq(engines[0], petrol);

  dummy.enabled = true;
  diesel.enabled = true;
  engines = Engines.getEnabled();
  do_check_eq(engines.length, 3);

  _("Unregister an engine by name");
  Engines.unregister('dummy');
  do_check_eq(Engines.get('dummy'), undefined);
  engines = Engines.getAll();
  do_check_eq(engines.length, 2);
  do_check_eq(engines.indexOf(dummy), -1);

  _("Unregister an engine by value");
  // Engines.unregister() checks for instanceof Engine, so let's make one:
  function ActualEngine() {}
  ActualEngine.prototype = {__proto__: Engine.prototype,
                            name: 'actual'};
  Engines.register(ActualEngine);
  let actual = Engines.get('actual');
  do_check_true(actual instanceof ActualEngine);
  do_check_true(actual instanceof Engine);

  Engines.unregister(actual);
  do_check_eq(Engines.get('actual'), undefined);
}
