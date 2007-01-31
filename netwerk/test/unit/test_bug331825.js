do_import_script("netwerk/test/httpserver/httpd.js");

var server;
var BUGID = "331825";

function TestListener() {
}
TestListener.prototype.onStartRequest = function(request, context) {
}
TestListener.prototype.onStopRequest = function(request, context, status) {
  var channel = request.QueryInterface(Components.interfaces.nsIHttpChannel);
  do_check_eq(channel.responseStatus, 304);

  server.stop();
  do_test_finished();
}

function run_test() {
  // start server
  server = new nsHttpServer();

  server.registerPathHandler("/bug" + BUGID, bug331825);

  server.start(4444);

  // make request
  var channel =
      Components.classes["@mozilla.org/network/io-service;1"].
      getService(Components.interfaces.nsIIOService).
      newChannel("http://localhost:4444/bug" + BUGID, null, null);

  channel.QueryInterface(Components.interfaces.nsIHttpChannel);
  channel.setRequestHeader("If-None-Match", "foobar", false);
  channel.asyncOpen(new TestListener(), null);

  do_test_pending();
}

// PATH HANDLER FOR /bug331825
function bug331825(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 304, "Not Modified");
}
