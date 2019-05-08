/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "ddg",
  default: {
    // Not included anywhere.
  },
  available: {
    excluded: [
      // Should be available everywhere.
    ],
  },
});

add_task(async function setup() {
  await test.setup();
});

add_task(async function test_searchConfig_duckduckgo() {
  await test.run();
});
