Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

XPCOMUtils.defineLazyGetter(this, "uri", function() {
  return URL + "/redirect";
});

XPCOMUtils.defineLazyGetter(this, "noRedirectURI", function() {
  return URL + "/content";
});

var httpserver = null;

function make_channel(url) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
}

const requestBody = "request body";

function redirectHandler(metadata, response)
{
  response.setStatusLine(metadata.httpVersion, 307, "Moved Temporarily");
  response.setHeader("Location", noRedirectURI, false);
  return;
}

function contentHandler(metadata, response)
{
  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.writeFrom(metadata.bodyInputStream,
                                      metadata.bodyInputStream.available());
}

function noRedirectStreamObserver(request, buffer)
{
  Assert.equal(buffer, requestBody);
  var chan = make_channel(uri);
  var uploadStream = Cc["@mozilla.org/io/string-input-stream;1"]
                       .createInstance(Ci.nsIStringInputStream);
  uploadStream.setData(requestBody, requestBody.length);
  chan.QueryInterface(Ci.nsIUploadChannel).setUploadStream(uploadStream,
                                                           "text/plain",
                                                           -1);
  chan.asyncOpen2(new ChannelListener(noHeaderStreamObserver, null));
}

function noHeaderStreamObserver(request, buffer)
{
  Assert.equal(buffer, requestBody);
  var chan = make_channel(uri);
  var uploadStream = Cc["@mozilla.org/io/string-input-stream;1"]
                       .createInstance(Ci.nsIStringInputStream);
  var streamBody = "Content-Type: text/plain\r\n" +
      "Content-Length: " + requestBody.length + "\r\n\r\n" +
      requestBody;
  uploadStream.setData(streamBody, streamBody.length);
  chan.QueryInterface(Ci.nsIUploadChannel).setUploadStream(uploadStream, "", -1);
  chan.asyncOpen2(new ChannelListener(headerStreamObserver, null));
}

function headerStreamObserver(request, buffer)
{
  Assert.equal(buffer, requestBody);
  httpserver.stop(do_test_finished);
}

function run_test()
{
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/redirect", redirectHandler);
  httpserver.registerPathHandler("/content", contentHandler);
  httpserver.start(-1);

  var prefs = Cc["@mozilla.org/preferences-service;1"]
                .getService(Components.interfaces.nsIPrefBranch);
  prefs.setBoolPref("network.http.prompt-temp-redirect", false);

  var chan = make_channel(noRedirectURI);
  var uploadStream = Cc["@mozilla.org/io/string-input-stream;1"]
                       .createInstance(Ci.nsIStringInputStream);
  uploadStream.setData(requestBody, requestBody.length);
  chan.QueryInterface(Ci.nsIUploadChannel).setUploadStream(uploadStream,
                                                           "text/plain",
                                                           -1);
  chan.asyncOpen2(new ChannelListener(noRedirectStreamObserver, null));
  do_test_pending();
}
