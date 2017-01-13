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
} = Cu.import("resource://gre/modules/ExtensionStorageSync.jsm", {});
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
    // DELETEd buckets will go here.
    this.deletedBuckets = [];
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

  getDeletedBuckets() {
    return this.deletedBuckets;
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
          let oneBody;
          if (req.method == "DELETE") {
            let id = req.path.match(/^\/buckets\/default\/collections\/.+\/records\/(.+)$/)[1];
            oneBody = {
              "data": {
                "deleted": true,
                "id": id,
                "last_modified": this.etag,
              },
            };
          } else {
            oneBody = {"data": Object.assign({}, req.body.data, {last_modified: this.etag}),
                       "permissions": []};
          }

          return {
            path: req.path,
            status: 201,   // FIXME -- only for new posts??
            headers: {"ETag": 3000},   // FIXME???
            body: oneBody,
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

      const records = this.collections.get(collectionId);
      // Can't JSON a Set directly, so convert to Array
      let data = Array.from(records);
      if (request.queryString.includes("_since=")) {
        data = data.filter(r => !(r._inPast || false));
      }

      // Remove records that we only needed to serve once.
      // FIXME: come up with a more coherent idea of time here.
      // See bug 1321570.
      for (const record of records) {
        if (record._onlyOnce) {
          records.delete(record);
        }
      }

      const body = JSON.stringify({
        "data": data,
      });
      response.write(body);
    }

    this.httpServer.registerPathHandler(remoteRecordsPath, handleGetRecords.bind(this));
  }

  installDeleteBucket() {
    this.httpServer.registerPrefixHandler("/v1/buckets/", (request, response) => {
      if (request.method != "DELETE") {
        dump(`got a non-delete action on bucket: ${request.method} ${request.path}\n`);
        return;
      }

      const noPrefix = request.path.slice("/v1/buckets/".length);
      const [bucket, afterBucket] = noPrefix.split("/", 1);
      if (afterBucket && afterBucket != "") {
        dump(`got a delete for a non-bucket: ${request.method} ${request.path}\n`);
      }

      this.deletedBuckets.push(bucket);
      // Fake like this actually deletes the records.
      for (const [, set] of this.collections) {
        set.clear();
      }

      response.write(JSON.stringify({
        data: {
          deleted: true,
          last_modified: 1475161309026,
          id: "b09f1618-d789-302d-696e-74ec53ee18a8", // FIXME
        },
      }));
    });
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

  // Add an already-encrypted record.
  addRecord(collectionId, record) {
    this.collections.get(collectionId).add(record);
  }

  // Add a record that is only served if no `_since` is present.
  //
  // Since in real life, Kinto only serves a record as part of a
  // changes feed if `_since` is before the record's modification
  // time, this can be helpful to test certain kinds of syncing logic.
  //
  // FIXME: tracking of "time" in this mock server really needs to be
  // implemented correctly rather than these hacks. See bug 1321570.
  addRecordInPast(collectionId, record) {
    record._inPast = true;
    this.addRecord(collectionId, record);
  }

  encryptAndAddRecord(transformer, collectionId, record) {
    return transformer.encode(record).then(encrypted => {
      this.addRecord(collectionId, encrypted);
    });
  }

  // Like encryptAndAddRecord, but add a flag that will only serve
  // this record once.
  //
  // Since in real life, Kinto only serves a record as part of a changes feed
  // once, this can be useful for testing complicated syncing logic.
  //
  // FIXME: This kind of logic really needs to be subsumed into some
  // more-realistic tracking of "time" (simulated by etags). See bug 1321570.
  encryptAndAddRecordOnlyOnce(transformer, collectionId, record) {
    return transformer.encode(record).then(encrypted => {
      encrypted._onlyOnce = true;
      this.addRecord(collectionId, encrypted);
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
    sessionStatus() {
      return Promise.resolve(true);
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
const defaultExtensionId = "{13bdde76-4dc7-11e6-9bdc-54ee758d6342}";
const defaultExtension = {id: defaultExtensionId};

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
const defaultCollectionId = extensionIdToCollectionId(loggedInUser, defaultExtensionId);

function uuid() {
  const uuidgen = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
  return uuidgen.generateUUID().toString();
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

      let newKeys = yield ExtensionStorageSync.ensureKeysFor([extensionId]);
      ok(newKeys.hasKeysFor([extensionId]), `key isn't present for ${extensionId}`);

      let posts = server.getPosts();
      equal(posts.length, 1);
      const post = posts[0];
      assertPostedNewRecord(post);
      const body = yield assertPostedEncryptedKeys(post);
      ok(body.keys.collections[extensionId], `keys object should have a key for ${extensionId}`);

      // Try adding another key to make sure that the first post was
      // OK, even on a new profile.
      yield cryptoCollection._clear();
      server.clearPosts();
      // Restore the first posted keyring, but add a last_modified date
      const firstPostedKeyring = Object.assign({}, post.body.data, {last_modified: server.etag});
      server.addRecordInPast("storage-sync-crypto", firstPostedKeyring);
      const extensionId2 = uuid();
      newKeys = yield ExtensionStorageSync.ensureKeysFor([extensionId2]);
      ok(newKeys.hasKeysFor([extensionId]), `didn't forget key for ${extensionId}`);
      ok(newKeys.hasKeysFor([extensionId2]), `new key generated for ${extensionId2}`);

      posts = server.getPosts();
      equal(posts.length, 1);
      const newPost = posts[posts.length - 1];
      const newBody = yield assertPostedEncryptedKeys(newPost);
      ok(newBody.keys.collections[extensionId], `keys object should have a key for ${extensionId}`);
      ok(newBody.keys.collections[extensionId2], `keys object should have a key for ${extensionId2}`);
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
      ok(Utils.isHMACMismatch(error) || KeyRingEncryptionRemoteTransformer.isOutdatedKB(error),
         "decrypting the keyring with the old kB should throw an HMAC mismatch");
    });
  });
});

add_task(function* checkSyncKeyRing_overwrites_on_conflict() {
  // If there is already a record on the server that was encrypted
  // with a different kB, we wipe the server, clear sync state, and
  // overwrite it with our keys.
  const extensionId = uuid();
  const transformer = new KeyRingEncryptionRemoteTransformer();
  let extensionKey;
  yield* withSyncContext(function* (context) {
    yield* withServer(function* (server) {
      // The old device has this kB, which is very similar to the
      // current kB but with the last f changed to an e.
      const NOVEL_KB = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdee";
      const oldUser = Object.assign({}, loggedInUser, {kB: NOVEL_KB});
      server.installCollection("storage-sync-crypto");
      server.installDeleteBucket();
      server.etag = 765;
      yield* withSignedInUser(oldUser, function* () {
        const FAKE_KEYRING = {
          id: "keys",
          keys: {},
          uuid: "abcd",
          kbHash: "abcd",
        };
        yield server.encryptAndAddRecord(transformer, "storage-sync-crypto", FAKE_KEYRING);
      });

      // Now we have this new user with a different kB.
      yield* withSignedInUser(loggedInUser, function* () {
        yield cryptoCollection._clear();

        // Do an `ensureKeysFor` to generate some keys.
        // This will try to sync, notice that the record is
        // undecryptable, and clear the server.
        let collectionKeys = yield ExtensionStorageSync.ensureKeysFor([extensionId]);
        ok(collectionKeys.hasKeysFor([extensionId]),
           `ensureKeysFor should always return a keyring with a key for ${extensionId}`);
        extensionKey = collectionKeys.keyForCollection(extensionId).keyPairB64;

        deepEqual(server.getDeletedBuckets(), ["default"],
                  "Kinto server should have been wiped when keyring was thrown away");

        let posts = server.getPosts();
        equal(posts.length, 1,
             "new keyring should have been uploaded");
        const postedKeys = posts[0];
        // The POST was to an empty server, so etag shouldn't be respected
        equal(postedKeys.headers.Authorization, "Bearer some-access-token",
              "keyring upload should be authorized");
        equal(postedKeys.headers["If-None-Match"], "*",
              "keyring upload should be to empty Kinto server");
        equal(postedKeys.path, collectionRecordsPath("storage-sync-crypto") + "/keys",
              "keyring upload should be to keyring path");

        let body = yield new KeyRingEncryptionRemoteTransformer().decode(postedKeys.body.data);
        ok(body.uuid, "new keyring should have a UUID");
        equal(typeof body.uuid, "string", "keyring UUIDs should be strings");
        notEqual(body.uuid, "abcd",
                 "new keyring should not have the same UUID as previous keyring");
        ok(body.keys,
           "new keyring should have a keys attribute");
        ok(body.keys.default, "new keyring should have a default key");
        // We should keep the extension key that was in our uploaded version.
        deepEqual(extensionKey, body.keys.collections[extensionId],
                  "ensureKeysFor should have returned keyring with the same key that was uploaded");

        // This should be a no-op; the keys were uploaded as part of ensurekeysfor
        yield ExtensionStorageSync.checkSyncKeyRing();
        equal(server.getPosts().length, 1,
              "checkSyncKeyRing should not need to post keys after they were reuploaded");
      });
    });
  });
});

add_task(function* checkSyncKeyRing_flushes_on_uuid_change() {
  // If we can decrypt the record, but the UUID has changed, that
  // means another client has wiped the server and reuploaded a
  // keyring, so reset sync state and reupload everything.
  const extensionId = uuid();
  const extension = {id: extensionId};
  const collectionId = extensionIdToCollectionId(loggedInUser, extensionId);
  const transformer = new KeyRingEncryptionRemoteTransformer();
  yield* withSyncContext(function* (context) {
    yield* withServer(function* (server) {
      server.installCollection("storage-sync-crypto");
      server.installCollection(collectionId);
      server.installDeleteBucket();
      yield* withSignedInUser(loggedInUser, function* () {
        yield cryptoCollection._clear();

        // Do an `ensureKeysFor` to get access to keys.
        let collectionKeys = yield ExtensionStorageSync.ensureKeysFor([extensionId]);
        ok(collectionKeys.hasKeysFor([extensionId]),
           `ensureKeysFor should always return a keyring that has a key for ${extensionId}`);
        const extensionKey = collectionKeys.keyForCollection(extensionId).keyPairB64;

        // Set something to make sure that it gets re-uploaded when
        // uuid changes.
        yield ExtensionStorageSync.set(extension, {"my-key": 5}, context);
        yield ExtensionStorageSync.syncAll();

        let posts = server.getPosts();
        equal(posts.length, 2,
              "should have posted a new keyring and an extension datum");
        const postedKeys = posts[0];
        equal(postedKeys.path, collectionRecordsPath("storage-sync-crypto") + "/keys",
              "should have posted keyring to /keys");

        let body = yield transformer.decode(postedKeys.body.data);
        ok(body.uuid,
           "keyring should have a UUID");
        ok(body.keys,
           "keyring should have a keys attribute");
        ok(body.keys.default,
           "keyring should have a default key");
        deepEqual(extensionKey, body.keys.collections[extensionId],
                  "new keyring should have the same key that we uploaded");

        // Another client comes along and replaces the UUID.
        // In real life, this would mean changing the keys too, but
        // this test verifies that just changing the UUID is enough.
        const newKeyRingData = Object.assign({}, body, {
          uuid: "abcd",
          // Technically, last_modified should be served outside the
          // object, but the transformer will pass it through in
          // either direction, so this is OK.
          last_modified: 765,
        });
        server.clearCollection("storage-sync-crypto");
        server.etag = 765;
        yield server.encryptAndAddRecordOnlyOnce(transformer, "storage-sync-crypto", newKeyRingData);

        // Fake adding another extension just so that the keyring will
        // really get synced.
        const newExtension = uuid();
        const newKeyRing = yield ExtensionStorageSync.ensureKeysFor([newExtension]);

        // This should have detected the UUID change and flushed everything.
        // The keyring should, however, be the same, since we just
        // changed the UUID of the previously POSTed one.
        deepEqual(newKeyRing.keyForCollection(extensionId).keyPairB64, extensionKey,
                  "ensureKeysFor should have pulled down a new keyring with the same keys");

        // Syncing should reupload the data for the extension.
        yield ExtensionStorageSync.syncAll();
        posts = server.getPosts();
        equal(posts.length, 4,
              "should have posted keyring for new extension and reuploaded extension data");

        const finalKeyRingPost = posts[2];
        const reuploadedPost = posts[3];

        equal(finalKeyRingPost.path, collectionRecordsPath("storage-sync-crypto") + "/keys",
              "keyring for new extension should have been posted to /keys");
        let finalKeyRing = yield transformer.decode(finalKeyRingPost.body.data);
        equal(finalKeyRing.uuid, "abcd",
              "newly uploaded keyring should preserve UUID from replacement keyring");

        // Confirm that the data got reuploaded
        equal(reuploadedPost.path, collectionRecordsPath(collectionId) + "/key-my_2D_key",
              "extension data should be posted to path corresponding to its key");
        let reuploadedData = yield new CollectionKeyEncryptionRemoteTransformer(extensionId).decode(reuploadedPost.body.data);
        equal(reuploadedData.key, "my-key",
              "extension data should have a key attribute corresponding to the extension data key");
        equal(reuploadedData.data, 5,
              "extension data should have a data attribute corresponding to the extension data value");
      });
    });
  });
});

add_task(function* test_storage_sync_pulls_changes() {
  const extensionId = defaultExtensionId;
  const collectionId = defaultCollectionId;
  const extension = defaultExtension;
  yield* withContextAndServer(function* (context, server) {
    yield* withSignedInUser(loggedInUser, function* () {
      let transformer = new CollectionKeyEncryptionRemoteTransformer(extensionId);
      server.installCollection(collectionId);
      server.installCollection("storage-sync-crypto");

      let calls = [];
      yield ExtensionStorageSync.addOnChangedListener(extension, function() {
        calls.push(arguments);
      }, context);

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
  const extensionId = defaultExtensionId;
  const collectionId = defaultCollectionId;
  const extension = defaultExtension;
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

add_task(function* test_storage_sync_pulls_deletes() {
  const collectionId = defaultCollectionId;
  const extension = defaultExtension;
  yield* withContextAndServer(function* (context, server) {
    yield* withSignedInUser(loggedInUser, function* () {
      server.installCollection(collectionId);
      server.installCollection("storage-sync-crypto");

      yield ExtensionStorageSync.set(extension, {"my-key": 5}, context);
      yield ExtensionStorageSync.syncAll();
      server.clearPosts();

      let calls = [];
      yield ExtensionStorageSync.addOnChangedListener(extension, function() {
        calls.push(arguments);
      }, context);

      yield server.addRecord(collectionId, {
        "id": "key-my_2D_key",
        "deleted": true,
      });

      yield ExtensionStorageSync.syncAll();
      const remoteValues = (yield ExtensionStorageSync.get(extension, "my-key", context));
      ok(!remoteValues["my-key"],
         "ExtensionStorageSync.get() shows value was deleted by sync");

      equal(server.getPosts().length, 0,
            "pulling the delete shouldn't cause posts");

      equal(calls.length, 1,
            "syncing calls on-changed listener");
      deepEqual(calls[0][0], {"my-key": {oldValue: 5}});
      calls = [];

      // Syncing again doesn't do anything
      yield ExtensionStorageSync.syncAll();

      equal(calls.length, 0,
            "syncing again shouldn't call on-changed listener");
    });
  });
});

add_task(function* test_storage_sync_pushes_deletes() {
  const extensionId = uuid();
  const collectionId = extensionIdToCollectionId(loggedInUser, extensionId);
  const extension = {id: extensionId};
  yield cryptoCollection._clear();
  yield* withContextAndServer(function* (context, server) {
    yield* withSignedInUser(loggedInUser, function* () {
      server.installCollection(collectionId);
      server.installCollection("storage-sync-crypto");
      server.etag = 1000;

      yield ExtensionStorageSync.set(extension, {"my-key": 5}, context);

      let calls = [];
      ExtensionStorageSync.addOnChangedListener(extension, function() {
        calls.push(arguments);
      }, context);

      yield ExtensionStorageSync.syncAll();
      let posts = server.getPosts();
      equal(posts.length, 2,
            "pushing a non-deleted value should post keys and post the value to the server");

      yield ExtensionStorageSync.remove(extension, ["my-key"], context);
      equal(calls.length, 1,
            "deleting a value should call the on-changed listener");

      yield ExtensionStorageSync.syncAll();
      equal(calls.length, 1,
            "pushing a deleted value shouldn't call the on-changed listener");

      // Doesn't push keys because keys were pushed by a previous test.
      posts = server.getPosts();
      equal(posts.length, 3,
            "deleting a value should trigger another push");
      const post = posts[2];
      assertPostedUpdatedRecord(post, 1000);
      equal(post.path, collectionRecordsPath(collectionId) + "/key-my_2D_key",
            "pushing a deleted value should go to the same path");
      ok(post.method, "DELETE");
      ok(!post.body,
         "deleting a value shouldn't have a body");
    });
  });
});
