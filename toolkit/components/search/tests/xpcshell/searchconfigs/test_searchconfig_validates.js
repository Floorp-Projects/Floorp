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

let searchConfigSchemaV1;
let searchConfigSchema;

add_setup(async function () {
  searchConfigSchemaV1 = await IOUtils.readJSON(
    PathUtils.join(do_get_cwd().path, "search-engine-config-schema.json")
  );
  searchConfigSchema = await IOUtils.readJSON(
    PathUtils.join(do_get_cwd().path, "search-engine-config-v2-schema.json")
  );
});

async function checkSearchConfigValidates(schema, searchConfig) {
  disallowAdditionalProperties(schema);
  let validator = new JsonSchema.Validator(schema);

  for (let entry of searchConfig) {
    // Records in Remote Settings contain additional properties independent of
    // the schema. Hence, we don't want to validate their presence.
    delete entry.schema;
    delete entry.id;
    delete entry.last_modified;

    let result = validator.validate(entry);
    // entry.webExtension.id supports search-config v1.
    let message = `Should validate ${
      entry.identifier ?? entry.recordType ?? entry.webExtension.id
    }`;
    if (!result.valid) {
      message += `:\n${JSON.stringify(result.errors, null, 2)}`;
    }
    Assert.ok(result.valid, message);

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
  }
}

async function checkUISchemaValid(configSchema, uiSchema) {
  for (let key of Object.keys(configSchema.properties)) {
    Assert.ok(
      uiSchema["ui:order"].includes(key),
      `Should have ${key} listed at the top-level of the ui schema`
    );
  }
}

add_task(async function test_search_config_validates_to_schema_v1() {
  let selector = new SearchEngineSelectorOld(() => {});
  let searchConfig = await selector.getEngineConfiguration();

  await checkSearchConfigValidates(searchConfigSchemaV1, searchConfig);
});

add_task(async function test_ui_schema_valid_v1() {
  let uiSchema = await IOUtils.readJSON(
    PathUtils.join(do_get_cwd().path, "search-engine-config-ui-schema.json")
  );

  await checkUISchemaValid(searchConfigSchemaV1, uiSchema);
});

add_task(async function test_search_config_validates_to_schema() {
  delete SearchUtils.newSearchConfigEnabled;
  SearchUtils.newSearchConfigEnabled = true;

  let selector = new SearchEngineSelector(() => {});
  let searchConfig = await selector.getEngineConfiguration();

  await checkSearchConfigValidates(searchConfigSchema, searchConfig);
});

add_task(async function test_ui_schema_valid() {
  let uiSchema = await IOUtils.readJSON(
    PathUtils.join(do_get_cwd().path, "search-engine-config-v2-ui-schema.json")
  );

  await checkUISchemaValid(searchConfigSchema, uiSchema);
});
