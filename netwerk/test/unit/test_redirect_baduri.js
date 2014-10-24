Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");

/*
 * Test whether we fail bad URIs in HTTP redirect as CORRUPTED_CONTENT.
 */

var httpServer = null;

var BadRedirectPath = "/BadRedirect";
XPCOMUtils.defineLazyGetter(this, "BadRedirectURI", function() {
  return "http://localhost:" + httpServer.identity.primaryPort + BadRedirectPath;
});

function make_channel(url, callback, ctx) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newChannel2(url,
                         "",
                         null,
                         null,      // aLoadingNode
                         Services.scriptSecurityManager.getSystemPrincipal(),
                         null,      // aTriggeringPrincipal
                         Ci.nsILoadInfo.SEC_NORMAL,
                         Ci.nsIContentPolicy.TYPE_OTHER);
}

function BadRedirectHandler(metadata, response)
{
  response.setStatusLine(metadata.httpVersion, 301, "Moved");
  // '>' in URI will fail to parse: we should not render response
  response.setHeader("Location", 'http://localhost:4444>BadRedirect', false);
}

function checkFailed(request, buffer)
{
  do_check_eq(request.status, Components.results.NS_ERROR_CORRUPTED_CONTENT);

  httpServer.stop(do_test_finished);
}

function run_test()
{
  httpServer = new HttpServer();
  httpServer.registerPathHandler(BadRedirectPath, BadRedirectHandler);
  httpServer.start(-1);

  var chan = make_channel(BadRedirectURI);
  chan.asyncOpen(new ChannelListener(checkFailed, null, CL_EXPECT_FAILURE),
                 null);
  do_test_pending();
}
