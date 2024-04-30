/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

// Root URL of the fake hub, see the `data` dir in the tests.
const FAKE_HUB =
  "chrome://mochitests/content/browser/toolkit/components/ml/tests/browser/data";

const FAKE_MODEL_ARGS = {
  model: "acme/bert",
  revision: "main",
  file: "config.json",
};

const FAKE_RELEASED_MODEL_ARGS = {
  model: "acme/bert",
  revision: "v0.1",
  file: "config.json",
};

const FAKE_ONNX_MODEL_ARGS = {
  model: "acme/bert",
  revision: "main",
  file: "onnx/config.json",
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

/**
 * Make sure we reject bad model hub URLs.
 */
add_task(async function test_bad_hubs() {
  for (const badHub of badHubs) {
    Assert.throws(
      () => new ModelHub({ rootUrl: badHub }),
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
  goodHubs.forEach(url => new ModelHub({ rootUrl: url }));
});

const badInputs = [
  [
    {
      model: "ac me/bert",
      revision: "main",
      file: "config.json",
    },
    "Org can only contain letters, numbers, and hyphens",
  ],
  [
    {
      model: "1111/bert",
      revision: "main",
      file: "config.json",
    },
    "Org cannot contain only numbers",
  ],
  [
    {
      model: "-acme/bert",
      revision: "main",
      file: "config.json",
    },
    "Org start or end with a hyphen, or use consecutive hyphens",
  ],
  [
    {
      model: "a-c-m-e/#bert",
      revision: "main",
      file: "config.json",
    },
    "Models can only contain letters, numbers, and hyphens, underscord, periods",
  ],
  [
    {
      model: "a-c-m-e/b$ert",
      revision: "main",
      file: "config.json",
    },
    "Models cannot contain spaces or control characters",
  ],
  [
    {
      model: "a-c-m-e/b$ert",
      revision: "main",
      file: ".filename",
    },
    "File",
  ],
];

/**
 * Make sure we reject bad inputs.
 */
add_task(async function test_bad_inputs() {
  const hub = new ModelHub({ rootUrl: FAKE_HUB });

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

/**
 * Test that we can retrieve a file as an ArrayBuffer.
 */
add_task(async function test_getting_file() {
  const hub = new ModelHub({ rootUrl: FAKE_HUB });

  let [array, headers] = await hub.getModelFileAsArrayBuffer(FAKE_MODEL_ARGS);

  Assert.equal(headers["Content-Type"], "application/json");

  // check the content of the file.
  let jsonData = JSON.parse(
    String.fromCharCode.apply(null, new Uint8Array(array))
  );

  Assert.equal(jsonData.hidden_size, 768);
});

/**
 * Test that we can retrieve a file from a released model and skip head calls
 */
add_task(async function test_getting_released_file() {
  const hub = new ModelHub({ rootUrl: FAKE_HUB });
  console.log(hub);

  let spy = sinon.spy(hub, "getETag");
  let [array, headers] = await hub.getModelFileAsArrayBuffer(
    FAKE_RELEASED_MODEL_ARGS
  );

  Assert.equal(headers["Content-Type"], "application/json");

  // check the content of the file.
  let jsonData = JSON.parse(
    String.fromCharCode.apply(null, new Uint8Array(array))
  );

  Assert.equal(jsonData.hidden_size, 768);

  // check that head calls were not made
  Assert.ok(!spy.called, "getETag should have never been called.");
  spy.restore();
});

/**
 * Make sure files can be located in sub directories
 */
add_task(async function test_getting_file_in_subdir() {
  const hub = new ModelHub({ rootUrl: FAKE_HUB });

  let [array, metadata] = await hub.getModelFileAsArrayBuffer(
    FAKE_ONNX_MODEL_ARGS
  );

  Assert.equal(metadata["Content-Type"], "application/json");

  // check the content of the file.
  let jsonData = JSON.parse(
    String.fromCharCode.apply(null, new Uint8Array(array))
  );

  Assert.equal(jsonData.hidden_size, 768);
});

/**
 * Test that we can use a custom URL template.
 */
add_task(async function test_getting_file_custom_path() {
  const hub = new ModelHub({
    rootUrl: FAKE_HUB,
    urlTemplate: "{model}/resolve/{revision}",
  });

  let res = await hub.getModelFileAsArrayBuffer(FAKE_MODEL_ARGS);

  Assert.equal(res[1]["Content-Type"], "application/json");
});

/**
 * Test that we can't use an URL with a query for the template
 */
add_task(async function test_getting_file_custom_path_rogue() {
  const urlTemplate = "{model}/resolve/{revision}/?some_id=bedqwdw";
  Assert.throws(
    () => new ModelHub({ rootUrl: FAKE_HUB, urlTemplate }),
    /Invalid URL template/,
    `Should throw with ${urlTemplate}`
  );
});

/**
 * Test that the file can be returned as a response and its content correct.
 */
add_task(async function test_getting_file_as_response() {
  const hub = new ModelHub({ rootUrl: FAKE_HUB });

  let response = await hub.getModelFileAsResponse(FAKE_MODEL_ARGS);

  // check the content of the file.
  let jsonData = await response.json();
  Assert.equal(jsonData.hidden_size, 768);
});

/**
 * Test that the cache is used when the data is retrieved from the server
 * and that the cache is updated with the new data.
 */
add_task(async function test_getting_file_from_cache() {
  const hub = new ModelHub({ rootUrl: FAKE_HUB });
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

/**
 * Test parsing of a well-formed full URL, including protocol and path.
 */
add_task(async function testWellFormedFullUrl() {
  const hub = new ModelHub({ rootUrl: FAKE_HUB });
  const url = `${FAKE_HUB}/org1/model1/v1/file/path`;
  const result = hub.parseUrl(url);

  Assert.equal(
    result.model,
    "org1/model1",
    "Model should be parsed correctly."
  );
  Assert.equal(result.revision, "v1", "Revision should be parsed correctly.");
  Assert.equal(
    result.file,
    "file/path",
    "File path should be parsed correctly."
  );
});

/**
 * Test parsing of a well-formed relative URL, starting with a slash.
 */
add_task(async function testWellFormedRelativeUrl() {
  const hub = new ModelHub({ rootUrl: FAKE_HUB });

  const url = "/org1/model1/v1/file/path";
  const result = hub.parseUrl(url);

  Assert.equal(
    result.model,
    "org1/model1",
    "Model should be parsed correctly."
  );
  Assert.equal(result.revision, "v1", "Revision should be parsed correctly.");
  Assert.equal(
    result.file,
    "file/path",
    "File path should be parsed correctly."
  );
});

/**
 * Ensures an error is thrown when the URL does not start with the expected root URL or a slash.
 */
add_task(async function testInvalidDomain() {
  const hub = new ModelHub({ rootUrl: FAKE_HUB });
  const url = "https://example.com/org1/model1/v1/file/path";
  Assert.throws(
    () => hub.parseUrl(url),
    new RegExp(`Error: Invalid domain for model URL: ${url}`),
    `Should throw with ${url}`
  );
});

/** Tests the method's error handling when the URL format does not include the required segments.
 *
 */
add_task(async function testTooFewParts() {
  const hub = new ModelHub({ rootUrl: FAKE_HUB });
  const url = "/org1/model1";
  Assert.throws(
    () => hub.parseUrl(url),
    new RegExp(`Error: Invalid model URL: ${url}`),
    `Should throw with ${url}`
  );
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
 * Test checking existence of data in the cache.
 */
add_task(async function test_PutAndCheckExists() {
  const cache = await initializeCache();
  const testData = new ArrayBuffer(8); // Example data
  const key = "file.txt";
  await cache.put("org/model", "v1", "file.txt", testData, {
    ETag: "ETAG123",
  });

  // Checking if the file exists
  let exists = await cache.fileExists("org/model", "v1", key);
  Assert.ok(exists, "The file should exist in the cache.");

  // Removing all files from the model
  await cache.deleteModel("org/model", "v1");

  exists = await cache.fileExists("org/model", "v1", key);
  Assert.ok(!exists, "The file should be gone from the cache.");

  await deleteCache(cache);
});

/**
 * Test adding data to the cache and retrieving it.
 */
add_task(async function test_PutAndGet() {
  const cache = await initializeCache();
  const testData = new ArrayBuffer(8); // Example data
  await cache.put("org/model", "v1", "file.txt", testData, {
    ETag: "ETAG123",
  });

  const [retrievedData, headers] = await cache.getFile(
    "org/model",
    "v1",
    "file.txt"
  );
  Assert.deepEqual(
    retrievedData,
    testData,
    "The retrieved data should match the stored data."
  );
  Assert.equal(
    headers.ETag,
    "ETAG123",
    "The retrieved ETag should match the stored ETag."
  );

  await deleteCache(cache);
});

/**
 * Test retrieving the headers for a cache entry.
 */
add_task(async function test_GetHeaders() {
  const cache = await initializeCache();
  const testData = new ArrayBuffer(8);
  const headers = {
    ETag: "ETAG123",
    status: 200,
    extra: "extra",
  };

  await cache.put("org/model", "v1", "file.txt", testData, headers);

  const storedHeaders = await cache.getHeaders("org/model", "v1", "file.txt");

  // The `extra` field should be removed from the stored headers because
  // it's not part of the allowed keys.
  // The content-type one is added when not present
  Assert.deepEqual(
    {
      ETag: "ETAG123",
      status: 200,
      "Content-Type": "application/octet-stream",
    },
    storedHeaders,
    "The retrieved headers should match the stored headers."
  );
  await deleteCache(cache);
});

/**
 * Test listing all models stored in the cache.
 */
add_task(async function test_ListModels() {
  const cache = await initializeCache();
  await cache.put("org1/modelA", "v1", "file1.txt", new ArrayBuffer(8), null);
  await cache.put("org2/modelB", "v1", "file2.txt", new ArrayBuffer(8), null);

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
  await cache.put("org/model", "v1", "file.txt", new ArrayBuffer(8), null);
  await cache.deleteModel("org/model", "v1");

  const dataAfterDelete = await cache.getFile("org/model", "v1", "file.txt");
  Assert.equal(
    dataAfterDelete,
    null,
    "The data for the deleted model should not exist."
  );
  await deleteCache(cache);
});
