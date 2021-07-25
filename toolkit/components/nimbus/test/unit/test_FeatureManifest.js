"use strict";

const { FeatureManifest } = ChromeUtils.import(
  "resource://nimbus/FeatureManifest.js"
);
const { Ajv } = ChromeUtils.import("resource://testing-common/ajv-4.1.1.js");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
Cu.importGlobalProperties(["fetch"]);

XPCOMUtils.defineLazyGetter(this, "fetchSchema", async () => {
  const response = await fetch(
    "resource://testing-common/ExperimentFeatureManifest.schema.json"
  );
  const schema = await response.json();
  if (!schema) {
    throw new Error("Failed to load NimbusSchema");
  }
  return schema.definitions.Feature;
});

add_task(async function test_feature_manifest_is_valid() {
  const ajv = new Ajv({ allErrors: true });
  const validate = ajv.compile(await fetchSchema);

  // Validate each entry in the feature manifest.
  Object.keys(FeatureManifest).forEach(featureId => {
    const valid = validate(FeatureManifest[featureId]);
    if (!valid) {
      throw new Error(
        `The manifest entry for ${featureId} is not valid in tookit/components/nimbus/NimbusFeature.js: ` +
          JSON.stringify(validate.errors, undefined, 2)
      );
    }
  });
});
