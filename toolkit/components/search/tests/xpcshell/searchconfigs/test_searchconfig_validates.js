/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  JsonSchema: "resource://gre/modules/JsonSchema.sys.mjs",
  SearchEngineSelectorOld:
    "resource://gre/modules/SearchEngineSelectorOld.sys.mjs",
});

/**
 * Checks to see if a value is an object or not.
 *
 * @param {*} value
 *   The value to check.
 * @returns {boolean}
 */
function isObject(value) {
  return value != null && typeof value == "object" && !Array.isArray(value);
}

/**
 * This function modifies the schema to prevent allowing additional properties
 * on objects. This is used to enforce that the schema contains everything that
 * we deliver via the search configuration.
 *
 * These checks are not enabled in-product, as we want to allow older versions
 * to keep working if we add new properties for whatever reason.
 *
 * @param {object} section
 *   The section to check to see if an additionalProperties flag should be added.
 */
function disallowAdditionalProperties(section) {
  // It is generally acceptable for new properties to be added to the
  // configuration as older builds will ignore them.
  //
  // As a result, we only check for new properties on nightly builds, and this
  // avoids us having to uplift schema changes. This also helps preserve the
  // schemas as documentation of "what was supported in this version".
  if (!AppConstants.NIGHTLY_BUILD) {
    return;
  }

  // If the section is a `oneOf` section, avoid the additionalProperties check.
  // Otherwise, the validator expects all properties of any `oneOf` item to be
  // present.
  if (isObject(section)) {
    if (section.properties && !("recordType" in section.properties)) {
      section.additionalProperties = false;
    }
    if ("then" in section) {
      section.then.additionalProperties = false;
    }
  }

  for (let value of Object.values(section)) {
    if (isObject(value)) {
      disallowAdditionalProperties(value);
    } else if (Array.isArray(value)) {
      for (let item of value) {
        disallowAdditionalProperties(item);
      }
    }
  }
}

/**
 * Asserts the remote setting collection validates against the schema.
 *
 * @param {object} options
 *   The options for the assertion.
 * @param {string} options.collectionName
 *   The name of the collection under validation.
 * @param {object[]} options.collectionData
 *   The collection data to validate.
 * @param {string[]} [options.ignoreFields=[]]
 *   A list of fields to ignore in the collection data, e.g. where remote
 *   settings itself adds extra fields. `schema`, `id`, and `last_modified` are
 *   always ignored.
 * @param {Function} [options.extraAssertsFn]
 *   An optional function to run additional assertions on each entry in the
 *   collection.
 * @param {Function} options.getEntryId
 *   A function to get the identifier for each entry in the collection.
 */
async function assertSearchConfigValidates({
  collectionName,
  collectionData,
  ignoreFields = [],
  extraAssertsFn,
  getEntryId,
}) {
  let schema = await IOUtils.readJSON(
    PathUtils.join(do_get_cwd().path, `${collectionName}-schema.json`)
  );

  disallowAdditionalProperties(schema);
  let validator = new JsonSchema.Validator(schema);

  for (let entry of collectionData) {
    // Records in Remote Settings contain additional properties independent of
    // the schema. Hence, we don't want to validate their presence.
    for (let field of [...ignoreFields, "schema", "id", "last_modified"]) {
      delete entry[field];
    }

    let result = validator.validate(entry);
    let message = `Should validate ${getEntryId(entry)}`;
    if (!result.valid) {
      message += `:\n${JSON.stringify(result.errors, null, 2)}`;
    }
    Assert.ok(result.valid, message);

    extraAssertsFn?.(entry);
  }
}

add_setup(async function () {
  updateAppInfo({ ID: "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}" });
});

add_task(async function test_search_config_validates_to_schema_v1() {
  let selector = new SearchEngineSelectorOld(() => {});

  await assertSearchConfigValidates({
    collectionName: "search-config",
    collectionData: await selector.getEngineConfiguration(),
    getEntryId: entry => entry.webExtension.id,
  });
});

add_task(async function test_search_config_override_validates_to_schema_v1() {
  let selector = new SearchEngineSelectorOld(() => {});

  await assertSearchConfigValidates({
    collectionName: "search-config-overrides",
    collectionData: await selector.getEngineConfigurationOverrides(),
    getEntryId: entry => entry.telemetryId,
  });
});

add_task(
  { skip_if: () => !SearchUtils.newSearchConfigEnabled },
  async function test_search_config_validates_to_schema() {
    delete SearchUtils.newSearchConfigEnabled;
    SearchUtils.newSearchConfigEnabled = true;

    let selector = new SearchEngineSelector(() => {});

    await assertSearchConfigValidates({
      collectionName: "search-config-v2",
      collectionData: await selector.getEngineConfiguration(),
      getEntryId: entry => entry.identifier,
      extraAssertsFn: entry => {
        // All engine objects should have the base URL defined for each entry in
        // entry.base.urls.
        // Unfortunately this is difficult to enforce in the schema as it would
        // need a `required` field that works across multiple levels.
        if (entry.recordType == "engine") {
          for (let urlEntry of Object.values(entry.base.urls)) {
            Assert.ok(
              urlEntry.base,
              "Should have a base url for every URL defined on the top-level base object."
            );
          }
        }
      },
    });
  }
);

add_task(
  { skip_if: () => !SearchUtils.newSearchConfigEnabled },
  async function test_search_config_valid_partner_codes() {
    delete SearchUtils.newSearchConfigEnabled;
    SearchUtils.newSearchConfigEnabled = true;

    let selector = new SearchEngineSelector(() => {});

    for (let entry of await selector.getEngineConfiguration()) {
      if (entry.recordType == "engine") {
        for (let variant of entry.variants) {
          if (
            "partnerCode" in variant &&
            "distributions" in variant.environment
          ) {
            Assert.ok(
              variant.telemetrySuffix,
              `${entry.identifier} should have a telemetrySuffix when a distribution is specified with a partnerCode.`
            );
          }
        }
      }
    }
  }
);

add_task(
  { skip_if: () => !SearchUtils.newSearchConfigEnabled },
  async function test_search_config_override_validates_to_schema() {
    let selector = new SearchEngineSelector(() => {});

    await assertSearchConfigValidates({
      collectionName: "search-config-overrides-v2",
      collectionData: await selector.getEngineConfigurationOverrides(),
      getEntryId: entry => entry.identifier,
    });
  }
);

add_task(async function test_search_config_icons_validates_to_schema() {
  let searchIcons = RemoteSettings("search-config-icons");

  await assertSearchConfigValidates({
    collectionName: "search-config-icons",
    collectionData: await searchIcons.get(),
    ignoreFields: ["attachment"],
    getEntryId: entry => entry.engineIdentifiers[0],
  });
});

add_task(async function test_search_default_override_allowlist_validates() {
  let allowlist = RemoteSettings("search-default-override-allowlist");

  await assertSearchConfigValidates({
    collectionName: "search-default-override-allowlist",
    collectionData: await allowlist.get(),
    ignoreFields: ["attachment"],
    getEntryId: entry => entry.engineName || entry.thirdPartyId,
  });
});
