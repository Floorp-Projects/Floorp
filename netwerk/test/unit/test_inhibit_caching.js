Cu.import('resource://gre/modules/LoadContextInfo.jsm');
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");

var first = true;
function contentHandler(metadata, response)
{
  response.setHeader("Content-Type", 'text/plain');
  var body = "first";
  if (!first) {
    body = "second";
  }
  first = false;
  response.bodyOutputStream.write(body, body.length);
}

XPCOMUtils.defineLazyGetter(this, "uri", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = null;

function run_test()
{
  // setup test
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/test", contentHandler);
  httpserver.start(-1);

  add_test(test_first_response);
  add_test(test_inhibit_caching);

  run_next_test();
}

// Makes a regular request
function test_first_response() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  var chan = ios.newChannel2(uri+"/test",
                         "",
                         null,
                         null,      // aLoadingNode
                         Services.scriptSecurityManager.getSystemPrincipal(),
                         null,      // aTriggeringPrincipal
                         Ci.nsILoadInfo.SEC_NORMAL,
                         Ci.nsIContentPolicy.TYPE_OTHER);

  chan.asyncOpen(new ChannelListener(check_first_response, null), null);
}

// Checks that we got the appropriate response
function check_first_response(request, buffer) {
  request.QueryInterface(Ci.nsIHttpChannel);
  do_check_eq(request.responseStatus, 200);
  do_check_eq(buffer, "first");
  // Open the cache entry to check its contents
  asyncOpenCacheEntry(uri+"/test","disk", Ci.nsICacheStorage.OPEN_READONLY, null, cache_entry_callback);
}

// Checks that the cache entry has the correct contents
function cache_entry_callback(status, entry) {
  equal(status, Cr.NS_OK);
  var inputStream = entry.openInputStream(0);
  pumpReadStream(inputStream, function(read) {
      inputStream.close();
      equal(read,"first");
      run_next_test();
  });
}

// Makes a request with the INHIBIT_CACHING load flag
function test_inhibit_caching() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  var chan = ios.newChannel2(uri+"/test",
                         "",
                         null,
                         null,      // aLoadingNode
                         Services.scriptSecurityManager.getSystemPrincipal(),
                         null,      // aTriggeringPrincipal
                         Ci.nsILoadInfo.SEC_NORMAL,
                         Ci.nsIContentPolicy.TYPE_OTHER);

  chan.QueryInterface(Ci.nsIRequest).loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
  chan.asyncOpen(new ChannelListener(check_second_response, null), null);
}

// Checks that we got a different response from the first request
function check_second_response(request, buffer) {
  request.QueryInterface(Ci.nsIHttpChannel);
  do_check_eq(request.responseStatus, 200);
  do_check_eq(buffer, "second");
  // Checks that the cache entry still contains the content from the first request
  asyncOpenCacheEntry(uri+"/test","disk", Ci.nsICacheStorage.OPEN_READONLY, null, cache_entry_callback);
}
