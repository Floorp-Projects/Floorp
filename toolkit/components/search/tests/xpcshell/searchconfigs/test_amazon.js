/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "amazon",
  default: {
    // Not included anywhere.
  },
  available: {
    included: [{
      // We don't currently enforce by region, but do locale instead.
      // regions: [
      //   "at", "au", "be", "ca", "ch", "de", "fr", "gb", "ie", "it", "jp", "nl",
      //   "us",
      // ],
      locales: {
        matches: [
          "ach", "af", "ar", "as", "az", "bg", "bn", "bn-IN", "br", "cak", "cy",
          "da", "de", "dsb", "el", "eo", "es-AR", "eu", "fa", "ff", "fr",
          "ga-IE", "gd", "gl", "gn", "gu-IN", "hr", "hsb", "hy-AM", "ia", "is",
          "it", "ja-JP-mac", "ja", "ka", "km", "kn", "lij", "lt", "mai", "mk",
          "ml", "mr", "ms", "my", "nb-NO", "nn-NO", "or", "pa-IN", "pt-PT", "ro",
          "si", "son", "sq", "sr", "ta", "te", "th", "tl", "trs", "ur", "uz",
          "wo", "zh-CN",
        ],
        startsWith: ["en"],
      },
    }],
  },
});

add_task(async function setup() {
  // We only need to do setup on one of the tests.
  await test.setup();
});

add_task(async function test_searchConfig_amazon() {
  await test.run();
});
