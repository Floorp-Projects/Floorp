do_import_script("netwerk/test/httpserver/httpd.js");

var httpserver = null;
// Need to randomize, because apparently no one clears our cache
var randomPath = "/redirect/" + Math.random();
var randomURI = "http://localhost:4444" + randomPath;

function make_channel(url, callback, ctx) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newChannel(url, "", null);
}

const responseBody = "response body";

function redirectHandler(metadata, response)
{
  response.setStatusLine(metadata.httpVersion, 301, "Moved");
  response.setHeader("Location", "http://localhost:4444/content", false);
  return;
}

function contentHandler(metadata, response)
{
  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.write(responseBody, responseBody.length);
}

function firstTimeThrough(request, buffer)
{
  do_check_eq(buffer, responseBody);
  var chan = make_channel(randomURI);
  chan.loadFlags |= Ci.nsICachingChannel.LOAD_ONLY_FROM_CACHE;
  chan.asyncOpen(new ChannelListener(finish_test, null), null);
}

function finish_test(request, buffer)
{
  httpserver.stop();
  do_check_eq(buffer, responseBody);
  do_test_finished();
}

function run_test()
{
  httpserver = new nsHttpServer();
  httpserver.registerPathHandler(randomPath, redirectHandler);
  httpserver.registerPathHandler("/content", contentHandler);
  httpserver.start(4444);

  var chan = make_channel(randomURI);
  chan.asyncOpen(new ChannelListener(firstTimeThrough, null), null);
  do_test_pending();
}
