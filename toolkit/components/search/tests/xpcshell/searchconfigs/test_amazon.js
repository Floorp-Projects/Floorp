/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const mainShippedRegions = [
  "at",
  "au",
  "be",
  "ca",
  "ch",
  "cn",
  "de",
  "es",
  "fr",
  "mc",
  "gb",
  "ie",
  "it",
  "jp",
  "nl",
  "pt",
  "se",
  "sm",
  "us",
  "va",
];

const amazondotcomLocales = [
  "ach",
  "af",
  "ar",
  "az",
  "bg",
  "cak",
  "cy",
  "da",
  "el",
  "en-US",
  "en-GB",
  "eo",
  "es-AR",
  "eu",
  "fa",
  "ga-IE",
  "gd",
  "gl",
  "gn",
  "hr",
  "hy-AM",
  "ia",
  "is",
  "ka",
  "km",
  "lt",
  "mk",
  "ms",
  "my",
  "nb-NO",
  "nn-NO",
  "pt-PT",
  "ro",
  "si",
  "sq",
  "sr",
  "th",
  "tl",
  "trs",
  "uz",
];

const test = new SearchConfigTest({
  identifier: "amazon",
  default: {
    // Not default anywhere.
  },
  available: {
    included: [
      {
        // The main regions we ship Amazon to. Below this are special cases.
        regions: mainShippedRegions,
      },
      {
        // Amazon.com ships to all of these locales, excluding the ones where
        // we ship other items, but it does not matter that they are duplicated
        // in the available list.
        locales: {
          matches: amazondotcomLocales,
        },
      },
      {
        // Amazon.in
        regions: ["in"],
        locales: {
          matches: ["bn", "gu-IN", "kn", "mr", "pa-IN", "ta", "te", "ur"],
        },
      },
    ],
    excluded: [
      {
        // Extra special case for cn as that only ships to the one locale.
        regions: ["in"],
        locales: {
          matches: amazondotcomLocales,
        },
      },
    ],
  },
  details: [
    {
      domain: "amazon.com.au",
      telemetryId: "amazon-au",
      aliases: ["@amazon"],
      included: [
        {
          regions: ["au"],
        },
      ],
      suggestionUrlBase: "https://completion.amazon.com.au/search/complete",
      suggestUrlCode: "mkt=111172",
    },
    {
      domain: "amazon.ca",
      telemetryId: "amazon-ca",
      aliases: ["@amazon"],
      included: [
        {
          regions: ["ca"],
        },
      ],
      searchUrlCode: "tag=mozillacanada-20",
      suggestionUrlBase: "https://completion.amazon.ca/search/complete",
      suggestUrlCode: "mkt=7",
    },
    {
      domain: "amazon.cn",
      telemetryId: "amazondotcn",
      included: [
        {
          regions: ["cn"],
        },
      ],
      searchUrlCode: "ix=sunray",
      noSuggestionsURL: true,
    },
    {
      domain: "amazon.co.jp",
      telemetryId: "amazon-jp",
      aliases: ["@amazon"],
      included: [
        {
          regions: ["jp"],
        },
      ],
      searchUrlCode: "tag=mozillajapan-fx-22",
      suggestionUrlBase: "https://completion.amazon.co.jp/search/complete",
      suggestUrlCode: "mkt=6",
    },
    {
      domain: "amazon.co.uk",
      telemetryId: "amazon-en-GB",
      aliases: ["@amazon"],
      included: [
        {
          regions: ["gb", "ie"],
        },
      ],
      searchUrlCode: "tag=firefox-uk-21",
      suggestionUrlBase: "https://completion.amazon.co.uk/search/complete",
      suggestUrlCode: "mkt=3",
    },
    {
      domain: "amazon.com",
      telemetryId: "amazondotcom-us",
      aliases: ["@amazon"],
      included: [
        {
          regions: ["us"],
        },
      ],
      searchUrlCode: "tag=moz-us-20",
    },
    {
      domain: "amazon.com",
      telemetryId: "amazondotcom",
      aliases: ["@amazon"],
      included: [
        {
          locales: {
            matches: amazondotcomLocales,
          },
        },
      ],
      excluded: [{ regions: mainShippedRegions }],
      searchUrlCode: "tag=mozilla-20",
    },
    {
      domain: "amazon.de",
      telemetryId: "amazon-de",
      aliases: ["@amazon"],
      included: [
        {
          regions: ["at", "ch", "de"],
        },
      ],
      searchUrlCode: "tag=firefox-de-21",
      suggestionUrlBase: "https://completion.amazon.de/search/complete",
      suggestUrlCode: "mkt=4",
    },
    {
      domain: "amazon.es",
      telemetryId: "amazon-es",
      aliases: ["@amazon"],
      included: [
        {
          regions: ["es", "pt"],
        },
      ],
      searchUrlCode: "tag=mozillaspain-21",
      suggestionUrlBase: "https://completion.amazon.es/search/complete",
      suggestUrlCode: "mkt=44551",
    },
    {
      domain: "amazon.fr",
      telemetryId: "amazon-france",
      aliases: ["@amazon"],
      included: [
        {
          regions: ["fr", "mc"],
        },
        {
          regions: ["be"],
          locales: {
            matches: ["fr"],
          },
        },
      ],
      searchUrlCode: "tag=firefox-fr-21",
      suggestionUrlBase: "https://completion.amazon.fr/search/complete",
      suggestUrlCode: "mkt=5",
    },
    {
      domain: "amazon.in",
      telemetryId: "amazon-in",
      aliases: ["@amazon"],
      included: [
        {
          locales: {
            matches: ["bn", "gu-IN", "kn", "mr", "pa-IN", "ta", "te", "ur"],
          },
          regions: ["in"],
        },
      ],
      suggestionUrlBase: "https://completion.amazon.in/search/complete",
      suggestUrlCode: "mkt=44571",
    },
    {
      domain: "amazon.it",
      telemetryId: "amazon-it",
      aliases: ["@amazon"],
      included: [
        {
          regions: ["it", "sm", "va"],
        },
      ],
      searchUrlCode: "tag=firefoxit-21",
      suggestionUrlBase: "https://completion.amazon.it/search/complete",
      suggestUrlCode: "mkt=35691",
    },
    {
      domain: "amazon.nl",
      telemetryId: "amazon-nl",
      aliases: ["@amazon"],
      included: [
        {
          regions: ["nl"],
        },
      ],
      searchUrlCode: "tag=mozillanether-21",
      suggestionUrlBase: "https://completion.amazon.nl/search/complete",
      suggestUrlCode: "mkt=328451",
    },
    {
      domain: "amazon.nl",
      telemetryId: "amazon-nl",
      aliases: ["@amazon"],
      included: [
        {
          regions: ["be"],
        },
      ],
      excluded: [
        {
          locales: {
            matches: ["fr"],
          },
        },
      ],
      searchUrlCode: "tag=mozillanether-21",
      suggestionUrlBase: "https://completion.amazon.nl/search/complete",
      suggestUrlCode: "mkt=328451",
    },
    {
      domain: "amazon.se",
      telemetryId: "amazon-se",
      aliases: ["@amazon"],
      included: [
        {
          regions: ["se"],
        },
      ],
      searchUrlCode: "tag=mozillasweede-21",
      suggestionUrlBase: "https://completion.amazon.se/search/complete",
      suggestUrlCode: "mkt=704403121",
    },
  ],
});

add_task(async function setup() {
  // We only need to do setup on one of the tests.
  await test.setup("89.0");
});

add_task(async function test_searchConfig_amazon() {
  await test.run();
});

add_task(async function test_searchConfig_amazon_pre89() {
  AddonTestUtils.createAppInfo(
    "xpcshell@tests.mozilla.org",
    "XPCShell",
    "88.0",
    "88.0"
  );
  // For pre-89, Amazon has a slightly different config.
  let details = test._config.details.find(
    d => d.telemetryId == "amazondotcom-us"
  );
  details.telemetryId = "amazondotcom";
  details.searchUrlCode = "tag=mozilla-20";

  // nl not present due to urls that don't work.
  let availableIn = test._config.available.included;
  availableIn[0].regions = availableIn[0].regions.filter(
    r => r != "be" && r != "nl"
  );
  availableIn.push({
    regions: ["be"],
    locales: {
      matches: ["fr"],
    },
  });
  // Due to the way the exclusions work, no Amazon present in nl/be in the
  // dot com locales for pre-89.
  test._config.available.excluded[0].regions.push("be", "nl");
  test._config.details = test._config.details.filter(
    d => d.telemetryId != "amazon-nl"
  );

  await test.run();
});
