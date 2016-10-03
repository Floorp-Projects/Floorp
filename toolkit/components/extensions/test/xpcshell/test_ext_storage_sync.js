/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

do_get_profile();   // so we can use FxAccounts

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/ExtensionStorageSync.jsm");
const {
  CollectionKeyEncryptionRemoteTransformer,
  cryptoCollection,
  idToKey,
  extensionIdToCollectionId,
  keyToId,
} = Cu.import("resource://gre/modules/ExtensionStorageSync.jsm");
Cu.import("resource://services-sync/engines/extension-storage.js");
Cu.import("resource://services-sync/keys.js");
Cu.import("resource://services-sync/util.js");

/* globals BulkKeyBundle, CommonUtils, EncryptionRemoteTransformer */
/* globals KeyRingEncryptionRemoteTransformer */
/* globals Utils */

function handleCannedResponse(cannedResponse, request, response) {
  response.setStatusLine(null, cannedResponse.status.status,
                         cannedResponse.status.statusText);
  // send the headers
  for (let headerLine of cannedResponse.sampleHeaders) {
    let headerElements = headerLine.split(":");
    response.setHeader(headerElements[0], headerElements[1].trimLeft());
  }
  response.setHeader("Date", (new Date()).toUTCString());

  response.write(cannedResponse.responseBody);
}

function collectionRecordsPath(collectionId) {
  return `/buckets/default/collections/${collectionId}/records`;
}

class KintoServer {
  constructor() {
    // Set up an HTTP Server
    this.httpServer = new HttpServer();
    this.httpServer.start(-1);

    // Map<CollectionId, Set<Object>> corresponding to the data in the
    // Kinto server
    this.collections = new Map();

    // ETag to serve with responses
    this.etag = 1;

    this.port = this.httpServer.identity.primaryPort;
    // POST requests we receive from the client go here
    this.posts = [];
    // Anything in here will force the next POST to generate a conflict
    this.conflicts = [];

    this.installConfigPath();
    this.installBatchPath();
    this.installCatchAll();
  }

  clearPosts() {
    this.posts = [];
  }

  getPosts() {
    return this.posts;
  }

  installConfigPath() {
    const configPath = "/v1/";
    const responseBody = JSON.stringify({
      "settings": {"batch_max_requests": 25},
      "url": `http://localhost:${this.port}/v1/`,
      "documentation": "https://kinto.readthedocs.org/",
      "version": "1.5.1",
      "commit": "cbc6f58",
      "hello": "kinto",
    });
    const configResponse = {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": responseBody,
    };

    function handleGetConfig(request, response) {
      if (request.method != "GET") {
        dump(`ARGH, got ${request.method}\n`);
      }
      return handleCannedResponse(configResponse, request, response);
    }

    this.httpServer.registerPathHandler(configPath, handleGetConfig);
  }

  installBatchPath() {
    const batchPath = "/v1/batch";

    function handlePost(request, response) {
      let bodyStr = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
      let body = JSON.parse(bodyStr);
      let defaults = body.defaults;
      for (let req of body.requests) {
        let headers = Object.assign({}, defaults && defaults.headers || {}, req.headers);
        // FIXME: assert auth is "Bearer ...token..."
        this.posts.push(Object.assign({}, req, {headers}));
      }

      response.setStatusLine(null, 200, "OK");
      response.setHeader("Content-Type", "application/json; charset=UTF-8");
      response.setHeader("Date", (new Date()).toUTCString());

      let postResponse = {
        responses: body.requests.map(req => {
          return {
            path: req.path,
            status: 201,   // FIXME -- only for new posts??
            headers: {"ETag": 3000},   // FIXME???
            body: {"data": Object.assign({}, req.body.data, {last_modified: this.etag}),
                   "permissions": []},
          };
        }),
      };

      if (this.conflicts.length > 0) {
        const {collectionId, encrypted} = this.conflicts.shift();
        this.collections.get(collectionId).add(encrypted);
        dump(`responding with etag ${this.etag}\n`);
        postResponse = {
          responses: body.requests.map(req => {
            return {
              path: req.path,
              status: 412,
              headers: {"ETag": this.etag}, // is this correct??
              body: {
                details: {
                  existing: encrypted,
                },
              },
            };
          }),
        };
      }

      response.write(JSON.stringify(postResponse));

      //   "sampleHeaders": [
      //     "Access-Control-Allow-Origin: *",
      //     "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
      //     "Server: waitress",
      //     "Etag: \"4000\""
      //   ],
    }

    this.httpServer.registerPathHandler(batchPath, handlePost.bind(this));
  }

  installCatchAll() {
    this.httpServer.registerPathHandler("/", (request, response) => {
      dump(`got request: ${request.method}:${request.path}?${request.queryString}\n`);
      dump(`${CommonUtils.readBytesFromInputStream(request.bodyInputStream)}\n`);
    });
  }

  installCollection(collectionId) {
    this.collections.set(collectionId, new Set());

    const remoteRecordsPath = "/v1" + collectionRecordsPath(encodeURIComponent(collectionId));

    function handleGetRecords(request, response) {
      if (request.method != "GET") {
        do_throw(`only GET is supported on ${remoteRecordsPath}`);
      }

      response.setStatusLine(null, 200, "OK");
      response.setHeader("Content-Type", "application/json; charset=UTF-8");
      response.setHeader("Date", (new Date()).toUTCString());
      response.setHeader("ETag", this.etag.toString());

      const body = JSON.stringify({
        // Can't JSON a Set directly, so convert to Array
        "data": Array.from(this.collections.get(collectionId)),
      });
      response.write(body);
    }

    this.httpServer.registerPathHandler(remoteRecordsPath, handleGetRecords.bind(this));
  }

  // Utility function to install a keyring at the start of a test.
  installKeyRing(keysData, etag, {conflict = false} = {}) {
    this.installCollection("storage-sync-crypto");
    const keysRecord = {
      "id": "keys",
      "keys": keysData,
      "last_modified": etag,
    };
    this.etag = etag;
    const methodName = conflict ? "encryptAndAddRecordWithConflict" : "encryptAndAddRecord";
    this[methodName](new KeyRingEncryptionRemoteTransformer(),
                     "storage-sync-crypto", keysRecord);
  }

  encryptAndAddRecord(transformer, collectionId, record) {
    return transformer.encode(record).then(encrypted => {
      this.collections.get(collectionId).add(encrypted);
    });
  }

  // Conflicts block the next push and then appear in the collection specified.
  encryptAndAddRecordWithConflict(transformer, collectionId, record) {
    return transformer.encode(record).then(encrypted => {
      this.conflicts.push({collectionId, encrypted});
    });
  }

  clearCollection(collectionId) {
    this.collections.get(collectionId).clear();
  }

  stop() {
    this.httpServer.stop(() => { });
  }
}

// Run a block of code with access to a KintoServer.
function* withServer(f) {
  let server = new KintoServer();
  // Point the sync.storage client to use the test server we've just started.
  Services.prefs.setCharPref("webextensions.storage.sync.serverURL",
                             `http://localhost:${server.port}/v1`);
  try {
    yield* f(server);
  } finally {
    server.stop();
  }
}

// Run a block of code with access to both a sync context and a
// KintoServer. This is meant as a workaround for eslint's refusal to
// let me have 5 nested callbacks.
function* withContextAndServer(f) {
  yield* withSyncContext(function* (context) {
    yield* withServer(function* (server) {
      yield* f(context, server);
    });
  });
}

// Run a block of code with fxa mocked out to return a specific user.
function* withSignedInUser(user, f) {
  const oldESSFxAccounts = ExtensionStorageSync._fxaService;
  const oldERTFxAccounts = EncryptionRemoteTransformer.prototype._fxaService;
  ExtensionStorageSync._fxaService = EncryptionRemoteTransformer.prototype._fxaService = {
    getSignedInUser() {
      return Promise.resolve(user);
    },
    getOAuthToken() {
      return Promise.resolve("some-access-token");
    },
  };

  try {
    yield* f();
  } finally {
    ExtensionStorageSync._fxaService = oldESSFxAccounts;
    EncryptionRemoteTransformer.prototype._fxaService = oldERTFxAccounts;
  }
}

// Some assertions that make it easier to write tests about what was
// posted and when.

// Assert that the request was made with the correct access token.
// This should be true of all requests, so this is usually called from
// another assertion.
function assertAuthenticatedRequest(post) {
  equal(post.headers.Authorization, "Bearer some-access-token");
}

// Assert that this post was made with the correct request headers to
// create a new resource while protecting against someone else
// creating it at the same time (in other words, "If-None-Match: *").
// Also calls assertAuthenticatedRequest(post).
function assertPostedNewRecord(post) {
  assertAuthenticatedRequest(post);
  equal(post.headers["If-None-Match"], "*");
}

// Assert that this post was made with the correct request headers to
// update an existing resource while protecting against concurrent
// modification (in other words, `If-Match: "${etag}"`).
// Also calls assertAuthenticatedRequest(post).
function assertPostedUpdatedRecord(post, since) {
  assertAuthenticatedRequest(post);
  equal(post.headers["If-Match"], `"${since}"`);
}

// Assert that this post was an encrypted keyring, and produce the
// decrypted body. Sanity check the body while we're here.
const assertPostedEncryptedKeys = Task.async(function* (post) {
  equal(post.path, collectionRecordsPath("storage-sync-crypto") + "/keys");

  let body = yield new KeyRingEncryptionRemoteTransformer().decode(post.body.data);
  ok(body.keys, `keys object should be present in decoded body`);
  ok(body.keys.default, `keys object should have a default key`);
  return body;
});

// assertEqual, but for keyring[extensionId] == key.
function assertKeyRingKey(keyRing, extensionId, expectedKey, message) {
  if (!message) {
    message = `expected keyring's key for ${extensionId} to match ${expectedKey.keyPairB64}`;
  }
  ok(keyRing.hasKeysFor([extensionId]),
     `expected keyring to have a key for ${extensionId}\n`);
  deepEqual(keyRing.keyForCollection(extensionId).keyPairB64, expectedKey.keyPairB64,
            message);
}

// Tests using this ID will share keys in local storage, so be careful.
const extensionId = "{13bdde76-4dc7-11e6-9bdc-54ee758d6342}";
const extension = {id: extensionId};

const BORING_KB = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
const ANOTHER_KB = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde0";
const loggedInUser = {
  uid: "0123456789abcdef0123456789abcdef",
  kB: BORING_KB,
  oauthTokens: {
    "sync:addon-storage": {
      token: "some-access-token",
    },
  },
};
const collectionId = extensionIdToCollectionId(loggedInUser, extensionId);

function uuid() {
  const uuidgen = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
  return uuidgen.generateUUID();
}

add_task(function* test_key_to_id() {
  equal(keyToId("foo"), "key-foo");
  equal(keyToId("my-new-key"), "key-my_2D_new_2D_key");
  equal(keyToId(""), "key-");
  equal(keyToId("™"), "key-_2122_");
  equal(keyToId("\b"), "key-_8_");
  equal(keyToId("abc\ndef"), "key-abc_A_def");
  equal(keyToId("Kinto's fancy_string"), "key-Kinto_27_s_20_fancy_5F_string");

  const KEYS = ["foo", "my-new-key", "", "Kinto's fancy_string", "™", "\b"];
  for (let key of KEYS) {
    equal(idToKey(keyToId(key)), key);
  }

  equal(idToKey("hi"), null);
  equal(idToKey("-key-hi"), null);
  equal(idToKey("key--abcd"), null);
  equal(idToKey("key-%"), null);
  equal(idToKey("key-_HI"), null);
  equal(idToKey("key-_HI_"), null);
  equal(idToKey("key-"), "");
  equal(idToKey("key-1"), "1");
  equal(idToKey("key-_2D_"), "-");
});

add_task(function* test_extension_id_to_collection_id() {
  const newKBUser = Object.assign(loggedInUser, {kB: ANOTHER_KB});
  const extensionId = "{9419cce6-5435-11e6-84bf-54ee758d6342}";
  const extensionId2 = "{9419cce6-5435-11e6-84bf-54ee758d6343}";

  // "random" 32-char hex userid
  equal(extensionIdToCollectionId(loggedInUser, extensionId),
        "abf4e257dad0c89027f8f25bd196d4d69c100df375655a0c49f4cea7b791ea7d");
  equal(extensionIdToCollectionId(loggedInUser, extensionId),
        extensionIdToCollectionId(newKBUser, extensionId));
  equal(extensionIdToCollectionId(loggedInUser, extensionId2),
        "6584b0153336fb274912b31a3225c15a92b703cdc3adfe1917c1aa43122a52b8");
});

add_task(function* ensureKeysFor_posts_new_keys() {
  const extensionId = uuid();
  yield* withContextAndServer(function* (context, server) {
    yield* withSignedInUser(loggedInUser, function* () {
      server.installCollection("storage-sync-crypto");
      server.etag = 1000;

      // Prompt ExtensionStorageSync to initialize crypto
      yield ExtensionStorageSync.get({id: extensionId}, "random-key", context);

      let newKeys = yield ExtensionStorageSync.ensureKeysFor([extensionId]);
      ok(newKeys.hasKeysFor([extensionId]), `key isn't present for ${extensionId}`);

      let posts = server.getPosts();
      equal(posts.length, 1);
      const post = posts[0];
      assertPostedNewRecord(post);
      const body = yield assertPostedEncryptedKeys(post);
      ok(body.keys.collections[extensionId], `keys object should have a key for ${extensionId}`);
    });
  });
});

add_task(function* ensureKeysFor_pulls_key() {
  // ensureKeysFor is implemented by adding a key to our local record
  // and doing a sync. This means that if the same key exists
  // remotely, we get a "conflict". Ensure that we handle this
  // correctly -- we keep the server key (since presumably it's
  // already been used to encrypt records) and we don't wipe out other
  // collections' keys.
  const extensionId = uuid();
  const extensionId2 = uuid();
  const DEFAULT_KEY = new BulkKeyBundle("[default]");
  DEFAULT_KEY.generateRandom();
  const RANDOM_KEY = new BulkKeyBundle(extensionId);
  RANDOM_KEY.generateRandom();
  yield* withContextAndServer(function* (context, server) {
    yield* withSignedInUser(loggedInUser, function* () {
      const keysData = {
        "default": DEFAULT_KEY.keyPairB64,
        "collections": {
          [extensionId]: RANDOM_KEY.keyPairB64,
        },
      };
      server.installKeyRing(keysData, 999);

      // Prompt ExtensionStorageSync to initialize crypto
      yield ExtensionStorageSync.get({id: extensionId}, "random-key", context);

      let collectionKeys = yield ExtensionStorageSync.ensureKeysFor([extensionId]);
      assertKeyRingKey(collectionKeys, extensionId, RANDOM_KEY);

      let posts = server.getPosts();
      equal(posts.length, 0,
            "ensureKeysFor shouldn't push when the server keyring has the right key");

      // Another client generates a key for extensionId2
      const newKey = new BulkKeyBundle(extensionId2);
      newKey.generateRandom();
      keysData.collections[extensionId2] = newKey.keyPairB64;
      server.clearCollection("storage-sync-crypto");
      server.installKeyRing(keysData, 1000);

      let newCollectionKeys = yield ExtensionStorageSync.ensureKeysFor([extensionId, extensionId2]);
      assertKeyRingKey(newCollectionKeys, extensionId2, newKey);
      assertKeyRingKey(newCollectionKeys, extensionId, RANDOM_KEY,
                       `ensureKeysFor shouldn't lose the old key for ${extensionId}`);

      posts = server.getPosts();
      equal(posts.length, 0, "ensureKeysFor shouldn't push when updating keys");
    });
  });
});

add_task(function* ensureKeysFor_handles_conflicts() {
  // Syncing is done through a pull followed by a push of any merged
  // changes. Accordingly, the only way to have a "true" conflict --
  // i.e. with the server rejecting a change -- is if
  // someone pushes changes between our pull and our push. Ensure that
  // if this happens, we still behave sensibly (keep the remote key).
  const extensionId = uuid();
  const DEFAULT_KEY = new BulkKeyBundle("[default]");
  DEFAULT_KEY.generateRandom();
  const RANDOM_KEY = new BulkKeyBundle(extensionId);
  RANDOM_KEY.generateRandom();
  yield* withContextAndServer(function* (context, server) {
    yield* withSignedInUser(loggedInUser, function* () {
      const keysData = {
        "default": DEFAULT_KEY.keyPairB64,
        "collections": {
          [extensionId]: RANDOM_KEY.keyPairB64,
        },
      };
      server.installKeyRing(keysData, 765, {conflict: true});

      // Prompt ExtensionStorageSync to initialize crypto
      yield ExtensionStorageSync.get({id: extensionId}, "random-key", context);
      yield cryptoCollection._clear();

      let collectionKeys = yield ExtensionStorageSync.ensureKeysFor([extensionId]);
      assertKeyRingKey(collectionKeys, extensionId, RANDOM_KEY,
                       `syncing keyring should keep the server key for ${extensionId}`);

      let posts = server.getPosts();
      equal(posts.length, 1,
            "syncing keyring should have tried to post a keyring");
      const failedPost = posts[0];
      assertPostedNewRecord(failedPost);
      let body = yield assertPostedEncryptedKeys(failedPost);
      // This key will be the one the client generated locally, so
      // we don't know what its value will be
      ok(body.keys.collections[extensionId],
         `decrypted failed post should have a key for ${extensionId}`);
      notEqual(body.keys.collections[extensionId], RANDOM_KEY.keyPairB64,
               `decrypted failed post should have a randomly-generated key for ${extensionId}`);
    });
  });
});

add_task(function* checkSyncKeyRing_reuploads_keys() {
  // Verify that when keys are present, they are reuploaded with the
  // new kB when we call touchKeys().
  const extensionId = uuid();
  let extensionKey;
  yield* withContextAndServer(function* (context, server) {
    yield* withSignedInUser(loggedInUser, function* () {
      server.installCollection("storage-sync-crypto");
      server.etag = 765;

      // Prompt ExtensionStorageSync to initialize crypto
      yield ExtensionStorageSync.get({id: extensionId}, "random-key", context);
      yield cryptoCollection._clear();

      // Do an `ensureKeysFor` to generate some keys.
      let collectionKeys = yield ExtensionStorageSync.ensureKeysFor([extensionId]);
      ok(collectionKeys.hasKeysFor([extensionId]),
         `ensureKeysFor should return a keyring that has a key for ${extensionId}`);
      extensionKey = collectionKeys.keyForCollection(extensionId).keyPairB64;
      equal(server.getPosts().length, 1,
            "generating a key that doesn't exist on the server should post it");
    });

    // The user changes their password. This is their new kB, with
    // the last f changed to an e.
    const NOVEL_KB = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdee";
    const newUser = Object.assign({}, loggedInUser, {kB: NOVEL_KB});
    let postedKeys;
    yield* withSignedInUser(newUser, function* () {
      yield ExtensionStorageSync.checkSyncKeyRing();

      let posts = server.getPosts();
      equal(posts.length, 2,
            "when kB changes, checkSyncKeyRing should post the keyring reencrypted with the new kB");
      postedKeys = posts[1];
      assertPostedUpdatedRecord(postedKeys, 765);

      let body = yield assertPostedEncryptedKeys(postedKeys);
      deepEqual(body.keys.collections[extensionId], extensionKey,
                `the posted keyring should have the same key for ${extensionId} as the old one`);
    });

    // Verify that with the old kB, we can't decrypt the record.
    yield* withSignedInUser(loggedInUser, function* () {
      let error;
      try {
        yield new KeyRingEncryptionRemoteTransformer().decode(postedKeys.body.data);
      } catch (e) {
        error = e;
      }
      ok(error, "decrypting the keyring with the old kB should fail");
      ok(Utils.isHMACMismatch(error),
         "decrypting the keyring with the old kB should throw an HMAC mismatch");
    });
  });
});

add_task(function* test_storage_sync_pulls_changes() {
  yield* withContextAndServer(function* (context, server) {
    yield* withSignedInUser(loggedInUser, function* () {
      let transformer = new CollectionKeyEncryptionRemoteTransformer(extensionId);
      server.installCollection(collectionId);
      server.installCollection("storage-sync-crypto");

      let calls = [];
      yield ExtensionStorageSync.addOnChangedListener(extension, function() {
        calls.push(arguments);
      }, context);

      // This has to happen AFTER invoking EncryptionStorageSync so
      // that it can set up the crypto keys collection.
      yield ExtensionStorageSync.ensureKeysFor([extensionId]);
      yield server.encryptAndAddRecord(transformer, collectionId, {
        "id": "key-remote_2D_key",
        "key": "remote-key",
        "data": 6,
      });

      yield ExtensionStorageSync.syncAll();
      const remoteValue = (yield ExtensionStorageSync.get(extension, "remote-key", context))["remote-key"];
      equal(remoteValue, 6,
            "ExtensionStorageSync.get() returns value retrieved from sync");

      equal(calls.length, 1,
            "syncing calls on-changed listener");
      deepEqual(calls[0][0], {"remote-key": {newValue: 6}});
      calls = [];

      // Syncing again doesn't do anything
      yield ExtensionStorageSync.syncAll();

      equal(calls.length, 0,
            "syncing again shouldn't call on-changed listener");

      // Updating the server causes us to pull down the new value
      server.etag = 1000;
      server.clearCollection(collectionId);
      yield server.encryptAndAddRecord(transformer, collectionId, {
        "id": "key-remote_2D_key",
        "key": "remote-key",
        "data": 7,
      });

      yield ExtensionStorageSync.syncAll();
      const remoteValue2 = (yield ExtensionStorageSync.get(extension, "remote-key", context))["remote-key"];
      equal(remoteValue2, 7,
            "ExtensionStorageSync.get() returns value updated from sync");

      equal(calls.length, 1,
            "syncing calls on-changed listener on update");
      deepEqual(calls[0][0], {"remote-key": {oldValue: 6, newValue: 7}});
    });
  });
});

add_task(function* test_storage_sync_pushes_changes() {
  yield* withContextAndServer(function* (context, server) {
    yield* withSignedInUser(loggedInUser, function* () {
      let transformer = new CollectionKeyEncryptionRemoteTransformer(extensionId);
      server.installCollection(collectionId);
      server.installCollection("storage-sync-crypto");
      server.etag = 1000;

      yield ExtensionStorageSync.set(extension, {"my-key": 5}, context);

      // install this AFTER we set the key to 5...
      let calls = [];
      ExtensionStorageSync.addOnChangedListener(extension, function() {
        calls.push(arguments);
      }, context);

      yield ExtensionStorageSync.syncAll();
      const localValue = (yield ExtensionStorageSync.get(extension, "my-key", context))["my-key"];
      equal(localValue, 5,
            "pushing an ExtensionStorageSync value shouldn't change local value");

      let posts = server.getPosts();
      equal(posts.length, 1,
            "pushing a value should cause a post to the server");
      const post = posts[0];
      assertPostedNewRecord(post);
      equal(post.path, collectionRecordsPath(collectionId) + "/key-my_2D_key",
            "pushing a value should have a path corresponding to its id");

      const encrypted = post.body.data;
      ok(encrypted.ciphertext,
         "pushing a value should post an encrypted record");
      ok(!encrypted.data,
         "pushing a value should not have any plaintext data");
      equal(encrypted.id, "key-my_2D_key",
            "pushing a value should use a kinto-friendly record ID");

      const record = yield transformer.decode(encrypted);
      equal(record.key, "my-key",
            "when decrypted, a pushed value should have a key field corresponding to its storage.sync key");
      equal(record.data, 5,
            "when decrypted, a pushed value should have a data field corresponding to its storage.sync value");
      equal(record.id, "key-my_2D_key",
            "when decrypted, a pushed value should have an id field corresponding to its record ID");

      equal(calls.length, 0,
            "pushing a value shouldn't call the on-changed listener");

      yield ExtensionStorageSync.set(extension, {"my-key": 6}, context);
      yield ExtensionStorageSync.syncAll();

      // Doesn't push keys because keys were pushed by a previous test.
      posts = server.getPosts();
      equal(posts.length, 2,
            "updating a value should trigger another push");
      const updatePost = posts[1];
      assertPostedUpdatedRecord(updatePost, 1000);
      equal(updatePost.path, collectionRecordsPath(collectionId) + "/key-my_2D_key",
            "pushing an updated value should go to the same path");

      const updateEncrypted = updatePost.body.data;
      ok(updateEncrypted.ciphertext,
         "pushing an updated value should still be encrypted");
      ok(!updateEncrypted.data,
         "pushing an updated value should not have any plaintext visible");
      equal(updateEncrypted.id, "key-my_2D_key",
            "pushing an updated value should maintain the same ID");
    });
  });
});
