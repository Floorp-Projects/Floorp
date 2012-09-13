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

  var prefserv = Cc["@mozilla.org/preferences-service;1"].
                 getService(Ci.nsIPrefService);
  var prefs = prefserv.getBranch("network.proxy.");
  prefs.setIntPref("type", 2);
  prefs.setCharPref("autoconfig_url", "data:text/plain," +
    "function FindProxyForURL(url, host) {return 'PROXY localhost:4444';}"
  );

  // this test assumed that a AsyncOnChannelRedirect query is made for
  // each proxy failover or on the inital proxy only when PAC mode is used.
  // Neither of those are documented anywhere that I can find and the latter
  // hasn't been a useful property because it is PAC dependent and the type
  // is generally unknown and OS driven. 769764 changed that to remove the
  // internal redirect used to setup the initial proxy/channel as that isn't
  // a redirect in any sense.

  var chan = make_channel("http://localhost:4444/content");
  chan.asyncOpen(new ChannelListener(finish_test, null, CL_EXPECT_FAILURE), null);
  chan.cancel(Cr.NS_BINDING_ABORTED);
  do_test_pending();
}
