/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test initializing from the search settings.
 */

"use strict";

const legacyUseSavedOrderPrefName =
  SearchUtils.BROWSER_SEARCH_PREF + "useDBForOrder";

var settingsTemplate;

/**
 * Test reading from search.json.mozlz4
 */
add_task(async function setup() {
  Services.prefs
    .getDefaultBranch(SearchUtils.BROWSER_SEARCH_PREF + "param.")
    .setCharPref("test", "expected");

  await SearchTestUtils.useTestEngines("data1");
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

async function loadSettingsFile(settingsFile, setVersion) {
  settingsTemplate = await readJSONFile(do_get_file(settingsFile));
  if (setVersion) {
    settingsTemplate.version = SearchUtils.SETTINGS_VERSION;
  }

  delete settingsTemplate.visibleDefaultEngines;

  await promiseSaveSettingsData(settingsTemplate);
}

/**
 * Start the search service and confirm the engine properties match the expected values.
 *
 * @param {string} settingsFile
 *   The path to the settings file to use.
 * @param {boolean} setVersion
 *   True if to set the version in the copied settings file.
 * @param {boolean} expectedUseDBValue
 *   The value expected for the `useSavedOrder` metadata attribute.
 */
async function checkLoadSettingProperties(
  settingsFile,
  setVersion,
  expectedUseDBValue
) {
  info("init search service");
  let ss = Services.search.wrappedJSObject;

  await loadSettingsFile(settingsFile, setVersion);

  const settingsFileWritten = promiseAfterSettings();

  await ss.reset();
  await Services.search.init();

  await settingsFileWritten;

  let engines = await ss.getEngines();

  Assert.equal(
    engines[0].name,
    "engine1",
    "Should have loaded the correct first engine"
  );
  Assert.equal(engines[0].alias, "testAlias", "Should have set the alias");
  Assert.equal(engines[0].hidden, false, "Should have not hidden the engine");
  Assert.equal(
    engines[1].name,
    "engine2",
    "Should have loaded the correct second engine"
  );
  Assert.equal(engines[1].alias, "", "Should have not set the alias");
  Assert.equal(engines[1].hidden, true, "Should have hidden the engine");

  // The extra engine is the second in the list.
  isSubObjectOf(EXPECTED_ENGINE.engine, engines[2]);

  let engineFromSS = ss.getEngineByName(EXPECTED_ENGINE.engine.name);
  Assert.ok(!!engineFromSS);
  isSubObjectOf(EXPECTED_ENGINE.engine, engineFromSS);

  Assert.equal(
    engineFromSS.getSubmission("foo").uri.spec,
    "http://www.google.com/search?q=foo",
    "Should have the correct URL with no mozparams"
  );

  Assert.equal(
    ss._settings.getMetaDataAttribute("useSavedOrder"),
    expectedUseDBValue,
    "Should have set the useSavedOrder metadata correctly."
  );

  removeSettingsFile();
}

add_task(async function test_legacy_setting_engine_properties() {
  Services.prefs.setBoolPref(legacyUseSavedOrderPrefName, true);

  await checkLoadSettingProperties("data/search-legacy.json", false, true);

  Assert.ok(
    !Services.prefs.prefHasUserValue(legacyUseSavedOrderPrefName),
    "Should have cleared the legacy pref."
  );
});

add_task(async function test_current_setting_engine_properties() {
  await checkLoadSettingProperties("data/search.json", true, false);
});

add_task(async function test_settings_metadata_properties() {
  let ss = Services.search.wrappedJSObject;

  await loadSettingsFile("data/search.json");

  const settingsFileWritten = promiseAfterSettings();
  await ss.reset();
  await Services.search.init();

  await settingsFileWritten;

  let metaDataProperties = [
    "locale",
    "region",
    "channel",
    "experiment",
    "distroID",
  ];

  for (let name of metaDataProperties) {
    Assert.notEqual(
      ss._settings.getMetaDataAttribute(`${name}`),
      undefined,
      `Search settings should have ${name} property defined.`
    );
  }

  removeSettingsFile();
});

add_task(async function test_settings_write_when_settings_changed() {
  let ss = Services.search.wrappedJSObject;
  await loadSettingsFile("data/search.json");

  const settingsFileWritten = promiseAfterSettings();
  await ss.reset();
  await Services.search.init();
  await settingsFileWritten;

  Assert.ok(
    ss._settings.isCurrentAndCachedSettingsEqual(),
    "Settings and cached settings should be the same after search service initializaiton."
  );

  const settingsFileWritten2 = promiseAfterSettings();
  ss._settings.setMetaDataAttribute("value", "test");

  Assert.ok(
    !ss._settings.isCurrentAndCachedSettingsEqual(),
    "Settings should differ from cached settings after a new attribute is set."
  );

  await settingsFileWritten2;
  info("Settings write complete");

  Assert.ok(
    ss._settings.isCurrentAndCachedSettingsEqual(),
    "Settings and cached settings should be the same after new attribte on settings is written."
  );

  removeSettingsFile();
});

add_task(async function test_set_and_get_engine_metadata_attribute() {
  let ss = Services.search.wrappedJSObject;
  await loadSettingsFile("data/search.json");

  const settingsFileWritten = promiseAfterSettings();
  await ss.reset();
  await Services.search.init();
  await settingsFileWritten;

  let engines = await ss.getEngines();
  const settingsFileWritten2 = promiseAfterSettings();
  ss._settings.setEngineMetaDataAttribute(engines[0].name, "value", "test");
  await settingsFileWritten2;

  Assert.equal(
    "test",
    ss._settings.getEngineMetaDataAttribute(engines[0].name, "value"),
    `${engines[0].name}'s metadata property "value" should be set as "test" after calling getEngineMetaDataAttribute.`
  );

  let userSettings = await ss._settings.get();
  let engine = userSettings.engines.find(e => e._name == engines[0].name);

  Assert.equal(
    "test",
    engine._metaData.value,
    `${engines[0].name}'s metadata property "value" should be set as "test" from settings file.`
  );

  removeSettingsFile();
});

add_task(
  async function test_settings_write_prevented_when_settings_unchanged() {
    let ss = Services.search.wrappedJSObject;
    await loadSettingsFile("data/search.json");

    const settingsFileWritten = promiseAfterSettings();
    await ss.reset();
    await Services.search.init();
    await settingsFileWritten;

    Assert.ok(
      ss._settings.isCurrentAndCachedSettingsEqual(),
      "Settings and cached settings should be the same after search service initializaiton."
    );

    // Update settings.
    const settingsFileWritten2 = promiseAfterSettings();
    ss._settings.setMetaDataAttribute("value", "test");

    Assert.ok(
      !ss._settings.isCurrentAndCachedSettingsEqual(),
      "Settings should differ from cached settings after a new attribute is set."
    );
    await settingsFileWritten2;

    // Set the same attribute as before to ensure there was no change.
    // Settings write should be prevented.
    let promiseWritePrevented = SearchTestUtils.promiseSearchNotification(
      "write-prevented-when-settings-unchanged"
    );
    ss._settings.setMetaDataAttribute("value", "test");

    Assert.ok(
      ss._settings.isCurrentAndCachedSettingsEqual(),
      "Settings and cached settings should be the same."
    );
    await promiseWritePrevented;

    removeSettingsFile();
  }
);

/**
 * Test that the JSON settings written in the profile is correct.
 */
add_task(async function test_settings_write() {
  let ss = Services.search.wrappedJSObject;
  info("test settings writing");

  await loadSettingsFile("data/search.json");

  const settingsFileWritten = promiseAfterSettings();
  await ss.reset();
  await Services.search.init();
  await settingsFileWritten;

  let settingsData = await promiseSettingsData();

  // Remove buildID and locale, as they are no longer used.
  delete settingsTemplate.buildID;
  delete settingsTemplate.locale;

  for (let engine of settingsTemplate.engines) {
    // Remove _shortName from the settings template, as it is no longer supported,
    // but older settings used to have it, so we keep it in the template as an
    // example.
    if ("_shortName" in engine) {
      delete engine._shortName;
    }
    if ("_urls" in engine) {
      // Only app-provided engines support purpose & mozparams, others do not,
      // so filter them out of the expected template.
      for (let urls of engine._urls) {
        urls.params = urls.params.filter(p => !p.purpose && !p.mozparam);
        // resultDomain is also no longer supported.
        if ("resultDomain" in urls) {
          delete urls.resultDomain;
        }
      }
    }
    // Remove queryCharset, if it is the same as the default, as we don't save
    // it in that case.
    if (engine?.queryCharset == SearchUtils.DEFAULT_QUERY_CHARSET) {
      delete engine.queryCharset;
    }
  }

  // Note: the file is copied with an old version number, which should have
  // been updated on write.
  settingsTemplate.version = SearchUtils.SETTINGS_VERSION;

  isSubObjectOf(settingsTemplate, settingsData, (prop, value) => {
    if (prop != "_iconURL" && prop != "{}") {
      return false;
    }
    // Skip items that are to do with icons for extensions, as we can't
    // control the uuid.
    return value.startsWith("moz-extension://");
  });
});

async function settings_write_check(disableFn) {
  let ss = Services.search.wrappedJSObject;

  sinon.stub(ss._settings, "_write").returns(Promise.resolve());

  // Simulate the search service being initialized.
  disableFn(true);

  ss._settings.setMetaDataAttribute("value", "test");

  Assert.ok(
    ss._settings._write.notCalled,
    "Should not have attempted to _write"
  );

  // Wait for two periods of the normal delay to ensure we still do not write.
  await new Promise(r =>
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(r, SearchSettings.SETTNGS_INVALIDATION_DELAY * 2)
  );

  Assert.ok(
    ss._settings._write.notCalled,
    "Should not have attempted to _write"
  );

  disableFn(false);

  await TestUtils.waitForCondition(
    () => ss._settings._write.calledOnce,
    "Should attempt to write the settings."
  );

  sinon.restore();
}

add_task(async function test_settings_write_prevented_during_init() {
  await settings_write_check(
    disable => (Services.search.wrappedJSObject._initialized = !disable)
  );
});

add_task(async function test_settings_write_prevented_during_reload() {
  await settings_write_check(
    disable => (Services.search.wrappedJSObject._reloadingEngines = disable)
  );
});

var EXPECTED_ENGINE = {
  engine: {
    name: "Test search engine",
    alias: "",
    description: "A test search engine (based on Google search)",
    searchForm: "http://www.google.com/",
    wrappedJSObject: {
      _extensionID: "test-addon-id@mozilla.org",
      _iconURL:
        "data:image/png;base64,AAABAAEAEBAAAAEAGABoAwAAFgAAACgAAAAQAAA" +
        "AIAAAAAEAGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADs9Pt8xetPtu9F" +
        "sfFNtu%2BTzvb2%2B%2Fne4dFJeBw0egA%2FfAJAfAA8ewBBegAAAAD%2B%2F" +
        "Ptft98Mp%2BwWsfAVsvEbs%2FQeqvF8xO7%2F%2F%2F63yqkxdgM7gwE%2Fgg" +
        "M%2BfQA%2BegBDeQDe7PIbotgQufcMufEPtfIPsvAbs%2FQvq%2Bfz%2Bf%2F" +
        "%2B%2B%2FZKhR05hgBBhQI8hgBAgAI9ewD0%2B%2Fg3pswAtO8Cxf4Kw%2FsJ" +
        "vvYAqupKsNv%2B%2Fv7%2F%2FP5VkSU0iQA7jQA9hgBDgQU%2BfQH%2F%2Ff%" +
        "2FQ6fM4sM4KsN8AteMCruIqqdbZ7PH8%2Fv%2Fg6Nc%2Fhg05kAA8jAM9iQI%" +
        "2BhQA%2BgQDQu6b97uv%2F%2F%2F7V8Pqw3eiWz97q8%2Ff%2F%2F%2F%2F7%" +
        "2FPptpkkqjQE4kwA7kAA5iwI8iAA8hQCOSSKdXjiyflbAkG7u2s%2F%2B%2F%" +
        "2F39%2F%2F7r8utrqEYtjQE8lgA7kwA7kwA9jwA9igA9hACiWSekVRyeSgiYS" +
        "BHx6N%2F%2B%2Fv7k7OFRmiYtlAA5lwI7lwI4lAA7kgI9jwE9iwI4iQCoVhWc" +
        "TxCmb0K%2BooT8%2Fv%2F7%2F%2F%2FJ2r8fdwI1mwA3mQA3mgA8lAE8lAE4j" +
        "wA9iwE%2BhwGfXifWvqz%2B%2Ff%2F58u%2Fev6Dt4tr%2B%2F%2F2ZuIUsgg" +
        "A7mgM6mAM3lgA5lgA6kQE%2FkwBChwHt4dv%2F%2F%2F728ei1bCi7VAC5XQ7" +
        "kz7n%2F%2F%2F6bsZkgcB03lQA9lgM7kwA2iQktZToPK4r9%2F%2F%2F9%2F%" +
        "2F%2FSqYK5UwDKZAS9WALIkFn%2B%2F%2F3%2F%2BP8oKccGGcIRJrERILYFE" +
        "MwAAuEAAdX%2F%2Ff7%2F%2FP%2B%2BfDvGXQLIZgLEWgLOjlf7%2F%2F%2F%" +
        "2F%2F%2F9QU90EAPQAAf8DAP0AAfMAAOUDAtr%2F%2F%2F%2F7%2B%2Fu2bCT" +
        "IYwDPZgDBWQDSr4P%2F%2Fv%2F%2F%2FP5GRuABAPkAA%2FwBAfkDAPAAAesA" +
        "AN%2F%2F%2B%2Fz%2F%2F%2F64g1C5VwDMYwK8Yg7y5tz8%2Fv%2FV1PYKDOc" +
        "AAP0DAf4AAf0AAfYEAOwAAuAAAAD%2F%2FPvi28ymXyChTATRrIb8%2F%2F3v" +
        "8fk6P8MAAdUCAvoAAP0CAP0AAfYAAO4AAACAAQAAAAAAAAAAAAAAAAAAAAAAA" +
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACAAQAA",
      _urls: [
        {
          type: "application/x-suggestions+json",
          method: "GET",
          template:
            "http://suggestqueries.google.com/complete/search?output=firefox&client=firefox" +
            "&hl={moz:locale}&q={searchTerms}",
          params: "",
        },
        {
          type: "text/html",
          method: "GET",
          template: "http://www.google.com/search",
          params: [
            {
              name: "q",
              value: "{searchTerms}",
              purpose: undefined,
            },
          ],
        },
      ],
    },
  },
};
