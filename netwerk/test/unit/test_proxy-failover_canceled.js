const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");

var httpServer = null;

function make_channel(url, callback, ctx) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newChannel(url, "", null);
}

const responseBody = "response body";

function contentHandler(metadata, response)
{
  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.write(responseBody, responseBody.length);
}

function finish_test(request, buffer)
{
  do_check_eq(buffer, "");
  httpServer.stop(do_test_finished);
}

function run_test()
{
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(4444);

  var nc = new ChannelEventSink();
  var on_modify_request_count = 0;

  modifyrequestobserver = {observe: function() {
    // We get 2 on-modify-request notifications:
    // 1. when proxy service resolves the proxy settings from PAC function
    // 2. when we try to fail over the first proxy (moving to the second one)
    //
    // In the second case we want to cancel the proxy engage, so, do not allow
    // redirects from now.

    if (++on_modify_request_count == 2)
      nc._flags = ES_ABORT_REDIRECT;
  }}

  var os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  os.addObserver(modifyrequestobserver, "http-on-modify-request", false);

  var prefserv = Cc["@mozilla.org/preferences-service;1"].
                 getService(Ci.nsIPrefService);
  var prefs = prefserv.getBranch("network.proxy.");
  prefs.setIntPref("type", 2);
  prefs.setCharPref("autoconfig_url", "data:text/plain," +
    "function FindProxyForURL(url, host) {return 'PROXY a_non_existent_domain_x7x6c572v:80; PROXY localhost:4444';}"
  );

  var chan = make_channel("http://localhost:4444/content");
  chan.notificationCallbacks = nc;
  chan.asyncOpen(new ChannelListener(finish_test, null, CL_EXPECT_FAILURE), null);
  do_test_pending();
}
