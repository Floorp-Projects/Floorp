Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");

var httpserv;

function TestListener() {
}

TestListener.prototype.onStartRequest = function(request, context) {
}

TestListener.prototype.onStopRequest = function(request, context, status) {
  httpserv.stop(do_test_finished);
}

function run_test() {
  httpserv = new HttpServer();

  httpserv.registerPathHandler("/bug412945", bug412945);

  httpserv.start(-1);

  // make request
  var channel =
      Components.classes["@mozilla.org/network/io-service;1"].
      getService(Components.interfaces.nsIIOService).
      newChannel2("http://localhost:" + httpserv.identity.primaryPort +
                  "/bug412945",
                  null,
                  null,
                  null,      // aLoadingNode
                  Services.scriptSecurityManager.getSystemPrincipal(),
                  null,      // aTriggeringPrincipal
                  Ci.nsILoadInfo.SEC_NORMAL,
                  Ci.nsIContentPolicy.TYPE_OTHER);

  channel.QueryInterface(Components.interfaces.nsIHttpChannel);
  channel.requestMethod = "POST";
  channel.asyncOpen(new TestListener(), null);

  do_test_pending();
}

function bug412945(metadata, response) {
  if (!metadata.hasHeader("Content-Length") ||
      metadata.getHeader("Content-Length") != "0")
  {
    do_throw("Content-Length header not found!");
  }
}
