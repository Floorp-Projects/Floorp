/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test migration of application provided search engines that
 * are being renamed (bug 1899574).
 *
 * Prior to the settings file version 7, we used to index search
 * engines by name. For version 7 or later, we now index by the engine
 * identifier.
 *
 * We are renaming some of the Wikipedia engines to match their search pages.
 * For those users already on v7 or later, the name change won't matter as we
 * index by identifier.
 * For users on a version before v7, we must ensure that we migrate the engine
 * settings correctly.
 */

"use strict";

const TEST_CONFIG_V2 = [
  {
    recordType: "engine",
    identifier: "example",
    base: {
      name: "example",
      urls: {
        search: {
          base: "https://example.com",
          params: [],
          searchTermParamName: "search",
        },
      },
    },
    variants: [
      {
        environment: { allRegionsAndLocales: true },
      },
    ],
  },
  {
    recordType: "defaultEngines",
    globalDefault: "example",
    specificDefaults: [],
  },
  {
    recordType: "engineOrders",
    orders: [],
  },
];

// The important part of the names here is that they are different to the
// names in the settings file, this is to ensure the migration is correctly
// triggered.
// The engines in this list should match those in `ENGINE_ID_TO_OLD_NAME_MAP` in
// `SearchSettings`.
const ENGINE_NAME_TO_NEW_NAME_MAP = new Map([
  ["wikipedia-hy", "Վիքիպեդիա (hy)"],
  ["wikipedia-kn", "ವಿಕಿಪೀಡಿಯ (kn)"],
  ["wikipedia-lv", "Vikipēdija (lv)"],
  ["wikipedia-NO", "Wikipedia (nb)"],
  ["wikipedia-el", "Βικιπαίδεια (el)"],
  ["wikipedia-lt", "Vikipedija (lt)"],
  ["wikipedia-my", "ဝီကီပီးဒီးယား (my)"],
  ["wikipedia-pa", "ਵਿਕੀਪੀਡੀਆ (pa)"],
  ["wikipedia-pt", "Wikipédia (pt)"],
  ["wikipedia-si", "විකිපීඩියා (si)"],
  ["wikipedia-tr", "Vikipedi (tr)"],
]);

for (let [identifier, name] of ENGINE_NAME_TO_NEW_NAME_MAP.entries()) {
  TEST_CONFIG_V2.push({
    recordType: "engine",
    identifier,
    base: {
      aliases: ["config"],
      name,
      urls: {
        search: {
          base: "https://example.com",
          params: [],
          searchTermParamName: "search",
        },
      },
    },
    variants: [
      {
        environment: { allRegionsAndLocales: true },
      },
    ],
  });
}

/**
 * Loads the settings file and ensures it has not already been migrated.
 *
 * @param {string} settingsFile The settings file to load
 */
async function loadSettingsFile(settingsFile) {
  let settingsTemplate = await readJSONFile(do_get_file(settingsFile));

  Assert.less(
    settingsTemplate.version,
    7,
    "Should be a version older than when indexing engines by id was introduced"
  );
  for (let engine of settingsTemplate.engines) {
    Assert.ok(!("id" in engine));
  }

  await promiseSaveSettingsData(settingsTemplate);
}

/**
 * Test reading from search.json.mozlz4
 */
add_setup(async function () {
  await SearchTestUtils.useTestEngines("data1", null, TEST_CONFIG_V2);
  await loadSettingsFile("data/search-migration-renames.json");
});

add_task(async function test_migration_after_renames() {
  await Services.search.wrappedJSObject.reset();
  await Services.search.init();

  for (let [identifier, name] of ENGINE_NAME_TO_NEW_NAME_MAP.entries()) {
    let id = identifier.split("-");
    let engine = await Services.search.getEngineById(
      `${id[0]}@search.mozilla.org${id[1]}`
    );
    Assert.ok(engine, `Should have loaded an engine for ${identifier}`);

    Assert.deepEqual(
      engine.aliases,
      [identifier, "@config"],
      `Should have retained the correct alias for ${identifier}`
    );
    Assert.equal(
      engine.name,
      name,
      `Should have the new engine name for ${identifier}`
    );
  }
});
