/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "ebay",
  aliases: ["@ebay"],
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
  searchUrlBase: "https://rover.ebay.com/rover/1/",
  details: [{
    // Note: These should be based on region, but we don't currently enforce that.
    // Note: the order here is important. A region/locale match higher up in the
    // list will override a region/locale match lower down.
    domain: "befr.ebay.be",
    included: [{
      regions: ["be"],
      locales: { matches: ["br", "fr", "fy-NL", "nl", "wo"]},
    }],
    searchUrlEnd: "1553-53471-19255-0/1",
  }, {
    domain: "ebay.at",
    included: [{
      regions: ["at"],
      locales: { matches: ["de", "dsb", "hsb"]},
    }],
    searchUrlEnd: "5221-53469-19255-0/1",
  }, {
    domain: "ebay.ca",
    included: [{
      locales: { matches: ["en-CA"] },
    }, {
      regions: ["ca"],
      locales: { matches: ["br", "fr", "wo"]},
    }],
    searchUrlEnd: "706-53473-19255-0/1",
  }, {
    domain: "ebay.ch",
    included: [{
      locales: { matches: ["rm"] },
    }, {
      regions: ["ch"],
      locales: { matches: ["br", "de", "dsb", "fr", "hsb", "wo"]},
    }],
    searchUrlEnd: "5222-53480-19255-0/1",
  }, {
    domain: "ebay.com",
    included: [{
      locales: { matches: ["en-US"] },
    }],
    excluded: [{regions: ["au"]}],
    searchUrlEnd: "711-53200-19255-0/1",
  }, {
    domain: "ebay.com.au",
    included: [{
      regions: ["au"],
      locales: { matches: ["cy", "en-GB", "gd"]},
    }],
    searchUrlEnd: "705-53470-19255-0/1",
  }, {
    domain: "ebay.ie",
    included: [{
      locales: { matches: ["ga-IE", "ie"] },
    }, {
      regions: ["ie"],
      locales: { matches: ["cy", "en-GB", "gd"]},
    }],
    searchUrlEnd: "5282-53468-19255-0/1",
  }, {
    domain: "ebay.co.uk",
    included: [{
      locales: { matches: ["cy", "en-GB", "gd"] },
    }],
    excluded: [{regions: ["au", "ie"]}],
    searchUrlEnd: "710-53481-19255-0/1",
  }, {
    domain: "ebay.de",
    included: [{
      locales: { matches: ["de", "dsb", "hsb"] },
    }],
    excluded: [{regions: ["at", "ch"]}],
    searchUrlEnd: "707-53477-19255-0/1",
  }, {
    domain: "ebay.es",
    included: [{
      locales: { matches: ["an", "ast", "ca", "es-ES", "eu", "gl"] },
    }],
    searchUrlEnd: "1185-53479-19255-0/1",
  }, {
    domain: "ebay.fr",
    included: [{
      locales: { matches: ["br", "fr", "wo"] },
    }],
    excluded: [{regions: ["be", "ca", "ch"]}],
    searchUrlEnd: "709-53476-19255-0/1",
  }, {
    domain: "ebay.it",
    included: [{
      locales: { matches: ["it", "lij"] },
    }],
    searchUrlEnd: "724-53478-19255-0/1",
  }, {
    domain: "ebay.nl",
    included: [{
      locales: { matches: ["fy-NL", "nl"] },
    }],
    excluded: [{regions: ["be"]}],
    searchUrlEnd: "1346-53482-19255-0/1",
  }],
  noSuggestionsURL: true,
});

add_task(async function setup() {
  await test.setup();
});

add_task(async function test_searchConfig_ebay() {
  await test.run();
});
