/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let searchIconsSchema;

add_setup(async function () {
  searchIconsSchema = await IOUtils.readJSON(
    PathUtils.join(do_get_cwd().path, "search-config-icons-schema.json")
  );
});

add_task(async function test_ui_schema_valid() {
  let uiSchema = await IOUtils.readJSON(
    PathUtils.join(do_get_cwd().path, "search-config-icons-ui-schema.json")
  );

  await checkUISchemaValid(searchIconsSchema, uiSchema);
});
