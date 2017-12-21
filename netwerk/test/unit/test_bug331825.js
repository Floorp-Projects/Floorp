Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

var server;
const BUGID = "331825";

function TestListener() {
}
TestListener.prototype.onStartRequest = function(request, context) {
}
TestListener.prototype.onStopRequest = function(request, context, status) {
  var channel = request.QueryInterface(Components.interfaces.nsIHttpChannel);
  Assert.equal(channel.responseStatus, 304);

  server.stop(do_test_finished);
}

function run_test() {
  // start server
  server = new HttpServer();

  server.registerPathHandler("/bug" + BUGID, bug331825);

  server.start(-1);

  // make request
  var channel = NetUtil.newChannel({
    uri: "http://localhost:" + server.identity.primaryPort + "/bug" + BUGID,
    loadUsingSystemPrincipal: true
  });

  channel.QueryInterface(Components.interfaces.nsIHttpChannel);
  channel.setRequestHeader("If-None-Match", "foobar", false);
  channel.asyncOpen2(new TestListener());

  do_test_pending();
}

// PATH HANDLER FOR /bug331825
function bug331825(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 304, "Not Modified");
}
