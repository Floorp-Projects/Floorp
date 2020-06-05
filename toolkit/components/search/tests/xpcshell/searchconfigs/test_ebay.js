/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const availableRegions = [
  ...Services.intl.getAvailableLocaleDisplayNames("region"),
  null,
];

const DOMAIN_LOCALES = {
  "ebay-ca": ["en-CA"],
  "ebay-ch": ["rm"],
  "ebay-de": ["de", "dsb", "hsb"],
  "ebay-es": ["an", "ast", "ca", "ca-valencia", "es-ES", "eu", "gl"],
  "ebay-ie": ["ga-IE", "ie"],
  "ebay-it": ["it", "lij"],
  "ebay-nl": ["fy-NL", "nl"],
  "ebay-uk": ["cy", "en-GB", "gd"],
};

const test = new SearchConfigTest({
  identifier: "ebay",
  aliases: ["@ebay"],
  default: {
    // Not included anywhere.
  },
  available: {
    included: [
      {
        // We don't currently enforce by region, but do locale instead.
        // regions: [
        //   "us", "gb", "ca", "ie", "fr", "it", "de", "at", "es", "nl", "ch", "au"
        // ],
        locales: {
          matches: [
            "an",
            "ast",
            "br",
            "ca",
            "ca-valencia",
            "cy",
            "de",
            "dsb",
            "en-CA",
            "en-GB",
            "es-ES",
            "eu",
            "fr",
            "fy-NL",
            "ga-IE",
            "gd",
            "gl",
            "hsb",
            "it",
            "lij",
            "nl",
            "rm",
            "wo",
          ],
        },
      },
      {
        regions: ["au", "be", "ca", "ch", "gb", "ie", "nl", "us"],
        locales: {
          matches: ["en-US"],
        },
      },
    ],
  },
  searchUrlBase: "https://rover.ebay.com/rover/1/",
  suggestionUrlBase: "https://autosug.ebay.com/autosug",
  details: [
    {
      // Note: These should be based on region, but we don't currently enforce that.
      // Note: the order here is important. A region/locale match higher up in the
      // list will override a region/locale match lower down.
      domain: "befr.ebay.be",
      telemetryId: "ebay-be",
      included: [
        {
          regions: ["be"],
          locales: {
            matches: ["br", "unknown", "en-US", "fr", "fy-NL", "nl", "wo"],
          },
        },
      ],
      searchUrlEnd: "1553-53471-19255-0/1",
      suggestUrlCode: "sId=23",
    },
    {
      domain: "ebay.at",
      telemetryId: "ebay-at",
      included: [
        {
          regions: ["at"],
          locales: { matches: ["de", "dsb", "hsb"] },
        },
      ],
      searchUrlEnd: "5221-53469-19255-0/1",
      suggestUrlCode: "sId=16",
    },
    {
      domain: "ebay.ca",
      telemetryId: "ebay-ca",
      included: [
        {
          locales: { matches: DOMAIN_LOCALES["ebay-ca"] },
        },
        {
          regions: ["ca"],
        },
      ],
      excluded: [
        {
          locales: {
            matches: [
              ...DOMAIN_LOCALES["ebay-ch"],
              ...DOMAIN_LOCALES["ebay-de"],
              ...DOMAIN_LOCALES["ebay-es"],
              ...DOMAIN_LOCALES["ebay-ie"],
              ...DOMAIN_LOCALES["ebay-it"],
              ...DOMAIN_LOCALES["ebay-nl"],
              ...DOMAIN_LOCALES["ebay-uk"],
            ],
          },
        },
      ],
      searchUrlEnd: "706-53473-19255-0/1",
      suggestUrlCode: "sId=2",
    },
    {
      domain: "ebay.ch",
      telemetryId: "ebay-ch",
      included: [
        {
          locales: { matches: DOMAIN_LOCALES["ebay-ch"] },
        },
        {
          regions: ["ch"],
        },
      ],
      excluded: [
        {
          locales: {
            matches: [
              ...DOMAIN_LOCALES["ebay-ca"],
              ...DOMAIN_LOCALES["ebay-es"],
              ...DOMAIN_LOCALES["ebay-ie"],
              ...DOMAIN_LOCALES["ebay-it"],
              ...DOMAIN_LOCALES["ebay-nl"],
              ...DOMAIN_LOCALES["ebay-uk"],
            ],
          },
        },
      ],
      searchUrlEnd: "5222-53480-19255-0/1",
      suggestUrlCode: "sId=193",
    },
    {
      domain: "ebay.com",
      telemetryId: "ebay",
      included: [
        {
          locales: { matches: ["unknown", "en-US"] },
        },
      ],
      excluded: [{ regions: ["au", "be", "ca", "ch", "gb", "ie", "nl"] }],
      searchUrlEnd: "711-53200-19255-0/1",
      suggestUrlCode: "sId=0",
    },
    {
      domain: "ebay.com.au",
      telemetryId: "ebay-au",
      included: [
        {
          regions: ["au"],
          locales: { matches: ["cy", "unknown", "en-GB", "en-US", "gd"] },
        },
      ],
      searchUrlEnd: "705-53470-19255-0/1",
      suggestUrlCode: "sId=15",
    },
    {
      domain: "ebay.ie",
      telemetryId: "ebay-ie",
      included: [
        {
          locales: { matches: DOMAIN_LOCALES["ebay-ie"] },
        },
        {
          regions: ["ie"],
          locales: { matches: ["cy", "unknown", "en-GB", "en-US", "gd"] },
        },
      ],
      searchUrlEnd: "5282-53468-19255-0/1",
      suggestUrlCode: "sId=205",
    },
    {
      domain: "ebay.co.uk",
      telemetryId: "ebay-uk",
      included: [
        {
          locales: { matches: DOMAIN_LOCALES["ebay-uk"] },
        },
        {
          locales: { matches: ["unknown", "en-US"] },
          regions: ["gb"],
        },
      ],
      excluded: [{ regions: ["au", "ie"] }],
      searchUrlEnd: "710-53481-19255-0/1",
      suggestUrlCode: "sId=3",
    },
    {
      domain: "ebay.de",
      telemetryId: "ebay-de",
      included: [
        {
          locales: { matches: DOMAIN_LOCALES["ebay-de"] },
        },
      ],
      excluded: [{ regions: ["at", "ch"] }],
      searchUrlEnd: "707-53477-19255-0/1",
      suggestUrlCode: "sId=77",
    },
    {
      domain: "ebay.es",
      telemetryId: "ebay-es",
      included: [
        {
          locales: {
            matches: DOMAIN_LOCALES["ebay-es"],
          },
        },
      ],
      searchUrlEnd: "1185-53479-19255-0/1",
      suggestUrlCode: "sId=186",
    },
    {
      domain: "ebay.fr",
      telemetryId: "ebay-fr",
      included: [
        {
          locales: { matches: ["br", "fr", "wo"] },
        },
      ],
      excluded: [{ regions: ["be", "ca", "ch"] }],
      searchUrlEnd: "709-53476-19255-0/1",
      suggestUrlCode: "sId=71",
    },
    {
      domain: "ebay.it",
      telemetryId: "ebay-it",
      included: [
        {
          locales: { matches: DOMAIN_LOCALES["ebay-it"] },
        },
      ],
      searchUrlEnd: "724-53478-19255-0/1",
      suggestUrlCode: "sId=101",
    },
    {
      domain: "ebay.nl",
      telemetryId: "ebay-nl",
      included: [
        {
          locales: { matches: DOMAIN_LOCALES["ebay-nl"] },
        },
        {
          locales: { matches: ["unknown", "en-US"] },
          regions: ["nl"],
        },
      ],
      excluded: [{ regions: ["be"] }],
      searchUrlEnd: "1346-53482-19255-0/1",
      suggestUrlCode: "sId=146",
    },
  ],
});

add_task(async function setup() {
  await test.setup();
});

add_task(async function test_searchConfig_ebay() {
  await test.run(true);
  // Only applies to the default locale fallback for the legacy config.
  // Note: when we remove the legacy config, we should remove the "unknown"
  // references in the 'details' section of the test above.
  test._config.available.included[0].locales.matches.push("unknown");
  // In the legacy configuration, eBay was turned on for most regions with en-US
  // locale by default, but turned off by abSearch.
  test._config.available.included[1].regions = availableRegions.filter(
    region => !["by", "kz", "ru", "tr"].includes(region)
  );
  await test.run(false);
});
