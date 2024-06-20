/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "bing",
  aliases: ["@bing"],
  default: {
    // Not included anywhere.
  },
  available: SearchUtils.newSearchConfigEnabled
    ? {
        excluded: [
          // Should be available everywhere.
        ],
      }
    : {
        included: [
          {
            // regions: [
            // These arent currently enforced.
            // "au", "at", "be", "br", "ca", "fi", "fr", "de",
            // "in", "ie", "it", "jp", "my", "mx", "nl", "nz",
            // "no", "sg", "es", "se", "ch", "gb", "us",
            // ],
            locales: [
              "ach",
              "af",
              "an",
              "ar",
              "ast",
              "az",
              "bn",
              "bs",
              "ca",
              "ca-valencia",
              "cak",
              "cs",
              "cy",
              "da",
              "de",
              "dsb",
              "el",
              "en-CA",
              "en-GB",
              "en-US",
              "eo",
              "es-CL",
              "es-ES",
              "es-MX",
              "eu",
              "fa",
              "ff",
              "fi",
              "fr",
              "fur",
              "fy-NL",
              "gd",
              "gl",
              "gn",
              "gu-IN",
              "he",
              "hi-IN",
              "hr",
              "hsb",
              "hy-AM",
              "ia",
              "id",
              "is",
              "it",
              "ja-JP-macos",
              "ja",
              "ka",
              "kab",
              "km",
              "kn",
              "lij",
              "lo",
              "lt",
              "meh",
              "mk",
              "ms",
              "my",
              "nb-NO",
              "ne-NP",
              "nl",
              "nn-NO",
              "oc",
              "pa-IN",
              "pt-BR",
              "rm",
              "ro",
              "sc",
              "sco",
              "son",
              "sq",
              "sr",
              "sv-SE",
              "te",
              "th",
              "tl",
              "tr",
              "trs",
              "uk",
              "ur",
              "uz",
              "wo",
              "xh",
              "zh-CN",
            ],
          },
        ],
      },
  details: [
    {
      included: [{}],
      domain: "bing.com",
      telemetryId:
        SearchUtils.MODIFIED_APP_CHANNEL == "esr" ? "bing-esr" : "bing",
      codes: SearchUtils.newSearchConfigEnabled
        ? {
            searchbar: "form=MOZLBR",
            keyword: "form=MOZLBR",
            contextmenu: "form=MOZLBR",
            homepage: "form=MOZLBR",
            newtab: "form=MOZLBR",
          }
        : {
            searchbar: "form=MOZSBR",
            keyword: "form=MOZLBR",
            contextmenu: "form=MOZCON",
            homepage: "form=MOZSPG",
            newtab: "form=MOZTSB",
          },
      searchUrlCode:
        SearchUtils.MODIFIED_APP_CHANNEL == "esr" ? "pc=MOZR" : "pc=MOZI",
    },
  ],
});

add_setup(async function () {
  await test.setup();
});

add_task(async function test_searchConfig_bing() {
  await test.run();
});
