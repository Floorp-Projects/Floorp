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
        locales: {
          matches: ["ru", "tr", "be", "kk"],
          startsWith: ["en"],
        },
      },
    ],
  },
  available: {
    included: [
      {
        locales: {
          matches: ["az", "ru", "be", "kk", "tr"],
        },
      },
      {
        regions: ["ru", "tr", "by", "kz"],
        locales: {
          startsWith: ["en"],
        },
      },
    ],
  },
  details: [
    {
      included: [{ locales: { matches: ["az"] } }],
      domain: "yandex.az",
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
      codes: {
        searchbar: "clid=2186618",
        keyword: "clid=2186621",
        contextmenu: "clid=2186623",
        homepage: "clid=2186617",
        newtab: "clid=2186620",
      },
    },
    {
      included: [{ locales: { matches: ["ru"] } }],
      domain: "yandex.ru",
      codes: {
        searchbar: "clid=2186618",
        keyword: "clid=2186621",
        contextmenu: "clid=2186623",
        homepage: "clid=2186617",
        newtab: "clid=2186620",
      },
    },
    {
      included: [{ locales: { matches: ["be"] } }],
      domain: "yandex.by",
      codes: {
        searchbar: "clid=2186618",
        keyword: "clid=2186621",
        contextmenu: "clid=2186623",
        homepage: "clid=2186617",
        newtab: "clid=2186620",
      },
    },
    {
      included: [{ locales: { matches: ["kk"] } }],
      domain: "yandex.kz",
      codes: {
        searchbar: "clid=2186618",
        keyword: "clid=2186621",
        contextmenu: "clid=2186623",
        homepage: "clid=2186617",
        newtab: "clid=2186620",
      },
    },
    {
      included: [{ locales: { matches: ["tr"] } }],
      domain: "yandex.com.tr",
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

add_task(async function setup() {
  await test.setup();
});

add_task(async function test_searchConfig_yandex() {
  await test.run(false);
  await test.run(true);
});
