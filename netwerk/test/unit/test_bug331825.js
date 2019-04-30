const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

var server;
const BUGID = "331825";

function TestListener() {
}
TestListener.prototype.onStartRequest = function(request) {
}
TestListener.prototype.onStopRequest = function(request, status) {
  var channel = request.QueryInterface(Ci.nsIHttpChannel);
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

  channel.QueryInterface(Ci.nsIHttpChannel);
  channel.setRequestHeader("If-None-Match", "foobar", false);
  channel.asyncOpen(new TestListener());

  do_test_pending();
}

// PATH HANDLER FOR /bug331825
function bug331825(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 304, "Not Modified");
}
