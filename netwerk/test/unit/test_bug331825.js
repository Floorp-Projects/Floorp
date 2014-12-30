Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");

var server;
const BUGID = "331825";

function TestListener() {
}
TestListener.prototype.onStartRequest = function(request, context) {
}
TestListener.prototype.onStopRequest = function(request, context, status) {
  var channel = request.QueryInterface(Components.interfaces.nsIHttpChannel);
  do_check_eq(channel.responseStatus, 304);

  server.stop(do_test_finished);
}

function run_test() {
  // start server
  server = new HttpServer();

  server.registerPathHandler("/bug" + BUGID, bug331825);

  server.start(-1);

  // make request
  var channel =
      Components.classes["@mozilla.org/network/io-service;1"].
      getService(Components.interfaces.nsIIOService).
      newChannel2("http://localhost:" + server.identity.primaryPort + "/bug" + BUGID,
                  null,
                  null,
                  null,      // aLoadingNode
                  Services.scriptSecurityManager.getSystemPrincipal(),
                  null,      // aTriggeringPrincipal
                  Ci.nsILoadInfo.SEC_NORMAL,
                  Ci.nsIContentPolicy.TYPE_OTHER);

  channel.QueryInterface(Components.interfaces.nsIHttpChannel);
  channel.setRequestHeader("If-None-Match", "foobar", false);
  channel.asyncOpen(new TestListener(), null);

  do_test_pending();
}

// PATH HANDLER FOR /bug331825
function bug331825(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 304, "Not Modified");
}
