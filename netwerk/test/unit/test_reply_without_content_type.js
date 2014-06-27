//
// tests HTTP replies that lack content-type (where we try to sniff content-type).
//

// Note: sets Cc and Ci variables

Cu.import("resource://testing-common/httpd.js");

var httpserver = new HttpServer();
var testpath = "/simple";
var httpbody = "<html><body>omg hai</body></html>";
var buffer = "";

var dbg=0
if (dbg) { print("============== START =========="); }

function run_test() {
  setup_test();
  do_test_pending();
}

function setup_test() {
  if (dbg) { print("============== setup_test: in"); }
  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(-1);
  var channel = setupChannel(testpath);
  // ChannelListener defined in head_channels.js
  channel.asyncOpen(new ChannelListener(checkRequest, channel), null);
  if (dbg) { print("============== setup_test: out"); }
}

function setupChannel(path) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
                       getService(Ci.nsIIOService);
  var chan = ios.newChannel("http://localhost:" +
                            httpserver.identity.primaryPort + path, "", null);
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.requestMethod = "GET";
  return chan;
}

function serverHandler(metadata, response) {
  if (dbg) { print("============== serverHandler: in"); }
//  no content type set
//  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
  if (dbg) { print("============== serverHandler: out"); }
}

function checkRequest(request, data, context) {
  if (dbg) { print("============== checkRequest: in"); }
  do_check_eq(data, httpbody);
  do_check_eq(request.QueryInterface(Ci.nsIChannel).contentType,"text/html");
  httpserver.stop(do_test_finished);
  if (dbg) { print("============== checkRequest: out"); }
}

