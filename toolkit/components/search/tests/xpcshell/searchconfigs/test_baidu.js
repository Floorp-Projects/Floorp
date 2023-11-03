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
        locales: ["zh-CN"],
      },
    ],
  },
  available: {
    included: [
      {
        locales: ["zh-CN"],
      },
    ],
  },
  details: [
    {
      included: [{}],
      domain: "baidu.com",
      telemetryId: "baidu",
    },
  ],
});

add_setup(async function () {
  await test.setup();
});

add_task(async function test_searchConfig_baidu() {
  await test.run();
});
