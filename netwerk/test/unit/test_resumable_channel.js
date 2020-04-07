/* Tests various aspects of nsIResumableChannel in combination with HTTP */
"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = null;

const NS_ERROR_ENTITY_CHANGED = 0x804b0020;
const NS_ERROR_NOT_RESUMABLE = 0x804b0019;

const rangeBody = "Body of the range request handler.\r\n";

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

function AuthPrompt2() {}

AuthPrompt2.prototype = {
  user: "guest",
  pass: "guest",

  QueryInterface: ChromeUtils.generateQI(["nsIAuthPrompt2"]),

  promptAuth: function ap2_promptAuth(channel, level, authInfo) {
    authInfo.username = this.user;
    authInfo.password = this.pass;
    return true;
  },

  asyncPromptAuth: function ap2_async(chan, cb, ctx, lvl, info) {
    throw 0x80004001;
  },
};

function Requestor() {}

Requestor.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIInterfaceRequestor"]),

  getInterface: function requestor_gi(iid) {
    if (iid.equals(Ci.nsIAuthPrompt2)) {
      // Allow the prompt to store state by caching it here
      if (!this.prompt2) {
        this.prompt2 = new AuthPrompt2();
      }
      return this.prompt2;
    }

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  prompt2: null,
};

function run_test() {
  dump("*** run_test\n");
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/auth", authHandler);
  httpserver.registerPathHandler("/range", rangeHandler);
  httpserver.registerPathHandler("/acceptranges", acceptRangesHandler);
  httpserver.registerPathHandler("/redir", redirHandler);

  var entityID;

  function get_entity_id(request, data, ctx) {
    dump("*** get_entity_id()\n");
    Assert.ok(
      request instanceof Ci.nsIResumableChannel,
      "must be a resumable channel"
    );
    entityID = request.entityID;
    dump("*** entity id = " + entityID + "\n");

    // Try a non-resumable URL (responds with 200)
    var chan = make_channel(URL);
    chan.nsIResumableChannel.resumeAt(1, entityID);
    chan.asyncOpen(new ChannelListener(try_resume, null, CL_EXPECT_FAILURE));
  }

  function try_resume(request, data, ctx) {
    dump("*** try_resume()\n");
    Assert.equal(request.status, NS_ERROR_NOT_RESUMABLE);

    // Try a successful resume
    var chan = make_channel(URL + "/range");
    chan.nsIResumableChannel.resumeAt(1, entityID);
    chan.asyncOpen(new ChannelListener(try_resume_zero, null));
  }

  function try_resume_zero(request, data, ctx) {
    dump("*** try_resume_zero()\n");
    Assert.ok(request.nsIHttpChannel.requestSucceeded);
    Assert.equal(data, rangeBody.substring(1));

    // Try a server which doesn't support range requests
    var chan = make_channel(URL + "/acceptranges");
    chan.nsIResumableChannel.resumeAt(0, entityID);
    chan.nsIHttpChannel.setRequestHeader("X-Range-Type", "none", false);
    chan.asyncOpen(new ChannelListener(try_no_range, null, CL_EXPECT_FAILURE));
  }

  function try_no_range(request, data, ctx) {
    dump("*** try_no_range()\n");
    Assert.ok(request.nsIHttpChannel.requestSucceeded);
    Assert.equal(request.status, NS_ERROR_NOT_RESUMABLE);

    // Try a server which supports "bytes" range requests
    var chan = make_channel(URL + "/acceptranges");
    chan.nsIResumableChannel.resumeAt(0, entityID);
    chan.nsIHttpChannel.setRequestHeader("X-Range-Type", "bytes", false);
    chan.asyncOpen(new ChannelListener(try_bytes_range, null));
  }

  function try_bytes_range(request, data, ctx) {
    dump("*** try_bytes_range()\n");
    Assert.ok(request.nsIHttpChannel.requestSucceeded);
    Assert.equal(data, rangeBody);

    // Try a server which supports "foo" and "bar" range requests
    var chan = make_channel(URL + "/acceptranges");
    chan.nsIResumableChannel.resumeAt(0, entityID);
    chan.nsIHttpChannel.setRequestHeader("X-Range-Type", "foo, bar", false);
    chan.asyncOpen(
      new ChannelListener(try_foo_bar_range, null, CL_EXPECT_FAILURE)
    );
  }

  function try_foo_bar_range(request, data, ctx) {
    dump("*** try_foo_bar_range()\n");
    Assert.ok(request.nsIHttpChannel.requestSucceeded);
    Assert.equal(request.status, NS_ERROR_NOT_RESUMABLE);

    // Try a server which supports "foobar" range requests
    var chan = make_channel(URL + "/acceptranges");
    chan.nsIResumableChannel.resumeAt(0, entityID);
    chan.nsIHttpChannel.setRequestHeader("X-Range-Type", "foobar", false);
    chan.asyncOpen(
      new ChannelListener(try_foobar_range, null, CL_EXPECT_FAILURE)
    );
  }

  function try_foobar_range(request, data, ctx) {
    dump("*** try_foobar_range()\n");
    Assert.ok(request.nsIHttpChannel.requestSucceeded);
    Assert.equal(request.status, NS_ERROR_NOT_RESUMABLE);

    // Try a server which supports "bytes" and "foobar" range requests
    var chan = make_channel(URL + "/acceptranges");
    chan.nsIResumableChannel.resumeAt(0, entityID);
    chan.nsIHttpChannel.setRequestHeader(
      "X-Range-Type",
      "bytes, foobar",
      false
    );
    chan.asyncOpen(new ChannelListener(try_bytes_foobar_range, null));
  }

  function try_bytes_foobar_range(request, data, ctx) {
    dump("*** try_bytes_foobar_range()\n");
    Assert.ok(request.nsIHttpChannel.requestSucceeded);
    Assert.equal(data, rangeBody);

    // Try a server which supports "bytesfoo" and "bar" range requests
    var chan = make_channel(URL + "/acceptranges");
    chan.nsIResumableChannel.resumeAt(0, entityID);
    chan.nsIHttpChannel.setRequestHeader(
      "X-Range-Type",
      "bytesfoo, bar",
      false
    );
    chan.asyncOpen(
      new ChannelListener(try_bytesfoo_bar_range, null, CL_EXPECT_FAILURE)
    );
  }

  function try_bytesfoo_bar_range(request, data, ctx) {
    dump("*** try_bytesfoo_bar_range()\n");
    Assert.ok(request.nsIHttpChannel.requestSucceeded);
    Assert.equal(request.status, NS_ERROR_NOT_RESUMABLE);

    // Try a server which doesn't send Accept-Ranges header at all
    var chan = make_channel(URL + "/acceptranges");
    chan.nsIResumableChannel.resumeAt(0, entityID);
    chan.asyncOpen(new ChannelListener(try_no_accept_ranges, null));
  }

  function try_no_accept_ranges(request, data, ctx) {
    dump("*** try_no_accept_ranges()\n");
    Assert.ok(request.nsIHttpChannel.requestSucceeded);
    Assert.equal(data, rangeBody);

    // Try a successful suspend/resume from 0
    var chan = make_channel(URL + "/range");
    chan.nsIResumableChannel.resumeAt(0, entityID);
    chan.asyncOpen(
      new ChannelListener(
        try_suspend_resume,
        null,
        CL_SUSPEND | CL_EXPECT_3S_DELAY
      )
    );
  }

  function try_suspend_resume(request, data, ctx) {
    dump("*** try_suspend_resume()\n");
    Assert.ok(request.nsIHttpChannel.requestSucceeded);
    Assert.equal(data, rangeBody);

    // Try a successful resume from 0
    var chan = make_channel(URL + "/range");
    chan.nsIResumableChannel.resumeAt(0, entityID);
    chan.asyncOpen(new ChannelListener(success, null));
  }

  function success(request, data, ctx) {
    dump("*** success()\n");
    Assert.ok(request.nsIHttpChannel.requestSucceeded);
    Assert.equal(data, rangeBody);

    // Authentication (no password; working resume)
    // (should not give us any data)
    var chan = make_channel(URL + "/range");
    chan.nsIResumableChannel.resumeAt(1, entityID);
    chan.nsIHttpChannel.setRequestHeader("X-Need-Auth", "true", false);
    chan.asyncOpen(
      new ChannelListener(test_auth_nopw, null, CL_EXPECT_FAILURE)
    );
  }

  function test_auth_nopw(request, data, ctx) {
    dump("*** test_auth_nopw()\n");
    Assert.ok(!request.nsIHttpChannel.requestSucceeded);
    Assert.equal(request.status, NS_ERROR_ENTITY_CHANGED);

    // Authentication + not working resume
    var chan = make_channel(
      "http://guest:guest@localhost:" +
        httpserver.identity.primaryPort +
        "/auth"
    );
    chan.nsIResumableChannel.resumeAt(1, entityID);
    chan.notificationCallbacks = new Requestor();
    chan.asyncOpen(new ChannelListener(test_auth, null, CL_EXPECT_FAILURE));
  }
  function test_auth(request, data, ctx) {
    dump("*** test_auth()\n");
    Assert.equal(request.status, NS_ERROR_NOT_RESUMABLE);
    Assert.ok(request.nsIHttpChannel.responseStatus < 300);

    // Authentication + working resume
    var chan = make_channel(
      "http://guest:guest@localhost:" +
        httpserver.identity.primaryPort +
        "/range"
    );
    chan.nsIResumableChannel.resumeAt(1, entityID);
    chan.notificationCallbacks = new Requestor();
    chan.nsIHttpChannel.setRequestHeader("X-Need-Auth", "true", false);
    chan.asyncOpen(new ChannelListener(test_auth_resume, null));
  }

  function test_auth_resume(request, data, ctx) {
    dump("*** test_auth_resume()\n");
    Assert.equal(data, rangeBody.substring(1));
    Assert.ok(request.nsIHttpChannel.requestSucceeded);

    // 404 page (same content length as real content)
    var chan = make_channel(URL + "/range");
    chan.nsIResumableChannel.resumeAt(1, entityID);
    chan.nsIHttpChannel.setRequestHeader("X-Want-404", "true", false);
    chan.asyncOpen(new ChannelListener(test_404, null, CL_EXPECT_FAILURE));
  }

  function test_404(request, data, ctx) {
    dump("*** test_404()\n");
    Assert.equal(request.status, NS_ERROR_ENTITY_CHANGED);
    Assert.equal(request.nsIHttpChannel.responseStatus, 404);

    // 416 Requested Range Not Satisfiable
    var chan = make_channel(URL + "/range");
    chan.nsIResumableChannel.resumeAt(1000, entityID);
    chan.asyncOpen(new ChannelListener(test_416, null, CL_EXPECT_FAILURE));
  }

  function test_416(request, data, ctx) {
    dump("*** test_416()\n");
    Assert.equal(request.status, NS_ERROR_ENTITY_CHANGED);
    Assert.equal(request.nsIHttpChannel.responseStatus, 416);

    // Redirect + successful resume
    var chan = make_channel(URL + "/redir");
    chan.nsIHttpChannel.setRequestHeader("X-Redir-To", URL + "/range", false);
    chan.nsIResumableChannel.resumeAt(1, entityID);
    chan.asyncOpen(new ChannelListener(test_redir_resume, null));
  }

  function test_redir_resume(request, data, ctx) {
    dump("*** test_redir_resume()\n");
    Assert.ok(request.nsIHttpChannel.requestSucceeded);
    Assert.equal(data, rangeBody.substring(1));
    Assert.equal(request.nsIHttpChannel.responseStatus, 206);

    // Redirect + failed resume
    var chan = make_channel(URL + "/redir");
    chan.nsIHttpChannel.setRequestHeader("X-Redir-To", URL + "/", false);
    chan.nsIResumableChannel.resumeAt(1, entityID);
    chan.asyncOpen(
      new ChannelListener(test_redir_noresume, null, CL_EXPECT_FAILURE)
    );
  }

  function test_redir_noresume(request, data, ctx) {
    dump("*** test_redir_noresume()\n");
    Assert.equal(request.status, NS_ERROR_NOT_RESUMABLE);

    httpserver.stop(do_test_finished);
  }

  httpserver.start(-1);
  var chan = make_channel(URL + "/range");
  chan.asyncOpen(new ChannelListener(get_entity_id, null));
  do_test_pending();
}

// HANDLERS

function handleAuth(metadata, response) {
  // btoa("guest:guest"), but that function is not available here
  var expectedHeader = "Basic Z3Vlc3Q6Z3Vlc3Q=";

  var body;
  if (
    metadata.hasHeader("Authorization") &&
    metadata.getHeader("Authorization") == expectedHeader
  ) {
    response.setStatusLine(metadata.httpVersion, 200, "OK, authorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);

    return true;
  }
  // didn't know guest:guest, failure
  response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
  response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);
  return false;
}

// /auth
function authHandler(metadata, response) {
  response.setHeader("Content-Type", "text/html", false);
  var body = handleAuth(metadata, response) ? "success" : "failure";
  response.bodyOutputStream.write(body, body.length);
}

// /range
function rangeHandler(metadata, response) {
  response.setHeader("Content-Type", "text/html", false);

  if (metadata.hasHeader("X-Need-Auth")) {
    if (!handleAuth(metadata, response)) {
      body = "auth failed";
      response.bodyOutputStream.write(body, body.length);
      return;
    }
  }

  if (metadata.hasHeader("X-Want-404")) {
    response.setStatusLine(metadata.httpVersion, 404, "Not Found");
    body = rangeBody;
    response.bodyOutputStream.write(body, body.length);
    return;
  }

  var body = rangeBody;

  if (metadata.hasHeader("Range")) {
    // Syntax: bytes=[from]-[to] (we don't support multiple ranges)
    var matches = metadata
      .getHeader("Range")
      .match(/^\s*bytes=(\d+)?-(\d+)?\s*$/);
    var from = matches[1] === undefined ? 0 : matches[1];
    var to = matches[2] === undefined ? rangeBody.length - 1 : matches[2];
    if (from >= rangeBody.length) {
      response.setStatusLine(metadata.httpVersion, 416, "Start pos too high");
      response.setHeader("Content-Range", "*/" + rangeBody.length, false);
      return;
    }
    body = body.substring(from, to + 1);
    // always respond to successful range requests with 206
    response.setStatusLine(metadata.httpVersion, 206, "Partial Content");
    response.setHeader(
      "Content-Range",
      from + "-" + to + "/" + rangeBody.length,
      false
    );
  }

  response.bodyOutputStream.write(body, body.length);
}

// /acceptranges
function acceptRangesHandler(metadata, response) {
  response.setHeader("Content-Type", "text/html", false);
  if (metadata.hasHeader("X-Range-Type")) {
    response.setHeader(
      "Accept-Ranges",
      metadata.getHeader("X-Range-Type"),
      false
    );
  }
  response.bodyOutputStream.write(rangeBody, rangeBody.length);
}

// /redir
function redirHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 302, "Found");
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Location", metadata.getHeader("X-Redir-To"), false);
  var body = "redirect\r\n";
  response.bodyOutputStream.write(body, body.length);
}
