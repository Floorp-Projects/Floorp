// test bug 1312774.
// Create 6 (=network.http.max-persistent-connections-per-server)
// common Http requests and 2 urgent-start Http requests to a single
// host and path, in parallel.
// Let all the requests unanswered by the server handler. (process them
// async and don't finish)
// The first 6 pending common requests will fill the limit for per-server
// parallelism.
// But the two urgent requests must reach the server despite those 6 common
// pending requests.
// The server handler doesn't let the test finish until all 8 expected requests
// arrive.
// Note: if the urgent request handling is broken (the urgent-marked requests
// get blocked by queuing) this test will time out

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");
var server = new HttpServer();
server.start(-1);
var baseURL = "http://localhost:" + server.identity.primaryPort + "/";
var maxConnections = 0;
var urgentRequests = 0;
var totalRequests  = 0;
var debug = false;


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

function commonHttpRequest(id) {
  let uri = baseURL;
  var chan = make_channel(uri);
  var listner = new HttpResponseListener(id);
  chan.setRequestHeader("X-ID", id, false);
  chan.setRequestHeader("Cache-control", "no-store", false);
  chan.asyncOpen(listner);
  log("Create common http request id=" + id);
}

function urgentStartHttpRequest(id) {
  let uri = baseURL;
  var chan = make_channel(uri);
  var listner = new HttpResponseListener(id);
  var cos = chan.QueryInterface(Ci.nsIClassOfService);
  cos.addClassFlags(Ci.nsIClassOfService.UrgentStart);
  chan.setRequestHeader("X-ID", id, false);
  chan.setRequestHeader("Cache-control", "no-store", false);
  chan.asyncOpen(listner);
  log("Create urgent-start http request id=" + id);
}

function setup_httpRequests() {
  log("setup_httpRequests");
  for (var i = 0; i < maxConnections ; i++) {
    commonHttpRequest(i);
    do_test_pending();
  }
}

function setup_urgentStartRequests() {
  for (var i = 0; i < urgentRequests; i++) {
    urgentStartHttpRequest(1000 + i);
    do_test_pending();
  }
}

function HttpResponseListener(id)
{
  this.id = id
};

var testOrder = 0;
HttpResponseListener.prototype =
{
  onStartRequest: function (request) {
  },

  onDataAvailable: function (request, stream, off, cnt) {
  },

  onStopRequest: function (request, status) {
    log("STOP id=" + this.id);
    do_test_finished();
  }
};

var responseQueue = new Array();
function setup_http_server()
{
  log("setup_http_server");
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  maxConnections = prefs.getIntPref("network.http.max-persistent-connections-per-server");
  urgentRequests = 2;
  totalRequests = maxConnections + urgentRequests;
  var allCommonHttpRequestReceived = false;
  // Start server; will be stopped at test cleanup time.
  server.registerPathHandler('/', function(metadata, response)
  {
    var id = metadata.getHeader("X-ID");
    log("Server recived the response id=" + id);
    response.processAsync();
    responseQueue.push(response);

    if (responseQueue.length == maxConnections && !allCommonHttpRequestReceived) {
      allCommonHttpRequestReceived = true;
      setup_urgentStartRequests();
    }
    // Wait for all expected requests to come but don't process then.
    // Collect them in a queue for later processing.  We don't want to
    // respond to the client until all the expected requests are made
    // to the server.
    if (responseQueue.length == maxConnections + urgentRequests) {
      processResponse();
    }
  });

  registerCleanupFunction(function() {
    server.stop(serverStopListener);
  });
}

function processResponse() {
  while (responseQueue.length) {
    var resposne = responseQueue.pop();
    resposne.finish();
  }
}

function run_test() {
  setup_http_server();
  setup_httpRequests();
}

