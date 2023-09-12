/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test initializing from broken search settings. This is one where the engines
 * array for some reason has lost all the default engines, but retained either
 * one or two, or a user-supplied engine. We don't know why this happens, but
 * we have seen it (bug 1578807).
 */

"use strict";

const { getAppInfo } = ChromeUtils.importESModule(
  "resource://testing-common/AppInfo.sys.mjs"
);

const enginesSettings = {
  version: SearchUtils.SETTINGS_VERSION,
  buildID: "TBD",
  appVersion: "TBD",
  locale: "en-US",
  metaData: {
    searchDefault: "Test search engine",
    searchDefaultHash: "TBD",
    // Intentionally in the past, but shouldn't actually matter for this test.
    searchDefaultExpir: 1567694909002,
    current: "",
    hash: "TBD",
    visibleDefaultEngines:
      "engine,engine-pref,engine-rel-searchform-purpose,engine-chromeicon,engine-resourceicon,engine-reordered",
    visibleDefaultEnginesHash: "TBD",
  },
  engines: [
    // This is a user-installed engine - the only one that was listed due to the
    // original issue.
    {
      _name: "A second test engine",
      _shortName: "engine2",
      _loadPath: "[profile]/searchplugins/engine2.xml",
      description: "A second test search engine (based on DuckDuckGo)",
      _iconURL:
        "data:image/x-icon;base64,AAABAAEAEBAQAAEABAAoAQAAFgAAACgAAAAQAAAAIAAAAAEABAAAAAAAAAAAAAAAAAAAAAAAEAAAAAAAAAAEAgQAhIOEAMjHyABIR0gA6ejpAGlqaQCpqKkAKCgoAPz9/AAZGBkAmJiYANjZ2ABXWFcAent6ALm6uQA8OjwAiIiIiIiIiIiIiI4oiL6IiIiIgzuIV4iIiIhndo53KIiIiB/WvXoYiIiIfEZfWBSIiIEGi/foqoiIgzuL84i9iIjpGIoMiEHoiMkos3FojmiLlUipYliEWIF+iDe0GoRa7D6GPbjcu1yIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
      _iconMapObj: {
        '{"width":16,"height":16}':
          "data:image/x-icon;base64,AAABAAEAEBAQAAEABAAoAQAAFgAAACgAAAAQAAAAIAAAAAEABAAAAAAAAAAAAAAAAAAAAAAAEAAAAAAAAAAEAgQAhIOEAMjHyABIR0gA6ejpAGlqaQCpqKkAKCgoAPz9/AAZGBkAmJiYANjZ2ABXWFcAent6ALm6uQA8OjwAiIiIiIiIiIiIiI4oiL6IiIiIgzuIV4iIiIhndo53KIiIiB/WvXoYiIiIfEZfWBSIiIEGi/foqoiIgzuL84i9iIjpGIoMiEHoiMkos3FojmiLlUipYliEWIF+iDe0GoRa7D6GPbjcu1yIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
      },
      _isBuiltin: false,
      _metaData: {
        order: 1,
      },
      _urls: [
        {
          template: "https://duckduckgo.com/?q={searchTerms}",
          rels: [],
          resultDomain: "duckduckgo.com",
          params: [],
        },
      ],
      queryCharset: "UTF-8",
      filePath: "TBD",
    },
  ],
};

add_setup(async function () {
  await AddonTestUtils.promiseStartupManager();

  // Allow telemetry probes which may otherwise be disabled for some applications (e.g. Thunderbird)
  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );

  await SearchTestUtils.useTestEngines();
  Services.prefs.setCharPref(SearchUtils.BROWSER_SEARCH_PREF + "region", "US");
  Services.locale.availableLocales = ["en-US"];
  Services.locale.requestedLocales = ["en-US"];

  // We dynamically generate the hashes because these depend on the profile.
  enginesSettings.metaData.searchDefaultHash = SearchUtils.getVerificationHash(
    enginesSettings.metaData.searchDefault
  );
  enginesSettings.metaData.hash = SearchUtils.getVerificationHash(
    enginesSettings.metaData.current
  );
  enginesSettings.metaData.visibleDefaultEnginesHash =
    SearchUtils.getVerificationHash(
      enginesSettings.metaData.visibleDefaultEngines
    );
  const appInfo = getAppInfo();
  enginesSettings.buildID = appInfo.platformBuildID;
  enginesSettings.appVersion = appInfo.version;

  await IOUtils.writeJSON(
    PathUtils.join(PathUtils.profileDir, SETTINGS_FILENAME),
    enginesSettings,
    { compress: true }
  );
});

add_task(async function test_cached_engine_properties() {
  info("init search service");

  const initResult = await Services.search.init();

  info("init'd search service");
  Assert.ok(
    Components.isSuccessCode(initResult),
    "Should have successfully created the search service"
  );

  const engines = await Services.search.getEngines();

  const expectedEngines = [
    // Default engines
    "Test search engine",
    // Rest of engines in order
    "engine-resourceicon",
    "engine-chromeicon",
    "engine-pref",
    "engine-rel-searchform-purpose",
    "Test search engine (Reordered)",
    "A second test engine",
  ];

  Assert.deepEqual(
    engines.map(e => e.name),
    expectedEngines,
    "Should have the expected default engines"
  );
});
