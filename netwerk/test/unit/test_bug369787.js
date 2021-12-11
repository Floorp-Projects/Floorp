"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

const BUGID = "369787";
var server = null;
var channel = null;

function change_content_type() {
  var origType = channel.contentType;
  const newType = "x-foo/x-bar";
  channel.contentType = newType;
  Assert.equal(channel.contentType, newType);
  channel.contentType = origType;
  Assert.equal(channel.contentType, origType);
}

function TestListener() {}
TestListener.prototype.onStartRequest = function(request) {
  try {
    // request might be different from channel
    channel = request.QueryInterface(Ci.nsIChannel);

    change_content_type();
  } catch (ex) {
    print(ex);
    throw ex;
  }
};
TestListener.prototype.onStopRequest = function(request, status) {
  try {
    change_content_type();
  } catch (ex) {
    print(ex);
    // don't re-throw ex to avoid hanging the test
  }

  do_timeout(0, after_channel_closed);
};

function after_channel_closed() {
  try {
    change_content_type();
  } finally {
    server.stop(do_test_finished);
  }
}

function run_test() {
  // start server
  server = new HttpServer();

  server.registerPathHandler("/bug" + BUGID, bug369787);

  server.start(-1);

  // make request
  channel = NetUtil.newChannel({
    uri: "http://localhost:" + server.identity.primaryPort + "/bug" + BUGID,
    loadUsingSystemPrincipal: true,
  });
  channel.QueryInterface(Ci.nsIHttpChannel);
  channel.asyncOpen(new TestListener());

  do_test_pending();
}

// PATH HANDLER FOR /bug369787
function bug369787(metadata, response) {
  /* do nothing */
}
