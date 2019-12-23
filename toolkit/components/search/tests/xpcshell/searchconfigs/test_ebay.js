/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let availableRegions = Services.intl.getAvailableLocaleDisplayNames("region");

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
        // For en-US ebay is currently included everywhere apart from these regions.
        regions: availableRegions.filter(
          region => !["by", "kz", "ru", "tr"].includes(region)
        ),
        locales: {
          matches: ["en-US"],
        },
      },
    ],
  },
  searchUrlBase: "https://rover.ebay.com/rover/1/",
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
          locales: { matches: ["br", "en-US", "fr", "fy-NL", "nl", "wo"] },
        },
      ],
      searchUrlEnd: "1553-53471-19255-0/1",
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
    },
    {
      domain: "ebay.com",
      telemetryId: "ebay",
      included: [
        {
          locales: { matches: ["en-US"] },
        },
      ],
      excluded: [{ regions: ["au", "be", "ca", "ch", "gb", "ie", "nl"] }],
      searchUrlEnd: "711-53200-19255-0/1",
    },
    {
      domain: "ebay.com.au",
      telemetryId: "ebay-au",
      included: [
        {
          regions: ["au"],
          locales: { matches: ["cy", "en-GB", "en-US", "gd"] },
        },
      ],
      searchUrlEnd: "705-53470-19255-0/1",
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
          locales: { matches: ["cy", "en-GB", "en-US", "gd"] },
        },
      ],
      searchUrlEnd: "5282-53468-19255-0/1",
    },
    {
      domain: "ebay.co.uk",
      telemetryId: "ebay-uk",
      included: [
        {
          locales: { matches: DOMAIN_LOCALES["ebay-uk"] },
        },
        {
          locales: { matches: ["en-US"] },
          regions: ["gb"],
        },
      ],
      excluded: [{ regions: ["au", "ie"] }],
      searchUrlEnd: "710-53481-19255-0/1",
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
    },
    {
      domain: "ebay.nl",
      telemetryId: "ebay-nl",
      included: [
        {
          locales: { matches: DOMAIN_LOCALES["ebay-nl"] },
        },
        {
          locales: { matches: ["en-US"] },
          regions: ["nl"],
        },
      ],
      excluded: [{ regions: ["be"] }],
      searchUrlEnd: "1346-53482-19255-0/1",
    },
  ],
  noSuggestionsURL: true,
});

add_task(async function setup() {
  await test.setup();
});

add_task(async function test_searchConfig_ebay() {
  await test.run(false);
  await test.run(true);
});
