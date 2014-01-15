Cu.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort;
});

var httpServer = null;
// Need to randomize, because apparently no one clears our cache
var randomPath = "/redirect/" + Math.random();

XPCOMUtils.defineLazyGetter(this, "randomURI", function() {
  return URL + randomPath;
});

function inChildProcess() {
  return Cc["@mozilla.org/xre/app-info;1"]
           .getService(Ci.nsIXULRuntime)
           .processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

function make_channel(url, callback, ctx) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newChannel(url, "", null);
}

const redirectTargetBody = "response body";
const response301Body = "redirect body";

function redirectHandler(metadata, response)
{
  response.setStatusLine(metadata.httpVersion, 301, "Moved");
  response.bodyOutputStream.write(response301Body, response301Body.length);
  response.setHeader("Location", "data:text/plain," + redirectTargetBody, false);
}

function finish_test(request, buffer)
{
  if (inChildProcess()) {
    // redirects to protocols other than http/ftp will fail until bug 590682 is fixed.
    do_check_eq(buffer, response301Body);
  } else {
    do_check_eq(buffer, redirectTargetBody);
  }

  httpServer.stop(do_test_finished);
}

function run_test()
{
  httpServer = new HttpServer();
  httpServer.registerPathHandler(randomPath, redirectHandler);
  httpServer.start(-1);

  var chan = make_channel(randomURI);
  chan.asyncOpen(new ChannelListener(finish_test, null, 0), null);
  do_test_pending();
}
