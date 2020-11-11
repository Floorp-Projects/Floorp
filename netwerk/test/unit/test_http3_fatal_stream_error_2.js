/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Test the case when a connection error happens while many streams are
// open and some HttpTransaction queued.
// To achieve that by almost filling the limit with request which
// response is delayed. Fill the limit with a request that will cause a
// protocol error. After that add a couple of more requests.

// The stream concurrency limit is 16, therefore send 15 request to
// "/no_response".
let num_parallel = 15;
// Send some more request that will be queued (because of the stream
// concurrency limit) and restarted after the protocol error.
// This request will succeed because they are send over HTTP2.
let num_queued = 10;

let count_failed_partial = 0;
let count_failed_protocol_error = 0;
let count_success = 0;

registerCleanupFunction(async () => {
  http3_clear_prefs();
});

add_task(async function setup() {
  await http3_setup_tests(true);
});

let Http3FailedListener = function() {};

Http3FailedListener.prototype = {
  onStartRequest: function testOnStartRequest(request) {},

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, status) {
    if (status == Cr.NS_OK) {
      count_success++;
    } else if (status == Cr.NS_ERROR_NET_PARTIAL_TRANSFER) {
      count_failed_partial++;
    } else if (status == Cr.NS_ERROR_NET_HTTP3_PROTOCOL_ERROR) {
      count_failed_protocol_error++;
    } else {
      Assert.ok(false);
    }
    this.finish();
  },
};

function chanPromise(chan, listener) {
  return new Promise(resolve => {
    function finish() {
      resolve();
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

add_task(async function test_fatal_error() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );

  let h2Port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);

  let h3Port = env.get("MOZHTTP3_PORT_FAILED");

  let no_response_uri = "https://foo.example.com:" + h2Port + "/no_response";
  let wrong_response_uri =
    "https://foo.example.com:" + h2Port + "/wrong_frame_after_data";

  var promisesArray = [];
  for (var i = 0; i < num_parallel; i++) {
    var p = start_chan(no_response_uri);
    promisesArray.push(p);
  }
  // Add the failing request
  var p = start_chan(wrong_response_uri);
  promisesArray.push(p);
  // Add more request thatt will be queued
  for (var i = 0; i < num_queued; i++) {
    var p = start_chan(wrong_response_uri);
    promisesArray.push(p);
  }
  await Promise.all(promisesArray);
  Assert.equal(count_failed_partial, 1);
  Assert.equal(count_failed_protocol_error, num_parallel);
  Assert.equal(count_success, num_queued);
});

async function start_chan(uri) {
  let chan = makeChan(uri);
  let listener = new Http3FailedListener();
  return altsvcSetupPromise(chan, listener);
}
