/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "rakuten",
  aliases: [],
  default: {
    // Not default anywhere.
  },
  available: {
    included: [
      {
        locales: ["ja", "ja-JP-macos"],
      },
    ],
  },
  details: [
    {
      included: [{}],
      domain: "rakuten.co.jp",
      telemetryId: "rakuten",
      searchUrlCodeNotInQuery: "013ca98b.cd7c5f0c",
    },
  ],
});

add_setup(async function () {
  await test.setup();
});

add_task(async function test_searchConfig_rakuten() {
  await test.run();
});
