/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const test = new SearchConfigTest({
  identifier: "google",
  aliases: ["@google"],
  default: {
    // Included everywhere apart from the exclusions below. These are basically
    // just excluding what Yandex and Baidu include.
    excluded: [
      {
        regions: ["ru", "tr", "by", "kz"],
        locales: {
          matches: ["ru", "tr", "be", "kk"],
          startsWith: ["en"],
        },
      },
      {
        regions: ["cn"],
        locales: {
          matches: ["zh-CN"],
        },
      },
    ],
  },
  available: {
    excluded: [
      // Should be available everywhere.
    ],
  },
  details: [
    {
      included: [{ regions: ["us"] }],
      domain: "google.com",
      codes: AppConstants.MOZ_APP_VERSION_DISPLAY.endsWith("esr")
        ? "client=firefox-b-1-e"
        : "client=firefox-b-1-d",
    },
    {
      excluded: [{ regions: ["us"] }],
      included: [],
      domain: "google.com",
      codes: AppConstants.MOZ_APP_VERSION_DISPLAY.endsWith("esr")
        ? "client=firefox-b-e"
        : "client=firefox-b-d",
    },
  ],
});

add_task(async function setup() {
  await test.setup();
});

add_task(async function test_searchConfig_google() {
  await test.run(false);
  await test.run(true);
});
