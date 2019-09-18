/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "bing",
  aliases: ["@bing"],
  default: {
    // Not included anywhere.
  },
  available: {
    included: [
      {
        // regions: [
        // These arent currently enforced.
        // "au", "at", "be", "br", "ca", "fi", "fr", "de",
        // "in", "ie", "it", "jp", "my", "mx", "nl", "nz",
        // "no", "sg", "es", "se", "ch", "gb", "us",
        // ],
        locales: {
          matches: [
            "ach",
            "af",
            "an",
            "ar",
            "ast",
            "az",
            "ca",
            "ca-valencia",
            "cak",
            "da",
            "de",
            "dsb",
            "el",
            "eo",
            "es-CL",
            "es-ES",
            "es-MX",
            "eu",
            "fa",
            "ff",
            "fi",
            "fr",
            "fy-NL",
            "gn",
            "gu-IN",
            "hi-IN",
            "hr",
            "hsb",
            "ia",
            "is",
            "it",
            "ja-JP-mac",
            "ja",
            "ka",
            "kab",
            "km",
            "kn",
            "lij",
            "lo",
            "lt",
            "mai",
            "mk",
            "ml",
            "ms",
            "my",
            "nb-NO",
            "ne-NP",
            "nl",
            "nn-NO",
            "oc",
            "or",
            "pa-IN",
            "pt-BR",
            "rm",
            "ro",
            "son",
            "sq",
            "sr",
            "sv-SE",
            "th",
            "tl",
            "trs",
            "uk",
            "ur",
            "uz",
            "wo",
            "xh",
            "zh-CN",
          ],
          startsWith: ["bn", "en"],
        },
      },
    ],
  },
  details: [
    {
      included: [{}],
      domain: "bing.com",
      codes: {
        searchbar: "form=MOZSBR",
        keyword: "form=MOZLBR",
        contextmenu: "form=MOZCON",
        homepage: "form=MOZSPG",
        newtab: "form=MOZTSB",
      },
      searchUrlCode: "pc=MOZI",
      searchFormUrlCode: "pc=MOZI",
    },
  ],
});

add_task(async function setup() {
  await test.setup();
});

add_task(async function test_searchConfig_bing() {
  await test.run(false);
  await test.run(true);
});
