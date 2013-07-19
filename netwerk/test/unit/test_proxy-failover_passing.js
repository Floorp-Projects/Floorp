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
  do_check_eq(buffer, responseBody);
  httpServer.stop(do_test_finished);
}

function run_test()
{
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);

  var prefserv = Cc["@mozilla.org/preferences-service;1"].
                 getService(Ci.nsIPrefService);
  var prefs = prefserv.getBranch("network.proxy.");
  prefs.setIntPref("type", 2);
  prefs.setCharPref("autoconfig_url", "data:text/plain," +
    "function FindProxyForURL(url, host) {return 'PROXY a_non_existent_domain_x7x6c572v:80; PROXY localhost:" +
    httpServer.identity.primaryPort + "';}"
  );

  var chan = make_channel("http://localhost:" +
                          httpServer.identity.primaryPort + "/content");
  chan.asyncOpen(new ChannelListener(finish_test, null), null);
  do_test_pending();
}
