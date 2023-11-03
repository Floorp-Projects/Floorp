/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "yandex",
  aliases: ["@\u044F\u043D\u0434\u0435\u043A\u0441", "@yandex"],
  default: {
    included: [
      {
        regions: ["ru", "tr", "by", "kz"],
        locales: ["ru", "tr", "be", "kk", "en-CA", "en-GB", "en-US"],
      },
    ],
  },
  available: {
    included: [
      {
        locales: ["az", "ru", "be", "kk", "tr"],
      },
      {
        regions: ["ru", "tr", "by", "kz"],
        locales: ["en-CA", "en-GB", "en-US"],
      },
    ],
  },
  details: [
    {
      included: [{ locales: ["az"] }],
      domain: "yandex.az",
      telemetryId: "yandex-az",
      codes: {
        searchbar: "clid=2186618",
        keyword: "clid=2186621",
        contextmenu: "clid=2186623",
        homepage: "clid=2186617",
        newtab: "clid=2186620",
      },
    },
    {
      included: [{ locales: { startsWith: ["en"] } }],
      domain: "yandex.com",
      telemetryId: "yandex-en",
      codes: {
        searchbar: "clid=2186618",
        keyword: "clid=2186621",
        contextmenu: "clid=2186623",
        homepage: "clid=2186617",
        newtab: "clid=2186620",
      },
    },
    {
      included: [{ locales: ["ru"] }],
      domain: "yandex.ru",
      telemetryId: "yandex-ru",
      codes: {
        searchbar: "clid=2186618",
        keyword: "clid=2186621",
        contextmenu: "clid=2186623",
        homepage: "clid=2186617",
        newtab: "clid=2186620",
      },
    },
    {
      included: [{ locales: ["be"] }],
      domain: "yandex.by",
      telemetryId: "yandex-by",
      codes: {
        searchbar: "clid=2186618",
        keyword: "clid=2186621",
        contextmenu: "clid=2186623",
        homepage: "clid=2186617",
        newtab: "clid=2186620",
      },
    },
    {
      included: [{ locales: ["kk"] }],
      domain: "yandex.kz",
      telemetryId: "yandex-kk",
      codes: {
        searchbar: "clid=2186618",
        keyword: "clid=2186621",
        contextmenu: "clid=2186623",
        homepage: "clid=2186617",
        newtab: "clid=2186620",
      },
    },
    {
      included: [{ locales: ["tr"] }],
      domain: "yandex.com.tr",
      telemetryId: "yandex-tr",
      codes: {
        searchbar: "clid=2186618",
        keyword: "clid=2186621",
        contextmenu: "clid=2186623",
        homepage: "clid=2186617",
        newtab: "clid=2186620",
      },
    },
  ],
});

add_setup(async function () {
  await test.setup();
});

add_task(async function test_searchConfig_yandex() {
  await test.run();
}).skip();
