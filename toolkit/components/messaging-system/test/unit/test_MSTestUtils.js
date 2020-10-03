"use strict";

const { ExperimentFakes, ExperimentTestUtils } = ChromeUtils.import(
  "resource://testing-common/MSTestUtils.jsm"
);

add_task(async function test_recipe_fake_validates() {
  const recipe = ExperimentFakes.recipe("foo");
  Assert.ok(
    await ExperimentTestUtils.validateExperiment(recipe),
    "should produce a valid experiment recipe"
  );
});
