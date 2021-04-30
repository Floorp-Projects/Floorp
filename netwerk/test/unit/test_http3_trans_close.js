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
  expectedAmount: 0,
  expectedStatus: Cr.NS_OK,
  amount: 0,

  onStartRequest: function testOnStartRequest(request) {
    Assert.equal(request.status, this.expectedStatus);
    if (Components.isSuccessCode(this.expectedStatus)) {
      Assert.equal(request.responseStatus, 200);
    }
  },

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    this.amount += cnt;
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, status) {
    let httpVersion = "";
    try {
      httpVersion = request.protocolVersion;
    } catch (e) {}
    Assert.equal(httpVersion, "h3-27");
    Assert.equal(this.amount, this.expectedAmount);

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

function makeChan(uri) {
  let chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

add_task(async function test_response_without_body() {
  let chan = makeChan("https://foo.example.com/no_body");
  let listener = new Http3Listener();
  listener.expectedAmount = 0;
  await chanPromise(chan, listener);
});

add_task(async function test_response_without_content_length() {
  let chan = makeChan("https://foo.example.com/no_content_length");
  let listener = new Http3Listener();
  listener.expectedAmount = 4000;
  await chanPromise(chan, listener);
});

add_task(async function test_content_length_smaller_than_data_len() {
  let chan = makeChan("https://foo.example.com/content_length_smaller");
  let listener = new Http3Listener();
  // content-lentgth is 4000, but data length is 8000.
  // We should return an error here - bug 1670086.
  listener.expectedAmount = 4000;
  await chanPromise(chan, listener);
});
