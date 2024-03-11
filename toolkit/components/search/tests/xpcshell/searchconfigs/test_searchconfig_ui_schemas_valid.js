/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let schemas = [
  ["search-config-schema.json", "search-config-ui-schema.json"],
  ["search-config-v2-schema.json", "search-config-v2-ui-schema.json"],
  ["search-config-icons-schema.json", "search-config-icons-ui-schema.json"],
  [
    "search-config-overrides-schema.json",
    "search-config-overrides-ui-schema.json",
  ],
  [
    "search-config-overrides-v2-schema.json",
    "search-config-overrides-v2-ui-schema.json",
  ],
  [
    "search-default-override-allowlist-schema.json",
    "search-default-override-allowlist-ui-schema.json",
  ],
];

add_task(async function test_ui_schemas_valid() {
  for (let [schema, uiSchema] of schemas) {
    info(`Validating ${uiSchema} has every top-level from ${schema}`);
    let schemaData = await IOUtils.readJSON(
      PathUtils.join(do_get_cwd().path, schema)
    );
    let uiSchemaData = await IOUtils.readJSON(
      PathUtils.join(do_get_cwd().path, uiSchema)
    );

    await checkUISchemaValid(schemaData, uiSchemaData);
  }
});
