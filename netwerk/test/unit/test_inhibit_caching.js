const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

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
  var chan = NetUtil.newChannel({uri: uri+"/test", loadUsingSystemPrincipal: true});
  chan.asyncOpen(new ChannelListener(check_first_response, null));
}

// Checks that we got the appropriate response
function check_first_response(request, buffer) {
  request.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(request.responseStatus, 200);
  Assert.equal(buffer, "first");
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
  var chan = NetUtil.newChannel({uri: uri+"/test", loadUsingSystemPrincipal: true});
  chan.QueryInterface(Ci.nsIRequest).loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
  chan.asyncOpen(new ChannelListener(check_second_response, null));
}

// Checks that we got a different response from the first request
function check_second_response(request, buffer) {
  request.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(request.responseStatus, 200);
  Assert.equal(buffer, "second");
  // Checks that the cache entry still contains the content from the first request
  asyncOpenCacheEntry(uri+"/test","disk", Ci.nsICacheStorage.OPEN_READONLY, null, cache_entry_callback);
}
