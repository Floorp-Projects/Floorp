"use strict";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort;
});

var httpServer = null;

function make_uri(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newURI(url);
}

function isParentProcess() {
    let appInfo = Cc["@mozilla.org/xre/app-info;1"];
    return (!appInfo || appInfo.getService(Ci.nsIXULRuntime).processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT);
}

if (isParentProcess()) {
  // ensure the cache service is prepped when running the test
  // We only do this in the main process, as the cache storage service leaks
  // when instantiated in the content process.
  Cc["@mozilla.org/netwerk/cache-storage-service;1"].getService(Ci.nsICacheStorageService);
}

var gotOnProgress;
var gotOnStatus;

function make_channel(url, body, cb) {
  gotOnProgress = false;
  gotOnStatus = false;
  var chan = NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true})
                    .QueryInterface(Ci.nsIHttpChannel);
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
    channelIntercepted: function(channel) {
      channel.QueryInterface(Ci.nsIInterceptedChannel);
      if (body) {
        var synthesized = Cc["@mozilla.org/io/string-input-stream;1"]
                            .createInstance(Ci.nsIStringInputStream);
        synthesized.data = body;

        channel.startSynthesizedResponse(synthesized, null, '', false);
        channel.finishSynthesizedResponse();
      }
      if (cb) {
        cb(channel);
      }
      return {
        dispatch: function() { }
      };
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
  chan.asyncOpen2(new ChannelListener(handle_remote_response, null));
});

// synthesize a response
add_test(function() {
  var chan = make_channel(URL + '/body', NON_REMOTE_BODY);
  chan.asyncOpen2(new ChannelListener(handle_synthesized_response, null, CL_ALLOW_UNKNOWN_CL));
});

// hit the network instead of synthesizing, to test that no previous synthesized
// cache entry is used.
add_test(function() {
  var chan = make_channel(URL + '/body', null, function(chan) {
    chan.resetInterception();
  });
  chan.asyncOpen2(new ChannelListener(handle_remote_response, null));
});

// synthesize a different response to ensure no previous response is cached
add_test(function() {
  var chan = make_channel(URL + '/body', NON_REMOTE_BODY_2);
  chan.asyncOpen2(new ChannelListener(handle_synthesized_response_2, null, CL_ALLOW_UNKNOWN_CL));
});

// ensure that the channel waits for a decision and synthesizes headers correctly
add_test(function() {
  var chan = make_channel(URL + '/body', null, function(channel) {
    do_timeout(100, function() {
      var synthesized = Cc["@mozilla.org/io/string-input-stream;1"]
                          .createInstance(Ci.nsIStringInputStream);
      synthesized.data = NON_REMOTE_BODY;
      channel.synthesizeHeader("Content-Length", NON_REMOTE_BODY.length);
      channel.startSynthesizedResponse(synthesized, null, '', false);
      channel.finishSynthesizedResponse();
    });
  });
  chan.asyncOpen2(new ChannelListener(handle_synthesized_response, null));
});

// ensure that the channel waits for a decision
add_test(function() {
  var chan = make_channel(URL + '/body', null, function(chan) {
    do_timeout(100, function() {
      chan.resetInterception();
    });
  });
  chan.asyncOpen2(new ChannelListener(handle_remote_response, null));
});

// ensure that the intercepted channel supports suspend/resume
add_test(function() {
  var chan = make_channel(URL + '/body', null, function(intercepted) {
    var synthesized = Cc["@mozilla.org/io/string-input-stream;1"]
                        .createInstance(Ci.nsIStringInputStream);
    synthesized.data = NON_REMOTE_BODY;

    // set the content-type to ensure that the stream converter doesn't hold up notifications
    // and cause the test to fail
    intercepted.synthesizeHeader("Content-Type", "text/plain");
    intercepted.startSynthesizedResponse(synthesized, null, '', false);
    intercepted.finishSynthesizedResponse();
  });
  chan.asyncOpen2(new ChannelListener(handle_synthesized_response, null,
				     CL_ALLOW_UNKNOWN_CL | CL_SUSPEND | CL_EXPECT_3S_DELAY));
});

// ensure that the intercepted channel can be cancelled
add_test(function() {
  var chan = make_channel(URL + '/body', null, function(intercepted) {
    intercepted.cancelInterception(Cr.NS_BINDING_ABORTED);
  });
  chan.asyncOpen2(new ChannelListener(run_next_test, null, CL_EXPECT_FAILURE));
});

// ensure that the channel can't be cancelled via nsIInterceptedChannel after making a decision
add_test(function() {
  var chan = make_channel(URL + '/body', null, function(chan) {
    chan.resetInterception();
    do_timeout(0, function() {
      var gotexception = false;
      try {
        chan.cancelInterception();
      } catch (x) {
        gotexception = true;
      }
      do_check_true(gotexception);
    });
  });
  chan.asyncOpen2(new ChannelListener(handle_remote_response, null));
});

// ensure that the intercepted channel can be canceled during the response
add_test(function() {
  var chan = make_channel(URL + '/body', null, function(intercepted) {
    var synthesized = Cc["@mozilla.org/io/string-input-stream;1"]
                        .createInstance(Ci.nsIStringInputStream);
    synthesized.data = NON_REMOTE_BODY;

    let channel = intercepted.channel;
    intercepted.startSynthesizedResponse(synthesized, null, '', false);
    intercepted.finishSynthesizedResponse();
    channel.cancel(Cr.NS_BINDING_ABORTED);
  });
  chan.asyncOpen2(new ChannelListener(run_next_test, null,
                                     CL_EXPECT_FAILURE | CL_ALLOW_UNKNOWN_CL));
});

// ensure that the intercepted channel can be canceled before the response
add_test(function() {
  var chan = make_channel(URL + '/body', null, function(intercepted) {
    var synthesized = Cc["@mozilla.org/io/string-input-stream;1"]
                        .createInstance(Ci.nsIStringInputStream);
    synthesized.data = NON_REMOTE_BODY;

    intercepted.channel.cancel(Cr.NS_BINDING_ABORTED);

    // This should not throw, but result in the channel firing callbacks
    // with an error status.
    intercepted.startSynthesizedResponse(synthesized, null, '', false);
    intercepted.finishSynthesizedResponse();
  });
  chan.asyncOpen2(new ChannelListener(run_next_test, null,
                                     CL_EXPECT_FAILURE | CL_ALLOW_UNKNOWN_CL));
});

// Ensure that nsIInterceptedChannel.channelIntercepted() can return an error.
// In this case we should automatically ResetInterception() and complete the
// network request.
add_test(function() {
  var chan = make_channel(URL + '/body', null, function(chan) {
    throw('boom');
  });
  chan.asyncOpen2(new ChannelListener(handle_remote_response, null));
});

add_test(function() {
  httpServer.stop(run_next_test);
});
