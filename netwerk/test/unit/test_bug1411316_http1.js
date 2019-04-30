// Test bug 1411316.
//
// Summary:
// The purpose of this test is to test whether the HttpConnectionMgr really
// cancel and close all connecitons when get "net:cancel-all-connections".
//
// Test step:
// 1. Create 6 http requests. Server would not process responses and just put
//    all requests in its queue.
// 2. Once server receive all 6 requests, call notifyObservers with the
//    topic "net:cancel-all-connections".
// 3. We expect that all 6 active connections should be closed with the status
//    NS_ERROR_ABORT.

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

var server = new HttpServer();
server.start(-1);
var baseURL = "http://localhost:" + server.identity.primaryPort + "/";
var maxConnections = 0;
var debug = false;
var requestId = 0;

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

function createHttpRequest(status) {
  let uri = baseURL;
  var chan = make_channel(uri);
  var listner = new HttpResponseListener(++requestId, status);
  chan.setRequestHeader("X-ID", requestId, false);
  chan.setRequestHeader("Cache-control", "no-store", false);
  chan.asyncOpen(listner);
  log("Create http request id=" + requestId);
}

function setupHttpRequests(status) {
  log("setupHttpRequests");
  for (var i = 0; i < maxConnections ; i++) {
    createHttpRequest(status);
    do_test_pending();
  }
}

function HttpResponseListener(id, onStopRequestStatus)
{
  this.id = id
  this.onStopRequestStatus = onStopRequestStatus;
};

HttpResponseListener.prototype =
{
  onStartRequest: function (request) {
  },

  onDataAvailable: function (request, stream, off, cnt) {
  },

  onStopRequest: function (request, status) {
    log("STOP id=" + this.id + " status=" + status);
    Assert.ok(this.onStopRequestStatus == status);
    do_test_finished();
  }
};

var responseQueue = new Array();
function setup_http_server()
{
  log("setup_http_server");
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  maxConnections = prefs.getIntPref("network.http.max-persistent-connections-per-server");

  var allDummyHttpRequestReceived = false;
  // Start server; will be stopped at test cleanup time.
  server.registerPathHandler('/', function(metadata, response)
  {
    var id = metadata.getHeader("X-ID");
    log("Server recived the response id=" + id);

    response.processAsync();
    response.setHeader("X-ID", id);
    responseQueue.push(response);

    if (responseQueue.length == maxConnections) {
      log("received all http requets");
      Services.obs.notifyObservers(null, "net:cancel-all-connections");
    }

  });

  registerCleanupFunction(function() {
    server.stop(serverStopListener);
  });

}

function run_test() {
  setup_http_server();
  setupHttpRequests(Cr.NS_ERROR_ABORT);
}
