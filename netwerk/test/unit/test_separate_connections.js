Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
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
  chan.loadInfo.originAttributes = { userContextId: userContextId };
  return chan;
}

let previousHashKeys = [];

function Listener(userContextId) {
  this.userContextId = userContextId;
}

let gTestsRun = 0;
Listener.prototype = {
  onStartRequest: function(request, context) {
    request.QueryInterface(Ci.nsIHttpChannel)
           .QueryInterface(Ci.nsIHttpChannelInternal);

    do_check_eq(request.loadInfo.originAttributes.userContextId, this.userContextId);

    let hashKey = request.connectionInfoHashKey;
    if (gSecondRoundStarted) {
      // Compare the hash keys with the previous set ones.
      // Hash keys should match if and only if their userContextId are the same.
      for (let userContextId = 0; userContextId < 3; userContextId++) {
        if (userContextId == this.userContextId) {
          do_check_eq(hashKey, previousHashKeys[userContextId]);
        } else {
          do_check_neq(hashKey, previousHashKeys[userContextId]);
        }
      }
    } else {
      // Set the hash keys in the first round.
      previousHashKeys[this.userContextId] = hashKey;
    }
  },
  onDataAvailable: function(request, ctx, stream, off, cnt) {
    read_stream(stream, cnt);
  },
  onStopRequest: function() {
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
    chan.asyncOpen2(listener);
  }
}

function run_test() {
  do_test_pending();
  httpserv = new HttpServer();
  httpserv.registerPathHandler("/", handler);
  httpserv.start(-1);

  doTest();
}

