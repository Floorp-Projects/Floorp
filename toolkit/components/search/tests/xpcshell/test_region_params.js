/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const CONFIG = [
  {
    webExtension: { id: "engine@search.mozilla.org" },
    appliesTo: [
      { included: { everywhere: true } },
      {
        included: { everywhere: true },
        experiment: "acohortid",
        regionParams: { US: [{ name: "client", value: "veryspecial" }] },
      },
    ],
    default: "yes",
    params: {
      searchUrlGetParams: [{ name: "client", value: "default" }],
    },
    regionParams: {
      US: [{ name: "client", value: "special" }],
    },
  },
];

add_task(async function setup() {
  await useTestEngines("data", null, CONFIG);
  await AddonTestUtils.promiseStartupManager();

  let confUrl = `data:application/json,${JSON.stringify(CONFIG)}`;
  Services.prefs.setStringPref("search.config.url", confUrl);
});

add_task(async function test_region_params() {
  Region._setCurrentRegion("GB");
  await Services.search.init();
  let engine = await Services.search.getDefault();
  let params = engine.getSubmission("test").uri.query.split("&");
  Assert.ok(params.includes("client=default"), "Correct default params");

  Region._setCurrentRegion("US");
  engine = await Services.search.getDefault();
  params = engine.getSubmission("test").uri.query.split("&");
  Assert.ok(params.includes("client=special"), "Override param in US");

  Region._setCurrentRegion("ES");
  engine = await Services.search.getDefault();
  params = engine.getSubmission("test").uri.query.split("&");
  Assert.ok(params.includes("client=default"), "Revert back to default");

  Services.prefs.setCharPref("browser.search.experiment", "acohortid");
  await asyncReInit();

  Region._setCurrentRegion("US");
  engine = await Services.search.getDefault();
  params = engine.getSubmission("test").uri.query.split("&");
  Assert.ok(
    params.includes("client=veryspecial"),
    "appliesTo section param override used"
  );
});
