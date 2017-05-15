/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

do_get_profile();   // so we can use FxAccounts

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");
const {
  CollectionKeyEncryptionRemoteTransformer,
  CryptoCollection,
  ExtensionStorageSync,
  idToKey,
  KeyRingEncryptionRemoteTransformer,
  keyToId,
} = Cu.import("resource://gre/modules/ExtensionStorageSync.jsm", {});
Cu.import("resource://services-sync/engines/extension-storage.js");
Cu.import("resource://services-sync/keys.js");
Cu.import("resource://services-sync/util.js");

/* globals BulkKeyBundle, CommonUtils, EncryptionRemoteTransformer */
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

    // Set<Object> corresponding to records that might be served.
    // The format of these objects is defined in the documentation for #addRecord.
    this.records = [];

    // Collections that we have set up access to (see `installCollection`).
    this.collections = new Set();

    // ETag to serve with responses
    this.etag = 1;

    this.port = this.httpServer.identity.primaryPort;

    // POST requests we receive from the client go here
    this.posts = [];
    // DELETEd buckets will go here.
    this.deletedBuckets = [];
    // Anything in here will force the next POST to generate a conflict
    this.conflicts = [];
    // If this is true, reject the next request with a 401
    this.rejectNextAuthResponse = false;
    this.failedAuths = [];

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

  rejectNextAuthWith(response) {
    this.rejectNextAuthResponse = response;
  }

  checkAuth(request, response) {
    // FIXME: assert auth is "Bearer ...token..."
    if (this.rejectNextAuthResponse) {
      response.setStatusLine(null, 401, "Unauthorized");
      response.write(this.rejectNextAuthResponse);
      this.rejectNextAuthResponse = false;
      this.failedAuths.push(request);
      return true;
    }
    return false;
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
      if (this.checkAuth(request, response)) {
        return;
      }

      let bodyStr = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
      let body = JSON.parse(bodyStr);
      let defaults = body.defaults;
      for (let req of body.requests) {
        let headers = Object.assign({}, (defaults && defaults.headers) || {}, req.headers);
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
        const nextConflict = this.conflicts.shift();
        this.records.push(nextConflict);
        const {data} = nextConflict;
        dump(`responding with etag ${this.etag}\n`);
        postResponse = {
          responses: body.requests.map(req => {
            return {
              path: req.path,
              status: 412,
              headers: {"ETag": this.etag}, // is this correct??
              body: {
                details: {
                  existing: data,
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

  /**
   * Add a record to those that can be served by this server.
   *
   * @param {Object} properties  An object describing the record that
   *   should be served. The properties of this object are:
   * - collectionId {string} This record should only be served if a
   *   request is for this collection.
   * - predicate {Function} If present, this record should only be served if the
   *   predicate returns true. The predicate will be called with
   *   {request: Request, response: Response, since: number, server: KintoServer}.
   * - data {string} The record to serve.
   * - conflict {boolean} If present and true, this record is added to
   *   "conflicts" and won't be served, but will cause a conflict on
   *   the next push.
   */
  addRecord(properties) {
    if (!properties.conflict) {
      this.records.push(properties);
    } else {
      this.conflicts.push(properties);
    }

    this.installCollection(properties.collectionId);
  }

  /**
   * Tell the server to set up a route for this collection.
   *
   * This will automatically be called for any collection to which you `addRecord`.
   *
   * @param {string} collectionId   the collection whose route we
   *    should set up.
   */
  installCollection(collectionId) {
    if (this.collections.has(collectionId)) {
      return;
    }
    this.collections.add(collectionId);
    const remoteRecordsPath = "/v1" + collectionRecordsPath(encodeURIComponent(collectionId));
    this.httpServer.registerPathHandler(remoteRecordsPath, this.handleGetRecords.bind(this, collectionId));
  }

  handleGetRecords(collectionId, request, response) {
    if (this.checkAuth(request, response)) {
      return;
    }

    if (request.method != "GET") {
      do_throw(`only GET is supported on ${request.path}`);
    }

    let sinceMatch = request.queryString.match(/(^|&)_since=(\d+)/);
    let since = sinceMatch && parseInt(sinceMatch[2], 10);

    response.setStatusLine(null, 200, "OK");
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.setHeader("Date", (new Date()).toUTCString());
    response.setHeader("ETag", this.etag.toString());

    const records = this.records.filter(properties => {
      if (properties.collectionId != collectionId) {
        return false;
      }

      if (properties.predicate) {
        const predAllowed = properties.predicate({
          request: request,
          response: response,
          since: since,
          server: this,
        });
        if (!predAllowed) {
          return false;
        }
      }

      return true;
    }).map(properties => properties.data);

    const body = JSON.stringify({
      "data": records,
    });
    response.write(body);
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
      this.records = [];

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
  installKeyRing(fxaService, keysData, salts, etag, properties) {
    const keysRecord = {
      "id": "keys",
      "keys": keysData,
      "salts": salts,
      "last_modified": etag,
    };
    this.etag = etag;
    const transformer = new KeyRingEncryptionRemoteTransformer(fxaService);
    this.encryptAndAddRecord(transformer, Object.assign({}, properties, {
      collectionId: "storage-sync-crypto",
      data: keysRecord,
    }));
  }

  encryptAndAddRecord(transformer, properties) {
    return transformer.encode(properties.data).then(encrypted => {
      this.addRecord(Object.assign({}, properties, {data: encrypted}));
    });
  }

  stop() {
    this.httpServer.stop(() => { });
  }
}

/**
 * Predicate that represents a record appearing at some time.
 * Requests with "_since" before this time should see this record,
 * unless the server itself isn't at this time yet (etag is before
 * this time).
 *
 * Requests with _since after this time shouldn't see this record any
 * more, since it hasn't changed after this time.
 *
 * @param {int} startTime  the etag at which time this record should
 *    start being available (and thus, the predicate should start
 *    returning true)
 * @returns {Function}
 */
function appearsAt(startTime) {
  return function({since, server}) {
    return since < startTime && startTime < server.etag;
  };
}

// Run a block of code with access to a KintoServer.
async function withServer(f) {
  let server = new KintoServer();
  // Point the sync.storage client to use the test server we've just started.
  Services.prefs.setCharPref("webextensions.storage.sync.serverURL",
                             `http://localhost:${server.port}/v1`);
  try {
    await f(server);
  } finally {
    server.stop();
  }
}

// Run a block of code with access to both a sync context and a
// KintoServer. This is meant as a workaround for eslint's refusal to
// let me have 5 nested callbacks.
async function withContextAndServer(f) {
  await withSyncContext(async function(context) {
    await withServer(async function(server) {
      await f(context, server);
    });
  });
}

// Run a block of code with fxa mocked out to return a specific user.
// Calls the given function with an ExtensionStorageSync instance that
// was constructed using a mocked FxAccounts instance.
async function withSignedInUser(user, f) {
  let fxaServiceMock = {
    getSignedInUser() {
      return Promise.resolve(user);
    },
    getOAuthToken() {
      return Promise.resolve("some-access-token");
    },
    sessionStatus() {
      return Promise.resolve(true);
    },
    removeCachedOAuthToken() {
      return Promise.resolve();
    },
  };

  let telemetryMock = {
    _calls: [],
    _histograms: {},
    scalarSet(name, value) {
      this._calls.push({method: "scalarSet", name, value});
    },
    keyedScalarSet(name, key, value) {
      this._calls.push({method: "keyedScalarSet", name, key, value});
    },
    getKeyedHistogramById(name) {
      let self = this;
      return {
        add(key, value) {
          if (!self._histograms[name]) {
            self._histograms[name] = [];
          }
          self._histograms[name].push(value);
        },
      };
    },
  };
  let extensionStorageSync = new ExtensionStorageSync(fxaServiceMock, telemetryMock);
  await f(extensionStorageSync, fxaServiceMock);
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
const assertPostedEncryptedKeys = async function(fxaService, post) {
  equal(post.path, collectionRecordsPath("storage-sync-crypto") + "/keys");

  let body = await new KeyRingEncryptionRemoteTransformer(fxaService).decode(post.body.data);
  ok(body.keys, `keys object should be present in decoded body`);
  ok(body.keys.default, `keys object should have a default key`);
  ok(body.salts, `salts object should be present in decoded body`);
  return body;
};

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

// Assert that this post was posted for a given extension.
const assertExtensionRecord = async function(fxaService, post, extension, key) {
  const extensionId = extension.id;
  const cryptoCollection = new CryptoCollection(fxaService);
  const hashedId = "id-" + (await cryptoCollection.hashWithExtensionSalt(keyToId(key), extensionId));
  const collectionId = await cryptoCollection.extensionIdToCollectionId(extensionId);
  const transformer = new CollectionKeyEncryptionRemoteTransformer(cryptoCollection, extensionId);
  equal(post.path, `${collectionRecordsPath(collectionId)}/${hashedId}`,
        "decrypted data should be posted to path corresponding to its key");
  let decoded = await transformer.decode(post.body.data);
  equal(decoded.key, key,
        "decrypted data should have a key attribute corresponding to the extension data key");
  return decoded;
};

// Tests using this ID will share keys in local storage, so be careful.
const defaultExtensionId = "{13bdde76-4dc7-11e6-9bdc-54ee758d6342}";
const defaultExtension = {id: defaultExtensionId};

const BORING_KB = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
const loggedInUser = {
  uid: "0123456789abcdef0123456789abcdef",
  kB: BORING_KB,
  oauthTokens: {
    "sync:addon-storage": {
      token: "some-access-token",
    },
  },
};

function uuid() {
  const uuidgen = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
  return uuidgen.generateUUID().toString();
}

add_task(async function test_key_to_id() {
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

add_task(async function test_extension_id_to_collection_id() {
  const extensionId = "{9419cce6-5435-11e6-84bf-54ee758d6342}";
  // FIXME: this doesn't actually require the signed in user, but the
  // extensionIdToCollectionId method exists on CryptoCollection,
  // which needs an fxaService to be instantiated.
  await withSignedInUser(loggedInUser, async function(extensionStorageSync, fxaService) {
    // Fake a static keyring since the server doesn't exist.
    const salt = "Scgx8RJ8Y0rxMGFYArUiKeawlW+0zJyFmtTDvro9qPo=";
    const cryptoCollection = new CryptoCollection(fxaService);
    await cryptoCollection._setSalt(extensionId, salt);

    equal(await cryptoCollection.extensionIdToCollectionId(extensionId),
          "ext-0_QHA1P93_yJoj7ONisrR0lW6uN4PZ3Ii-rT-QOjtvo");
  });
});

add_task(async function ensureCanSync_posts_new_keys() {
  const extensionId = uuid();
  await withContextAndServer(async function(context, server) {
    await withSignedInUser(loggedInUser, async function(extensionStorageSync, fxaService) {
      server.installCollection("storage-sync-crypto");
      server.etag = 1000;

      let newKeys = await extensionStorageSync.ensureCanSync([extensionId]);
      ok(newKeys.hasKeysFor([extensionId]), `key isn't present for ${extensionId}`);

      let posts = server.getPosts();
      equal(posts.length, 1);
      const post = posts[0];
      assertPostedNewRecord(post);
      const body = await assertPostedEncryptedKeys(fxaService, post);
      const oldSalt = body.salts[extensionId];
      ok(body.keys.collections[extensionId], `keys object should have a key for ${extensionId}`);
      ok(oldSalt, `salts object should have a salt for ${extensionId}`);

      // Try adding another key to make sure that the first post was
      // OK, even on a new profile.
      await extensionStorageSync.cryptoCollection._clear();
      server.clearPosts();
      // Restore the first posted keyring, but add a last_modified date
      const firstPostedKeyring = Object.assign({}, post.body.data, {last_modified: server.etag});
      server.addRecord({
        data: firstPostedKeyring,
        collectionId: "storage-sync-crypto",
        predicate: appearsAt(250),
      });
      const extensionId2 = uuid();
      newKeys = await extensionStorageSync.ensureCanSync([extensionId2]);
      ok(newKeys.hasKeysFor([extensionId]), `didn't forget key for ${extensionId}`);
      ok(newKeys.hasKeysFor([extensionId2]), `new key generated for ${extensionId2}`);

      posts = server.getPosts();
      equal(posts.length, 1);
      const newPost = posts[posts.length - 1];
      const newBody = await assertPostedEncryptedKeys(fxaService, newPost);
      ok(newBody.keys.collections[extensionId], `keys object should have a key for ${extensionId}`);
      ok(newBody.keys.collections[extensionId2], `keys object should have a key for ${extensionId2}`);
      ok(newBody.salts[extensionId], `salts object should have a key for ${extensionId}`);
      ok(newBody.salts[extensionId2], `salts object should have a key for ${extensionId2}`);
      equal(oldSalt, newBody.salts[extensionId], `old salt should be preserved in post`);
    });
  });
});

add_task(async function ensureCanSync_pulls_key() {
  // ensureCanSync is implemented by adding a key to our local record
  // and doing a sync. This means that if the same key exists
  // remotely, we get a "conflict". Ensure that we handle this
  // correctly -- we keep the server key (since presumably it's
  // already been used to encrypt records) and we don't wipe out other
  // collections' keys.
  const extensionId = uuid();
  const extensionId2 = uuid();
  const extensionOnlyKey = uuid();
  const extensionOnlySalt = uuid();
  const DEFAULT_KEY = new BulkKeyBundle("[default]");
  DEFAULT_KEY.generateRandom();
  const RANDOM_KEY = new BulkKeyBundle(extensionId);
  RANDOM_KEY.generateRandom();
  await withContextAndServer(async function(context, server) {
    await withSignedInUser(loggedInUser, async function(extensionStorageSync, fxaService) {
      // FIXME: generating a random salt probably shouldn't require a CryptoCollection?
      const cryptoCollection = new CryptoCollection(fxaService);
      const RANDOM_SALT = cryptoCollection.getNewSalt();
      await extensionStorageSync.cryptoCollection._clear();
      const keysData = {
        "default": DEFAULT_KEY.keyPairB64,
        "collections": {
          [extensionId]: RANDOM_KEY.keyPairB64,
        },
      };
      const saltData = {
        [extensionId]: RANDOM_SALT,
      };
      server.installKeyRing(fxaService, keysData, saltData, 950, {
        predicate: appearsAt(900),
      });

      let collectionKeys = await extensionStorageSync.ensureCanSync([extensionId]);
      assertKeyRingKey(collectionKeys, extensionId, RANDOM_KEY);

      let posts = server.getPosts();
      equal(posts.length, 0,
            "ensureCanSync shouldn't push when the server keyring has the right key");

      // Another client generates a key for extensionId2
      const newKey = new BulkKeyBundle(extensionId2);
      newKey.generateRandom();
      keysData.collections[extensionId2] = newKey.keyPairB64;
      saltData[extensionId2] = cryptoCollection.getNewSalt();
      server.installKeyRing(fxaService, keysData, saltData, 1050, {
        predicate: appearsAt(1000),
      });

      let newCollectionKeys = await extensionStorageSync.ensureCanSync([extensionId, extensionId2]);
      assertKeyRingKey(newCollectionKeys, extensionId2, newKey);
      assertKeyRingKey(newCollectionKeys, extensionId, RANDOM_KEY,
                       `ensureCanSync shouldn't lose the old key for ${extensionId}`);

      posts = server.getPosts();
      equal(posts.length, 0, "ensureCanSync shouldn't push when updating keys");

      // Another client generates a key, but not a salt, for extensionOnlyKey
      const onlyKey = new BulkKeyBundle(extensionOnlyKey);
      onlyKey.generateRandom();
      keysData.collections[extensionOnlyKey] = onlyKey.keyPairB64;
      server.installKeyRing(fxaService, keysData, saltData, 1150, {
        predicate: appearsAt(1100),
      });

      let withNewKey = await extensionStorageSync.ensureCanSync([extensionId, extensionOnlyKey]);
      dump(`got ${JSON.stringify(withNewKey.asWBO().cleartext)}\n`);
      assertKeyRingKey(withNewKey, extensionOnlyKey, onlyKey);
      assertKeyRingKey(withNewKey, extensionId, RANDOM_KEY,
                       `ensureCanSync shouldn't lose the old key for ${extensionId}`);

      posts = server.getPosts();
      equal(posts.length, 1, "ensureCanSync should push when generating a new salt");
      const withNewKeyRecord = await assertPostedEncryptedKeys(fxaService, posts[0]);
      // We don't a priori know what the new salt is
      dump(`${JSON.stringify(withNewKeyRecord)}\n`);
      ok(withNewKeyRecord.salts[extensionOnlyKey],
         `ensureCanSync should generate a salt for an extension that only had a key`);

      // Another client generates a key, but not a salt, for extensionOnlyKey
      const newSalt = cryptoCollection.getNewSalt();
      saltData[extensionOnlySalt] = newSalt;
      server.installKeyRing(fxaService, keysData, saltData, 1250, {
        predicate: appearsAt(1200),
      });

      let withOnlySaltKey = await extensionStorageSync.ensureCanSync([extensionId, extensionOnlySalt]);
      assertKeyRingKey(withOnlySaltKey, extensionId, RANDOM_KEY,
                       `ensureCanSync shouldn't lose the old key for ${extensionId}`);
      // We don't a priori know what the new key is
      ok(withOnlySaltKey.hasKeysFor([extensionOnlySalt]),
         `ensureCanSync generated a key for an extension that only had a salt`);

      posts = server.getPosts();
      equal(posts.length, 2, "ensureCanSync should push when generating a new key");
      const withNewSaltRecord = await assertPostedEncryptedKeys(fxaService, posts[1]);
      equal(withNewSaltRecord.salts[extensionOnlySalt], newSalt,
            "ensureCanSync should keep the existing salt when generating only a key");
    });
  });
});

add_task(async function ensureCanSync_handles_conflicts() {
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
  await withContextAndServer(async function(context, server) {
    await withSignedInUser(loggedInUser, async function(extensionStorageSync, fxaService) {
      // FIXME: generating salts probably shouldn't rely on a CryptoCollection
      const cryptoCollection = new CryptoCollection(fxaService);
      const RANDOM_SALT = cryptoCollection.getNewSalt();
      const keysData = {
        "default": DEFAULT_KEY.keyPairB64,
        "collections": {
          [extensionId]: RANDOM_KEY.keyPairB64,
        },
      };
      const saltData = {
        [extensionId]: RANDOM_SALT,
      };
      server.installKeyRing(fxaService, keysData, saltData, 765, {conflict: true});

      await extensionStorageSync.cryptoCollection._clear();

      let collectionKeys = await extensionStorageSync.ensureCanSync([extensionId]);
      assertKeyRingKey(collectionKeys, extensionId, RANDOM_KEY,
                       `syncing keyring should keep the server key for ${extensionId}`);

      let posts = server.getPosts();
      equal(posts.length, 1,
            "syncing keyring should have tried to post a keyring");
      const failedPost = posts[0];
      assertPostedNewRecord(failedPost);
      let body = await assertPostedEncryptedKeys(fxaService, failedPost);
      // This key will be the one the client generated locally, so
      // we don't know what its value will be
      ok(body.keys.collections[extensionId],
         `decrypted failed post should have a key for ${extensionId}`);
      notEqual(body.keys.collections[extensionId], RANDOM_KEY.keyPairB64,
               `decrypted failed post should have a randomly-generated key for ${extensionId}`);
    });
  });
});

add_task(async function ensureCanSync_handles_deleted_conflicts() {
  // A keyring can be deleted, and this changes the format of the 412
  // Conflict response from the Kinto server. Make sure we handle it correctly.
  const extensionId = uuid();
  const extensionId2 = uuid();
  await withContextAndServer(async function(context, server) {
    server.installCollection("storage-sync-crypto");
    server.installDeleteBucket();
    await withSignedInUser(loggedInUser, async function(extensionStorageSync, fxaService) {
      server.etag = 700;
      await extensionStorageSync.cryptoCollection._clear();

      // Generate keys that we can check for later.
      let collectionKeys = await extensionStorageSync.ensureCanSync([extensionId]);
      const extensionKey = collectionKeys.keyForCollection(extensionId);
      server.clearPosts();

      // This is the response that the Kinto server return when the
      // keyring has been deleted.
      server.addRecord({collectionId: "storage-sync-crypto", conflict: true, data: null, etag: 765});

      // Try to add a new extension to trigger a sync of the keyring.
      let collectionKeys2 = await extensionStorageSync.ensureCanSync([extensionId2]);

      assertKeyRingKey(collectionKeys2, extensionId, extensionKey,
                       `syncing keyring should keep our local key for ${extensionId}`);

      deepEqual(server.getDeletedBuckets(), ["default"],
                "Kinto server should have been wiped when keyring was thrown away");

      let posts = server.getPosts();
      equal(posts.length, 2,
            "syncing keyring should have tried to post a keyring twice");
      // The first post got a conflict.
      const failedPost = posts[0];
      assertPostedUpdatedRecord(failedPost, 700);
      let body = await assertPostedEncryptedKeys(fxaService, failedPost);

      deepEqual(body.keys.collections[extensionId], extensionKey.keyPairB64,
                `decrypted failed post should have the key for ${extensionId}`);

      // The second post was after the wipe, and succeeded.
      const afterWipePost = posts[1];
      assertPostedNewRecord(afterWipePost);
      let afterWipeBody = await assertPostedEncryptedKeys(fxaService, afterWipePost);

      deepEqual(afterWipeBody.keys.collections[extensionId], extensionKey.keyPairB64,
                `decrypted new post should have preserved the key for ${extensionId}`);
    });
  });
});

add_task(async function ensureCanSync_handles_flushes() {
  // One of the ways that bug 1359879 presents is as bug 1350088. This
  // seems to be the symptom that results when the user had two
  // devices, one of which was not syncing at the time the keyring was
  // lost. Ensure we can recover for these users as well.
  const extensionId = uuid();
  const extensionId2 = uuid();
  await withContextAndServer(async function(context, server) {
    server.installCollection("storage-sync-crypto");
    server.installDeleteBucket();
    await withSignedInUser(loggedInUser, async function(extensionStorageSync, fxaService) {
      server.etag = 700;
      // Generate keys that we can check for later.
      let collectionKeys = await extensionStorageSync.ensureCanSync([extensionId]);
      const extensionKey = collectionKeys.keyForCollection(extensionId);
      server.clearPosts();

      // last_modified is new, but there is no data.
      server.etag = 800;

      // Try to add a new extension to trigger a sync of the keyring.
      let collectionKeys2 = await extensionStorageSync.ensureCanSync([extensionId2]);

      assertKeyRingKey(collectionKeys2, extensionId, extensionKey,
                       `syncing keyring should keep our local key for ${extensionId}`);

      deepEqual(server.getDeletedBuckets(), ["default"],
                "Kinto server should have been wiped when keyring was thrown away");

      let posts = server.getPosts();
      equal(posts.length, 1,
            "syncing keyring should have tried to post a keyring once");

      const post = posts[0];
      assertPostedNewRecord(post);
      let postBody = await assertPostedEncryptedKeys(fxaService, post);

      deepEqual(postBody.keys.collections[extensionId], extensionKey.keyPairB64,
                `decrypted new post should have preserved the key for ${extensionId}`);
    });
  });
});

add_task(async function checkSyncKeyRing_reuploads_keys() {
  // Verify that when keys are present, they are reuploaded with the
  // new kB when we call touchKeys().
  const extensionId = uuid();
  let extensionKey, extensionSalt;
  await withContextAndServer(async function(context, server) {
    await withSignedInUser(loggedInUser, async function(extensionStorageSync, fxaService) {
      server.installCollection("storage-sync-crypto");
      server.etag = 765;

      await extensionStorageSync.cryptoCollection._clear();

      // Do an `ensureCanSync` to generate some keys.
      let collectionKeys = await extensionStorageSync.ensureCanSync([extensionId]);
      ok(collectionKeys.hasKeysFor([extensionId]),
         `ensureCanSync should return a keyring that has a key for ${extensionId}`);
      extensionKey = collectionKeys.keyForCollection(extensionId).keyPairB64;
      equal(server.getPosts().length, 1,
            "generating a key that doesn't exist on the server should post it");
      const body = await assertPostedEncryptedKeys(fxaService, server.getPosts()[0]);
      extensionSalt = body.salts[extensionId];
    });

    // The user changes their password. This is their new kB, with
    // the last f changed to an e.
    const NOVEL_KB = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdee";
    const newUser = Object.assign({}, loggedInUser, {kB: NOVEL_KB});
    let postedKeys;
    await withSignedInUser(newUser, async function(extensionStorageSync, fxaService) {
      await extensionStorageSync.checkSyncKeyRing();

      let posts = server.getPosts();
      equal(posts.length, 2,
            "when kB changes, checkSyncKeyRing should post the keyring reencrypted with the new kB");
      postedKeys = posts[1];
      assertPostedUpdatedRecord(postedKeys, 765);

      let body = await assertPostedEncryptedKeys(fxaService, postedKeys);
      deepEqual(body.keys.collections[extensionId], extensionKey,
                `the posted keyring should have the same key for ${extensionId} as the old one`);
      deepEqual(body.salts[extensionId], extensionSalt,
                `the posted keyring should have the same salt for ${extensionId} as the old one`);
    });

    // Verify that with the old kB, we can't decrypt the record.
    await withSignedInUser(loggedInUser, async function(extensionStorageSync, fxaService) {
      let error;
      try {
        await new KeyRingEncryptionRemoteTransformer(fxaService).decode(postedKeys.body.data);
      } catch (e) {
        error = e;
      }
      ok(error, "decrypting the keyring with the old kB should fail");
      ok(Utils.isHMACMismatch(error) || KeyRingEncryptionRemoteTransformer.isOutdatedKB(error),
         "decrypting the keyring with the old kB should throw an HMAC mismatch");
    });
  });
});

add_task(async function checkSyncKeyRing_overwrites_on_conflict() {
  // If there is already a record on the server that was encrypted
  // with a different kB, we wipe the server, clear sync state, and
  // overwrite it with our keys.
  const extensionId = uuid();
  let extensionKey;
  await withSyncContext(async function(context) {
    await withServer(async function(server) {
      // The old device has this kB, which is very similar to the
      // current kB but with the last f changed to an e.
      const NOVEL_KB = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdee";
      const oldUser = Object.assign({}, loggedInUser, {kB: NOVEL_KB});
      server.installDeleteBucket();
      await withSignedInUser(oldUser, async function(extensionStorageSync, fxaService) {
        await server.installKeyRing(fxaService, {}, {}, 765);
      });

      // Now we have this new user with a different kB.
      await withSignedInUser(loggedInUser, async function(extensionStorageSync, fxaService) {
        await extensionStorageSync.cryptoCollection._clear();

        // Do an `ensureCanSync` to generate some keys.
        // This will try to sync, notice that the record is
        // undecryptable, and clear the server.
        let collectionKeys = await extensionStorageSync.ensureCanSync([extensionId]);
        ok(collectionKeys.hasKeysFor([extensionId]),
           `ensureCanSync should always return a keyring with a key for ${extensionId}`);
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

        let body = await new KeyRingEncryptionRemoteTransformer(fxaService).decode(postedKeys.body.data);
        ok(body.uuid, "new keyring should have a UUID");
        equal(typeof body.uuid, "string", "keyring UUIDs should be strings");
        notEqual(body.uuid, "abcd",
                 "new keyring should not have the same UUID as previous keyring");
        ok(body.keys,
           "new keyring should have a keys attribute");
        ok(body.keys.default, "new keyring should have a default key");
        // We should keep the extension key that was in our uploaded version.
        deepEqual(extensionKey, body.keys.collections[extensionId],
                  "ensureCanSync should have returned keyring with the same key that was uploaded");

        // This should be a no-op; the keys were uploaded as part of ensurekeysfor
        await extensionStorageSync.checkSyncKeyRing();
        equal(server.getPosts().length, 1,
              "checkSyncKeyRing should not need to post keys after they were reuploaded");
      });
    });
  });
});

add_task(async function checkSyncKeyRing_flushes_on_uuid_change() {
  // If we can decrypt the record, but the UUID has changed, that
  // means another client has wiped the server and reuploaded a
  // keyring, so reset sync state and reupload everything.
  const extensionId = uuid();
  const extension = {id: extensionId};
  await withSyncContext(async function(context) {
    await withServer(async function(server) {
      server.installCollection("storage-sync-crypto");
      server.installDeleteBucket();
      await withSignedInUser(loggedInUser, async function(extensionStorageSync, fxaService) {
        const cryptoCollection = new CryptoCollection(fxaService);
        const transformer = new KeyRingEncryptionRemoteTransformer(fxaService);
        await extensionStorageSync.cryptoCollection._clear();

        // Do an `ensureCanSync` to get access to keys and salt.
        let collectionKeys = await extensionStorageSync.ensureCanSync([extensionId]);
        const collectionId = await cryptoCollection.extensionIdToCollectionId(extensionId);
        server.installCollection(collectionId);

        ok(collectionKeys.hasKeysFor([extensionId]),
           `ensureCanSync should always return a keyring that has a key for ${extensionId}`);
        const extensionKey = collectionKeys.keyForCollection(extensionId).keyPairB64;

        // Set something to make sure that it gets re-uploaded when
        // uuid changes.
        await extensionStorageSync.set(extension, {"my-key": 5}, context);
        await extensionStorageSync.syncAll();

        let posts = server.getPosts();
        equal(posts.length, 2,
              "should have posted a new keyring and an extension datum");
        const postedKeys = posts[0];
        equal(postedKeys.path, collectionRecordsPath("storage-sync-crypto") + "/keys",
              "should have posted keyring to /keys");

        let body = await transformer.decode(postedKeys.body.data);
        ok(body.uuid,
           "keyring should have a UUID");
        ok(body.keys,
           "keyring should have a keys attribute");
        ok(body.keys.default,
           "keyring should have a default key");
        ok(body.salts[extensionId],
           `keyring should have a salt for ${extensionId}`);
        const extensionSalt = body.salts[extensionId];
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
        server.etag = 1000;
        await server.encryptAndAddRecord(transformer, {
          collectionId: "storage-sync-crypto",
          data: newKeyRingData,
          predicate: appearsAt(800),
        });

        // Fake adding another extension just so that the keyring will
        // really get synced.
        const newExtension = uuid();
        const newKeyRing = await extensionStorageSync.ensureCanSync([newExtension]);

        // This should have detected the UUID change and flushed everything.
        // The keyring should, however, be the same, since we just
        // changed the UUID of the previously POSTed one.
        deepEqual(newKeyRing.keyForCollection(extensionId).keyPairB64, extensionKey,
                  "ensureCanSync should have pulled down a new keyring with the same keys");

        // Syncing should reupload the data for the extension.
        await extensionStorageSync.syncAll();
        posts = server.getPosts();
        equal(posts.length, 4,
              "should have posted keyring for new extension and reuploaded extension data");

        const finalKeyRingPost = posts[2];
        const reuploadedPost = posts[3];

        equal(finalKeyRingPost.path, collectionRecordsPath("storage-sync-crypto") + "/keys",
              "keyring for new extension should have been posted to /keys");
        let finalKeyRing = await transformer.decode(finalKeyRingPost.body.data);
        equal(finalKeyRing.uuid, "abcd",
              "newly uploaded keyring should preserve UUID from replacement keyring");
        deepEqual(finalKeyRing.salts[extensionId], extensionSalt,
                  "newly uploaded keyring should preserve salts from existing salts");

        // Confirm that the data got reuploaded
        let reuploadedData = await assertExtensionRecord(fxaService, reuploadedPost, extension, "my-key");
        equal(reuploadedData.data, 5,
              "extension data should have a data attribute corresponding to the extension data value");
      });
    });
  });
});

add_task(async function test_storage_sync_pulls_changes() {
  const extensionId = defaultExtensionId;
  const extension = defaultExtension;
  await withContextAndServer(async function(context, server) {
    await withSignedInUser(loggedInUser, async function(extensionStorageSync, fxaService) {
      const cryptoCollection = new CryptoCollection(fxaService);
      let transformer = new CollectionKeyEncryptionRemoteTransformer(cryptoCollection, extensionId);
      server.installCollection("storage-sync-crypto");

      let calls = [];
      await extensionStorageSync.addOnChangedListener(extension, function() {
        calls.push(arguments);
      }, context);

      await extensionStorageSync.ensureCanSync([extensionId]);
      const collectionId = await cryptoCollection.extensionIdToCollectionId(extensionId);
      await server.encryptAndAddRecord(transformer, {
        collectionId,
        data: {
          "id": "key-remote_2D_key",
          "key": "remote-key",
          "data": 6,
        },
        predicate: appearsAt(850),
      });
      server.etag = 900;

      await extensionStorageSync.syncAll();
      const remoteValue = (await extensionStorageSync.get(extension, "remote-key", context))["remote-key"];
      equal(remoteValue, 6,
            "ExtensionStorageSync.get() returns value retrieved from sync");

      equal(calls.length, 1,
            "syncing calls on-changed listener");
      deepEqual(calls[0][0], {"remote-key": {newValue: 6}});
      calls = [];

      // Syncing again doesn't do anything
      await extensionStorageSync.syncAll();

      equal(calls.length, 0,
            "syncing again shouldn't call on-changed listener");

      // Updating the server causes us to pull down the new value
      server.etag = 1000;
      await server.encryptAndAddRecord(transformer, {
        collectionId,
        data: {
          "id": "key-remote_2D_key",
          "key": "remote-key",
          "data": 7,
        },
        predicate: appearsAt(950),
      });

      await extensionStorageSync.syncAll();
      const remoteValue2 = (await extensionStorageSync.get(extension, "remote-key", context))["remote-key"];
      equal(remoteValue2, 7,
            "ExtensionStorageSync.get() returns value updated from sync");

      equal(calls.length, 1,
            "syncing calls on-changed listener on update");
      deepEqual(calls[0][0], {"remote-key": {oldValue: 6, newValue: 7}});
    });
  });
});

add_task(async function test_storage_sync_pushes_changes() {
  // FIXME: This test relies on the fact that previous tests pushed
  // keys and salts for the default extension ID
  const extension = defaultExtension;
  const extensionId = defaultExtensionId;
  await withContextAndServer(async function(context, server) {
    await withSignedInUser(loggedInUser, async function(extensionStorageSync, fxaService) {
      const cryptoCollection = new CryptoCollection(fxaService);
      const collectionId = await cryptoCollection.extensionIdToCollectionId(extensionId);
      server.installCollection(collectionId);
      server.installCollection("storage-sync-crypto");
      server.etag = 1000;

      await extensionStorageSync.set(extension, {"my-key": 5}, context);

      // install this AFTER we set the key to 5...
      let calls = [];
      extensionStorageSync.addOnChangedListener(extension, function() {
        calls.push(arguments);
      }, context);

      await extensionStorageSync.syncAll();
      const localValue = (await extensionStorageSync.get(extension, "my-key", context))["my-key"];
      equal(localValue, 5,
            "pushing an ExtensionStorageSync value shouldn't change local value");
      const hashedId = "id-" + (await cryptoCollection.hashWithExtensionSalt("key-my_2D_key", extensionId));

      let posts = server.getPosts();
      // FIXME: Keys were pushed in a previous test
      equal(posts.length, 1,
            "pushing a value should cause a post to the server");
      const post = posts[0];
      assertPostedNewRecord(post);
      equal(post.path, `${collectionRecordsPath(collectionId)}/${hashedId}`,
            "pushing a value should have a path corresponding to its id");

      const encrypted = post.body.data;
      ok(encrypted.ciphertext,
         "pushing a value should post an encrypted record");
      ok(!encrypted.data,
         "pushing a value should not have any plaintext data");
      equal(encrypted.id, hashedId,
            "pushing a value should use a kinto-friendly record ID");

      const record = await assertExtensionRecord(fxaService, post, extension, "my-key");
      equal(record.data, 5,
            "when decrypted, a pushed value should have a data field corresponding to its storage.sync value");
      equal(record.id, "key-my_2D_key",
            "when decrypted, a pushed value should have an id field corresponding to its record ID");

      equal(calls.length, 0,
            "pushing a value shouldn't call the on-changed listener");

      await extensionStorageSync.set(extension, {"my-key": 6}, context);
      await extensionStorageSync.syncAll();

      // Doesn't push keys because keys were pushed by a previous test.
      posts = server.getPosts();
      equal(posts.length, 2,
            "updating a value should trigger another push");
      const updatePost = posts[1];
      assertPostedUpdatedRecord(updatePost, 1000);
      equal(updatePost.path, `${collectionRecordsPath(collectionId)}/${hashedId}`,
            "pushing an updated value should go to the same path");

      const updateEncrypted = updatePost.body.data;
      ok(updateEncrypted.ciphertext,
         "pushing an updated value should still be encrypted");
      ok(!updateEncrypted.data,
         "pushing an updated value should not have any plaintext visible");
      equal(updateEncrypted.id, hashedId,
            "pushing an updated value should maintain the same ID");
    });
  });
});

add_task(async function test_storage_sync_retries_failed_auth() {
  const extensionId = uuid();
  const extension = {id: extensionId};
  await withContextAndServer(async function(context, server) {
    await withSignedInUser(loggedInUser, async function(extensionStorageSync, fxaService) {
      const cryptoCollection = new CryptoCollection(fxaService);
      let transformer = new CollectionKeyEncryptionRemoteTransformer(cryptoCollection, extensionId);
      server.installCollection("storage-sync-crypto");

      await extensionStorageSync.ensureCanSync([extensionId]);
      await extensionStorageSync.set(extension, {"my-key": 5}, context);
      const collectionId = await cryptoCollection.extensionIdToCollectionId(extensionId);
      // Put a remote record just to verify that eventually we succeeded
      await server.encryptAndAddRecord(transformer, {
        collectionId,
        data: {
          "id": "key-remote_2D_key",
          "key": "remote-key",
          "data": 6,
        },
        predicate: appearsAt(850),
      });
      server.etag = 900;

      // This is a typical response from a production stack if your
      // bearer token is bad.
      server.rejectNextAuthWith("{\"code\": 401, \"errno\": 104, \"error\": \"Unauthorized\", \"message\": \"Please authenticate yourself to use this endpoint\"}");
      await extensionStorageSync.syncAll();

      equal(server.failedAuths.length, 1,
            "an auth was failed");

      const remoteValue = (await extensionStorageSync.get(extension, "remote-key", context))["remote-key"];
      equal(remoteValue, 6,
            "ExtensionStorageSync.get() returns value retrieved from sync");


      // Try again with an emptier JSON body to make sure this still
      // works with a less-cooperative server.
      await server.encryptAndAddRecord(transformer, {
        collectionId,
        data: {
          "id": "key-remote_2D_key",
          "key": "remote-key",
          "data": 7,
        },
        predicate: appearsAt(950),
      });
      server.etag = 1000;
      // Need to write a JSON response.
      // kinto.js 9.0.2 doesn't throw unless there's json.
      // See https://github.com/Kinto/kinto-http.js/issues/192.
      server.rejectNextAuthWith("{}");

      await extensionStorageSync.syncAll();

      equal(server.failedAuths.length, 2,
            "an auth was failed");

      const newRemoteValue = (await extensionStorageSync.get(extension, "remote-key", context))["remote-key"];
      equal(newRemoteValue, 7,
            "ExtensionStorageSync.get() returns value retrieved from sync");
    });
  });
});

add_task(async function test_storage_sync_pulls_conflicts() {
  const extensionId = uuid();
  const extension = {id: extensionId};
  await withContextAndServer(async function(context, server) {
    await withSignedInUser(loggedInUser, async function(extensionStorageSync, fxaService) {
      const cryptoCollection = new CryptoCollection(fxaService);
      let transformer = new CollectionKeyEncryptionRemoteTransformer(cryptoCollection, extensionId);
      server.installCollection("storage-sync-crypto");

      await extensionStorageSync.ensureCanSync([extensionId]);
      const collectionId = await cryptoCollection.extensionIdToCollectionId(extensionId);
      await server.encryptAndAddRecord(transformer, {
        collectionId,
        data: {
          "id": "key-remote_2D_key",
          "key": "remote-key",
          "data": 6,
        },
        predicate: appearsAt(850),
      });
      server.etag = 900;

      await extensionStorageSync.set(extension, {"remote-key": 8}, context);

      let calls = [];
      await extensionStorageSync.addOnChangedListener(extension, function() {
        calls.push(arguments);
      }, context);

      await extensionStorageSync.syncAll();
      const remoteValue = (await extensionStorageSync.get(extension, "remote-key", context))["remote-key"];
      equal(remoteValue, 8,
            "locally set value overrides remote value");

      equal(calls.length, 1,
            "conflicts manifest in on-changed listener");
      deepEqual(calls[0][0], {"remote-key": {newValue: 8}});
      calls = [];

      // Syncing again doesn't do anything
      await extensionStorageSync.syncAll();

      equal(calls.length, 0,
            "syncing again shouldn't call on-changed listener");

      // Updating the server causes us to pull down the new value
      server.etag = 1000;
      await server.encryptAndAddRecord(transformer, {
        collectionId,
        data: {
          "id": "key-remote_2D_key",
          "key": "remote-key",
          "data": 7,
        },
        predicate: appearsAt(950),
      });

      await extensionStorageSync.syncAll();
      const remoteValue2 = (await extensionStorageSync.get(extension, "remote-key", context))["remote-key"];
      equal(remoteValue2, 7,
            "conflicts do not prevent retrieval of new values");

      equal(calls.length, 1,
            "syncing calls on-changed listener on update");
      deepEqual(calls[0][0], {"remote-key": {oldValue: 8, newValue: 7}});
    });
  });
});

add_task(async function test_storage_sync_pulls_deletes() {
  const extension = defaultExtension;
  await withContextAndServer(async function(context, server) {
    await withSignedInUser(loggedInUser, async function(extensionStorageSync, fxaService) {
      const cryptoCollection = new CryptoCollection(fxaService);
      const collectionId = await cryptoCollection.extensionIdToCollectionId(defaultExtensionId);
      server.installCollection(collectionId);
      server.installCollection("storage-sync-crypto");

      await extensionStorageSync.set(extension, {"my-key": 5}, context);
      await extensionStorageSync.syncAll();
      server.clearPosts();

      let calls = [];
      await extensionStorageSync.addOnChangedListener(extension, function() {
        calls.push(arguments);
      }, context);

      const transformer = new CollectionKeyEncryptionRemoteTransformer(new CryptoCollection(fxaService), extension.id);
      await server.encryptAndAddRecord(transformer, {
        collectionId,
        data: {
          "id": "key-my_2D_key",
          "data": 6,
          "_status": "deleted",
        },
      });

      await extensionStorageSync.syncAll();
      const remoteValues = (await extensionStorageSync.get(extension, "my-key", context));
      ok(!remoteValues["my-key"],
         "ExtensionStorageSync.get() shows value was deleted by sync");

      equal(server.getPosts().length, 0,
            "pulling the delete shouldn't cause posts");

      equal(calls.length, 1,
            "syncing calls on-changed listener");
      deepEqual(calls[0][0], {"my-key": {oldValue: 5}});
      calls = [];

      // Syncing again doesn't do anything
      await extensionStorageSync.syncAll();

      equal(calls.length, 0,
            "syncing again shouldn't call on-changed listener");
    });
  });
});

add_task(async function test_storage_sync_pushes_deletes() {
  const extensionId = uuid();
  const extension = {id: extensionId};
  await withContextAndServer(async function(context, server) {
    await withSignedInUser(loggedInUser, async function(extensionStorageSync, fxaService) {
      const cryptoCollection = new CryptoCollection(fxaService);
      await cryptoCollection._clear();
      await cryptoCollection._setSalt(extensionId, cryptoCollection.getNewSalt());
      const collectionId = await cryptoCollection.extensionIdToCollectionId(extensionId);

      server.installCollection(collectionId);
      server.installCollection("storage-sync-crypto");
      server.etag = 1000;

      await extensionStorageSync.set(extension, {"my-key": 5}, context);

      let calls = [];
      extensionStorageSync.addOnChangedListener(extension, function() {
        calls.push(arguments);
      }, context);

      await extensionStorageSync.syncAll();
      let posts = server.getPosts();
      equal(posts.length, 2,
            "pushing a non-deleted value should post keys and post the value to the server");

      await extensionStorageSync.remove(extension, ["my-key"], context);
      equal(calls.length, 1,
            "deleting a value should call the on-changed listener");

      await extensionStorageSync.syncAll();
      equal(calls.length, 1,
            "pushing a deleted value shouldn't call the on-changed listener");

      // Doesn't push keys because keys were pushed by a previous test.
      const hashedId = "id-" + (await cryptoCollection.hashWithExtensionSalt("key-my_2D_key", extensionId));
      posts = server.getPosts();
      equal(posts.length, 3,
            "deleting a value should trigger another push");
      const post = posts[2];
      assertPostedUpdatedRecord(post, 1000);
      equal(post.path, `${collectionRecordsPath(collectionId)}/${hashedId}`,
            "pushing a deleted value should go to the same path");
      ok(post.method, "PUT");
      ok(post.body.data.ciphertext,
         "deleting a value should have an encrypted body");
      const decoded = await new CollectionKeyEncryptionRemoteTransformer(cryptoCollection, extensionId).decode(post.body.data);
      equal(decoded._status, "deleted");
      // Ideally, we'd check that decoded.deleted is not true, because
      // the encrypted record shouldn't have it, but the decoder will
      // add it when it sees _status == deleted
    });
  });
});
