"use strict";

const { Uptake } = ChromeUtils.importESModule(
  "resource://normandy/lib/Uptake.sys.mjs"
);

const Telemetry = Services.telemetry;

add_task(async function reportRecipeSubmitsFreshness() {
  Telemetry.clearScalars();
  const recipe = { id: 17, revision_id: "12" };
  await Uptake.reportRecipe(recipe, Uptake.RECIPE_SUCCESS);
  const scalars = Telemetry.getSnapshotForKeyedScalars("main", true);
  Assert.deepEqual(scalars.parent["normandy.recipe_freshness"], { 17: 12 });
});
