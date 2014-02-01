/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://services-sync/browserid_identity.js");
Cu.import("resource://services-sync/rest.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://testing-common/services/sync/utils.js");
Cu.import("resource://services-common/hawk.js");
Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/FxAccountsClient.jsm");

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

let MockFxAccounts = function() {
  this._now_is = Date.now();

  let mockInternal = {
    now: () => {
      return this._now_is;
    },

    fxAccountsClient: new MockFxAccountsClient()
  };

  FxAccounts.apply(this, [mockInternal]);
};
MockFxAccounts.prototype = {
  __proto__: FxAccounts.prototype,
};

function run_test() {
  initTestLogging("Trace");
  Log.repository.getLogger("Sync.Identity").level = Log.Level.Trace;
  run_next_test();
};

add_test(function test_initial_state() {
    _("Verify initial state");
    do_check_false(!!browseridManager._token);
    do_check_false(browseridManager.hasValidToken());
    run_next_test();
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
    do_check_eq(browseridManager.account, identityConfig.fxaccount.user.email);
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
  fxa._now_is = now;
  fxa.internal.fxAccountsClient = fxaClient;

  // Picked up by the signed-in user module
  do_check_eq(fxa.internal.now(), now);
  do_check_eq(fxa.internal.localtimeOffsetMsec, localtimeOffsetMsec);

  do_check_eq(fxa.now(), now);
  do_check_eq(fxa.localtimeOffsetMsec, localtimeOffsetMsec);

  // Mocks within mocks...
  configureFxAccountIdentity(browseridManager, identityConfig);
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
  fxa._now_is = now;
  fxa.internal.fxAccountsClient = fxaClient;

  configureFxAccountIdentity(browseridManager, identityConfig);
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

add_test(function test_userChangeAndLogOut() {
    _("BrowserIDManager notices when the FxAccounts.getSignedInUser().email changes.");
    let bidUser = new BrowserIDManager();
    configureFxAccountIdentity(bidUser, identityConfig);
    let request = new SyncStorageRequest(
      "https://example.net/somewhere/over/the/rainbow");
    let authenticator = bidUser.getRESTRequestAuthenticator();
    do_check_true(!!authenticator);
    let output = authenticator(request, 'GET');
    do_check_true(!!output);
    do_check_eq(bidUser.account, identityConfig.fxaccount.user.email);
    do_check_true(bidUser.hasValidToken());
    identityConfig.fxaccount.user.email = "something@new";
    do_check_false(bidUser.hasValidToken());
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

// End of tests
// Utility functions follow

function getTimestamp(hawkAuthHeader) {
  return parseInt(/ts="(\d+)"/.exec(hawkAuthHeader)[1], 10) * SECOND_MS;
}

function getTimestampDelta(hawkAuthHeader, now=Date.now()) {
  return Math.abs(getTimestamp(hawkAuthHeader) - now);
}

