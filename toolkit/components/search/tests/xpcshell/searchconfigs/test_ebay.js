/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DOMAIN_LOCALES = {
  "ebay-ca": ["en-CA"],
  "ebay-ch": ["rm"],
  "ebay-de": ["de", "dsb", "hsb"],
  "ebay-es": ["an", "ast", "ca", "ca-valencia", "es-ES", "eu", "gl"],
  "ebay-ie": ["ga-IE", "ie"],
  "ebay-it": ["fur", "it", "lij", "sc"],
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
        locales: [
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
          "fur",
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
          "sc",
          "wo",
        ],
      },
      {
        regions: ["au", "be", "ca", "ch", "gb", "ie", "nl", "us"],
        locales: ["en-US"],
      },
      {
        regions: ["gb"],
        locales: ["sco"],
      },
    ],
  },
  suggestionUrlBase: "https://autosug.ebay.com/autosug",
  details: [
    {
      // Note: These should be based on region, but we don't currently enforce that.
      // Note: the order here is important. A region/locale match higher up in the
      // list will override a region/locale match lower down.
      domain: "www.befr.ebay.be",
      telemetryId: "ebay-be",
      included: [
        {
          regions: ["be"],
          locales: ["br", "unknown", "en-US", "fr", "fy-NL", "nl", "wo"],
        },
      ],
      searchUrlCode: "mkrid=1553-53471-19255-0",
      suggestUrlCode: "sId=23",
    },
    {
      domain: "www.ebay.at",
      telemetryId: "ebay-at",
      included: [
        {
          regions: ["at"],
          locales: ["de", "dsb", "hsb"],
        },
      ],
      searchUrlCode: "mkrid=5221-53469-19255-0",
      suggestUrlCode: "sId=16",
    },
    {
      domain: "www.ebay.ca",
      telemetryId: "ebay-ca",
      included: [
        {
          locales: DOMAIN_LOCALES["ebay-ca"],
        },
        {
          regions: ["ca"],
        },
      ],
      excluded: [
        {
          locales: [
            ...DOMAIN_LOCALES["ebay-ch"],
            ...DOMAIN_LOCALES["ebay-de"],
            ...DOMAIN_LOCALES["ebay-es"],
            ...DOMAIN_LOCALES["ebay-ie"],
            ...DOMAIN_LOCALES["ebay-it"],
            ...DOMAIN_LOCALES["ebay-nl"],
            ...DOMAIN_LOCALES["ebay-uk"],
          ],
        },
      ],
      searchUrlCode: "mkrid=706-53473-19255-0",
      suggestUrlCode: "sId=2",
    },
    {
      domain: "www.ebay.ch",
      telemetryId: "ebay-ch",
      included: [
        {
          locales: DOMAIN_LOCALES["ebay-ch"],
        },
        {
          regions: ["ch"],
        },
      ],
      excluded: [
        {
          locales: [
            ...DOMAIN_LOCALES["ebay-ca"],
            ...DOMAIN_LOCALES["ebay-es"],
            ...DOMAIN_LOCALES["ebay-ie"],
            ...DOMAIN_LOCALES["ebay-it"],
            ...DOMAIN_LOCALES["ebay-nl"],
            ...DOMAIN_LOCALES["ebay-uk"],
          ],
        },
      ],
      searchUrlCode: "mkrid=5222-53480-19255-0",
      suggestUrlCode: "sId=193",
    },
    {
      domain: "www.ebay.com",
      telemetryId: "ebay",
      included: [
        {
          locales: ["unknown", "en-US"],
        },
      ],
      excluded: [{ regions: ["au", "be", "ca", "ch", "gb", "ie", "nl"] }],
      searchUrlCode: "mkrid=711-53200-19255-0",
      suggestUrlCode: "sId=0",
    },
    {
      domain: "www.ebay.com.au",
      telemetryId: "ebay-au",
      included: [
        {
          regions: ["au"],
          locales: ["cy", "unknown", "en-GB", "en-US", "gd"],
        },
      ],
      searchUrlCode: "mkrid=705-53470-19255-0",
      suggestUrlCode: "sId=15",
    },
    {
      domain: "www.ebay.ie",
      telemetryId: "ebay-ie",
      included: [
        {
          locales: DOMAIN_LOCALES["ebay-ie"],
        },
        {
          regions: ["ie"],
          locales: ["cy", "unknown", "en-GB", "en-US", "gd"],
        },
      ],
      searchUrlCode: "mkrid=5282-53468-19255-0",
      suggestUrlCode: "sId=205",
    },
    {
      domain: "www.ebay.co.uk",
      telemetryId: "ebay-uk",
      included: [
        {
          locales: DOMAIN_LOCALES["ebay-uk"],
        },
        {
          locales: ["unknown", "en-US", "sco"],
          regions: ["gb"],
        },
      ],
      excluded: [{ regions: ["au", "ie"] }],
      searchUrlCode: "mkrid=710-53481-19255-0",
      suggestUrlCode: "sId=3",
    },
    {
      domain: "www.ebay.de",
      telemetryId: "ebay-de",
      included: [
        {
          locales: DOMAIN_LOCALES["ebay-de"],
        },
      ],
      excluded: [{ regions: ["at", "ch"] }],
      searchUrlCode: "mkrid=707-53477-19255-0",
      suggestUrlCode: "sId=77",
    },
    {
      domain: "www.ebay.es",
      telemetryId: "ebay-es",
      included: [
        {
          locales: DOMAIN_LOCALES["ebay-es"],
        },
      ],
      searchUrlCode: "mkrid=1185-53479-19255-0",
      suggestUrlCode: "sId=186",
    },
    {
      domain: "www.ebay.fr",
      telemetryId: "ebay-fr",
      included: [
        {
          locales: ["br", "fr", "wo"],
        },
      ],
      excluded: [{ regions: ["be", "ca", "ch"] }],
      searchUrlCode: "mkrid=709-53476-19255-0",
      suggestUrlCode: "sId=71",
    },
    {
      domain: "www.ebay.it",
      telemetryId: "ebay-it",
      included: [
        {
          locales: DOMAIN_LOCALES["ebay-it"],
        },
      ],
      searchUrlCode: "mkrid=724-53478-19255-0",
      suggestUrlCode: "sId=101",
    },
    {
      domain: "www.ebay.nl",
      telemetryId: "ebay-nl",
      included: [
        {
          locales: DOMAIN_LOCALES["ebay-nl"],
        },
        {
          locales: ["unknown", "en-US"],
          regions: ["nl"],
        },
      ],
      excluded: [{ regions: ["be"] }],
      searchUrlCode: "mkrid=1346-53482-19255-0",
      suggestUrlCode: "sId=146",
    },
  ],
});

add_setup(async function () {
  await test.setup();
});

add_task(async function test_searchConfig_ebay() {
  await test.run();
});
