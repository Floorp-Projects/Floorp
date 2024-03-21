/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests to ensure that icons for application provided engines are correctly
 * updated from remote settings.
 */

"use strict";

// A skeleton configuration that gets filled in from TESTS during `add_setup`.
let CONFIG = [
  {
    identifier: "engine_no_initial_icon",
    recordType: "engine",
    base: {
      name: "engine_no_initial_icon name",
      urls: {
        search: {
          base: "https://example.com/1",
          searchTermParamName: "q",
        },
      },
    },
    variants: [{ environment: { allRegionsAndLocales: true } }],
  },
  {
    identifier: "engine_icon_updates",
    recordType: "engine",
    base: {
      name: "engine_icon_updates name",
      urls: {
        search: {
          base: "https://example.com/2",
          searchTermParamName: "q",
        },
      },
    },
    variants: [{ environment: { allRegionsAndLocales: true } }],
  },
  {
    recordType: "defaultEngines",
    globalDefault: "engine_no_initial_icon",
    specificDefaults: [],
  },
  {
    recordType: "engineOrders",
    orders: [],
  },
];

async function assertIconMatches(actualIconData, expectedIcon) {
  let expectedBuffer = new Uint8Array(await getFileDataBuffer(expectedIcon));

  Assert.equal(
    actualIconData.length,
    expectedBuffer.length,
    "Should have received matching buffer lengths for the expected icon"
  );
  Assert.ok(
    actualIconData.every((value, index) => value === expectedBuffer[index]),
    "Should have received matching data for the expected icon"
  );
}

async function assertEngineIcon(engineName, expectedIcon) {
  let engine = Services.search.getEngineByName(engineName);
  let engineIconURL = await engine.getIconURL(16);
  Assert.notEqual(
    engineIconURL,
    null,
    "Should have an icon URL for the engine."
  );

  let response = await fetch(engineIconURL);
  let buffer = new Uint8Array(await response.arrayBuffer());

  await assertIconMatches(buffer, expectedIcon);
}

let originalIconId = Services.uuid.generateUUID().toString();
let client;

add_setup(async function setup() {
  useHttpServer();
  SearchTestUtils.useMockIdleService();

  client = RemoteSettings("search-config-icons");
  await client.db.clear();

  sinon.stub(client.attachments, "_baseAttachmentsURL").returns(gDataUrl);

  await insertRecordIntoCollection(client, {
    id: originalIconId,
    filename: "remoteIcon.ico",
    // This uses a wildcard match to test the icon is still applied correctly.
    engineIdentifiers: ["engine_icon_upd*"],
    imageSize: 16,
  });

  await SearchTestUtils.useTestEngines("simple-engines", null, CONFIG);
  await Services.search.init();
});

add_task(async function test_icon_added_unknown_engine() {
  // If the engine is unknown, and this is a new icon, we should still download
  // the icon, in case the engine is added to the configuration later.
  let newIconId = Services.uuid.generateUUID().toString();

  let mock = await mockRecordWithAttachment({
    id: newIconId,
    filename: "bigIcon.ico",
    engineIdentifiers: ["engine_unknown"],
    imageSize: 16,
  });
  await client.db.update(mock.record, Date.now());

  await client.emit("sync", {
    data: {
      current: [mock.record],
      created: [mock.record],
      updated: [],
      deleted: [],
    },
  });

  SearchTestUtils.idleService._fireObservers("idle");

  let icon;
  await TestUtils.waitForCondition(async () => {
    try {
      icon = await client.attachments.get(mock.record);
    } catch (ex) {
      // Do nothing.
    }
    return !!icon;
  }, "Should have loaded the icon into the attachments store.");

  await assertIconMatches(new Uint8Array(icon.buffer), "bigIcon.ico");
});

add_task(async function test_icon_added_existing_engine() {
  // If the engine is unknown, and this is a new icon, we should still download
  // it, in case the engine is added to the configuration later.
  let newIconId = Services.uuid.generateUUID().toString();

  let mock = await mockRecordWithAttachment({
    id: newIconId,
    filename: "bigIcon.ico",
    engineIdentifiers: ["engine_no_initial_icon"],
    imageSize: 16,
  });
  await client.db.update(mock.record, Date.now());

  let promiseEngineUpdated = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  await client.emit("sync", {
    data: {
      current: [mock.record],
      created: [mock.record],
      updated: [],
      deleted: [],
    },
  });

  SearchTestUtils.idleService._fireObservers("idle");

  await promiseEngineUpdated;
  await assertEngineIcon("engine_no_initial_icon name", "bigIcon.ico");
});

add_task(async function test_icon_updated() {
  // Test that when an update for an engine icon is received, the engine is
  // correctly updated.

  // Check the engine has the expected icon to start with.
  await assertEngineIcon("engine_icon_updates name", "remoteIcon.ico");

  // Update the icon for the engine.
  let mock = await mockRecordWithAttachment({
    id: originalIconId,
    filename: "bigIcon.ico",
    engineIdentifiers: ["engine_icon_upd*"],
    imageSize: 16,
  });
  await client.db.update(mock.record, Date.now());

  let promiseEngineUpdated = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  await client.emit("sync", {
    data: {
      current: [mock.record],
      created: [],
      updated: [{ new: mock.record }],
      deleted: [],
    },
  });
  SearchTestUtils.idleService._fireObservers("idle");

  await promiseEngineUpdated;
  await assertEngineIcon("engine_icon_updates name", "bigIcon.ico");
});
