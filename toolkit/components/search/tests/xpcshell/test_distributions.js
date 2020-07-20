/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.jsm",
  SearchService: "resource://gre/modules/SearchService.jsm",
});

const tests = [];

tests.push({
  distribution: "acer-001",
  test: engines =>
    hasParams(engines, "Bing", "searchbar", "pc=MOZD") &&
    hasDefault(engines, "Bing") &&
    hasEnginesFirst(engines, ["Bing"]),
});

tests.push({
  distribution: "acer-002",
  test: engines =>
    hasParams(engines, "Bing", "searchbar", "pc=MOZD") &&
    hasDefault(engines, "Bing") &&
    hasEnginesFirst(engines, ["Bing"]),
});

tests.push({
  distribution: "acer-g-003",
  test: engines =>
    hasParams(engines, "Bing", "searchbar", "pc=MOZE") &&
    hasDefault(engines, "Bing") &&
    hasEnginesFirst(engines, ["Bing"]),
});

// Test a couple of locale/regions on Acer where Bing isn't normally present.
tests.push({
  locale: "pl",
  region: "PL",
  distribution: "acer-001",
  test: engines =>
    hasParams(engines, "Bing", "searchbar", "pc=MOZD") &&
    hasDefault(engines, "Bing") &&
    hasEnginesFirst(engines, ["Bing"]),
});

tests.push({
  locale: "ru",
  region: "RU",
  distribution: "acer-002",
  test: engines =>
    hasParams(engines, "Bing", "searchbar", "pc=MOZD") &&
    hasDefault(engines, "Bing") &&
    hasEnginesFirst(engines, ["Bing"]),
});

tests.push({
  locale: "ru",
  region: "RU",
  distribution: "acer-g-003",
  test: engines =>
    hasParams(engines, "Bing", "searchbar", "pc=MOZE") &&
    hasDefault(engines, "Bing") &&
    hasEnginesFirst(engines, ["Bing"]),
});

for (let canonicalId of ["canonical", "canonical-001", "canonical-002"]) {
  tests.push({
    locale: "en-US",
    region: "US",
    distribution: canonicalId,
    test: engines =>
      hasParams(engines, "Google", "searchbar", "client=ubuntu") &&
      hasParams(engines, "Google", "searchbar", "channel=fs"),
  });

  tests.push({
    locale: "en-US",
    region: "GB",
    distribution: canonicalId,
    test: engines =>
      hasParams(engines, "Google", "searchbar", "client=ubuntu") &&
      hasParams(engines, "Google", "searchbar", "channel=fs"),
  });

  tests.push({
    distribution: canonicalId,
    test: engines =>
      hasParams(engines, "DuckDuckGo", "searchbar", "t=canonical"),
  });

  tests.push({
    locale: "en-US",
    region: "US",
    distribution: canonicalId,
    test: engines =>
      hasParams(engines, "Amazon.com", "searchbar", "tag=wwwcanoniccom-20"),
  });

  tests.push({
    locale: "de",
    region: "DE",
    distribution: canonicalId,
    test: engines =>
      hasParams(engines, "Amazon.de", "searchbar", "tag=wwwcanoniccom-20"),
  });

  tests.push({
    locale: "en-GB",
    region: "GB",
    distribution: canonicalId,
    test: engines =>
      hasParams(engines, "Amazon.co.uk", "searchbar", "tag=wwwcanoniccom-20"),
  });

  tests.push({
    locale: "fr",
    region: "FR",
    distribution: canonicalId,
    test: engines =>
      hasParams(engines, "Amazon.fr", "searchbar", "tag=wwwcanoniccom-20"),
  });

  tests.push({
    locale: "it",
    region: "IT",
    distribution: canonicalId,
    test: engines =>
      hasParams(engines, "Amazon.it", "searchbar", "tag=wwwcanoniccom-20"),
  });

  tests.push({
    locale: "ja",
    region: "JP",
    distribution: canonicalId,
    test: engines =>
      hasParams(engines, "Amazon.co.jp", "searchbar", "tag=wwwcanoniccom-20"),
  });

  tests.push({
    locale: "ur",
    region: "IN",
    distribution: canonicalId,
    test: engines =>
      hasParams(engines, "Amazon.in", "searchbar", "tag=wwwcanoniccom-20"),
  });

  tests.push({
    locale: "zh-CN",
    region: "CN",
    distribution: canonicalId,
    test: engines =>
      hasParams(engines, "亚马逊", "searchbar", "tag=wwwcanoniccom-20"),
  });

  tests.push({
    locale: "zh-CN",
    region: "CN",
    distribution: canonicalId,
    test: engines =>
      hasParams(engines, "百度", "searchbar", "tn=ubuntuu_cb") &&
      hasParams(engines, "百度", "suggestions", "tn=ubuntuu_cb"),
  });
}

tests.push({
  locale: "ru",
  distribution: "mailru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900201") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900201") &&
    hasDefault(engines, "Поиск Mail.Ru") &&
    hasEnginesFirst(engines, ["Поиск Mail.Ru"]),
});

tests.push({
  locale: "ru",
  region: "RU",
  distribution: "mailru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900201") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900201") &&
    hasDefault(engines, "Поиск Mail.Ru") &&
    hasEnginesFirst(engines, ["Поиск Mail.Ru"]),
});

tests.push({
  locale: "az",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900209") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900209") &&
    hasDefault(engines, "Поиск Mail.Ru") &&
    hasEnginesFirst(engines, ["Поиск Mail.Ru"]),
});

tests.push({
  locale: "en-US",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900205") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900205") &&
    hasDefault(engines, "Поиск Mail.Ru") &&
    hasEnginesFirst(engines, ["Поиск Mail.Ru"]),
});

tests.push({
  locale: "hy-AM",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900211") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900211") &&
    hasDefault(engines, "Поиск Mail.Ru") &&
    hasEnginesFirst(engines, ["Поиск Mail.Ru"]),
});

tests.push({
  locale: "kk",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900206") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900206") &&
    hasDefault(engines, "Поиск Mail.Ru") &&
    hasEnginesFirst(engines, ["Поиск Mail.Ru"]),
});

tests.push({
  locale: "kk",
  region: "KZ",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900206") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900206") &&
    hasDefault(engines, "Поиск Mail.Ru") &&
    hasEnginesFirst(engines, ["Поиск Mail.Ru"]),
});

tests.push({
  locale: "ro",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900207") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900207") &&
    hasDefault(engines, "Поиск Mail.Ru") &&
    hasEnginesFirst(engines, ["Поиск Mail.Ru"]),
});

tests.push({
  locale: "ru",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900203") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900203") &&
    hasDefault(engines, "Поиск Mail.Ru") &&
    hasEnginesFirst(engines, ["Поиск Mail.Ru"]),
});

tests.push({
  locale: "ru",
  region: "RU",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900203") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900203") &&
    hasDefault(engines, "Поиск Mail.Ru") &&
    hasEnginesFirst(engines, ["Поиск Mail.Ru"]),
});

tests.push({
  locale: "tr",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900210") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900210") &&
    hasDefault(engines, "Поиск Mail.Ru") &&
    hasEnginesFirst(engines, ["Поиск Mail.Ru"]),
});

tests.push({
  locale: "tr",
  region: "TR",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900210") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900210") &&
    hasDefault(engines, "Поиск Mail.Ru") &&
    hasEnginesFirst(engines, ["Поиск Mail.Ru"]),
});

tests.push({
  locale: "uk",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900204") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900204") &&
    hasDefault(engines, "Поиск Mail.Ru") &&
    hasEnginesFirst(engines, ["Поиск Mail.Ru"]),
});

tests.push({
  locale: "uz",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900208") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900208") &&
    hasDefault(engines, "Поиск Mail.Ru") &&
    hasEnginesFirst(engines, ["Поиск Mail.Ru"]),
});

tests.push({
  locale: "zh-CN",
  distribution: "MozillaOnline",
  test: engines =>
    hasParams(engines, "百度", "searchbar", "tn=monline_4_dg") &&
    hasParams(engines, "百度", "suggestions", "tn=monline_4_dg") &&
    hasParams(engines, "百度", "homepage", "tn=monline_3_dg") &&
    hasParams(engines, "百度", "newtab", "tn=monline_3_dg") &&
    hasParams(engines, "百度", "contextmenu", "tn=monline_4_dg") &&
    hasParams(engines, "百度", "keyword", "tn=monline_4_dg") &&
    hasDefault(engines, "百度") &&
    hasEnginesFirst(engines, ["百度", "Bing", "Google", "亚马逊", "维基百科"]),
});

tests.push({
  locale: "zh-CN",
  distribution: "MozillaOnline",
  test: engines =>
    hasParams(engines, "亚马逊", "searchbar", "engine=amazon_shopping") &&
    hasParams(engines, "亚马逊", "suggestions", "tag=mozilla") &&
    hasParams(engines, "亚马逊", "homepage", "create=2028") &&
    hasParams(engines, "亚马逊", "homepage", "adid=1NZNRHJZ2Q87NTS7YW6N") &&
    hasParams(engines, "亚马逊", "homepage", "campaign=408") &&
    hasParams(engines, "亚马逊", "homepage", "create=2028") &&
    hasParams(engines, "亚马逊", "homepage", "mode=blended") &&
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

tests.push({
  locale: "cs",
  distribution: "seznam",
  test: engines =>
    hasParams(engines, "Seznam", "searchbar", "sourceid=FF_3") &&
    hasDefault(engines, "Seznam") &&
    hasEnginesFirst(engines, ["Seznam"]),
});

for (const locale of ["en-US", "en-GB", "fr", "de"]) {
  tests.push({
    locale,
    distribution: "sweetlabs-b-oem1",
    test: engines =>
      hasParams(engines, "Bing", "searchbar", "pc=MZSL01") &&
      hasDefault(engines, "Bing") &&
      hasEnginesFirst(engines, ["Bing"]),
  });

  tests.push({
    locale,
    distribution: "sweetlabs-b-r-oem1",
    test: engines =>
      hasParams(engines, "Bing", "searchbar", "pc=MZSL01") &&
      hasDefault(engines, "Bing") &&
      hasEnginesFirst(engines, ["Bing"]),
  });

  tests.push({
    locale,
    distribution: "sweetlabs-b-oem2",
    test: engines =>
      hasParams(engines, "Bing", "searchbar", "pc=MZSL02") &&
      hasDefault(engines, "Bing") &&
      hasEnginesFirst(engines, ["Bing"]),
  });

  tests.push({
    locale,
    distribution: "sweetlabs-b-r-oem2",
    test: engines =>
      hasParams(engines, "Bing", "searchbar", "pc=MZSL02") &&
      hasDefault(engines, "Bing") &&
      hasEnginesFirst(engines, ["Bing"]),
  });

  tests.push({
    locale,
    distribution: "sweetlabs-b-oem3",
    test: engines =>
      hasParams(engines, "Bing", "searchbar", "pc=MZSL03") &&
      hasDefault(engines, "Bing") &&
      hasEnginesFirst(engines, ["Bing"]),
  });

  tests.push({
    locale,
    distribution: "sweetlabs-b-r-oem3",
    test: engines =>
      hasParams(engines, "Bing", "searchbar", "pc=MZSL03") &&
      hasDefault(engines, "Bing") &&
      hasEnginesFirst(engines, ["Bing"]),
  });

  tests.push({
    locale,
    distribution: "sweetlabs-oem1",
    test: engines =>
      hasParams(engines, "Google", "searchbar", "client=firefox-b-oem1") &&
      hasDefault(engines, "Google") &&
      hasEnginesFirst(engines, ["Google"]),
  });

  tests.push({
    locale,
    distribution: "sweetlabs-r-oem1",
    test: engines =>
      hasParams(engines, "Google", "searchbar", "client=firefox-b-oem1") &&
      hasDefault(engines, "Google") &&
      hasEnginesFirst(engines, ["Google"]),
  });

  tests.push({
    locale,
    distribution: "sweetlabs-oem2",
    test: engines =>
      hasParams(engines, "Google", "searchbar", "client=firefox-b-oem2") &&
      hasDefault(engines, "Google") &&
      hasEnginesFirst(engines, ["Google"]),
  });

  tests.push({
    locale,
    distribution: "sweetlabs-r-oem2",
    test: engines =>
      hasParams(engines, "Google", "searchbar", "client=firefox-b-oem2") &&
      hasDefault(engines, "Google") &&
      hasEnginesFirst(engines, ["Google"]),
  });
}

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
  distribution: "yandex-drp",
  test: engines =>
    hasParams(engines, "Яндекс", "searchbar", "clid=2039342") &&
    // Test that fallback works correct as well.
    hasParams(engines, "Яндекс", "contextmenu", "clid=2039342") &&
    hasDefault(engines, "Яндекс") &&
    hasEnginesFirst(engines, ["Яндекс"]),
});

tests.push({
  locale: "ru",
  distribution: "yandex-planb",
  test: engines =>
    hasParams(engines, "Яндекс", "searchbar", "clid=1857376") &&
    hasDefault(engines, "Яндекс") &&
    hasEnginesFirst(engines, ["Яндекс"]),
});

tests.push({
  locale: "ru",
  distribution: "yandex-portals",
  test: engines =>
    hasParams(engines, "Яндекс", "searchbar", "clid=1923034") &&
    hasDefault(engines, "Яндекс") &&
    hasEnginesFirst(engines, ["Яндекс"]),
});

tests.push({
  locale: "ru",
  distribution: "yandex-ru",
  test: engines =>
    hasParams(engines, "Яндекс", "searchbar", "clid=1923018") &&
    hasDefault(engines, "Яндекс") &&
    hasEnginesFirst(engines, ["Яндекс"]),
});

tests.push({
  locale: "tr",
  distribution: "yandex-tr",
  test: engines =>
    hasParams(engines, "Yandex", "searchbar", "clid=1953197") &&
    hasDefault(engines, "Yandex") &&
    hasEnginesFirst(engines, ["Yandex"]),
});

tests.push({
  locale: "tr",
  distribution: "yandex-tr-gezginler",
  test: engines =>
    hasParams(engines, "Yandex", "searchbar", "clid=1945716") &&
    hasDefault(engines, "Yandex") &&
    hasEnginesFirst(engines, ["Yandex"]),
});

tests.push({
  locale: "tr",
  distribution: "yandex-tr-tamindir",
  test: engines =>
    hasParams(engines, "Yandex", "searchbar", "clid=1945686") &&
    hasDefault(engines, "Yandex") &&
    hasEnginesFirst(engines, ["Yandex"]),
});

tests.push({
  locale: "uk",
  distribution: "yandex-uk",
  test: engines =>
    hasParams(engines, "Яндекс", "searchbar", "clid=1923018") &&
    hasDefault(engines, "Яндекс") &&
    hasEnginesFirst(engines, ["Яндекс"]),
});

tests.push({
  locale: "ru",
  distribution: "yandex-ru-mz",
  test: engines =>
    hasParams(engines, "Яндекс", "searchbar", "clid=2320519") &&
    hasDefault(engines, "Яндекс") &&
    hasEnginesFirst(engines, ["Яндекс"]),
});

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

const engineSelector = new SearchEngineSelector();

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_expected_distribution_engines() {
  let searchService = new SearchService();
  for (const { distribution, locale = "en-US", region = "US", test } of tests) {
    let config = await engineSelector.fetchEngineConfiguration(
      locale,
      region,
      null,
      distribution
    );
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
