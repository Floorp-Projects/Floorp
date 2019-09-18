/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test initializing from a broken search cache. This is one where the engines
 * array for some reason has lost all the default engines, but retained either
 * one or two, or a user-supplied engine. We don't know why this happens, but
 * we have seen it (bug 1578807).
 */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { getAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);
const { getVerificationHash } = ChromeUtils.import(
  "resource://gre/modules/SearchEngine.jsm"
);

var cacheTemplate, appPluginsPath, profPlugins;

const enginesCache = {
  version: 1,
  buildID: "TBD",
  appVersion: "TBD",
  locale: "en-US",
  visibleDefaultEngines: [
    "engine",
    "engine-pref",
    "engine-rel-searchform-purpose",
    "engine-chromeicon",
    "engine-resourceicon",
    "engine-reordered",
  ],
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
      _readOnly: false,
      filePath: "TBD",
    },
  ],
};

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();

  useTestEngineConfig();
  Services.prefs.setCharPref(SearchUtils.BROWSER_SEARCH_PREF + "region", "US");
  Services.locale.availableLocales = ["en-US"];
  Services.locale.requestedLocales = ["en-US"];

  const profile = do_get_profile();
  const engineDir = profile.clone();
  engineDir.append("searchplugins");
  engineDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  const engineTemplateFile = do_get_file("data/engine2.xml");
  engineTemplateFile.copyTo(engineDir, "engine2.xml");
  const engineOutputFile = engineDir.clone();
  engineOutputFile.append("engine2.xml");

  // We dynamically generate the hashes because these depend on the profile.
  enginesCache.metaData.searchDefaultHash = getVerificationHash(
    enginesCache.metaData.searchDefault
  );
  enginesCache.metaData.hash = getVerificationHash(
    enginesCache.metaData.current
  );
  enginesCache.metaData.visibleDefaultEnginesHash = getVerificationHash(
    enginesCache.metaData.visibleDefaultEngines
  );
  enginesCache.engines[0].filePath = engineOutputFile.path;
  const appInfo = getAppInfo();
  enginesCache.buildID = appInfo.platformBuildID;
  enginesCache.appVersion = appInfo.version;

  await OS.File.writeAtomic(
    OS.Path.join(OS.Constants.Path.profileDir, CACHE_FILENAME),
    new TextEncoder().encode(JSON.stringify(enginesCache)),
    { compression: "lz4" }
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

  const scalars = Services.telemetry.getSnapshotForScalars("main", false)
    .parent;
  Assert.equal(
    scalars["browser.searchinit.engines_cache_corrupted"],
    true,
    "Should have recorded the engines cache as corrupted"
  );

  const engines = await Services.search.getEngines();

  Assert.deepEqual(
    engines.map(e => e.name),
    [
      // Default engine
      "Test search engine",
      // Two engines listed in searchOrder.
      "engine-resourceicon",
      "engine-chromeicon",
      "A second test engine",
      // Rest of the engines in order.
      "engine-pref",
      "engine-rel-searchform-purpose",
      "Test search engine (Reordered)",
    ],
    "Should have the expected default engines"
  );
});
