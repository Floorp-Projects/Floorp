//
//  HTTP headers test
//  Response headers can be changed after they have been received, e.g. empty
//  headers are deleted, some duplicate header are merged (if no error is
//  thrown), etc.
//
//  The "original header" is introduced to hold the header array in the order
//  and the form as they have been received from the network.
//  Here, the "original headers" are tested.
//

// Note: sets Cc and Ci variables

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = new HttpServer();
var testpath = "/simple";
var httpbody = "0123456789";
var channel;
var ios;

var dbg=1
if (dbg) { print("============== START =========="); }

function run_test() {
  setup_test();
  do_test_pending();
}

function setup_test() {
  if (dbg) { print("============== setup_test: in"); }

  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(-1);

  channel = setupChannel(testpath);

  // ChannelListener defined in head_channels.js
  channel.asyncOpen(new ChannelListener(checkResponse, channel), null);

  if (dbg) { print("============== setup_test: out"); }
}

function setupChannel(path) {
  var chan = NetUtil.newChannel ({
    uri: URL + path,
    loadUsingSystemPrincipal: true
  }).QueryInterface(Components.interfaces.nsIHttpChannel);
  chan.requestMethod = "GET";
  return chan;
}

function serverHandler(metadata, response) {
  if (dbg) { print("============== serverHandler: in"); }

  response.setHeader("Content-Type", "text/plain", false);
  response.setStatusLine("1.1", 200, "OK");

  // Set a empty header. A empty link header will not appear in header list,
  // but in the "original headers", it will be still exactly as received.
  response.setHeaderNoCheck("Link", "", true);
  response.setHeaderNoCheck("Link", "value1");
  response.setHeaderNoCheck("Link", "value2");
  response.setHeaderNoCheck("Location", "loc");
  response.bodyOutputStream.write(httpbody, httpbody.length);

  if (dbg) { print("============== serverHandler: out"); }
}

function checkResponse(request, data, context) {
  if (dbg) { print("============== checkResponse: in"); }

  do_check_eq(channel.responseStatus, 200);
  do_check_eq(channel.responseStatusText, "OK");
  do_check_true(channel.requestSucceeded);

  // Response header have only one link header.
  var linkHeaderFound = 0;
  var locationHeaderFound = 0;
  channel.visitResponseHeaders({
    visitHeader: function visit(aName, aValue) {
      if (aName == "Link") {
        linkHeaderFound++;
        do_check_eq(aValue, "value1, value2");
      }
      if (aName == "Location") {
        locationHeaderFound++;
        do_check_eq(aValue, "loc");
      }
    }
  });
  do_check_eq(linkHeaderFound, 1);
  do_check_eq(locationHeaderFound, 1);

  // The "original header" still contains 3 link headers.
  var linkOrgHeaderFound = 0;
  var locationOrgHeaderFound = 0;
  channel.visitOriginalResponseHeaders({
    visitHeader: function visitOrg(aName, aValue) {
      if (aName == "Link") {
        if (linkOrgHeaderFound == 0) {
          do_check_eq(aValue, "");
        } else if (linkOrgHeaderFound == 1 ) {
          do_check_eq(aValue, "value1");
        } else {
          do_check_eq(aValue, "value2");
        }
        linkOrgHeaderFound++;
      }
      if (aName == "Location") {
          locationOrgHeaderFound++;
          do_check_eq(aValue, "loc");
      }
    }
  });
  do_check_eq(linkOrgHeaderFound, 3);
  do_check_eq(locationOrgHeaderFound, 1);

  if (dbg) { print("============== Remove headers"); }
  // Remove header.
  channel.setResponseHeader("Link", "", false);
  channel.setResponseHeader("Location", "", false);

  var linkHeaderFound2 = false;
  var locationHeaderFound2 = 0;
  channel.visitResponseHeaders({
    visitHeader: function visit(aName, aValue) {
      if (aName == "Link") {
        linkHeaderFound2 = true;
      }
      if (aName == "Location") {
        locationHeaderFound2 = true;
      }
    }
  });
  do_check_false(linkHeaderFound2, "There should be no link header");
  do_check_false(locationHeaderFound2, "There should be no location headers.");

  // The "original header" still contains the empty header.
  var linkOrgHeaderFound2 = 0;
  var locationOrgHeaderFound2 = 0;
  channel.visitOriginalResponseHeaders({
    visitHeader: function visitOrg(aName, aValue) {
      if (aName == "Link") {
        if (linkOrgHeaderFound2 == 0) {
          do_check_eq(aValue, "");
        } else if (linkOrgHeaderFound2 == 1 ) {
          do_check_eq(aValue, "value1");
        } else {
          do_check_eq(aValue, "value2");
        }
        linkOrgHeaderFound2++;
      }
      if (aName == "Location") {
        locationOrgHeaderFound2++;
        do_check_eq(aValue, "loc");
      }
    }
  });
  do_check_true(linkOrgHeaderFound2 == 3,
                "Original link header still here.");
  do_check_true(locationOrgHeaderFound2 == 1,
                "Original location header still here.");

  if (dbg) { print("============== Test GetResponseHeader"); }
  var linkOrgHeaderFound3 = 0;
  channel.getOriginalResponseHeader("Link",{
    visitHeader: function visitOrg(aName, aValue) {
      if (linkOrgHeaderFound3 == 0) {
        do_check_eq(aValue, "");
      } else if (linkOrgHeaderFound3 == 1 ) {
        do_check_eq(aValue, "value1");
      } else {
        do_check_eq(aValue, "value2");
      }
      linkOrgHeaderFound3++;
    }
  });
  do_check_true(linkOrgHeaderFound2 == 3, 
                "Original link header still here.");

  httpserver.stop(do_test_finished);
  if (dbg) { print("============== checkResponse: out"); }
}
