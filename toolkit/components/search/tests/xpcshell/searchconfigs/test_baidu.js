/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "baidu",
  aliases: ["@\u767E\u5EA6", "@baidu"],
  default: {
    included: [
      {
        regions: ["cn"],
        locales: {
          matches: ["zh-CN"],
        },
      },
    ],
  },
  available: {
    included: [
      {
        locales: {
          matches: ["zh-CN"],
        },
      },
    ],
  },
  details: [
    {
      included: [{}],
      domain: "baidu.com",
      telemetryId: "baidu",
      searchUrlCode: "tn=monline_7_dg",
      suggestUrlCode: "tn=monline_7_dg",
    },
  ],
});

add_task(async function setup() {
  await test.setup();
});

add_task(async function test_searchConfig_baidu() {
  await test.run(false);
  await test.run(true);
});
