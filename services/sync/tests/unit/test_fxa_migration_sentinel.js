/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the reading and writing of the sync migration sentinel.
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");

Cu.import("resource://testing-common/services/sync/utils.js");
Cu.import("resource://testing-common/services/common/logging.js");

Cu.import("resource://services-sync/record.js");

// Set our username pref early so sync initializes with the legacy provider.
Services.prefs.setCharPref("services.sync.username", "foo");

// Now import sync
Cu.import("resource://services-sync/service.js");

const USER = "foo";
const PASSPHRASE = "abcdeabcdeabcdeabcdeabcdea";

function promiseStopServer(server) {
  return new Promise((resolve, reject) => {
    server.stop(resolve);
  });
}

let numServerRequests = 0;

// Helpers
function configureLegacySync() {
  let contents = {
    meta: {global: {}},
    crypto: {},
  };

  setBasicCredentials(USER, "password", PASSPHRASE);

  numServerRequests = 0;
  let server = new SyncServer({
    onRequest: () => {
      ++numServerRequests
    }
  });
  server.registerUser(USER, "password");
  server.createContents(USER, contents);
  server.start();

  Service.serverURL = server.baseURI;
  Service.clusterURL = server.baseURI;
  Service.identity.username = USER;
  Service._updateCachedURLs();

  return server;
}

// Test a simple round-trip of the get/set functions.
add_task(function *() {
  // Arrange for a legacy sync user.
  let server = configureLegacySync();

  Assert.equal((yield Service.getFxAMigrationSentinel()), null, "no sentinel to start");

  let sentinel = {foo: "bar"};
  yield Service.setFxAMigrationSentinel(sentinel);

  Assert.deepEqual((yield Service.getFxAMigrationSentinel()), sentinel, "got the sentinel back");

  yield promiseStopServer(server);
});

// Test the records are cached by the record manager.
add_task(function *() {
  // Arrange for a legacy sync user.
  let server = configureLegacySync();
  Service.login();

  // Reset the request count here as the login would have made some.
  numServerRequests = 0;

  Assert.equal((yield Service.getFxAMigrationSentinel()), null, "no sentinel to start");
  Assert.equal(numServerRequests, 1, "first fetch should hit the server");

  let sentinel = {foo: "bar"};
  yield Service.setFxAMigrationSentinel(sentinel);
  Assert.equal(numServerRequests, 2, "setting sentinel should hit the server");

  Assert.deepEqual((yield Service.getFxAMigrationSentinel()), sentinel, "got the sentinel back");
  Assert.equal(numServerRequests, 2, "second fetch should not should hit the server");

  // Clobber the caches and ensure we still get the correct value back when we
  // do hit the server.
  Service.recordManager.clearCache();
  Assert.deepEqual((yield Service.getFxAMigrationSentinel()), sentinel, "got the sentinel back");
  Assert.equal(numServerRequests, 3, "should have re-hit the server with empty caches");

  yield promiseStopServer(server);
});

// Test the records are cached by a sync.
add_task(function* () {
  let server = configureLegacySync();

  // A first sync clobbers meta/global due to it being empty, so we first
  // do a sync which forces a good set of data on the server.
  Service.sync();

  // Now create a sentinel exists on the server.  It's encrypted, so we need to
  // put an encrypted version.
  let cryptoWrapper = new CryptoWrapper("meta", "fxa_credentials");
  let sentinel = {foo: "bar"};
  cryptoWrapper.cleartext = {
    id: "fxa_credentials",
    sentinel: sentinel,
    deleted: false,
  }
  cryptoWrapper.encrypt(Service.identity.syncKeyBundle);
  let payload = {
    ciphertext: cryptoWrapper.ciphertext,
    IV:         cryptoWrapper.IV,
    hmac:       cryptoWrapper.hmac,
  };

  server.createContents(USER, {
    meta: {fxa_credentials: payload},
    crypto: {},
  });

  // Another sync - this will cause the encrypted record to be fetched.
  Service.sync();
  // Reset the request count here as the sync will have made many!
  numServerRequests = 0;

  // Asking for the sentinel should use the copy cached in the record manager.
  Assert.deepEqual((yield Service.getFxAMigrationSentinel()), sentinel, "got it");
  Assert.equal(numServerRequests, 0, "should not have hit the server");

  // And asking for it again should work (we have to work around the fact the
  // ciphertext is clobbered on first decrypt...)
  Assert.deepEqual((yield Service.getFxAMigrationSentinel()), sentinel, "got it again");
  Assert.equal(numServerRequests, 0, "should not have hit the server");

  yield promiseStopServer(server);
});

function run_test() {
  initTestLogging();
  run_next_test();
}
