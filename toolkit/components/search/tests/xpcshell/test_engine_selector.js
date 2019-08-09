/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  SearchEngineSelector: "resource://testing-common/SearchEngineSelector.jsm",
});

const CONFIG_URL =
  "data:application/json," +
  JSON.stringify({
    data: [
      {
        engineName: "aol",
        orderHint: 500,
        webExtensionLocale: "default",
        appliesTo: [
          {
            included: { everywhere: true },
          },
          {
            included: { regions: ["us"] },
            webExtensionLocale: "us",
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
    "us",
    "en-US"
  );
  Assert.equal(
    privateDefault,
    "altavista",
    "Should set altavista as privateDefault"
  );
  let names = engines.map(obj => obj.engineName);
  Assert.deepEqual(names, ["lycos", "altavista", "aol"], "Correct order");
  Assert.equal(
    engines[2].webExtensionLocale,
    "us",
    "Subsequent matches in applies to can override default"
  );

  ({ engines, privateDefault } = engineSelector.fetchEngineConfiguration(
    "kz",
    "zh-CN"
  ));
  Assert.equal(engines.length, 2, "Correct engines are returns");
  Assert.equal(privateDefault, null, "There should be no privateDefault");
  names = engines.map(obj => obj.engineName);
  Assert.deepEqual(
    names,
    ["excite", "aol"],
    "The engines should be in the correct order"
  );
});
