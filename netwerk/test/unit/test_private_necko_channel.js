//
// Private channel test
//

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

var httpserver = new HttpServer();
var testpath = "/simple";
var httpbody = "0123456789";

function run_test() {
  // Simulate a profile dir for xpcshell
  do_get_profile();

  // Start off with an empty cache
  evict_cache_entries();

  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(-1);

  var channel = setupChannel(testpath);
  channel.loadGroup = Cc["@mozilla.org/network/load-group;1"].createInstance();

  channel.QueryInterface(Ci.nsIPrivateBrowsingChannel);
  channel.setPrivate(true);

  channel.asyncOpen2(new ChannelListener(checkRequest, channel));

  do_test_pending();
}

function setupChannel(path) {
  return NetUtil.newChannel({
    uri: "http://localhost:" + httpserver.identity.primaryPort + path,
    loadUsingSystemPrincipal: true
  }).QueryInterface(Ci.nsIHttpChannel);
}

function serverHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
}

function checkRequest(request, data, context) {
  get_device_entry_count("disk", null, function(count) {
    do_check_eq(count, 0)
    get_device_entry_count("disk", LoadContextInfo.private, function(count) {
      do_check_eq(count, 1);
      httpserver.stop(do_test_finished);
    });
  });
}
