/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Log } from "resource://gre/modules/Log.sys.mjs";

import { Weave } from "resource://services-sync/main.sys.mjs";
import { SyncAuthManager } from "resource://services-sync/sync_auth.sys.mjs";

import { TokenServerClient } from "resource://services-common/tokenserverclient.sys.mjs";
import { configureFxAccountIdentity } from "resource://testing-common/services/sync/utils.sys.mjs";

// Create a new sync_auth object and initialize it with a
// mocked TokenServerClient which always receives the specified response.
export var initializeIdentityWithTokenServerResponse = function (response) {
  // First create a mock "request" object that well' hack into the token server.
  // A log for it
  let requestLog = Log.repository.getLogger("testing.mock-rest");
  if (!requestLog.appenders.length) {
    // might as well see what it says :)
    requestLog.addAppender(new Log.DumpAppender());
    requestLog.level = Log.Level.Trace;
  }

  // A mock request object.
  function MockRESTRequest() {}
  MockRESTRequest.prototype = {
    _log: requestLog,
    setHeader() {},
    async get() {
      this.response = response;
      return response;
    },
  };
  // The mocked TokenServer client which will get the response.
  function MockTSC() {}
  MockTSC.prototype = new TokenServerClient();
  MockTSC.prototype.constructor = MockTSC;
  MockTSC.prototype.newRESTRequest = function (url) {
    return new MockRESTRequest(url);
  };
  // Arrange for the same observerPrefix as sync_auth uses.
  MockTSC.prototype.observerPrefix = "weave:service";

  // tie it all together.
  Weave.Status.__authManager = Weave.Service.identity = new SyncAuthManager();
  let syncAuthManager = Weave.Service.identity;
  // a sanity check
  if (!(syncAuthManager instanceof SyncAuthManager)) {
    throw new Error("sync isn't configured to use sync_auth");
  }
  let mockTSC = new MockTSC();
  configureFxAccountIdentity(syncAuthManager);
  syncAuthManager._tokenServerClient = mockTSC;
};
