/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "qwant",
  aliases: [],
  default: {
    // Not default anywhere.
  },
  available: {
    included: [
      {
        locales: {
          matches: ["fr"],
        },
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

add_task(async function setup() {
  await test.setup();
});

add_task(async function test_searchConfig_qwant() {
  await test.run();
});
