// Test getLocalHost/getLocalPort and getRemoteHost/getRemotePort.

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");

var httpserver = new HttpServer();
httpserver.start(-1);
const PORT = httpserver.identity.primaryPort;

var gotOnStartRequest = false;

function CheckGetHostListener() {}

CheckGetHostListener.prototype = {
  onStartRequest: function(request, context) {
    dump("*** listener onStartRequest\n");

    gotOnStartRequest = true;

    request.QueryInterface(Components.interfaces.nsIHttpChannelInternal);
    try {
      do_check_eq(request.localAddress, "127.0.0.1");
      do_check_eq(request.localPort > 0, true);
      do_check_neq(request.localPort, PORT);
      do_check_eq(request.remoteAddress, "127.0.0.1");
      do_check_eq(request.remotePort, PORT);
    } catch (e) {
      do_check_true(0, "Get local/remote host/port throws an error!");
    }
  },

  onStopRequest: function(request, context, statusCode) {
    dump("*** listener onStopRequest\n");

    do_check_eq(gotOnStartRequest, true);
    httpserver.stop(do_test_finished);
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIRequestObserver) ||
        iid.equals(Components.interfaces.nsISupports)
        )
      return this;
    throw Components.results.NS_NOINTERFACE;
  },
}

function make_channel(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);
  return ios.newChannel2(url,
                         null,
                         null,
                         null,      // aLoadingNode
                         Services.scriptSecurityManager.getSystemPrincipal(),
                         null,      // aTriggeringPrincipal
                         Ci.nsILoadInfo.SEC_NORMAL,
                         Ci.nsIContentPolicy.TYPE_OTHER)
            .QueryInterface(Components.interfaces.nsIHttpChannel);
}

function test_handler(metadata, response) {
  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  var responseBody = "blah blah";
  response.bodyOutputStream.write(responseBody, responseBody.length);
}

function run_test() {
  httpserver.registerPathHandler("/testdir", test_handler);

  var channel = make_channel("http://localhost:" + PORT + "/testdir");
  channel.asyncOpen(new CheckGetHostListener(), null);
  do_test_pending();
}
