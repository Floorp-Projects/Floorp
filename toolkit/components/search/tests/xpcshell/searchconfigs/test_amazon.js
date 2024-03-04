/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "amazon",
  default: {
    // Not default anywhere.
  },
  available: {
    included: [
      {
        // The main regions we ship Amazon to. Below this are special cases.
        regions: ["us", "jp"],
      },
    ],
  },
  details: [
    {
      domain: "amazon.co.jp",
      telemetryId: "amazon-jp",
      aliases: ["@amazon"],
      included: [
        {
          regions: ["jp"],
        },
      ],
      searchUrlCode: "tag=mozillajapan-fx-22",
      noSuggestionsURL: true,
    },
    {
      domain: "amazon.com",
      telemetryId: "amazondotcom-us-adm",
      aliases: ["@amazon"],
      included: [
        {
          regions: ["us"],
        },
      ],
      noSuggestionsURL: true,
      searchUrlCode: "tag=admarketus-20",
    },
  ],
});

add_setup(async function () {
  // We only need to do setup on one of the tests.
  await test.setup("89.0");
});

add_task(async function test_searchConfig_amazon() {
  await test.run();
});

add_task(
  { skip_if: () => SearchUtils.newSearchConfigEnabled },
  async function test_searchConfig_amazon_pre89() {
    const version = "88.0";
    AddonTestUtils.createAppInfo(
      "xpcshell@tests.mozilla.org",
      "XPCShell",
      version,
      version
    );
    // For pre-89, Amazon has a slightly different config.
    let details = test._config.details.find(
      d => d.telemetryId == "amazondotcom-us-adm"
    );
    details.telemetryId = "amazondotcom";
    delete details.searchUrlCode;

    await test.run();
  }
);
