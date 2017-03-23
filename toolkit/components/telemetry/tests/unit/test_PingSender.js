/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

// This tests submitting a ping using the stand-alone pingsender program.

"use strict";

Cu.import("resource://gre/modules/TelemetryUtils.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);

add_task(function* test_pingSender() {
  // Make sure the code can find the pingsender executable
  const XRE_APP_DISTRIBUTION_DIR = "XREAppDist";
  let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  dir.initWithPath(OS.Constants.Path.libDir);

  Services.dirsvc.registerProvider({
    getFile(aProp, aPersistent) {
      aPersistent.value = true;
      if (aProp == XRE_APP_DISTRIBUTION_DIR) {
        return dir.clone();
      }
      return null;
    }
  });

  PingServer.start();

  const url = "http://localhost:" + PingServer.port + "/submit/telemetry/";
  const data = {
    type: "test-pingsender-type",
    id: TelemetryUtils.generateUUID(),
    creationDate: (new Date(1485810000)).toISOString(),
    version: 4,
    payload: {
      dummy: "stuff"
    }
  };

  Telemetry.runPingSender(url, JSON.stringify(data));

  let req = yield PingServer.promiseNextRequest();
  let ping = decodeRequestPayload(req);

  Assert.equal(req.getHeader("User-Agent"), "pingsender/1.0",
               "Should have received the correct user agent string.");
  Assert.equal(req.getHeader("X-PingSender-Version"), "1.0",
               "Should have received the correct PingSender version string.");
  Assert.equal(req.getHeader("Content-Encoding"), "gzip",
               "Should have a gzip encoded ping.");
  Assert.ok(req.getHeader("Date"), "Should have received a Date header.");
  Assert.equal(ping.id, data.id, "Should have received the correct ping id.");
  Assert.equal(ping.type, data.type,
               "Should have received the correct ping type.");
  Assert.deepEqual(ping.payload, data.payload,
                   "Should have received the correct payload.");
});

add_task(function* cleanup() {
  yield PingServer.stop();
});
