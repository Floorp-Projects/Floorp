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
        locales: amazondotcomLocales,
      },
      {
        // Amazon.in
        regions: ["in"],
        locales: ["bn", "gu-IN", "kn", "mr", "pa-IN", "ta", "te", "ur"],
      },
    ],
    excluded: [
      {
        // Extra special case for cn as that only ships to the one locale.
        regions: ["in"],
        locales: amazondotcomLocales,
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
      noSuggestionsURL: true,
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
      noSuggestionsURL: true,
    },
    {
      domain: "amazon.cn",
      telemetryId: "amazondotcn",
      included: [
        {
          regions: ["cn"],
        },
      ],
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
      noSuggestionsURL: true,
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
      noSuggestionsURL: true,
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
      noSuggestionsURL: true,
    },
    {
      domain: "amazon.com",
      telemetryId: "amazondotcom",
      aliases: ["@amazon"],
      included: [
        {
          locales: amazondotcomLocales,
        },
      ],
      excluded: [{ regions: mainShippedRegions }],
      noSuggestionsURL: true,
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
      noSuggestionsURL: true,
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
      noSuggestionsURL: true,
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
          locales: ["fr"],
        },
      ],
      noSuggestionsURL: true,
    },
    {
      domain: "amazon.in",
      telemetryId: "amazon-in",
      aliases: ["@amazon"],
      included: [
        {
          locales: ["bn", "gu-IN", "kn", "mr", "pa-IN", "ta", "te", "ur"],
          regions: ["in"],
        },
      ],
      noSuggestionsURL: true,
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
      noSuggestionsURL: true,
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
      noSuggestionsURL: true,
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
          locales: ["fr"],
        },
      ],
      noSuggestionsURL: true,
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
      noSuggestionsURL: true,
    },
  ],
});

add_setup(async function () {
  // We only need to do setup on one of the tests.
  await test.setup("89.0");
});

add_task(async function test_searchConfig_amazon() {
  await test.run();
});

add_task(
  { skip_if: () => SearchUtils.newSearchConfigEnabled },
  async function test_searchConfig_amazon_pre89() {
    const version = "88.0";
    if (SearchUtils.newSearchConfigEnabled) {
      updateAppInfo({
        name: "XPCShell",
        ID: "xpcshell@tests.mozilla.org",
        version,
        platformVersion: version,
      });
    } else {
      AddonTestUtils.createAppInfo(
        "xpcshell@tests.mozilla.org",
        "XPCShell",
        version,
        version
      );
    }
    // For pre-89, Amazon has a slightly different config.
    let details = test._config.details.find(
      d => d.telemetryId == "amazondotcom-us"
    );
    details.telemetryId = "amazondotcom";

    // nl not present due to urls that don't work.
    let availableIn = test._config.available.included;
    availableIn[0].regions = availableIn[0].regions.filter(
      r => r != "be" && r != "nl"
    );
    availableIn.push({
      regions: ["be"],
      locales: ["fr"],
    });
    // Due to the way the exclusions work, no Amazon present in nl/be in the
    // dot com locales for pre-89.
    test._config.available.excluded[0].regions.push("be", "nl");
    test._config.details = test._config.details.filter(
      d => d.telemetryId != "amazon-nl"
    );

    await test.run();
  }
);
