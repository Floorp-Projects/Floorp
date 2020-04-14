"use strict";

const { ExperimentAPI } = ChromeUtils.import(
  "resource://testing-common/ExperimentAPI.jsm"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/MSTestUtils.jsm"
);

/**
 * #getExperiment
 */
add_task(async function test_getExperiment_slug() {
  const manager = ExperimentFakes.manager();
  const expected = ExperimentFakes.experiment("foo");
  manager.store.addExperiment(expected);

  Assert.equal(
    ExperimentAPI.getExperiment(
      { slug: "foo" },
      expected,
      "should return an experiment by slug"
    )
  );
});
add_task(async function test_getExperiment_group() {
  const manager = ExperimentFakes.manager();
  const expected = ExperimentFakes.experiment("foo", {
    branch: { slug: "treatment", value: { title: "hi" }, groups: ["blue"] },
  });
  manager.store.addExperiment(expected);

  Assert.equal(
    ExperimentAPI.getExperiment(
      { group: "blue" },
      expected,
      "should return an experiment by slug"
    )
  );
});

/**
 * #getValue
 */
add_task(async function test_getValue() {
  const manager = ExperimentFakes.manager();
  const value = { title: "hi" };
  const expected = ExperimentFakes.experiment("foo", {
    branch: { slug: "treatment", value },
  });
  manager.store.addExperiment(expected);

  Assert.deepEqual(
    ExperimentAPI.getValue(
      { slug: "foo" },
      value,
      "should return an experiment value by slug"
    )
  );

  Assert.deepEqual(
    ExperimentAPI.getValue(
      { slug: "doesnotexist" },
      undefined,
      "should return undefined if the experiment is not found"
    )
  );
});
