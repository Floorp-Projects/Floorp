"use strict";

const { ExperimentAPI } = ChromeUtils.import(
  "resource://messaging-system/experiments/ExperimentAPI.jsm"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/MSTestUtils.jsm"
);
const { FileTestUtils } = ChromeUtils.import(
  "resource://testing-common/FileTestUtils.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

/**
 * #getExperiment
 */
add_task(async function test_getExperiment_slug() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const expected = ExperimentFakes.experiment("foo");

  await manager.onStartup();

  sandbox.stub(ExperimentAPI, "_store").get(() => ExperimentFakes.childStore());

  manager.store.addExperiment(expected);

  // Wait to sync to child
  await TestUtils.waitForCondition(
    () => ExperimentAPI.getExperiment({ slug: "foo" }),
    "Wait for child to sync"
  );

  Assert.deepEqual(
    ExperimentAPI.getExperiment({ slug: "foo" }),
    expected,
    "should return an experiment by slug"
  );

  sandbox.restore();
});

add_task(async function test_getExperiment_group() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const expected = ExperimentFakes.experiment("foo", {
    branch: { slug: "treatment", value: { title: "hi" }, groups: ["blue"] },
  });

  await manager.onStartup();

  sandbox.stub(ExperimentAPI, "_store").get(() => ExperimentFakes.childStore());

  manager.store.addExperiment(expected);

  // Wait to sync to child
  await TestUtils.waitForCondition(
    () => ExperimentAPI.getExperiment({ group: "blue" }),
    "Wait for child to sync"
  );

  Assert.deepEqual(
    ExperimentAPI.getExperiment({ group: "blue" }),
    expected,
    "should return an experiment by slug"
  );

  sandbox.restore();
});

/**
 * #getValue
 */
add_task(async function test_getValue() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const value = { title: "hi" };
  const expected = ExperimentFakes.experiment("foo", {
    branch: { slug: "treatment", value },
  });

  await manager.onStartup();

  sandbox.stub(ExperimentAPI, "_store").get(() => ExperimentFakes.childStore());

  manager.store.addExperiment(expected);

  await TestUtils.waitForCondition(
    () => ExperimentAPI.getExperiment({ slug: "foo" }),
    "Wait for child to sync"
  );

  Assert.deepEqual(
    ExperimentAPI.getValue({ slug: "foo" }),
    value,
    "should return an experiment value by slug"
  );

  Assert.equal(
    ExperimentAPI.getValue({ slug: "doesnotexist" }),
    undefined,
    "should return undefined if the experiment is not found"
  );

  sandbox.restore();
});
