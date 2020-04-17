"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/MSTestUtils.jsm"
);

add_task(async function test_getExperimentForGroup() {
  const store = ExperimentFakes.store();
  const experiment = ExperimentFakes.experiment("foo", {
    branch: { slug: "variant", groups: ["green"] },
  });
  store.addExperiment(ExperimentFakes.experiment("bar"));
  store.addExperiment(experiment);

  Assert.equal(
    store.getExperimentForGroup("green"),
    experiment,
    "should return a matching experiment for the given group"
  );
});

add_task(async function test_hasExperimentForGroups() {
  const store = ExperimentFakes.store();
  store.addExperiment(
    ExperimentFakes.experiment("foo", {
      branch: { slug: "variant", groups: ["green"] },
    })
  );
  store.addExperiment(
    ExperimentFakes.experiment("foo2", {
      branch: { slug: "variant", groups: ["yellow", "orange"] },
    })
  );
  store.addExperiment(
    ExperimentFakes.experiment("bar_expired", {
      active: false,
      branch: { slug: "variant", groups: ["purple"] },
    })
  );
  Assert.equal(
    store.hasExperimentForGroups([]),
    false,
    "should return false if the input is an empty array"
  );

  Assert.equal(
    store.hasExperimentForGroups(["green", "blue"]),
    true,
    "should return true if there is an experiment with any of the given groups"
  );

  Assert.equal(
    store.hasExperimentForGroups(["black", "yellow"]),
    true,
    "should return true if there is one of an experiment's multiple groups matches any of the given groups"
  );

  Assert.equal(
    store.hasExperimentForGroups(["purple"]),
    false,
    "should return false if there is a non-active experiment with the given groups"
  );

  Assert.equal(
    store.hasExperimentForGroups(["blue", "red"]),
    false,
    "should return false if none of the experiments have the given groups"
  );
});

add_task(async function test_getAll_getAllActive() {
  const store = ExperimentFakes.store();
  ["foo", "bar", "baz"].forEach(slug =>
    store.addExperiment(ExperimentFakes.experiment(slug, { active: false }))
  );
  store.addExperiment(ExperimentFakes.experiment("qux", { active: true }));

  Assert.deepEqual(
    store.getAll().map(e => e.slug),
    ["foo", "bar", "baz", "qux"],
    ".getAll() should return all experiments"
  );
  Assert.deepEqual(
    store.getAllActive().map(e => e.slug),
    ["qux"],
    ".getAllActive() should return all experiments that are active"
  );
});

add_task(async function test_addExperiment() {
  const store = ExperimentFakes.store();
  const exp = ExperimentFakes.experiment("foo");
  store.addExperiment(exp);

  Assert.equal(store.get("foo"), exp, "should save experiment by slug");
});

add_task(async function test_updateExperiment() {
  const experiment = Object.freeze(
    ExperimentFakes.experiment("foo", { value: true, active: true })
  );
  const store = ExperimentFakes.store();
  store.addExperiment(experiment);
  store.updateExperiment("foo", { active: false });

  const actual = store.get("foo");
  Assert.equal(actual.active, false, "should change updated props");
  Assert.equal(actual.value, true, "should not update other props");
});
