"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

ChromeUtils.defineLazyGetter(this, "URL", function () {
  return "http://localhost:" + httpserv.identity.primaryPort;
});

// This unit test ensures each container has its own connection pool.
// We verify this behavior by opening channels with different userContextId,
// and their connection info's hash keys should be different.

// In the first round of this test, we record the hash key in each container.
// In the second round, we check if each container's hash key is consistent
// and different from other container's hash key.

let httpserv = null;
let gSecondRoundStarted = false;

function handler(metadata, response) {
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  let body = "0123456789";
  response.bodyOutputStream.write(body, body.length);
}

function makeChan(url, userContextId) {
  let chan = NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
  chan.loadInfo.originAttributes = { userContextId };
  return chan;
}

let previousHashKeys = [];

function Listener(userContextId) {
  this.userContextId = userContextId;
}

let gTestsRun = 0;
Listener.prototype = {
  onStartRequest(request) {
    request
      .QueryInterface(Ci.nsIHttpChannel)
      .QueryInterface(Ci.nsIHttpChannelInternal);

    Assert.equal(
      request.loadInfo.originAttributes.userContextId,
      this.userContextId
    );

    let hashKey = request.connectionInfoHashKey;
    if (gSecondRoundStarted) {
      // Compare the hash keys with the previous set ones.
      // Hash keys should match if and only if their userContextId are the same.
      for (let userContextId = 0; userContextId < 3; userContextId++) {
        if (userContextId == this.userContextId) {
          Assert.equal(hashKey, previousHashKeys[userContextId]);
        } else {
          Assert.notEqual(hashKey, previousHashKeys[userContextId]);
        }
      }
    } else {
      // Set the hash keys in the first round.
      previousHashKeys[this.userContextId] = hashKey;
    }
  },
  onDataAvailable(request, stream, off, cnt) {
    read_stream(stream, cnt);
  },
  onStopRequest() {
    gTestsRun++;
    if (gTestsRun == 3) {
      gTestsRun = 0;
      if (gSecondRoundStarted) {
        // The second round finishes.
        httpserv.stop(do_test_finished);
      } else {
        // The first round finishes. Do the second round.
        gSecondRoundStarted = true;
        doTest();
      }
    }
  },
};

function doTest() {
  for (let userContextId = 0; userContextId < 3; userContextId++) {
    let chan = makeChan(URL, userContextId);
    let listener = new Listener(userContextId);
    chan.asyncOpen(listener);
  }
}

function run_test() {
  do_test_pending();
  httpserv = new HttpServer();
  httpserv.registerPathHandler("/", handler);
  httpserv.start(-1);

  doTest();
}
