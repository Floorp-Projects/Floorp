"use strict";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort;
});

var httpServer = null;

function make_uri(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newURI(url, null, null);
}

// ensure the cache service is prepped when running the test
Cc["@mozilla.org/netwerk/cache-storage-service;1"].getService(Ci.nsICacheStorageService);

var gotOnProgress;
var gotOnStatus;

function make_channel(url, body, cb) {
  gotOnProgress = false;
  gotOnStatus = false;
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var chan = ios.newChannel(url, null, null).QueryInterface(Ci.nsIHttpChannel);
  chan.notificationCallbacks = {
    numChecks: 0,
    QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkInterceptController,
                                           Ci.nsIInterfaceRequestor,
                                           Ci.nsIProgressEventSink]),
    getInterface: function(iid) {
      return this.QueryInterface(iid);
    },
    onProgress: function(request, context, progress, progressMax) {
      gotOnProgress = true;
    },
    onStatus: function(request, context, status, statusArg) {
      gotOnStatus = true;
    },
    shouldPrepareForIntercept: function() {
      do_check_eq(this.numChecks, 0);
      this.numChecks++;
      return true;
    },
    channelIntercepted: function(channel, stream) {
      channel.QueryInterface(Ci.nsIInterceptedChannel);
      if (body) {
        var synthesized = Cc["@mozilla.org/io/string-input-stream;1"]
                            .createInstance(Ci.nsIStringInputStream);
        synthesized.data = body;

        NetUtil.asyncCopy(synthesized, stream, function() {
          channel.finishSynthesizedResponse();
        });
      }
      if (cb) {
        cb(channel, stream);
      }
    },
  };
  return chan;
}

const REMOTE_BODY = "http handler body";
const NON_REMOTE_BODY = "synthesized body";
const NON_REMOTE_BODY_2 = "synthesized body #2";

function bodyHandler(metadata, response) {
  response.setHeader('Content-Type', 'text/plain');
  response.write(REMOTE_BODY);
}

function run_test() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler('/body', bodyHandler);
  httpServer.start(-1);

  run_next_test();
}

function handle_synthesized_response(request, buffer) {
  do_check_eq(buffer, NON_REMOTE_BODY);
  do_check_true(gotOnStatus);
  do_check_true(gotOnProgress);
  run_next_test();
}

function handle_synthesized_response_2(request, buffer) {
  do_check_eq(buffer, NON_REMOTE_BODY_2);
  do_check_true(gotOnStatus);
  do_check_true(gotOnProgress);
  run_next_test();
}

function handle_remote_response(request, buffer) {
  do_check_eq(buffer, REMOTE_BODY);
  do_check_true(gotOnStatus);
  do_check_true(gotOnProgress);
  run_next_test();
}

// hit the network instead of synthesizing
add_test(function() {
  var chan = make_channel(URL + '/body', null, function(chan) {
    chan.resetInterception();
  });
  chan.asyncOpen(new ChannelListener(handle_remote_response, null), null);
});

// synthesize a response
add_test(function() {
  var chan = make_channel(URL + '/body', NON_REMOTE_BODY);
  chan.asyncOpen(new ChannelListener(handle_synthesized_response, null, CL_ALLOW_UNKNOWN_CL), null);
});

// hit the network instead of synthesizing, to test that no previous synthesized
// cache entry is used.
add_test(function() {
  var chan = make_channel(URL + '/body', null, function(chan) {
    chan.resetInterception();
  });
  chan.asyncOpen(new ChannelListener(handle_remote_response, null), null);
});

// synthesize a different response to ensure no previous response is cached
add_test(function() {
  var chan = make_channel(URL + '/body', NON_REMOTE_BODY_2);
  chan.asyncOpen(new ChannelListener(handle_synthesized_response_2, null, CL_ALLOW_UNKNOWN_CL), null);
});

// ensure that the channel waits for a decision and synthesizes headers correctly
add_test(function() {
  var chan = make_channel(URL + '/body', null, function(channel, stream) {
    do_timeout(100, function() {
      var synthesized = Cc["@mozilla.org/io/string-input-stream;1"]
                          .createInstance(Ci.nsIStringInputStream);
      synthesized.data = NON_REMOTE_BODY;
      NetUtil.asyncCopy(synthesized, stream, function() {
        channel.synthesizeHeader("Content-Length", NON_REMOTE_BODY.length);
        channel.finishSynthesizedResponse();
      });
    });
  });
  chan.asyncOpen(new ChannelListener(handle_synthesized_response, null), null);
});

// ensure that the channel waits for a decision
add_test(function() {
  var chan = make_channel(URL + '/body', null, function(chan) {
    do_timeout(100, function() {
      chan.resetInterception();
    });
  });
  chan.asyncOpen(new ChannelListener(handle_remote_response, null), null);
});

// ensure that the intercepted channel supports suspend/resume
add_test(function() {
  var chan = make_channel(URL + '/body', null, function(intercepted, stream) {
    var synthesized = Cc["@mozilla.org/io/string-input-stream;1"]
                        .createInstance(Ci.nsIStringInputStream);
    synthesized.data = NON_REMOTE_BODY;

    NetUtil.asyncCopy(synthesized, stream, function() {
      // set the content-type to ensure that the stream converter doesn't hold up notifications
      // and cause the test to fail
      intercepted.synthesizeHeader("Content-Type", "text/plain");
      intercepted.finishSynthesizedResponse();
    });
  });
  chan.asyncOpen(new ChannelListener(handle_synthesized_response, null,
				     CL_ALLOW_UNKNOWN_CL | CL_SUSPEND | CL_EXPECT_3S_DELAY), null);
});

add_test(function() {
  httpServer.stop(run_next_test);
});
