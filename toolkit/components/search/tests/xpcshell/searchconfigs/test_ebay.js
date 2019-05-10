/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "ebay",
  default: {
    // Not included anywhere.
  },
  available: {
    included: [{
      // We don't currently enforce by region, but do locale instead.
      // regions: [
      //   "us", "gb", "ca", "ie", "fr", "it", "de", "at", "es", "nl", "ch", "au"
      // ],
      locales: {
        matches: ["an", "en-US", "ast", "br", "ca", "cy", "de", "dsb", "en-CA", "en-GB", "es-ES", "eu", "fr", "fy-NL", "ga-IE", "gd", "gl", "hsb", "it", "lij", "nl", "rm", "wo"],
      },
    }],
  },
});

add_task(async function setup() {
  await test.setup();
});

add_task(async function test_searchConfig_ebay() {
  await test.run();
});
