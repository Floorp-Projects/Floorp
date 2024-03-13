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
  },
  {
    engineId: "engine_non_default_sized_icon",
    icons: [
      {
        filename: "remoteIcon.ico",
        engineIdentifiers: ["engine_non_default_sized_icon"],
        imageSize: 32,
      },
    ],
    expectedIcon: "remoteIcon.ico",
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
  },
];

async function getFileDataBuffer(filename) {
  let data = await IOUtils.read(
    PathUtils.join(do_get_cwd().path, "data", filename)
  );
  return new TextEncoder().encode(data).buffer;
}

async function mockRecordWithAttachment({
  filename,
  engineIdentifiers,
  imageSize,
}) {
  let buffer = await getFileDataBuffer(filename);

  let stream = Cc["@mozilla.org/io/arraybuffer-input-stream;1"].createInstance(
    Ci.nsIArrayBufferInputStream
  );
  stream.setData(buffer, 0, buffer.byteLength);

  // Generate a hash.
  let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(
    Ci.nsICryptoHash
  );
  hasher.init(Ci.nsICryptoHash.SHA256);
  hasher.updateFromStream(stream, -1);
  let hash = hasher.finish(false);
  hash = Array.from(hash, (_, i) =>
    ("0" + hash.charCodeAt(i).toString(16)).slice(-2)
  ).join("");

  let record = {
    id: Services.uuid.generateUUID().toString(),
    engineIdentifiers,
    imageSize,
    attachment: {
      hash,
      location: `main-workspace/search-config-icons/${filename}`,
      filename,
      size: buffer.byteLength,
      mimetype: "application/json",
    },
  };

  let attachment = {
    record,
    blob: new Blob([buffer]),
  };

  return { record, attachment };
}

async function insertRecordIntoCollection(client, db, item) {
  let { record, attachment } = await mockRecordWithAttachment(item);
  await db.create(record);
  await client.attachments.cacheImpl.set(record.id, attachment);
  await db.importChanges({}, Date.now());
}

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
        await insertRecordIntoCollection(client, db, {
          ...icon,
          id: test.engineId,
        });
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
    } else {
      Assert.equal(
        await engine.getIconURL(),
        null,
        "Should not have an icon URL for the engine."
      );
    }
  });
}
