// test bug 1312782.
//
// Summary:
// Assume we have 6 http requests in queue, 4 are from the focused window and
// the other 2 are from the non-focused window. We want to test that the server
// should receive 4 requests from the focused window first and then receive the
// rest 2 requests.
//
// Test step:
// 1. Create 6 dummy http requests. Server would not process responses until get
//    all 6 requests.
// 2. Once server receive 6 dummy requests, create 4 http requests with the focused
//    window id and 2 requests with non-focused window id. Note that the requets's
//    id is a serial number starting from the focused window id.
// 3. Server starts to process the 6 dummy http requests, so the client can start to
//    process the pending queue. Server will queue those http requests again and wait
//    until get all 6 requests.
// 4. When the server receive all 6 requests, starts to check that the request ids of
//    the first 4 requests in the queue should be all less than focused window id
//    plus 4. Also, the request ids of the rest requests should be less than non-focused
//    window id + 2.

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

var Cc = Components.classes;
var Ci = Components.interfaces;
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
  var request = NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
  request.QueryInterface(Ci.nsIHttpChannel);
  return request;
}

function serverStopListener() {
  server.stop();
}

function createHttpRequest(windowId, requestId) {
  let uri = baseURL;
  var chan = make_channel(uri);
  chan.topLevelOuterContentWindowId = windowId;
  var listner = new HttpResponseListener(requestId);
  chan.setRequestHeader("X-ID", requestId, false);
  chan.setRequestHeader("Cache-control", "no-store", false);
  chan.asyncOpen2(listner);
  log("Create http request id=" + requestId);
}

function setup_dummyHttpRequests() {
  log("setup_dummyHttpRequests");
  for (var i = 0; i < maxConnections ; i++) {
    createHttpRequest(0, i);
    do_test_pending();
  }
}

function setup_focusedWindowHttpRequests() {
  log("setup_focusedWindowHttpRequests");
  for (var i = 0; i < FOCUSED_WINDOW_REQUEST_COUNT ; i++) {
    createHttpRequest(FOCUSED_WINDOW_ID, FOCUSED_WINDOW_ID + i);
    do_test_pending();
  }
}

function setup_nonFocusedWindowHttpRequests() {
  log("setup_nonFocusedWindowHttpRequests");
  for (var i = 0; i < NON_FOCUSED_WINDOW_REQUEST_COUNT ; i++) {
    createHttpRequest(NON_FOCUSED_WINDOW_ID, NON_FOCUSED_WINDOW_ID + i);
    do_test_pending();
  }
}

function HttpResponseListener(id)
{
  this.id = id
};

HttpResponseListener.prototype =
{
  onStartRequest: function (request, ctx) {
  },

  onDataAvailable: function (request, ctx, stream, off, cnt) {
  },

  onStopRequest: function (request, ctx, status) {
    log("STOP id=" + this.id);
    do_test_finished();
  }
};

function check_response_id(responses, maxWindowId)
{
  for (var i = 0; i < responses.length; i++) {
    var id = responses[i].getHeader("X-ID");
    log("response id=" + id  + " maxWindowId=" + maxWindowId);
    Assert.ok(id < maxWindowId);
  }
}

var responseQueue = new Array();
function setup_http_server()
{
  log("setup_http_server");
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  maxConnections = prefs.getIntPref("network.http.max-persistent-connections-per-server");
  FOCUSED_WINDOW_REQUEST_COUNT = Math.floor(maxConnections * 0.8);
  NON_FOCUSED_WINDOW_REQUEST_COUNT = maxConnections - FOCUSED_WINDOW_REQUEST_COUNT;
  NON_FOCUSED_WINDOW_ID = FOCUSED_WINDOW_ID + FOCUSED_WINDOW_REQUEST_COUNT;

  var allDummyHttpRequestReceived = false;
  // Start server; will be stopped at test cleanup time.
  server.registerPathHandler('/', function(metadata, response)
  {
    var id = metadata.getHeader("X-ID");
    log("Server recived the response id=" + id);

    response.processAsync();
    response.setHeader("X-ID", id);
    responseQueue.push(response);

    if (responseQueue.length == maxConnections && !allDummyHttpRequestReceived) {
      log("received all dummy http requets");
      allDummyHttpRequestReceived = true;
      setup_nonFocusedWindowHttpRequests();
      setup_focusedWindowHttpRequests();
      processResponses();
    } else if (responseQueue.length == maxConnections) {
      var focusedWindowResponses = responseQueue.slice(0, FOCUSED_WINDOW_REQUEST_COUNT);
      var nonFocusedWindowResponses = responseQueue.slice(FOCUSED_WINDOW_REQUEST_COUNT, responseQueue.length);
      check_response_id(focusedWindowResponses, FOCUSED_WINDOW_ID + FOCUSED_WINDOW_REQUEST_COUNT);
      check_response_id(nonFocusedWindowResponses, NON_FOCUSED_WINDOW_ID + NON_FOCUSED_WINDOW_REQUEST_COUNT);
      processResponses();
    }

  });

  do_register_cleanup(function() {
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
  // Make sure "network.http.active_tab_priority" is true, so we can expect to
  // receive http requests with focused window id before others.
  var prefs = Cc["@mozilla.org/preferences-service;1"]
                .getService(Components.interfaces.nsIPrefBranch);
  prefs.setBoolPref("network.http.active_tab_priority", true);

  setup_http_server();
  setup_dummyHttpRequests();

  var windowIdWrapper = Cc["@mozilla.org/supports-PRUint64;1"]
                        .createInstance(Ci.nsISupportsPRUint64);
  windowIdWrapper.data = FOCUSED_WINDOW_ID;
  var obsvc = Cc["@mozilla.org/observer-service;1"].
    getService(Ci.nsIObserverService);
  obsvc.notifyObservers(windowIdWrapper, "net:current-toplevel-outer-content-windowid");
}
