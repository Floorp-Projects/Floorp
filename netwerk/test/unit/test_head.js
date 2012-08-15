//
//  HTTP headers test
//

// Note: sets Cc and Ci variables
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");

var httpserver = new HttpServer();
var testpath = "/simple";
var httpbody = "0123456789";
var channel;
var ios;

var dbg=0
if (dbg) { print("============== START =========="); }

function run_test() {
  setup_test();
  do_test_pending();
}

function setup_test() {
  if (dbg) { print("============== setup_test: in"); }

  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(4444);

  channel = setupChannel(testpath);

  channel.setRequestHeader("ReplaceMe", "initial value", true);
  var setOK = channel.getRequestHeader("ReplaceMe");
  do_check_eq(setOK, "initial value");
  channel.setRequestHeader("ReplaceMe", "replaced", false);
  setOK = channel.getRequestHeader("ReplaceMe");
  do_check_eq(setOK, "replaced");

  channel.setRequestHeader("MergeMe", "foo1", true);
  channel.setRequestHeader("MergeMe", "foo2", true);
  channel.setRequestHeader("MergeMe", "foo3", true);
  setOK = channel.getRequestHeader("MergeMe");
  do_check_eq(setOK, "foo1, foo2, foo3");

  var uri = ios.newURI("http://foo1.invalid:80", null, null);
  channel.referrer = uri;
  do_check_true(channel.referrer.equals(uri));
  setOK = channel.getRequestHeader("Referer");
  do_check_eq(setOK, "http://foo1.invalid/");

  uri = ios.newURI("http://foo2.invalid:90/bar", null, null);
  channel.referrer = uri;
  setOK = channel.getRequestHeader("Referer");
  do_check_eq(setOK, "http://foo2.invalid:90/bar");

  // ChannelListener defined in head_channels.js
  channel.asyncOpen(new ChannelListener(checkRequestResponse, channel), null);

  if (dbg) { print("============== setup_test: out"); }
}

function setupChannel(path) {
  ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var chan = ios.newChannel("http://localhost:4444" + path, "", null);
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.requestMethod = "GET";
  return chan;
}

function serverHandler(metadata, response) {
  if (dbg) { print("============== serverHandler: in"); }

  var setOK = metadata.getHeader("ReplaceMe");
  do_check_eq(setOK, "replaced");
  setOK = metadata.getHeader("MergeMe");
  do_check_eq(setOK, "foo1, foo2, foo3");
  setOK = metadata.getHeader("Referer");
  do_check_eq(setOK, "http://foo2.invalid:90/bar");

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

  if (dbg) { print("============== serverHandler: out"); }
}

function checkRequestResponse(request, data, context) {
  if (dbg) { print("============== checkRequestResponse: in"); }

  do_check_eq(channel.responseStatus, 200);
  do_check_eq(channel.responseStatusText, "OK");
  do_check_true(channel.requestSucceeded);

  var response = channel.getResponseHeader("httpdMerge");
  do_check_eq(response, "bar1,bar2,bar3");
  channel.setResponseHeader("httpdMerge", "bar", true);
  do_check_eq(channel.getResponseHeader("httpdMerge"), "bar1,bar2,bar3, bar");

  response = channel.getResponseHeader("Proxy-Authenticate");
  do_check_eq(response, "line 1\nline 2\nline 3");

  channel.contentCharset = "UTF-8";
  do_check_eq(channel.contentCharset, "UTF-8");
  do_check_eq(channel.contentType, "text/plain");
  do_check_eq(channel.contentLength, httpbody.length);
  do_check_eq(data, httpbody);

  httpserver.stop(do_test_finished);
  if (dbg) { print("============== checkRequestResponse: out"); }
}
