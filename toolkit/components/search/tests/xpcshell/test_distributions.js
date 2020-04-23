/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.jsm",
});

const tests = [];

tests.push({
  distribution: "acer-001",
  test: engines => hasParams(engines, "Bing", "searchbar", "pc=MOZD"),
});

tests.push({
  distribution: "acer-002",
  test: engines => hasParams(engines, "Bing", "searchbar", "pc=MOZD"),
});

tests.push({
  locale: "ru",
  distribution: "mailru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900201") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900201"),
});

tests.push({
  locale: "az",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900209") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900209"),
});

tests.push({
  locale: "en-US",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900205") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900205"),
});

tests.push({
  locale: "hy-AM",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900211") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900211"),
});

tests.push({
  locale: "kk",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900206") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900206"),
});

tests.push({
  locale: "ro",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900207") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900207"),
});

tests.push({
  locale: "ru",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900203") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900203"),
});

tests.push({
  locale: "tr",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900210") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900210"),
});

tests.push({
  locale: "uk",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900204") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900204"),
});

tests.push({
  locale: "uz",
  distribution: "okru-001",
  test: engines =>
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "gp=900208") &&
    hasParams(engines, "Поиск Mail.Ru", "searchbar", "frc=900208"),
});

tests.push({
  locale: "zh-CN",
  distribution: "MozillaOnline",
  test: engines =>
    hasParams(engines, "百度", "searchbar", "tn=monline_4_dg") &&
    hasParams(engines, "百度", "suggestions", "tn=monline_4_dg") &&
    hasParams(engines, "百度", "homepage", "tn=monline_3_dg") &&
    hasParams(engines, "百度", "newtab", "tn=monline_3_dg"),
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
    hasParams(engines, "亚马逊", "homepage", "mode=blended"),
});

tests.push({
  locale: "fr",
  distribution: "qwant-001",
  test: engines =>
    hasParams(engines, "Qwant", "searchbar", "client=firefoxqwant"),
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
    hasParams(engines, "Qwant", "searchbar", "client=firefoxqwant"),
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
  test: engines => hasParams(engines, "Seznam", "searchbar", "sourceid=FF_3"),
});

tests.push({
  distribution: "sweetlabs-b-oem1",
  test: engines => hasParams(engines, "Bing", "searchbar", "pc=MZSL01"),
});

tests.push({
  distribution: "sweetlabs-b-r-oem1",
  test: engines => hasParams(engines, "Bing", "searchbar", "pc=MZSL01"),
});

tests.push({
  distribution: "sweetlabs-b-oem2",
  test: engines => hasParams(engines, "Bing", "searchbar", "pc=MZSL02"),
});

tests.push({
  distribution: "sweetlabs-b-r-oem2",
  test: engines => hasParams(engines, "Bing", "searchbar", "pc=MZSL02"),
});

tests.push({
  distribution: "sweetlabs-b-oem3",
  test: engines => hasParams(engines, "Bing", "searchbar", "pc=MZSL03"),
});

tests.push({
  distribution: "sweetlabs-b-r-oem3",
  test: engines => hasParams(engines, "Bing", "searchbar", "pc=MZSL03"),
});

tests.push({
  distribution: "sweetlabs-oem1",
  test: engines =>
    hasParams(engines, "Google", "searchbar", "client=firefox-b-oem1"),
});

tests.push({
  distribution: "sweetlabs-r-oem1",
  test: engines =>
    hasParams(engines, "Google", "searchbar", "client=firefox-b-oem1"),
});

tests.push({
  distribution: "sweetlabs-oem2",
  test: engines =>
    hasParams(engines, "Google", "searchbar", "client=firefox-b-oem2"),
});

tests.push({
  distribution: "sweetlabs-r-oem2",
  test: engines =>
    hasParams(engines, "Google", "searchbar", "client=firefox-b-oem2"),
});

tests.push({
  locale: "de",
  distribution: "1und1",
  test: engines => hasParams(engines, "1&1 Suche", "searchbar", "enc=UTF-8"),
});

tests.push({
  locale: "de",
  distribution: "gmx",
  test: engines => hasParams(engines, "GMX Suche", "searchbar", "enc=UTF-8"),
});

tests.push({
  locale: "de",
  distribution: "gmx",
  test: engines =>
    hasParams(engines, "GMX Shopping", "searchbar", "origin=br_osd"),
});

tests.push({
  locale: "ru",
  distribution: "yandex-drp",
  test: engines => hasParams(engines, "Яндекс", "searchbar", "clid=2039342"),
});

tests.push({
  locale: "ru",
  distribution: "yandex-planb",
  test: engines => hasParams(engines, "Яндекс", "searchbar", "clid=1857376"),
});

tests.push({
  locale: "ru",
  distribution: "yandex-portals",
  test: engines => hasParams(engines, "Яндекс", "searchbar", "clid=1923034"),
});

tests.push({
  locale: "ru",
  distribution: "yandex-ru",
  test: engines => hasParams(engines, "Яндекс", "searchbar", "clid=1923018"),
});

tests.push({
  locale: "ru",
  distribution: "yandex-tr",
  test: engines => hasParams(engines, "Яндекс", "searchbar", "clid=1953197"),
});

tests.push({
  locale: "ru",
  distribution: "yandex-tr-gezginler",
  test: engines => hasParams(engines, "Яндекс", "searchbar", "clid=1945716"),
});

tests.push({
  locale: "ru",
  distribution: "yandex-tr-tamindir",
  test: engines => hasParams(engines, "Яндекс", "searchbar", "clid=1945686"),
});

tests.push({
  locale: "ru",
  distribution: "yandex-uk",
  test: engines => hasParams(engines, "Яндекс", "searchbar", "clid=1923018"),
});

tests.push({
  locale: "ru",
  distribution: "yandex-ru-mz",
  test: engines => hasParams(engines, "Яндекс", "searchbar", "clid=2320519"),
});

function hasParams(engines, engineName, purpose, param) {
  let engine = engines.find(e => e._name === engineName);
  if (!engine) {
    Assert.ok(false, `Cannot find ${engineName}`);
  }
  let submission = engine.getSubmission("test", "text/html", purpose);
  let result = submission.uri.query.split("&").includes(param);
  Assert.ok(result, `expect ${submission.uri.query} to include ${param}`);
}

const engineSelector = new SearchEngineSelector();

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_application_name() {
  for (const { distribution, locale = "en-US", region = "US", test } of tests) {
    let config = await engineSelector.fetchEngineConfiguration(
      locale,
      region,
      null,
      distribution
    );
    let engines = await SearchTestUtils.searchConfigToEngines(config.engines);
    test(engines);
  }
});
