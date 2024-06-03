/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { FeatureManifest } = ChromeUtils.importESModule(
  "resource://nimbus/FeatureManifest.sys.mjs"
);

add_task(async function testSchemaUris() {
  for (const [featureId, featureDfn] of Object.entries(FeatureManifest)) {
    if (typeof featureDfn.schema !== "undefined") {
      info(`${featureId}: schema URI: ${featureDfn.schema.uri}`);
      try {
        const json = await fetch(featureDfn.schema.uri).then(rsp => rsp.text());
        Assert.ok(true, `${featureId}: schema fetch success`);
        JSON.parse(json);
        Assert.ok(true, `${featureId}: schema parses as JSON`);
      } catch (e) {
        throw new Error(
          `Could not fetch schema for ${featureId} at ${featureDfn.schema.uri}: ${e}`
        );
      }
    }
  }
  Assert.ok(true, "All schemas fetched successfully");
});
