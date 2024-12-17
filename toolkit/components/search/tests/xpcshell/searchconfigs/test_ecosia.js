/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "ecosia",
  aliases: [],
  default: {
    // Not default anywhere.
  },
  available: {
    included: [
      {
        locales: ["de"],
      },
      {
        regions: ["at", "be", "ch", "de", "es", "it", "nl", "se"],
      },
    ],
  },
  details: [
    {
      included: [{}],
      domain: "www.ecosia.org",
      telemetryId: "ecosia",
      searchUrlCode: "tt=mzl",
    },
  ],
});

add_setup(async function () {
  await test.setup();
});

add_task(async function test_searchConfig_ecosia() {
  await test.run();
});
