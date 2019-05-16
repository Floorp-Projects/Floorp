/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "yandex",
  default: {
    included: [{
      regions: [
        "ru", "tr", "by", "kz",
      ],
      locales: {
        matches: ["ru", "tr", "be", "kk"],
        // We don't currently enforce this.
        // startsWith: ["en"],
      },
    }],
  },
  available: {
    included: [{
      locales: {
        matches: ["az", "ru", "be", "kk", "tr"],
      },
    }],
  },
  domains: {
    "yandex.az": {
      included: [{
        locales: { matches: ["az"] },
      }],
    },
    "yandex.com": {
      included: [{
        locales: { startsWith: ["en"] },
      }],
    },
    "yandex.ru": {
      included: [{
        locales: { matches: ["ru"] },
      }],
    },
    "yandex.by": {
      included: [{
        locales: { matches: ["be"] },
      }],
    },
    "yandex.kz": {
      included: [{
        locales: { matches: ["kk"] },
      }],
    },
    "yandex.com.tr": {
      included: [{
        locales: { matches: ["tr"] },
      }],
    },
  },
});

add_task(async function setup() {
  await test.setup();
});

add_task(async function test_searchConfig_yandex() {
  await test.run();
});
