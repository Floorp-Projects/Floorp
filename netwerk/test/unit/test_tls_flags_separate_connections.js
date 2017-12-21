Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserv.identity.primaryPort;
});

// This unit test ensures connections with different tlsFlags have their own
// connection pool. We verify this behavior by opening channels with different
// tlsFlags, and their connection info's hash keys should be different.

// In the first round of this test, we record the hash key for each connection.
// In the second round, we check if each connection's hash key is consistent
// and different from other connection's hash key.

let httpserv = null;
let gSecondRoundStarted = false;

let randomFlagValues = [
    0x00000000,

    0xFFFFFFFF,

    0x12345678,
    0x12345678,

    0x11111111,
    0x22222222,

    0xAAAAAAAA,
    0x77777777,

    0xBBBBBBBB,
    0xCCCCCCCC
];

function handler(metadata, response) {
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  let body = "0123456789";
  response.bodyOutputStream.write(body, body.length);
}

function makeChan(url, tlsFlags) {
  let chan = NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
  chan.QueryInterface(Ci.nsIHttpChannelInternal);
  chan.tlsFlags = tlsFlags;

  return chan;
}

let previousHashKeys = {};

function Listener(tlsFlags) {
  this.tlsFlags = tlsFlags;
}

let gTestsRun = 0;
Listener.prototype = {
  onStartRequest: function(request, context) {
    request.QueryInterface(Ci.nsIHttpChannel)
           .QueryInterface(Ci.nsIHttpChannelInternal);

    Assert.equal(request.tlsFlags, this.tlsFlags);

    let hashKey = request.connectionInfoHashKey;
    if (gSecondRoundStarted) {
      // Compare the hash keys with the previous set ones.
      // Hash keys should match if and only if their tlsFlags are the same.
      for (let tlsFlags of randomFlagValues) {
        if (tlsFlags == this.tlsFlags) {
          Assert.equal(hashKey, previousHashKeys[tlsFlags]);
        } else {
          Assert.notEqual(hashKey, previousHashKeys[tlsFlags]);
        }
      }
    } else {
      // Set the hash keys in the first round.
      previousHashKeys[this.tlsFlags] = hashKey;
    }
  },
  onDataAvailable: function(request, ctx, stream, off, cnt) {
    read_stream(stream, cnt);
  },
  onStopRequest: function() {
    gTestsRun++;
    if (gTestsRun == randomFlagValues.length) {
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
  for (let tlsFlags of randomFlagValues) {
    let chan = makeChan(URL, tlsFlags);
    let listener = new Listener(tlsFlags);
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

