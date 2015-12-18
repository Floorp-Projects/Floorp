/*

Checkes if the concurrent cache read/write works when the write is interrupted because of max-entry-size limits.
This test is using a non-resumable response.
- with a profile, set max-entry-size to 0
- first channel makes a request for a non-resumable (chunked) response
- second channel makes a request for the same resource, concurrent read is bypassed (non-resumable response)
- first channel writes first bytes to the cache output stream, but that fails because of the max-entry-size limit and entry is doomed
- cache entry output stream is closed
- second channel gets the entry, opening the input stream must fail
- second channel must read the content again from the network
- both channels must deliver full content w/o errors

*/

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort;
});

var httpServer = null;

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
}

const responseBody = "c\r\ndata reached\r\n3\r\nhej\r\n0\r\n\r\n";
const responseBodyDecoded = "data reachedhej";

function contentHandler(metadata, response)
{
  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Transfer-Encoding: chunked\r\n");
  response.write("\r\n");
  response.write(responseBody);
  response.finish();
}

function run_test()
{
  do_get_profile();

  Services.prefs.setIntPref("browser.cache.disk.max_entry_size", 0);

  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);

  var chan1 = make_channel(URL + "/content");
  chan1.asyncOpen2(new ChannelListener(firstTimeThrough, null, CL_ALLOW_UNKNOWN_CL));
  var chan2 = make_channel(URL + "/content");
  chan2.asyncOpen2(new ChannelListener(secondTimeThrough, null, CL_ALLOW_UNKNOWN_CL));

  do_test_pending();
}

function firstTimeThrough(request, buffer)
{
  do_check_eq(buffer, responseBodyDecoded);
}

function secondTimeThrough(request, buffer)
{
  do_check_eq(buffer, responseBodyDecoded);
  httpServer.stop(do_test_finished);
}
