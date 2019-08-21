//
//  HTTP headers test
//

// Note: sets Cc and Ci variables

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const ReferrerInfo = Components.Constructor(
  "@mozilla.org/referrer-info;1",
  "nsIReferrerInfo",
  "init"
);

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = new HttpServer();
var testpath = "/simple";
var httpbody = "0123456789";
var channel;
var ios;

var dbg = 0;
if (dbg) {
  print("============== START ==========");
}

function run_test() {
  setup_test();
  do_test_pending();
}

function setup_test() {
  if (dbg) {
    print("============== setup_test: in");
  }

  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(-1);

  channel = setupChannel(testpath);

  channel.setRequestHeader("ReplaceMe", "initial value", true);
  var setOK = channel.getRequestHeader("ReplaceMe");
  Assert.equal(setOK, "initial value");
  channel.setRequestHeader("ReplaceMe", "replaced", false);
  setOK = channel.getRequestHeader("ReplaceMe");
  Assert.equal(setOK, "replaced");

  channel.setRequestHeader("MergeMe", "foo1", true);
  channel.setRequestHeader("MergeMe", "foo2", true);
  channel.setRequestHeader("MergeMe", "foo3", true);
  setOK = channel.getRequestHeader("MergeMe");
  Assert.equal(setOK, "foo1, foo2, foo3");

  channel.setEmptyRequestHeader("Empty");
  setOK = channel.getRequestHeader("Empty");
  Assert.equal(setOK, "");

  channel.setRequestHeader("ReplaceWithEmpty", "initial value", true);
  setOK = channel.getRequestHeader("ReplaceWithEmpty");
  Assert.equal(setOK, "initial value");
  channel.setEmptyRequestHeader("ReplaceWithEmpty");
  setOK = channel.getRequestHeader("ReplaceWithEmpty");
  Assert.equal(setOK, "");

  channel.setEmptyRequestHeader("MergeWithEmpty");
  setOK = channel.getRequestHeader("MergeWithEmpty");
  Assert.equal(setOK, "");
  channel.setRequestHeader("MergeWithEmpty", "foo", true);
  setOK = channel.getRequestHeader("MergeWithEmpty");
  Assert.equal(setOK, "foo");

  var uri = NetUtil.newURI("http://foo1.invalid:80");
  channel.referrerInfo = new ReferrerInfo(Ci.nsIReferrerInfo.EMPTY, true, uri);
  setOK = channel.getRequestHeader("Referer");
  Assert.equal(setOK, "http://foo1.invalid/");

  uri = NetUtil.newURI("http://foo2.invalid:90/bar");
  channel.referrerInfo = new ReferrerInfo(Ci.nsIReferrerInfo.EMPTY, true, uri);
  setOK = channel.getRequestHeader("Referer");
  Assert.equal(setOK, "http://foo2.invalid:90/bar");

  // ChannelListener defined in head_channels.js
  channel.asyncOpen(new ChannelListener(checkRequestResponse, channel));

  if (dbg) {
    print("============== setup_test: out");
  }
}

function setupChannel(path) {
  var chan = NetUtil.newChannel({
    uri: URL + path,
    loadUsingSystemPrincipal: true,
  });
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.requestMethod = "GET";
  return chan;
}

function serverHandler(metadata, response) {
  if (dbg) {
    print("============== serverHandler: in");
  }

  var setOK = metadata.getHeader("ReplaceMe");
  Assert.equal(setOK, "replaced");
  setOK = metadata.getHeader("MergeMe");
  Assert.equal(setOK, "foo1, foo2, foo3");
  setOK = metadata.getHeader("Empty");
  Assert.equal(setOK, "");
  setOK = metadata.getHeader("ReplaceWithEmpty");
  Assert.equal(setOK, "");
  setOK = metadata.getHeader("MergeWithEmpty");
  Assert.equal(setOK, "foo");
  setOK = metadata.getHeader("Referer");
  Assert.equal(setOK, "http://foo2.invalid:90/bar");

  response.setHeader("Content-Type", "text/plain", false);
  response.setStatusLine("1.1", 200, "OK");

  // note: httpd.js' "Response" class uses ',' (no space) for merge.
  response.setHeader("httpdMerge", "bar1", false);
  response.setHeader("httpdMerge", "bar2", true);
  response.setHeader("httpdMerge", "bar3", true);
  // Some special headers like Proxy-Authenticate merge with \n
  response.setHeader("Proxy-Authenticate", "line 1", true);
  response.setHeader("Proxy-Authenticate", "line 2", true);
  response.setHeader("Proxy-Authenticate", "line 3", true);

  response.bodyOutputStream.write(httpbody, httpbody.length);

  if (dbg) {
    print("============== serverHandler: out");
  }
}

function checkRequestResponse(request, data, context) {
  if (dbg) {
    print("============== checkRequestResponse: in");
  }

  Assert.equal(channel.responseStatus, 200);
  Assert.equal(channel.responseStatusText, "OK");
  Assert.ok(channel.requestSucceeded);

  var response = channel.getResponseHeader("httpdMerge");
  Assert.equal(response, "bar1,bar2,bar3");
  channel.setResponseHeader("httpdMerge", "bar", true);
  Assert.equal(channel.getResponseHeader("httpdMerge"), "bar1,bar2,bar3, bar");

  response = channel.getResponseHeader("Proxy-Authenticate");
  Assert.equal(response, "line 1\nline 2\nline 3");

  channel.contentCharset = "UTF-8";
  Assert.equal(channel.contentCharset, "UTF-8");
  Assert.equal(channel.contentType, "text/plain");
  Assert.equal(channel.contentLength, httpbody.length);
  Assert.equal(data, httpbody);

  httpserver.stop(do_test_finished);
  if (dbg) {
    print("============== checkRequestResponse: out");
  }
}
