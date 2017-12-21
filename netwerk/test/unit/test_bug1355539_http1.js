// test bug 1355539.
//
// Summary:
// Transactions in one pending queue are splited into two groups:
// [(Blocking Group)|(Non Blocking Group)]
// In each group, the transactions are ordered by its priority.
// This test will check if the transaction's order in pending queue is correct.
//
// Test step:
// 1. Create 6 dummy http requests. Server would not process responses until get
//    all 6 requests.
// 2. Once server receive 6 dummy requests, create another 6 http requests with the
//    defined priority and class flag in |transactionQueue|.
// 3. Server starts to process the 6 dummy http requests, so the client can start to
//    process the pending queue. Server will queue those http requests and put them in
//    |responseQueue|.
// 4. When the server receive all 6 requests, check if the order in |responseQueue| is
//    equal to |transactionQueue| by comparing the value of X-ID.

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

var Cc = Components.classes;
var Ci = Components.interfaces;
var server = new HttpServer();
server.start(-1);
var baseURL = "http://localhost:" + server.identity.primaryPort + "/";
var maxConnections = 0;
var debug = false;
var dummyResponseQueue = new Array();
var responseQueue = new Array();

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

function createHttpRequest(requestId, priority, isBlocking, callback) {
  let uri = baseURL;
  var chan = make_channel(uri);
  var listner = new HttpResponseListener(requestId, callback);
  chan.setRequestHeader("X-ID", requestId, false);
  chan.setRequestHeader("Cache-control", "no-store", false);
  chan.QueryInterface(Ci.nsISupportsPriority).priority = priority;
  if (isBlocking) {
    var cos = chan.QueryInterface(Ci.nsIClassOfService);
    cos.addClassFlags(Ci.nsIClassOfService.Leader);
  }
  chan.asyncOpen2(listner);
  log("Create http request id=" + requestId);
}

function setup_dummyHttpRequests(callback) {
  log("setup_dummyHttpRequests");
  for (var i = 0; i < maxConnections ; i++) {
    createHttpRequest(i, i, false, callback);
    do_test_pending();
  }
}

var transactionQueue = [
  {requestId: 101, priority: Ci.nsISupportsPriority.PRIORITY_HIGH, isBlocking: true},
  {requestId: 102, priority: Ci.nsISupportsPriority.PRIORITY_NORMAL, isBlocking: true},
  {requestId: 103, priority: Ci.nsISupportsPriority.PRIORITY_LOW, isBlocking: true},
  {requestId: 104, priority: Ci.nsISupportsPriority.PRIORITY_HIGH, isBlocking: false},
  {requestId: 105, priority: Ci.nsISupportsPriority.PRIORITY_NORMAL, isBlocking: false},
  {requestId: 106, priority: Ci.nsISupportsPriority.PRIORITY_LOW, isBlocking: false},
];

function setup_HttpRequests() {
  log("setup_HttpRequests");
  // Create channels in reverse order
  for (var i = transactionQueue.length - 1; i > -1;) {
    var e = transactionQueue[i];
    createHttpRequest(e.requestId, e.priority, e.isBlocking);
    do_test_pending();
    --i;
  }
}

function check_response_id(responses)
{
  for (var i = 0; i < responses.length; i++) {
    var id = responses[i].getHeader("X-ID");
    Assert.equal(id, transactionQueue[i].requestId);
  }
}

function HttpResponseListener(id, onStopCallback)
{
  this.id = id
  this.stopCallback = onStopCallback;
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
    if (this.stopCallback) {
      this.stopCallback();
    }
  }
};

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

    if (!allDummyHttpRequestReceived) {
      dummyResponseQueue.push(response);
    } else {
      responseQueue.push(response);
    }

    if (dummyResponseQueue.length == maxConnections) {
      log("received all dummy http requets");
      allDummyHttpRequestReceived = true;
      setup_HttpRequests();
      processDummyResponse();
    } else if (responseQueue.length == maxConnections) {
      log("received all http requets");
      check_response_id(responseQueue);
      processResponses();
    }

  });

  do_register_cleanup(function() {
    server.stop(serverStopListener);
  });

}

function processDummyResponse() {
  if (!dummyResponseQueue.length) {
    return;
  }
  var resposne = dummyResponseQueue.pop();
  resposne.finish();
}

function processResponses() {
  while (responseQueue.length) {
    var resposne = responseQueue.pop();
    resposne.finish();
  }
}

function run_test() {
  setup_http_server();
  setup_dummyHttpRequests(processDummyResponse);
}
