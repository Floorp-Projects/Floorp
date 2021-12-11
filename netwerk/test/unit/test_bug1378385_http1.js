// test bug 1378385.
//
// Summary:
// Assume we have 6 http requests in queue, 3 are from the focused window with
// normal priority and the other 3 are from the non-focused window with the
// highest priority.
// We want to test that when "network.http.active_tab_priority" is false,
// the server should receive 3 requests with the highest priority first
// and then receive the rest 3 requests.
//
// Test step:
// 1. Create 6 dummy http requests. Server would not process responses until told
//    all 6 requests.
// 2. Once server receive 6 dummy requests, create 3 http requests with the focused
//    window id and normal priority and 3 requests with non-focused window id and
//    the highrst priority.
//    Note that the requets's id is set to its window id.
// 3. Server starts to process the 6 dummy http requests, so the client can start to
//    process the pending queue. Server will queue those http requests again and wait
//    until get all 6 requests.
// 4. When the server receive all 6 requests, we want to check if 3 requests with higher
//    priority are sent before others.
//    First, we check that if the request id of the first 3 requests in the queue is
//    equal to non focused window id.
//    Second, we check if the request id of the rest requests is equal to focused
//    window id.

"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var server = new HttpServer();
server.start(-1);
var baseURL = "http://localhost:" + server.identity.primaryPort + "/";
var maxConnections = 0;
var debug = false;
const FOCUSED_WINDOW_ID = 123;
var NON_FOCUSED_WINDOW_ID;
var FOCUSED_WINDOW_REQUEST_COUNT;
var NON_FOCUSED_WINDOW_REQUEST_COUNT;

function log(msg) {
  if (!debug) {
    return;
  }

  if (msg) {
    dump("TEST INFO | " + msg + "\n");
  }
}

function make_channel(url) {
  var request = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  });
  request.QueryInterface(Ci.nsIHttpChannel);
  return request;
}

function serverStopListener() {
  server.stop();
}

function createHttpRequest(windowId, requestId, priority) {
  let uri = baseURL;
  var chan = make_channel(uri);
  chan.topBrowsingContextId = windowId;
  chan.QueryInterface(Ci.nsISupportsPriority).priority = priority;
  var listner = new HttpResponseListener(requestId);
  chan.setRequestHeader("X-ID", requestId, false);
  chan.setRequestHeader("Cache-control", "no-store", false);
  chan.asyncOpen(listner);
  log("Create http request id=" + requestId);
}

function setup_dummyHttpRequests() {
  log("setup_dummyHttpRequests");
  for (var i = 0; i < maxConnections; i++) {
    createHttpRequest(0, i, Ci.nsISupportsPriority.PRIORITY_NORMAL);
    do_test_pending();
  }
}

function setup_focusedWindowHttpRequests() {
  log("setup_focusedWindowHttpRequests");
  for (var i = 0; i < FOCUSED_WINDOW_REQUEST_COUNT; i++) {
    createHttpRequest(
      FOCUSED_WINDOW_ID,
      FOCUSED_WINDOW_ID,
      Ci.nsISupportsPriority.PRIORITY_NORMAL
    );
    do_test_pending();
  }
}

function setup_nonFocusedWindowHttpRequests() {
  log("setup_nonFocusedWindowHttpRequests");
  for (var i = 0; i < NON_FOCUSED_WINDOW_REQUEST_COUNT; i++) {
    createHttpRequest(
      NON_FOCUSED_WINDOW_ID,
      NON_FOCUSED_WINDOW_ID,
      Ci.nsISupportsPriority.PRIORITY_HIGHEST
    );
    do_test_pending();
  }
}

function HttpResponseListener(id) {
  this.id = id;
}

HttpResponseListener.prototype = {
  onStartRequest(request) {},

  onDataAvailable(request, stream, off, cnt) {},

  onStopRequest(request, status) {
    log("STOP id=" + this.id);
    do_test_finished();
  },
};

function check_response_id(responses, windowId) {
  for (var i = 0; i < responses.length; i++) {
    var id = responses[i].getHeader("X-ID");
    log("response id=" + id + " windowId=" + windowId);
    Assert.equal(id, windowId);
  }
}

var responseQueue = [];
function setup_http_server() {
  log("setup_http_server");
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(
    Ci.nsIPrefBranch
  );
  maxConnections = prefs.getIntPref(
    "network.http.max-persistent-connections-per-server"
  );
  FOCUSED_WINDOW_REQUEST_COUNT = Math.floor(maxConnections * 0.5);
  NON_FOCUSED_WINDOW_REQUEST_COUNT =
    maxConnections - FOCUSED_WINDOW_REQUEST_COUNT;
  NON_FOCUSED_WINDOW_ID = FOCUSED_WINDOW_ID + FOCUSED_WINDOW_REQUEST_COUNT;

  var allDummyHttpRequestReceived = false;
  // Start server; will be stopped at test cleanup time.
  server.registerPathHandler("/", function(metadata, response) {
    var id = metadata.getHeader("X-ID");
    log("Server recived the response id=" + id);

    response.processAsync();
    response.setHeader("X-ID", id);
    responseQueue.push(response);

    if (
      responseQueue.length == maxConnections &&
      !allDummyHttpRequestReceived
    ) {
      log("received all dummy http requets");
      allDummyHttpRequestReceived = true;
      setup_nonFocusedWindowHttpRequests();
      setup_focusedWindowHttpRequests();
      processResponses();
    } else if (responseQueue.length == maxConnections) {
      var nonFocusedWindowResponses = responseQueue.slice(
        0,
        NON_FOCUSED_WINDOW_REQUEST_COUNT
      );
      var focusedWindowResponses = responseQueue.slice(
        NON_FOCUSED_WINDOW_REQUEST_COUNT,
        responseQueue.length
      );
      check_response_id(nonFocusedWindowResponses, NON_FOCUSED_WINDOW_ID);
      check_response_id(focusedWindowResponses, FOCUSED_WINDOW_ID);
      processResponses();
    }
  });

  registerCleanupFunction(function() {
    server.stop(serverStopListener);
  });
}

function processResponses() {
  while (responseQueue.length) {
    var resposne = responseQueue.pop();
    resposne.finish();
  }
}

function run_test() {
  // Set "network.http.active_tab_priority" to false, so we can expect to
  // receive http requests with higher priority first.
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(
    Ci.nsIPrefBranch
  );
  prefs.setBoolPref("network.http.active_tab_priority", false);

  setup_http_server();
  setup_dummyHttpRequests();
}
