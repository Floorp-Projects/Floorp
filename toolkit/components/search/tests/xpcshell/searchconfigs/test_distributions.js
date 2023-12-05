/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SearchService: "resource://gre/modules/SearchService.sys.mjs",
});

const tests = [];

for (let canonicalId of ["canonical", "canonical-001"]) {
  tests.push({
    locale: "en-US",
    region: "US",
    distribution: canonicalId,
    test: engines =>
      hasParams(engines, "Google", "searchbar", "client=ubuntu") &&
      hasParams(engines, "Google", "searchbar", "channel=fs") &&
      hasTelemetryId(engines, "Google", "google-canonical"),
  });

  tests.push({
    locale: "en-US",
    region: "GB",
    distribution: canonicalId,
    test: engines =>
      hasParams(engines, "Google", "searchbar", "client=ubuntu") &&
      hasParams(engines, "Google", "searchbar", "channel=fs") &&
      hasTelemetryId(engines, "Google", "google-canonical"),
  });
}

tests.push({
  locale: "en-US",
  region: "US",
  distribution: "canonical-002",
  test: engines =>
    hasParams(engines, "Google", "searchbar", "client=ubuntu-sn") &&
    hasParams(engines, "Google", "searchbar", "channel=fs") &&
    hasTelemetryId(engines, "Google", "google-ubuntu-sn"),
});

tests.push({
  locale: "en-US",
  region: "GB",
  distribution: "canonical-002",
  test: engines =>
    hasParams(engines, "Google", "searchbar", "client=ubuntu-sn") &&
    hasParams(engines, "Google", "searchbar", "channel=fs") &&
    hasTelemetryId(engines, "Google", "google-ubuntu-sn"),
});

tests.push({
  locale: "zh-CN",
  region: "CN",
  distribution: "MozillaOnline",
  test: engines =>
    hasEnginesFirst(engines, ["百度", "Bing", "Google", "亚马逊", "维基百科"]),
});

tests.push({
  locale: "fr",
  distribution: "qwant-001",
  test: engines =>
    hasParams(engines, "Qwant", "searchbar", "client=firefoxqwant") &&
    hasDefault(engines, "Qwant") &&
    hasEnginesFirst(engines, ["Qwant", "Qwant Junior"]),
});

tests.push({
  locale: "fr",
  distribution: "qwant-001",
  test: engines =>
    hasParams(engines, "Qwant Junior", "searchbar", "client=firefoxqwant"),
});

tests.push({
  locale: "fr",
  distribution: "qwant-002",
  test: engines =>
    hasParams(engines, "Qwant", "searchbar", "client=firefoxqwant") &&
    hasDefault(engines, "Qwant") &&
    hasEnginesFirst(engines, ["Qwant", "Qwant Junior"]),
});

tests.push({
  locale: "fr",
  distribution: "qwant-002",
  test: engines =>
    hasParams(engines, "Qwant Junior", "searchbar", "client=firefoxqwant"),
});

for (const locale of ["en-US", "de"]) {
  tests.push({
    locale,
    distribution: "1und1",
    test: engines =>
      hasParams(engines, "1&1 Suche", "searchbar", "enc=UTF-8") &&
      hasDefault(engines, "1&1 Suche") &&
      hasEnginesFirst(engines, ["1&1 Suche"]),
  });

  tests.push({
    locale,
    distribution: "gmx",
    test: engines =>
      hasParams(engines, "GMX Suche", "searchbar", "enc=UTF-8") &&
      hasDefault(engines, "GMX Suche") &&
      hasEnginesFirst(engines, ["GMX Suche"]),
  });

  tests.push({
    locale,
    distribution: "gmx",
    test: engines =>
      hasParams(engines, "GMX Shopping", "searchbar", "origin=br_osd"),
  });

  tests.push({
    locale,
    distribution: "mail.com",
    test: engines =>
      hasParams(engines, "mail.com search", "searchbar", "enc=UTF-8") &&
      hasDefault(engines, "mail.com search") &&
      hasEnginesFirst(engines, ["mail.com search"]),
  });

  tests.push({
    locale,
    distribution: "webde",
    test: engines =>
      hasParams(engines, "WEB.DE Suche", "searchbar", "enc=UTF-8") &&
      hasDefault(engines, "WEB.DE Suche") &&
      hasEnginesFirst(engines, ["WEB.DE Suche"]),
  });
}

tests.push({
  locale: "ru",
  region: "RU",
  distribution: "gmx",
  test: engines => hasDefault(engines, "GMX Suche"),
});

tests.push({
  locale: "en-GB",
  distribution: "gmxcouk",
  test: engines =>
    hasURLs(
      engines,
      "GMX Search",
      "https://go.gmx.co.uk/br/moz_search_web/?enc=UTF-8&q=test",
      "https://suggestplugin.gmx.co.uk/s?q=test&brand=gmxcouk&origin=moz_splugin_ff&enc=UTF-8"
    ) &&
    hasDefault(engines, "GMX Search") &&
    hasEnginesFirst(engines, ["GMX Search"]),
});

tests.push({
  locale: "ru",
  region: "RU",
  distribution: "gmxcouk",
  test: engines => hasDefault(engines, "GMX Search"),
});

tests.push({
  locale: "es",
  distribution: "gmxes",
  test: engines =>
    hasURLs(
      engines,
      "GMX - Búsqueda web",
      "https://go.gmx.es/br/moz_search_web/?enc=UTF-8&q=test",
      "https://suggestplugin.gmx.es/s?q=test&brand=gmxes&origin=moz_splugin_ff&enc=UTF-8"
    ) &&
    hasDefault(engines, "GMX Search") &&
    hasEnginesFirst(engines, ["GMX Search"]),
});

tests.push({
  locale: "ru",
  region: "RU",
  distribution: "gmxes",
  test: engines => hasDefault(engines, "GMX - Búsqueda web"),
});

tests.push({
  locale: "fr",
  distribution: "gmxfr",
  test: engines =>
    hasURLs(
      engines,
      "GMX - Recherche web",
      "https://go.gmx.fr/br/moz_search_web/?enc=UTF-8&q=test",
      "https://suggestplugin.gmx.fr/s?q=test&brand=gmxfr&origin=moz_splugin_ff&enc=UTF-8"
    ) &&
    hasDefault(engines, "GMX Search") &&
    hasEnginesFirst(engines, ["GMX Search"]),
});

tests.push({
  locale: "ru",
  region: "RU",
  distribution: "gmxfr",
  test: engines => hasDefault(engines, "GMX - Recherche web"),
});

tests.push({
  locale: "en-US",
  region: "US",
  distribution: "mint-001",
  test: engines =>
    hasParams(engines, "DuckDuckGo", "searchbar", "t=lm") &&
    hasParams(engines, "Google", "searchbar", "client=firefox-b-1-lm") &&
    hasDefault(engines, "Google") &&
    hasEnginesFirst(engines, ["Google"]) &&
    hasTelemetryId(engines, "Google", "google-b-1-lm"),
});

tests.push({
  locale: "en-GB",
  region: "GB",
  distribution: "mint-001",
  test: engines =>
    hasParams(engines, "DuckDuckGo", "searchbar", "t=lm") &&
    hasParams(engines, "Google", "searchbar", "client=firefox-b-lm") &&
    hasDefault(engines, "Google") &&
    hasEnginesFirst(engines, ["Google"]) &&
    hasTelemetryId(engines, "Google", "google-b-lm"),
});

function hasURLs(engines, engineName, url, suggestURL) {
  let engine = engines.find(e => e._name === engineName);
  Assert.ok(engine, `Should be able to find ${engineName}`);

  let submission = engine.getSubmission("test", "text/html");
  Assert.equal(
    submission.uri.spec,
    url,
    `Should have the correct submission url for ${engineName}`
  );

  submission = engine.getSubmission("test", "application/x-suggestions+json");
  Assert.equal(
    submission.uri.spec,
    suggestURL,
    `Should have the correct suggestion url for ${engineName}`
  );
}

function hasParams(engines, engineName, purpose, param) {
  let engine = engines.find(e => e._name === engineName);
  Assert.ok(engine, `Should be able to find ${engineName}`);

  let submission = engine.getSubmission("test", "text/html", purpose);
  let queries = submission.uri.query.split("&");

  let paramNames = new Set();
  for (let query of queries) {
    let queryParam = query.split("=")[0];
    Assert.ok(
      !paramNames.has(queryParam),
      `Should not have a duplicate ${queryParam} param`
    );
    paramNames.add(queryParam);
  }

  let result = queries.includes(param);
  Assert.ok(result, `expect ${submission.uri.query} to include ${param}`);
  return true;
}

function hasTelemetryId(engines, engineName, telemetryId) {
  let engine = engines.find(e => e._name === engineName);
  Assert.ok(engine, `Should be able to find ${engineName}`);

  Assert.equal(
    engine.telemetryId,
    telemetryId,
    "Should have the correct telemetryId"
  );
  return true;
}

function hasDefault(engines, expectedDefaultName) {
  Assert.equal(
    engines[0].name,
    expectedDefaultName,
    "Should have the expected engine set as default"
  );
  return true;
}

function hasEnginesFirst(engines, expectedEngines) {
  for (let [i, expectedEngine] of expectedEngines.entries()) {
    Assert.equal(
      engines[i].name,
      expectedEngine,
      `Should have the expected engine in position ${i}`
    );
  }
}

engineSelector = SearchUtils.newSearchConfigEnabled
  ? new SearchEngineSelector()
  : new SearchEngineSelectorOld();

add_setup(async function () {
  if (!SearchUtils.newSearchConfigEnabled) {
    AddonTestUtils.init(GLOBAL_SCOPE);
    AddonTestUtils.createAppInfo(
      "xpcshell@tests.mozilla.org",
      "XPCShell",
      "42",
      "42"
    );
    await AddonTestUtils.promiseStartupManager();
  }

  await maybeSetupConfig();
});

add_task(async function test_expected_distribution_engines() {
  let searchService = new SearchService();
  for (const { distribution, locale = "en-US", region = "US", test } of tests) {
    let config = await engineSelector.fetchEngineConfiguration({
      locale,
      region,
      distroID: distribution,
    });
    let engines = await SearchTestUtils.searchConfigToEngines(config.engines);
    searchService._engines = engines;
    searchService._searchDefault = {
      id: config.engines[0].webExtension.id,
      locale:
        config.engines[0]?.webExtension?.locale ?? SearchUtils.DEFAULT_TAG,
    };
    engines = searchService._sortEnginesByDefaults(engines);
    test(engines);
  }
});
