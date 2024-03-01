/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

registerCleanupFunction(async () => {
  http3_clear_prefs();
});

add_task(async function setup() {
  await http3_setup_tests("h3-29");
});

let Http3Listener = function (amount) {
  this.amount = amount;
};

Http3Listener.prototype = {
  expectedStatus: Cr.NS_OK,
  amount: 0,
  onProgressMaxNotificationCount: 0,
  onProgressNotificationCount: 0,

  QueryInterface: ChromeUtils.generateQI(["nsIProgressEventSink"]),

  getInterface(iid) {
    if (iid.equals(Ci.nsIProgressEventSink)) {
      return this;
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  },

  onProgress(request, progress, progressMax) {
    // we will get notifications for the request and the response.
    if (progress === progressMax) {
      this.onProgressMaxNotificationCount += 1;
    }
    // For a large upload there should be a multiple notifications.
    this.onProgressNotificationCount += 1;
  },

  onStatus() {},

  onStartRequest: function testOnStartRequest(request) {
    Assert.equal(request.status, this.expectedStatus);
    if (Components.isSuccessCode(this.expectedStatus)) {
      Assert.equal(request.responseStatus, 200);
    }
    Assert.equal(
      this.amount,
      request.getResponseHeader("x-data-received-length")
    );
  },

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request) {
    let httpVersion = "";
    try {
      httpVersion = request.protocolVersion;
    } catch (e) {}
    Assert.equal(httpVersion, "h3-29");
    // We should get 2 correctOnProgress, i.e. one for request and one for the response.
    Assert.equal(this.onProgressMaxNotificationCount, 2);
    if (this.amount > 500000) {
      // 10 is an arbitrary number, but we cannot calculate exact number
      // because it depends on timing.
      Assert.ok(this.onProgressNotificationCount > 10);
    }
    this.finish();
  },
};

function chanPromise(chan, listener) {
  return new Promise(resolve => {
    function finish(result) {
      resolve(result);
    }
    listener.finish = finish;
    chan.asyncOpen(listener);
  });
}

function makeChan(uri, amount) {
  let chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;

  let stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  stream.data = generateContent(amount);
  let uchan = chan.QueryInterface(Ci.nsIUploadChannel);
  uchan.setUploadStream(stream, "text/plain", stream.available());
  chan.requestMethod = "POST";
  return chan;
}

// Generate a post.
function generateContent(size) {
  let content = "";
  for (let i = 0; i < size; i++) {
    content += "0";
  }
  return content;
}

// Send a large post that can fit into a neqo stream buffer at once.
// But the amount of data is larger than the necko's default stream
// buffer side, therefore ReadSegments will be called multiple times.
add_task(async function test_large_post() {
  let amount = 1 << 16;
  let listener = new Http3Listener(amount);
  let chan = makeChan("https://foo.example.com/post", amount);
  chan.notificationCallbacks = listener;
  await chanPromise(chan, listener);
});

// Send a large post that cannot fit into a neqo stream buffer at once.
// StreamWritable events will trigger sending more data when the buffer
// space is freed
add_task(async function test_large_post2() {
  let amount = 1 << 23;
  let listener = new Http3Listener(amount);
  let chan = makeChan("https://foo.example.com/post", amount);
  chan.notificationCallbacks = listener;
  await chanPromise(chan, listener);
});

// Send a post in the same way viaduct does in bug 1749957.
add_task(async function test_bug1749957_bug1750056() {
  let amount = 200; // Tests the bug as long as it's non-zero.
  let uri = "https://foo.example.com/post";
  let listener = new Http3Listener(amount);

  let chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);

  // https://searchfox.org/mozilla-central/rev/1920b17ac5988fcfec4e45e2a94478ebfbfc6f88/toolkit/components/viaduct/ViaductRequest.cpp#120-152
  {
    chan.requestMethod = "POST";
    chan.setRequestHeader("content-length", "" + amount, /* aMerge = */ false);

    let stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
      Ci.nsIStringInputStream
    );
    stream.data = generateContent(amount);
    let uchan = chan.QueryInterface(Ci.nsIUploadChannel2);
    uchan.explicitSetUploadStream(
      stream,
      /* aContentType = */ "",
      /* aContentLength = */ -1,
      "POST",
      /* aStreamHasHeaders = */ false
    );
  }

  chan.notificationCallbacks = listener;
  await chanPromise(chan, listener);
});
