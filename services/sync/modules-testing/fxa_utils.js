"use strict";

var EXPORTED_SYMBOLS = [
  "initializeIdentityWithTokenServerResponse",
];

ChromeUtils.import("resource://gre/modules/Log.jsm");
ChromeUtils.import("resource://services-sync/main.js");
ChromeUtils.import("resource://services-sync/browserid_identity.js");
ChromeUtils.import("resource://services-common/tokenserverclient.js");
ChromeUtils.import("resource://testing-common/services/common/logging.js");
ChromeUtils.import("resource://testing-common/services/sync/utils.js");

// Create a new browserid_identity object and initialize it with a
// mocked TokenServerClient which always receives the specified response.
var initializeIdentityWithTokenServerResponse = function(response) {
  // First create a mock "request" object that well' hack into the token server.
  // A log for it
  let requestLog = Log.repository.getLogger("testing.mock-rest");
  if (!requestLog.appenders.length) { // might as well see what it says :)
    requestLog.addAppender(new Log.DumpAppender());
    requestLog.level = Log.Level.Trace;
  }

  // A mock request object.
  function MockRESTRequest(url) {}
  MockRESTRequest.prototype = {
    _log: requestLog,
    setHeader() {},
    async get() {
      this.response = response;
      return response;
    },
  };
  // The mocked TokenServer client which will get the response.
  function MockTSC() { }
  MockTSC.prototype = new TokenServerClient();
  MockTSC.prototype.constructor = MockTSC;
  MockTSC.prototype.newRESTRequest = function(url) {
    return new MockRESTRequest(url);
  };
  // Arrange for the same observerPrefix as browserid_identity uses.
  MockTSC.prototype.observerPrefix = "weave:service";

  // tie it all together.
  Weave.Status.__authManager = Weave.Service.identity = new BrowserIDManager();
  let browseridManager = Weave.Service.identity;
  // a sanity check
  if (!(browseridManager instanceof BrowserIDManager)) {
    throw new Error("sync isn't configured for browserid_identity");
  }
  let mockTSC = new MockTSC();
  configureFxAccountIdentity(browseridManager);
  browseridManager._tokenServerClient = mockTSC;
};
