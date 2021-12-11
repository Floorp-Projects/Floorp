/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "yahoo-jp",
  identifierExactMatch: true,
  aliases: [],
  default: {
    // Not default anywhere.
  },
  available: {
    included: [
      {
        locales: {
          matches: ["ja", "ja-JP-macos"],
        },
      },
    ],
  },
  details: [
    {
      included: [{}],
      domain: "search.yahoo.co.jp",
      telemetryId: "yahoo-jp",
      searchUrlCode: "fr=mozff",
    },
  ],
});

add_task(async function setup() {
  await test.setup();
});

add_task(async function test_searchConfig_yahoojp() {
  await test.run();
});
