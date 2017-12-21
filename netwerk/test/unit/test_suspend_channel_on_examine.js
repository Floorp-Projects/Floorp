// This file tests async handling of a channel suspended in http-on-modify-request.

var CC = Components.Constructor;

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

var obs = Cc["@mozilla.org/observer-service;1"]
            .getService(Ci.nsIObserverService);

var baseUrl;

function responseHandler(metadata, response)
{
  var text = "testing";
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Set-Cookie", "chewy", false);
  response.bodyOutputStream.write(text, text.length);
}

function onExamineListener(callback) {
  obs.addObserver({
    observe: function(subject, topic, data) {
      var obs = Cc["@mozilla.org/observer-service;1"].getService();
      obs = obs.QueryInterface(Ci.nsIObserverService);
      obs.removeObserver(this, "http-on-examine-response");
      callback(subject.QueryInterface(Ci.nsIHttpChannel));
    }
  }, "http-on-examine-response");
}

function startChannelRequest(baseUrl, flags, callback) {
  var chan = NetUtil.newChannel({
    uri: baseUrl,
    loadUsingSystemPrincipal: true
  });
  chan.asyncOpen2(new ChannelListener(callback, null, flags));
}

// We first make a request that we'll cancel asynchronously.  The response will
// still contain the set-cookie header. Then verify the cookie was not actually
// retained.
add_test(function testAsyncCancel() {
  onExamineListener(chan => {
    // Suspend the channel then yield to make this async.
    chan.suspend();
    Promise.resolve().then(() => {
      chan.cancel(Cr.NS_BINDING_ABORTED);
      chan.resume();
    });
  });
  startChannelRequest(baseUrl, CL_EXPECT_FAILURE, (request, data, context) => {
    Assert.ok(!!!data, "no response");

    var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
    Assert.equal(cm.countCookiesFromHost("localhost"), 0, "no cookies set");

    do_execute_soon(run_next_test);
  });
});

function run_test() {
  var httpServer = new HttpServer();
  httpServer.registerPathHandler("/", responseHandler);
  httpServer.start(-1);

  baseUrl = `http://localhost:${httpServer.identity.primaryPort}`;

  run_next_test();

  do_register_cleanup(function(){
    httpServer.stop(() => {});
  });
}
