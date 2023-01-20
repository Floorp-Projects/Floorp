/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "ddg",
  aliases: ["@duckduckgo", "@ddg"],
  default: {
    // Not included anywhere.
  },
  available: {
    excluded: [
      // Should be available everywhere.
    ],
  },
  details: [
    {
      included: [{}],
      domain: "duckduckgo.com",
      telemetryId: AppConstants.IS_ESR ? "ddg-esr" : "ddg",
      searchUrlCode: AppConstants.IS_ESR ? "t=ftsa" : "t=ffab",
    },
  ],
});

add_task(async function setup() {
  await test.setup();
});

add_task(async function test_searchConfig_duckduckgo() {
  await test.run();
});
