/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests to ensure that icons for application provided engines are correctly
 * loaded from remote settings.
 */

"use strict";

// A skeleton configuration that gets filled in from TESTS during `add_setup`.
let CONFIG = [
  {
    recordType: "defaultEngines",
    globalDefault: "engine_no_icon",
    specificDefaults: [],
  },
  {
    recordType: "engineOrders",
    orders: [],
  },
];

let TESTS = [
  {
    engineId: "engine_no_icon",
    expectedIcon: null,
    expectedMimeType: null,
  },
  {
    engineId: "engine_exact_match",
    icons: [
      {
        filename: "remoteIcon.ico",
        engineIdentifiers: ["engine_exact_match"],
        imageSize: 16,
      },
    ],
    expectedIcon: "remoteIcon.ico",
    expectedMimeType: "image/x-icon",
  },
  {
    engineId: "engine_begins_with",
    icons: [
      {
        filename: "remoteIcon.ico",
        engineIdentifiers: ["engine_begins*"],
        imageSize: 16,
      },
    ],
    expectedIcon: "remoteIcon.ico",
    expectedMimeType: "image/x-icon",
  },
  {
    engineId: "engine_non_default_sized_icon",
    icons: [
      {
        filename: "remoteIcon.ico",
        engineIdentifiers: [
          // This also tests multiple engine idenifiers works.
          "enterprise_shuttle",
          "engine_non_default_sized_icon",
        ],
        imageSize: 32,
      },
    ],
    expectedIcon: "remoteIcon.ico",
    expectedMimeType: "image/x-icon",
  },
  {
    engineId: "engine_multiple_icons",
    icons: [
      {
        filename: "bigIcon.ico",
        engineIdentifiers: ["engine_multiple_icons"],
        imageSize: 16,
      },
      {
        filename: "remoteIcon.ico",
        engineIdentifiers: ["engine_multiple_icons"],
        imageSize: 32,
      },
    ],
    expectedIcon: "bigIcon.ico",
    expectedMimeType: "image/x-icon",
  },
  {
    engineId: "engine_svg_icon",
    icons: [
      {
        filename: "svgIcon.svg",
        engineIdentifiers: ["engine_svg_icon"],
        imageSize: 16,
      },
    ],
    expectedIcon: "svgIcon.svg",
    expectedMimeType: "image/svg+xml",
  },
];

add_setup(async function () {
  let client = RemoteSettings("search-config-icons");
  let db = client.db;

  await db.clear();

  for (let test of TESTS) {
    CONFIG.push({
      identifier: test.engineId,
      recordType: "engine",
      base: {
        name: test.engineId + " name",
        urls: {
          search: {
            base: "https://example.com/" + test.engineId,
            searchTermParamName: "q",
          },
        },
      },
      variants: [{ environment: { allRegionsAndLocales: true } }],
    });

    if ("icons" in test) {
      for (let icon of test.icons) {
        await insertRecordIntoCollection(client, { ...icon });
      }
    }
  }

  await SearchTestUtils.useTestEngines("simple-engines", null, CONFIG);
  await Services.search.init();
});

for (let test of TESTS) {
  add_task(async function () {
    info("Testing engine: " + test.engineId);

    let engine = Services.search.getEngineByName(test.engineId + " name");
    if (test.expectedIcon) {
      let engineIconURL = await engine.getIconURL(16);
      Assert.notEqual(
        engineIconURL,
        null,
        "Should have an icon URL for the engine."
      );

      let response = await fetch(engineIconURL);
      let buffer = new Uint8Array(await response.arrayBuffer());

      let expectedBuffer = new Uint8Array(
        await getFileDataBuffer(test.expectedIcon)
      );

      Assert.equal(
        buffer.length,
        expectedBuffer.length,
        "Should have received matching buffer lengths for the expected icon"
      );
      Assert.ok(
        buffer.every((value, index) => value === expectedBuffer[index]),
        "Should have received matching data for the expected icon"
      );

      let contentType = response.headers.get("content-type");

      Assert.equal(
        contentType,
        test.expectedMimeType,
        "Should have received matching MIME types for the expected icon"
      );
    } else {
      Assert.equal(
        await engine.getIconURL(),
        null,
        "Should not have an icon URL for the engine."
      );
    }
  });
}
