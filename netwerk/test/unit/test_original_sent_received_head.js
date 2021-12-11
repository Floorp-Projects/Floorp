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
//  Original headers will be stored in the cache as well. This test checks
//  that too.

// Note: sets Cc and Ci variables

"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = new HttpServer();
var testpath = "/simple";
var httpbody = "0123456789";

var dbg = 1;

function run_test() {
  if (dbg) {
    print("============== START ==========");
  }

  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(-1);
  run_next_test();
}

add_test(function test_headerChange() {
  if (dbg) {
    print("============== test_headerChange setup: in");
  }

  var channel1 = setupChannel(testpath);
  channel1.loadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE;

  // ChannelListener defined in head_channels.js
  channel1.asyncOpen(new ChannelListener(checkResponse, null));

  if (dbg) {
    print("============== test_headerChange setup: out");
  }
});

add_test(function test_fromCache() {
  if (dbg) {
    print("============== test_fromCache setup: in");
  }

  var channel2 = setupChannel(testpath);
  channel2.loadFlags = Ci.nsIRequest.LOAD_FROM_CACHE;

  // ChannelListener defined in head_channels.js
  channel2.asyncOpen(new ChannelListener(checkResponse, null));

  if (dbg) {
    print("============== test_fromCache setup: out");
  }
});

add_test(function finish() {
  if (dbg) {
    print("============== STOP ==========");
  }
  httpserver.stop(do_test_finished);
});

function setupChannel(path) {
  var chan = NetUtil.newChannel({
    uri: URL + path,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.requestMethod = "GET";
  return chan;
}

function serverHandler(metadata, response) {
  if (dbg) {
    print("============== serverHandler: in");
  }

  try {
    var etag = metadata.getHeader("If-None-Match");
  } catch (ex) {
    var etag = "";
  }
  if (etag == "testtag") {
    if (dbg) {
      print("============== 304 answerr: in");
    }
    response.setStatusLine("1.1", 304, "Not Modified");
  } else {
    response.setHeader("Content-Type", "text/plain", false);
    response.setStatusLine("1.1", 200, "OK");

    // Set a empty header. A empty link header will not appear in header list,
    // but in the "original headers", it will be still exactly as received.
    response.setHeaderNoCheck("Link", "", true);
    response.setHeaderNoCheck("Link", "value1");
    response.setHeaderNoCheck("Link", "value2");
    response.setHeaderNoCheck("Location", "loc");
    response.setHeader("Cache-Control", "max-age=10000", false);
    response.setHeader("ETag", "testtag", false);
    response.bodyOutputStream.write(httpbody, httpbody.length);
  }
  if (dbg) {
    print("============== serverHandler: out");
  }
}

function checkResponse(request, data, context) {
  if (dbg) {
    print("============== checkResponse: in");
  }

  request.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(request.responseStatus, 200);
  Assert.equal(request.responseStatusText, "OK");
  Assert.ok(request.requestSucceeded);

  // Response header have only one link header.
  var linkHeaderFound = 0;
  var locationHeaderFound = 0;
  request.visitResponseHeaders({
    visitHeader: function visit(aName, aValue) {
      if (aName == "link") {
        linkHeaderFound++;
        Assert.equal(aValue, "value1, value2");
      }
      if (aName == "location") {
        locationHeaderFound++;
        Assert.equal(aValue, "loc");
      }
    },
  });
  Assert.equal(linkHeaderFound, 1);
  Assert.equal(locationHeaderFound, 1);

  // The "original header" still contains 3 link headers.
  var linkOrgHeaderFound = 0;
  var locationOrgHeaderFound = 0;
  request.visitOriginalResponseHeaders({
    visitHeader: function visitOrg(aName, aValue) {
      if (aName == "link") {
        if (linkOrgHeaderFound == 0) {
          Assert.equal(aValue, "");
        } else if (linkOrgHeaderFound == 1) {
          Assert.equal(aValue, "value1");
        } else {
          Assert.equal(aValue, "value2");
        }
        linkOrgHeaderFound++;
      }
      if (aName == "location") {
        locationOrgHeaderFound++;
        Assert.equal(aValue, "loc");
      }
    },
  });
  Assert.equal(linkOrgHeaderFound, 3);
  Assert.equal(locationOrgHeaderFound, 1);

  if (dbg) {
    print("============== Remove headers");
  }
  // Remove header.
  request.setResponseHeader("Link", "", false);
  request.setResponseHeader("Location", "", false);

  var linkHeaderFound2 = false;
  var locationHeaderFound2 = 0;
  request.visitResponseHeaders({
    visitHeader: function visit(aName, aValue) {
      if (aName == "Link") {
        linkHeaderFound2 = true;
      }
      if (aName == "Location") {
        locationHeaderFound2 = true;
      }
    },
  });
  Assert.ok(!linkHeaderFound2, "There should be no link header");
  Assert.ok(!locationHeaderFound2, "There should be no location headers.");

  // The "original header" still contains the empty header.
  var linkOrgHeaderFound2 = 0;
  var locationOrgHeaderFound2 = 0;
  request.visitOriginalResponseHeaders({
    visitHeader: function visitOrg(aName, aValue) {
      if (aName == "link") {
        if (linkOrgHeaderFound2 == 0) {
          Assert.equal(aValue, "");
        } else if (linkOrgHeaderFound2 == 1) {
          Assert.equal(aValue, "value1");
        } else {
          Assert.equal(aValue, "value2");
        }
        linkOrgHeaderFound2++;
      }
      if (aName == "location") {
        locationOrgHeaderFound2++;
        Assert.equal(aValue, "loc");
      }
    },
  });
  Assert.ok(linkOrgHeaderFound2 == 3, "Original link header still here.");
  Assert.ok(
    locationOrgHeaderFound2 == 1,
    "Original location header still here."
  );

  if (dbg) {
    print("============== Test GetResponseHeader");
  }
  var linkOrgHeaderFound3 = 0;
  request.getOriginalResponseHeader("link", {
    visitHeader: function visitOrg(aName, aValue) {
      if (linkOrgHeaderFound3 == 0) {
        Assert.equal(aValue, "");
      } else if (linkOrgHeaderFound3 == 1) {
        Assert.equal(aValue, "value1");
      } else {
        Assert.equal(aValue, "value2");
      }
      linkOrgHeaderFound3++;
    },
  });
  Assert.ok(linkOrgHeaderFound2 == 3, "Original link header still here.");

  if (dbg) {
    print("============== checkResponse: out");
  }

  run_next_test();
}
