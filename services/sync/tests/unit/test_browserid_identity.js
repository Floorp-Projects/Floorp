/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://services-sync/browserid_identity.js");
Cu.import("resource://services-sync/rest.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://testing-common/services/sync/utils.js");
Cu.import("resource://services-common/hawkclient.js");
Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/FxAccountsClient.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://services-common/tokenserverclient.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/constants.js");

const SECOND_MS = 1000;
const MINUTE_MS = SECOND_MS * 60;
const HOUR_MS = MINUTE_MS * 60;

let identityConfig = makeIdentityConfig();
let browseridManager = new BrowserIDManager();
configureFxAccountIdentity(browseridManager, identityConfig);

/**
 * Mock client clock and skew vs server in FxAccounts signed-in user module and
 * API client.  browserid_identity.js queries these values to construct HAWK
 * headers.  We will use this to test clock skew compensation in these headers
 * below.
 */
let MockFxAccountsClient = function() {
  FxAccountsClient.apply(this);
};
MockFxAccountsClient.prototype = {
  __proto__: FxAccountsClient.prototype
};

function MockFxAccounts() {
  let fxa = new FxAccounts({
    _now_is: Date.now(),

    now: function () {
      return this._now_is;
    },

    fxAccountsClient: new MockFxAccountsClient()
  });
  fxa.internal.currentAccountState.getCertificate = function(data, keyPair, mustBeValidUntil) {
    this.cert = {
      validUntil: fxa.internal.now() + CERT_LIFETIME,
      cert: "certificate",
    };
    return Promise.resolve(this.cert.cert);
  };
  return fxa;
}

function run_test() {
  initTestLogging("Trace");
  Log.repository.getLogger("Sync.Identity").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.BrowserIDManager").level = Log.Level.Trace;
  run_next_test();
};

add_test(function test_initial_state() {
    _("Verify initial state");
    do_check_false(!!browseridManager._token);
    do_check_false(browseridManager.hasValidToken());
    run_next_test();
  }
);

add_task(function test_initialializeWithCurrentIdentity() {
    _("Verify start after initializeWithCurrentIdentity");
    browseridManager.initializeWithCurrentIdentity();
    yield browseridManager.whenReadyToAuthenticate.promise;
    do_check_true(!!browseridManager._token);
    do_check_true(browseridManager.hasValidToken());
    do_check_eq(browseridManager.account, identityConfig.fxaccount.user.email);
  }
);


add_test(function test_getResourceAuthenticator() {
    _("BrowserIDManager supplies a Resource Authenticator callback which returns a Hawk header.");
    let authenticator = browseridManager.getResourceAuthenticator();
    do_check_true(!!authenticator);
    let req = {uri: CommonUtils.makeURI(
      "https://example.net/somewhere/over/the/rainbow"),
               method: 'GET'};
    let output = authenticator(req, 'GET');
    do_check_true('headers' in output);
    do_check_true('authorization' in output.headers);
    do_check_true(output.headers.authorization.startsWith('Hawk'));
    _("Expected internal state after successful call.");
    do_check_eq(browseridManager._token.uid, identityConfig.fxaccount.token.uid);
    run_next_test();
  }
);

add_test(function test_getRESTRequestAuthenticator() {
    _("BrowserIDManager supplies a REST Request Authenticator callback which sets a Hawk header on a request object.");
    let request = new SyncStorageRequest(
      "https://example.net/somewhere/over/the/rainbow");
    let authenticator = browseridManager.getRESTRequestAuthenticator();
    do_check_true(!!authenticator);
    let output = authenticator(request, 'GET');
    do_check_eq(request.uri, output.uri);
    do_check_true(output._headers.authorization.startsWith('Hawk'));
    do_check_true(output._headers.authorization.contains('nonce'));
    do_check_true(browseridManager.hasValidToken());
    run_next_test();
  }
);

add_test(function test_resourceAuthenticatorSkew() {
  _("BrowserIDManager Resource Authenticator compensates for clock skew in Hawk header.");

  // Clock is skewed 12 hours into the future
  // We pick a date in the past so we don't risk concealing bugs in code that
  // uses new Date() instead of our given date.
  let now = new Date("Fri Apr 09 2004 00:00:00 GMT-0700").valueOf() + 12 * HOUR_MS;
  let browseridManager = new BrowserIDManager();
  let hawkClient = new HawkClient("https://example.net/v1", "/foo");

  // mock fxa hawk client skew
  hawkClient.now = function() {
    dump("mocked client now: " + now + '\n');
    return now;
  }
  // Imagine there's already been one fxa request and the hawk client has
  // already detected skew vs the fxa auth server.
  let localtimeOffsetMsec = -1 * 12 * HOUR_MS;
  hawkClient._localtimeOffsetMsec = localtimeOffsetMsec;

  let fxaClient = new MockFxAccountsClient();
  fxaClient.hawk = hawkClient;

  // Sanity check
  do_check_eq(hawkClient.now(), now);
  do_check_eq(hawkClient.localtimeOffsetMsec, localtimeOffsetMsec);

  // Properly picked up by the client
  do_check_eq(fxaClient.now(), now);
  do_check_eq(fxaClient.localtimeOffsetMsec, localtimeOffsetMsec);

  let fxa = new MockFxAccounts();
  fxa.internal._now_is = now;
  fxa.internal.fxAccountsClient = fxaClient;

  // Picked up by the signed-in user module
  do_check_eq(fxa.internal.now(), now);
  do_check_eq(fxa.internal.localtimeOffsetMsec, localtimeOffsetMsec);

  do_check_eq(fxa.now(), now);
  do_check_eq(fxa.localtimeOffsetMsec, localtimeOffsetMsec);

  // Mocks within mocks...
  configureFxAccountIdentity(browseridManager, identityConfig);

  // Ensure the new FxAccounts mock has a signed-in user.
  fxa.internal.currentAccountState.signedInUser = browseridManager._fxaService.internal.currentAccountState.signedInUser;

  browseridManager._fxaService = fxa;

  do_check_eq(browseridManager._fxaService.internal.now(), now);
  do_check_eq(browseridManager._fxaService.internal.localtimeOffsetMsec,
      localtimeOffsetMsec);

  do_check_eq(browseridManager._fxaService.now(), now);
  do_check_eq(browseridManager._fxaService.localtimeOffsetMsec,
      localtimeOffsetMsec);

  let request = new SyncStorageRequest("https://example.net/i/like/pie/");
  let authenticator = browseridManager.getResourceAuthenticator();
  let output = authenticator(request, 'GET');
  dump("output" + JSON.stringify(output));
  let authHeader = output.headers.authorization;
  do_check_true(authHeader.startsWith('Hawk'));

  // Skew correction is applied in the header and we're within the two-minute
  // window.
  do_check_eq(getTimestamp(authHeader), now - 12 * HOUR_MS);
  do_check_true(
      (getTimestampDelta(authHeader, now) - 12 * HOUR_MS) < 2 * MINUTE_MS);

  run_next_test();
});

add_test(function test_RESTResourceAuthenticatorSkew() {
  _("BrowserIDManager REST Resource Authenticator compensates for clock skew in Hawk header.");

  // Clock is skewed 12 hours into the future from our arbitary date
  let now = new Date("Fri Apr 09 2004 00:00:00 GMT-0700").valueOf() + 12 * HOUR_MS;
  let browseridManager = new BrowserIDManager();
  let hawkClient = new HawkClient("https://example.net/v1", "/foo");

  // mock fxa hawk client skew
  hawkClient.now = function() {
    return now;
  }
  // Imagine there's already been one fxa request and the hawk client has
  // already detected skew vs the fxa auth server.
  hawkClient._localtimeOffsetMsec = -1 * 12 * HOUR_MS;

  let fxaClient = new MockFxAccountsClient();
  fxaClient.hawk = hawkClient;
  let fxa = new MockFxAccounts();
  fxa.internal._now_is = now;
  fxa.internal.fxAccountsClient = fxaClient;

  configureFxAccountIdentity(browseridManager, identityConfig);

  // Ensure the new FxAccounts mock has a signed-in user.
  fxa.internal.currentAccountState.signedInUser = browseridManager._fxaService.internal.currentAccountState.signedInUser;

  browseridManager._fxaService = fxa;

  do_check_eq(browseridManager._fxaService.internal.now(), now);

  let request = new SyncStorageRequest("https://example.net/i/like/pie/");
  let authenticator = browseridManager.getResourceAuthenticator();
  let output = authenticator(request, 'GET');
  dump("output" + JSON.stringify(output));
  let authHeader = output.headers.authorization;
  do_check_true(authHeader.startsWith('Hawk'));

  // Skew correction is applied in the header and we're within the two-minute
  // window.
  do_check_eq(getTimestamp(authHeader), now - 12 * HOUR_MS);
  do_check_true(
      (getTimestampDelta(authHeader, now) - 12 * HOUR_MS) < 2 * MINUTE_MS);

  run_next_test();
});

add_task(function test_ensureLoggedIn() {
  configureFxAccountIdentity(browseridManager);
  yield browseridManager.initializeWithCurrentIdentity();
  Assert.equal(Status.login, LOGIN_SUCCEEDED, "original initialize worked");
  yield browseridManager.ensureLoggedIn();
  Assert.equal(Status.login, LOGIN_SUCCEEDED, "original ensureLoggedIn worked");
  Assert.ok(browseridManager._shouldHaveSyncKeyBundle,
            "_shouldHaveSyncKeyBundle should always be true after ensureLogin completes.");

  // arrange for no logged in user.
  let fxa = browseridManager._fxaService
  let signedInUser = fxa.internal.currentAccountState.signedInUser;
  fxa.internal.currentAccountState.signedInUser = null;
  browseridManager.initializeWithCurrentIdentity();
  Assert.ok(!browseridManager._shouldHaveSyncKeyBundle,
            "_shouldHaveSyncKeyBundle should be false so we know we are testing what we think we are.");
  Status.login = LOGIN_FAILED_NO_USERNAME;
  try {
    yield browseridManager.ensureLoggedIn();
    Assert.ok(false, "promise should have been rejected.")
  } catch(_) {
  }
  Assert.ok(browseridManager._shouldHaveSyncKeyBundle,
            "_shouldHaveSyncKeyBundle should always be true after ensureLogin completes.");
  fxa.internal.currentAccountState.signedInUser = signedInUser;
  Status.login = LOGIN_FAILED_LOGIN_REJECTED;
  try {
    yield browseridManager.ensureLoggedIn();
    Assert.ok(false, "LOGIN_FAILED_LOGIN_REJECTED should have caused immediate rejection");
  } catch (_) {
  }
  Assert.equal(Status.login, LOGIN_FAILED_LOGIN_REJECTED,
               "status should remain LOGIN_FAILED_LOGIN_REJECTED");
  Status.login = LOGIN_FAILED_NETWORK_ERROR;
  yield browseridManager.ensureLoggedIn();
  Assert.equal(Status.login, LOGIN_SUCCEEDED, "final ensureLoggedIn worked");
});

add_test(function test_tokenExpiration() {
    _("BrowserIDManager notices token expiration:");
    let bimExp = new BrowserIDManager();
    configureFxAccountIdentity(bimExp, identityConfig);

    let authenticator = bimExp.getResourceAuthenticator();
    do_check_true(!!authenticator);
    let req = {uri: CommonUtils.makeURI(
      "https://example.net/somewhere/over/the/rainbow"),
               method: 'GET'};
    authenticator(req, 'GET');

    // Mock the clock.
    _("Forcing the token to expire ...");
    Object.defineProperty(bimExp, "_now", {
      value: function customNow() {
        return (Date.now() + 3000001);
      },
      writable: true,
    });
    do_check_true(bimExp._token.expiration < bimExp._now());
    _("... means BrowserIDManager knows to re-fetch it on the next call.");
    do_check_false(bimExp.hasValidToken());
    run_next_test();
  }
);

add_test(function test_sha256() {
  // Test vectors from http://www.bichlmeier.info/sha256test.html
  let vectors = [
    ["",
     "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"],
    ["abc",
     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"],
    ["message digest",
     "f7846f55cf23e14eebeab5b4e1550cad5b509e3348fbc4efa3a1413d393cb650"],
    ["secure hash algorithm",
     "f30ceb2bb2829e79e4ca9753d35a8ecc00262d164cc077080295381cbd643f0d"],
    ["SHA256 is considered to be safe",
     "6819d915c73f4d1e77e4e1b52d1fa0f9cf9beaead3939f15874bd988e2a23630"],
    ["abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
     "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"],
    ["For this sample, this 63-byte string will be used as input data",
     "f08a78cbbaee082b052ae0708f32fa1e50c5c421aa772ba5dbb406a2ea6be342"],
    ["This is exactly 64 bytes long, not counting the terminating byte",
     "ab64eff7e88e2e46165e29f2bce41826bd4c7b3552f6b382a9e7d3af47c245f8"]
  ];
  let bidUser = new BrowserIDManager();
  for (let [input,output] of vectors) {
    do_check_eq(CommonUtils.bytesAsHex(bidUser._sha256(input)), output);
  }
  run_next_test();
});

add_test(function test_computeXClientStateHeader() {
  let kBhex = "fd5c747806c07ce0b9d69dcfea144663e630b65ec4963596a22f24910d7dd15d";
  let kB = CommonUtils.hexToBytes(kBhex);

  let bidUser = new BrowserIDManager();
  let header = bidUser._computeXClientState(kB);

  do_check_eq(header, "6ae94683571c7a7c54dab4700aa3995f");
  run_next_test();
});

add_task(function test_getTokenErrors() {
  _("BrowserIDManager correctly handles various failures to get a token.");

  _("Arrange for a 401 - Sync should reflect an auth error.");
  yield initializeIdentityWithTokenServerFailure({
    status: 401,
    headers: {"content-type": "application/json"},
    body: JSON.stringify({}),
  });
  Assert.equal(Status.login, LOGIN_FAILED_LOGIN_REJECTED, "login was rejected");

  // XXX - other interesting responses to return?

  // And for good measure, some totally "unexpected" errors - we generally
  // assume these problems are going to magically go away at some point.
  _("Arrange for an empty body with a 200 response - should reflect a network error.");
  yield initializeIdentityWithTokenServerFailure({
    status: 200,
    headers: [],
    body: "",
  });
  Assert.equal(Status.login, LOGIN_FAILED_NETWORK_ERROR, "login state is LOGIN_FAILED_NETWORK_ERROR");
});

add_task(function test_getHAWKErrors() {
  _("BrowserIDManager correctly handles various HAWK failures.");

  _("Arrange for a 401 - Sync should reflect an auth error.");
  yield initializeIdentityWithHAWKFailure({
    status: 401,
    headers: {"content-type": "application/json"},
    body: JSON.stringify({}),
  });
  Assert.equal(Status.login, LOGIN_FAILED_LOGIN_REJECTED, "login was rejected");

  // XXX - other interesting responses to return?

  // And for good measure, some totally "unexpected" errors - we generally
  // assume these problems are going to magically go away at some point.
  _("Arrange for an empty body with a 200 response - should reflect a network error.");
  yield initializeIdentityWithHAWKFailure({
    status: 200,
    headers: [],
    body: "",
  });
  Assert.equal(Status.login, LOGIN_FAILED_NETWORK_ERROR, "login state is LOGIN_FAILED_NETWORK_ERROR");
});

// End of tests
// Utility functions follow

// Create a new browserid_identity object and initialize it with a
// mocked TokenServerClient which always gets the specified response.
function* initializeIdentityWithTokenServerFailure(response) {
  // First create a mock "request" object that well' hack into the token server.
  // A log for it
  let requestLog = Log.repository.getLogger("testing.mock-rest");
  if (!requestLog.appenders.length) { // might as well see what it says :)
    requestLog.addAppender(new Log.DumpAppender());
    requestLog.level = Log.Level.Trace;
  }

  // A mock request object.
  function MockRESTRequest(url) {};
  MockRESTRequest.prototype = {
    _log: requestLog,
    setHeader: function() {},
    get: function(callback) {
      this.response = response;
      callback.call(this);
    }
  }
  // The mocked TokenServer client which will get the response.
  function MockTSC() { }
  MockTSC.prototype = new TokenServerClient();
  MockTSC.prototype.constructor = MockTSC;
  MockTSC.prototype.newRESTRequest = function(url) {
    return new MockRESTRequest(url);
  }
  // tie it all together.
  let mockTSC = new MockTSC()
  configureFxAccountIdentity(browseridManager);
  browseridManager._tokenServerClient = mockTSC;

  yield browseridManager.initializeWithCurrentIdentity();
  try {
    yield browseridManager.whenReadyToAuthenticate.promise;
    Assert.ok(false, "expecting this promise to resolve with an error");
  } catch (ex) {}
}


// Create a new browserid_identity object and initialize it with a
// hawk mock that simulates a failure.
// A token server mock will be used that doesn't hit a server, so we move
// directly to a hawk request.
function* initializeIdentityWithHAWKFailure(response) {
  // A mock request object.
  function MockRESTRequest() {};
  MockRESTRequest.prototype = {
    setHeader: function() {},
    post: function(data, callback) {
      this.response = response;
      callback.call(this);
    }
  }

  // The hawk client.
  function MockedHawkClient() {}
  MockedHawkClient.prototype = new HawkClient();
  MockedHawkClient.prototype.constructor = MockedHawkClient;
  MockedHawkClient.prototype.newHAWKAuthenticatedRESTRequest = function(uri, credentials, extra) {
    return new MockRESTRequest();
  }

  // tie it all together - configureFxAccountIdentity isn't useful here :(
  let fxaClient = new MockFxAccountsClient();
  fxaClient.hawk = new MockedHawkClient();
  let config = makeIdentityConfig();
  let internal = {
    fxAccountsClient: fxaClient,
  }
  let fxa = new FxAccounts(internal);
  fxa.internal.currentAccountState.signedInUser = {
      accountData: config.fxaccount.user,
  };

  browseridManager._fxaService = fxa;
  browseridManager._signedInUser = null;
  yield browseridManager.initializeWithCurrentIdentity();
  try {
    yield browseridManager.whenReadyToAuthenticate.promise;
    Assert.ok(false, "expecting this promise to resolve with an error");
  } catch (ex) {}
}


function getTimestamp(hawkAuthHeader) {
  return parseInt(/ts="(\d+)"/.exec(hawkAuthHeader)[1], 10) * SECOND_MS;
}

function getTimestampDelta(hawkAuthHeader, now=Date.now()) {
  return Math.abs(getTimestamp(hawkAuthHeader) - now);
}

