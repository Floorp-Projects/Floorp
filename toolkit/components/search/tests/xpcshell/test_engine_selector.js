/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.jsm",
});

const CONFIG_URL =
  "data:application/json," +
  JSON.stringify({
    data: [
      {
        engineName: "aol",
        orderHint: 500,
        webExtensionLocales: ["default"],
        appliesTo: [
          {
            included: { everywhere: true },
          },
          {
            included: { regions: ["us"] },
            webExtensionLocales: ["$USER_LOCALE"],
          },
        ],
      },
      {
        engineName: "lycos",
        orderHint: 1000,
        default: "yes",
        appliesTo: [
          {
            included: { everywhere: true },
            excluded: { locales: { matches: ["zh-CN"] } },
          },
        ],
      },
      {
        engineName: "altavista",
        orderHint: 2000,
        defaultPrivate: "yes",
        appliesTo: [
          {
            included: { locales: { matches: ["en-US"] } },
          },
          {
            included: { regions: ["default"] },
          },
        ],
      },
      {
        engineName: "excite",
        default: "yes-if-no-other",
        appliesTo: [
          {
            included: { everywhere: true },
            excluded: { regions: ["us"] },
          },
          {
            included: { everywhere: true },
            cohort: "acohortid",
          },
        ],
      },
      {
        engineName: "askjeeves",
      },
    ],
  });

const engineSelector = new SearchEngineSelector();

add_task(async function() {
  await engineSelector.init(CONFIG_URL);

  let { engines, privateDefault } = engineSelector.fetchEngineConfiguration(
    "en-US",
    "us"
  );
  Assert.equal(
    privateDefault,
    "altavista",
    "Should set altavista as privateDefault"
  );
  let names = engines.map(obj => obj.engineName);
  Assert.deepEqual(names, ["lycos", "altavista", "aol"], "Correct order");
  Assert.deepEqual(
    engines[2].webExtensionLocales,
    ["en-US"],
    "Subsequent matches in applies to can override default"
  );

  ({ engines, privateDefault } = engineSelector.fetchEngineConfiguration(
    "zh-CN",
    "kz"
  ));
  Assert.equal(engines.length, 2, "Correct engines are returns");
  Assert.equal(privateDefault, null, "There should be no privateDefault");
  names = engines.map(obj => obj.engineName);
  Assert.deepEqual(
    names,
    ["excite", "aol"],
    "The engines should be in the correct order"
  );

  Services.prefs.setCharPref("browser.search.cohort", "acohortid");
  ({ engines, privateDefault } = engineSelector.fetchEngineConfiguration(
    "en-US",
    "us"
  ));
  Assert.deepEqual(
    engines.map(obj => obj.engineName),
    ["lycos", "altavista", "aol", "excite"],
    "Engines are in the correct order and include the cohort engine"
  );

  ({ engines, privateDefault } = engineSelector.fetchEngineConfiguration(
    "en-US"
  ));
  Assert.deepEqual(
    engines.map(obj => obj.engineName),
    ["lycos", "altavista", "aol", "excite"],
    "The engines should be in the correct order"
  );
  Assert.equal(
    privateDefault,
    "altavista",
    "Should set altavista as privateDefault"
  );
});
