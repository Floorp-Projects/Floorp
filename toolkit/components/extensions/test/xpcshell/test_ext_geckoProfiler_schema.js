"use strict";

add_task(async function() {
  const acceptedExtensionIdsPref =
    "extensions.geckoProfiler.acceptedExtensionIds";
  Services.prefs.setCharPref(
    acceptedExtensionIdsPref,
    "profilertest@mozilla.com"
  );

  let extension = ExtensionTestUtils.loadExtension({
    background: () => {
      browser.test.sendMessage(
        "features",
        Object.values(browser.geckoProfiler.ProfilerFeature)
      );
    },
    manifest: {
      permissions: ["geckoProfiler"],
      applications: {
        gecko: {
          id: "profilertest@mozilla.com",
        },
      },
    },
  });

  await extension.startup();
  let acceptedFeatures = await extension.awaitMessage("features");
  await extension.unload();

  Services.prefs.clearUserPref(acceptedExtensionIdsPref);

  const allFeaturesAcceptedByProfiler = Services.profiler.GetAllFeatures();
  ok(
    allFeaturesAcceptedByProfiler.length >= 2,
    "Either we've massively reduced the profiler's feature set, or something is wrong."
  );

  // Check that the list of available values in the ProfilerFeature enum
  // matches the list of features supported by the profiler.
  for (const feature of allFeaturesAcceptedByProfiler) {
    ok(
      acceptedFeatures.includes(feature),
      `The schema of the geckoProfiler.start() method should accept the "${feature}" feature.`
    );
  }
  for (const feature of acceptedFeatures) {
    ok(
      // Bug 1594566 - ignore Responsiveness until the extension is updated
      allFeaturesAcceptedByProfiler.includes(feature) ||
        feature == "responsiveness",
      `The schema of the geckoProfiler.start() method mentions a "${feature}" feature which is not supported by the profiler.`
    );
  }
});
