/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "qwant",
  aliases: ["@qwant"],
  default: {
    // Not default anywhere.
  },
  available: {
    included: [
      {
        locales: ["fr"],
      },
      {
        regions: ["be", "ch", "es", "fr", "it", "nl"],
      },
    ],
  },
  details: [
    {
      included: [{}],
      domain: "www.qwant.com",
      telemetryId: "qwant",
      searchUrlCode: "client=brz-moz",
      suggestUrlCode: "client=opensearch",
    },
  ],
});

add_setup(async function () {
  if (SearchUtils.newSearchConfigEnabled) {
    await test.setup();
  } else {
    await test.setup("124.0");
  }
});

add_task(async function test_searchConfig_qwant() {
  await test.run();
});

add_task(
  { skip_if: () => SearchUtils.newSearchConfigEnabled },
  async function test_searchConfig_qwant_pre124() {
    const version = "123.0";
    AddonTestUtils.createAppInfo(
      "xpcshell@tests.mozilla.org",
      "XPCShell",
      version,
      version
    );
    // For pre-124, Qwant is not available in the extra regions.
    test._config.available.included.pop();

    await test.run();
  }
);
