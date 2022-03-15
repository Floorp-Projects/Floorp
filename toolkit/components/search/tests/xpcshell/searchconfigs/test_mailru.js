/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "mailru",
  aliases: [],
  default: {
    // Not default anywhere.
  },
  available: {
    included: [
      {
        locales: {
          matches: ["ru"],
        },
      },
    ],
  },
  details: [
    {
      included: [{}],
      domain: "go.mail.ru",
      telemetryId: "mailru",
      codes: "gp=900200",
      searchUrlCode: "frc=900200",
    },
  ],
});

add_task(async function setup() {
  await test.setup();
});

add_task(async function test_searchConfig_mailru() {
  await test.run();
}).skip();
