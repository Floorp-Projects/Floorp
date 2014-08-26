
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var test_index = 0;

var responses = [
  "response0", // This should be the first returned value
  "response1", // This response should not be recevied. Load response0 from cache
  "response2", // This request should fail
  "response3", // This request should succeed
  ];

function http_handler(metadata, response) {
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  var body = responses[test_index];
  response.bodyOutputStream.write(body, body.length);
}


function set_app_offline(appId, offline) {
  let ioservice = Cc['@mozilla.org/network/io-service;1'].
    getService(Ci.nsIIOService);

  ioservice.setAppOffline(appId, offline);
}

var httpserv;

function setup() {
  httpserv = new HttpServer();
  httpserv.registerPathHandler("/first", http_handler);
  httpserv.registerPathHandler("/second", http_handler);
  httpserv.start(12345);
}

function run_test() {
  setup();
  test0();
}

// Test that app 14 can open channel
function test0() {
  test_index = 0;
  run_test_in_child("child_app_offline.js", test1);
}

// Set app 14 offline and check that it still gets a cached response
function test1() {
  test_index = 1;
  set_app_offline(14, Ci.nsIAppOfflineInfo.OFFLINE);
  sendCommand('test1();\n', test2);
}

// Check that app 14 can't open a channel to a new location
function test2() {
  test_index = 2;
  sendCommand('test2();\n', test3);
}


// Set app online and check that it now works
function test3() {
  test_index = 3;
  set_app_offline(14, Ci.nsIAppOfflineInfo.ONLINE);
  sendCommand('test3();\n', ending);
}

function ending(val) {
  do_test_finished();
}
