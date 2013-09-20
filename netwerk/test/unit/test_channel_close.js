Cu.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = new HttpServer();
var testpath = "/simple";
var httpbody = "0123456789";

var live_channels = [];

function run_test() {
  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(-1);

  var local_channel;

  // Opened channel that has no remaining references on shutdown
  local_channel = setupChannel(testpath);
  local_channel.asyncOpen(
      new ChannelListener(checkRequest, local_channel), null);

  // Opened channel that has no remaining references after being opened
  setupChannel(testpath).asyncOpen(
      new ChannelListener(function() {}, null), null);
  
  // Unopened channel that has remaining references on shutdown
  live_channels.push(setupChannel(testpath));

  // Opened channel that has remaining references on shutdown
  live_channels.push(setupChannel(testpath));
  live_channels[1].asyncOpen(
      new ChannelListener(checkRequestFinish, live_channels[1]), null);

  do_test_pending();
}

function setupChannel(path) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var chan = ios.newChannel(URL + path, "", null);
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.requestMethod = "GET";
  return chan;
}

function serverHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
}

function checkRequest(request, data, context) {
  do_check_eq(data, httpbody);
}

function checkRequestFinish(request, data, context) {
  checkRequest(request, data, context);
  httpserver.stop(do_test_finished);
}
