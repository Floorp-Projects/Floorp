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
  // domains: {
  //   // Note: These should be based on region, but we don't currently enforce that.
  //   "ebay.com": {
  //     locales: { matches: ["en-US"] },
  //   },
  //   "ebay.co.uk": {
  //     locales: { matches: ["en-GB"] },
  //   },
  //   "ebay.ca": {
  //     locales: { matches: ["en-CA"] },
  //   },
  //   "ebay.ie": {
  //     locales: { matches: ["ie"] },
  //   },
  //   "ebay.fr": {
  //     locales: { matches: ["fr"] },
  //   },
  //   "ebay.it": {
  //     locales: { matches: ["it"] },
  //   },
  //   "ebay.de": {
  //     locales: { matches: ["de"] },
  //   },
  //   "ebay.at": {
  //     locales: { matches: ["ast"] },
  //   },
  //   "ebay.es": {
  //     locales: { matches: ["an", "es-ES"] },
  //   },
  //   "ebay.nl": {
  //     locales: { matches: ["nl"] },
  //   },
  //   "ebay.ch": {
  //     locales: { matches: ["rm"] },
  //   },
  //   "ebay.au": {
  //     regions: ["au"],
  //   },
  // },
  // noSuggestionsURL: true,
});

add_task(async function setup() {
  await test.setup();
});

add_task(async function test_searchConfig_ebay() {
  await test.run();
});
