/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

// Root URL of the fake hub, see the `data` dir in the tests.
const FAKE_HUB = "chrome://global/content/ml/tests";

const FAKE_MODEL_ARGS = {
  organization: "acme",
  modelName: "bert",
  modelVersion: "main",
  file: "config.json",
};

const badHubs = [
  "https://my.cool.hub",
  "https://sub.localhost/myhub", // Subdomain of allowed domain
  "https://model-hub.mozilla.org.evil.com", // Manipulating path to mimic domain
  "httpsz://localhost/myhub", // Similar-looking scheme
  "https://localhost.", // Trailing dot in domain
  "resource://user@localhost", // User info in URL
  "ftp://localhost/myhub", // Disallowed scheme with allowed host
  "https://model-hub.mozilla.org.hack", // Domain that contains allowed domain
];

add_task(async function test_bad_hubs() {
  for (const badHub of badHubs) {
    Assert.throws(
      () => new ModelHub(badHub),
      new RegExp(`Error: Invalid model hub root url: ${badHub}`),
      `Should throw with ${badHub}`
    );
  }
});

let goodHubs = [
  "https:///localhost/myhub", // Triple slashes, see https://stackoverflow.com/a/22775589
  "https://localhost:8080/myhub",
  "http://localhost/myhub",
  "https://model-hub.mozilla.org",
  "chrome://gre/somewhere/in/the/code/base",
];

add_task(async function test_allowed_hub() {
  goodHubs.forEach(url => new ModelHub(url));
});

const badInputs = [
  [
    {
      organization: "ac me",
      modelName: "bert",
      modelVersion: "main",
      file: "config.json",
    },
    "Org can only contain letters, numbers, and hyphens",
  ],
  [
    {
      organization: "1111",
      modelName: "bert",
      modelVersion: "main",
      file: "config.json",
    },
    "Org cannot contain only numbers",
  ],
  [
    {
      organization: "-acme",
      modelName: "bert",
      modelVersion: "main",
      file: "config.json",
    },
    "Org start or end with a hyphen, or use consecutive hyphens",
  ],
  [
    {
      organization: "a-c-m-e",
      modelName: "#bert",
      modelVersion: "main",
      file: "config.json",
    },
    "Models can only contain letters, numbers, and hyphens, underscord, periods",
  ],
  [
    {
      organization: "a-c-m-e",
      modelName: "b$ert",
      modelVersion: "main",
      file: "config.json",
    },
    "Models cannot contain spaces or control characters",
  ],
  [
    {
      organization: "a-c-m-e",
      modelName: "b$ert",
      modelVersion: "main",
      file: ".filename",
    },
    "File",
  ],
];

add_task(async function test_bad_inputs() {
  const hub = new ModelHub(FAKE_HUB);

  for (const badInput of badInputs) {
    const params = badInput[0];
    const errorMsg = badInput[1];
    try {
      await hub.getModelFileAsArrayBuffer(params);
    } catch (error) {
      continue;
    }
    throw new Error(errorMsg);
  }
});

add_task(async function test_getting_file() {
  const hub = new ModelHub(FAKE_HUB);

  let array = await hub.getModelFileAsArrayBuffer(FAKE_MODEL_ARGS);

  // check the content of the file.
  let jsonData = JSON.parse(
    String.fromCharCode.apply(null, new Uint8Array(array))
  );

  Assert.equal(jsonData.hidden_size, 768);
});

add_task(async function test_getting_file_from_cache() {
  const hub = new ModelHub(FAKE_HUB);
  let array = await hub.getModelFileAsArrayBuffer(FAKE_MODEL_ARGS);

  // stub to verify that the data was retrieved from IndexDB
  let matchMethod = hub.cache._testGetData;

  sinon.stub(hub.cache, "_testGetData").callsFake(function () {
    return matchMethod.apply(this, arguments).then(result => {
      Assert.notEqual(result, null);
      return result;
    });
  });

  // exercises the cache
  let array2 = await hub.getModelFileAsArrayBuffer(FAKE_MODEL_ARGS);
  hub.cache._testGetData.restore();

  Assert.deepEqual(array, array2);
});

// IndexedDB tests

/**
 * Helper function to initialize the cache
 */
async function initializeCache() {
  const randomSuffix = Math.floor(Math.random() * 10000);
  return await IndexedDBCache.init(`modelFiles-${randomSuffix}`);
}

/**
 * Helper function to delete the cache database
 */
async function deleteCache(cache) {
  await cache.dispose();
  indexedDB.deleteDatabase(cache.dbName);
}

/**
 * Test the initialization and creation of the IndexedDBCache instance.
 */
add_task(async function test_Init() {
  const cache = await initializeCache();
  Assert.ok(
    cache instanceof IndexedDBCache,
    "The cache instance should be created successfully."
  );
  Assert.ok(
    IDBDatabase.isInstance(cache.db),
    `The cache should have an IDBDatabase instance. Found ${cache.db}`
  );
  await deleteCache(cache);
});

/**
 * Test adding data to the cache and retrieving it.
 */
add_task(async function test_PutAndGet() {
  const cache = await initializeCache();
  const testData = new ArrayBuffer(8); // Example data
  await cache.put("org", "model", "v1", "file.txt", testData, "ETAG123");

  const retrievedData = await cache.getFile("org", "model", "v1", "file.txt");
  Assert.deepEqual(
    retrievedData,
    testData,
    "The retrieved data should match the stored data."
  );
  await deleteCache(cache);
});

/**
 * Test retrieving the ETag for a cache entry.
 */
add_task(async function test_GetETag() {
  const cache = await initializeCache();
  const testData = new ArrayBuffer(8);
  await cache.put("org", "model", "v1", "file.txt", testData, "ETAG123");

  const etag = await cache.getETag("org", "model", "v1", "file.txt");
  Assert.equal(
    etag,
    "ETAG123",
    "The retrieved ETag should match the stored ETag."
  );
  await deleteCache(cache);
});

/**
 * Test listing all models stored in the cache.
 */
add_task(async function test_ListModels() {
  const cache = await initializeCache();
  await cache.put(
    "org1",
    "modelA",
    "v1",
    "file1.txt",
    new ArrayBuffer(8),
    null
  );
  await cache.put(
    "org2",
    "modelB",
    "v1",
    "file2.txt",
    new ArrayBuffer(8),
    null
  );

  const models = await cache.listModels();
  Assert.ok(
    models.includes("org1/modelA/v1") && models.includes("org2/modelB/v1"),
    "All models should be listed."
  );
  await deleteCache(cache);
});

/**
 * Test deleting a model and its data from the cache.
 */
add_task(async function test_DeleteModel() {
  const cache = await initializeCache();
  await cache.put("org", "model", "v1", "file.txt", new ArrayBuffer(8), null);
  await cache.deleteModel("org", "model", "v1");

  const dataAfterDelete = await cache.getFile("org", "model", "v1", "file.txt");
  Assert.equal(
    dataAfterDelete,
    null,
    "The data for the deleted model should not exist."
  );
  await deleteCache(cache);
});
