/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

registerCleanupFunction(async () => {
  http3_clear_prefs();
});

add_task(async function setup() {
  await http3_setup_tests("h3-27");
});

let Http3Listener = function() {};

Http3Listener.prototype = {
  expectedStatus: Cr.NS_OK,
  amount: 0,

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

  onStopRequest: function testOnStopRequest(request, status) {
    let httpVersion = "";
    try {
      httpVersion = request.protocolVersion;
    } catch (e) {}
    Assert.equal(httpVersion, "h3-27");

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
  let listener = new Http3Listener();
  listener.amount = amount;
  let chan = makeChan("https://foo.example.com/post", amount);
  await chanPromise(chan, listener);
});

// Send a large post that cannot fit into a neqo stream buffer at once.
// StreamWritable events will trigger sending more data when the buffer
// space is freed
add_task(async function test_large_post2() {
  let amount = 1 << 23;
  let listener = new Http3Listener();
  listener.amount = amount;
  let chan = makeChan("https://foo.example.com/post", amount);
  await chanPromise(chan, listener);
});
