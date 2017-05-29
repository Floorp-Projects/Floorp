Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort;
});

var httpServer = null;
var randomPath = "/redirect/" + Math.random();
var redirects = [];
const numRedirects = 10;

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
}

const responseBody = "response body";

function contentHandler(request, response)
{
  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.write(responseBody, responseBody.length);
}

function finish_test(request, buffer)
{
  do_check_eq(buffer, responseBody);
  let chan = request.QueryInterface(Ci.nsIChannel);
  let redirectChain = chan.loadInfo.redirectChain;

  do_check_eq(numRedirects - 1, redirectChain.length);
  for (let i = 0; i < numRedirects - 1; ++i) {
    let principal = redirectChain[i].principal;
    do_check_eq(URL + redirects[i], principal.URI.spec);
    do_check_eq(redirectChain[i].referrerURI.spec, "http://test.com/");
    do_check_eq(redirectChain[i].remoteAddress, "127.0.0.1");
  }
  httpServer.stop(do_test_finished);
}

function redirectHandler(index, request, response) {
  response.setStatusLine(request.httpVersion, 301, "Moved");
  let path = redirects[index + 1];
  response.setHeader("Location", URL + path, false);
}

function run_test()
{
  httpServer = new HttpServer();
  for (let i = 0; i < numRedirects; ++i) {
    var randomPath = "/redirect/" + Math.random();
    redirects.push(randomPath);
    if (i < numRedirects - 1) {
      httpServer.registerPathHandler(randomPath, redirectHandler.bind(this, i));
    } else {
      // The last one doesn't redirect
      httpServer.registerPathHandler(redirects[numRedirects - 1],
                                     contentHandler);
    }
  }
  httpServer.start(-1);

  var chan = make_channel(URL + redirects[0]);
  var uri = NetUtil.newURI("http://test.com");
  httpChan = chan.QueryInterface(Components.interfaces.nsIHttpChannel);
  httpChan.referrer = uri;
  chan.asyncOpen2(new ChannelListener(finish_test, null));
  do_test_pending();
}
