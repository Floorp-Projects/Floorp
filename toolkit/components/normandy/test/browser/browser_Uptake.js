"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://normandy/lib/Uptake.jsm", this);

const Telemetry = Services.telemetry;

add_task(async function reportRecipeSubmitsFreshness() {
  Telemetry.clearScalars();
  const recipe = { id: 17, revision_id: "12" };
  await Uptake.reportRecipe(recipe, Uptake.RECIPE_SUCCESS);
  const scalars = Telemetry.getSnapshotForKeyedScalars("main", true);
  Assert.deepEqual(scalars.parent["normandy.recipe_freshness"], { 17: 12 });
});
