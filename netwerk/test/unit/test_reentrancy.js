const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

Cu.importGlobalProperties(["XMLHttpRequest"]);

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = new HttpServer();
var testpath = "/simple";
var httpbody = "<?xml version='1.0' ?><root>0123456789</root>";

function syncXHR()
{
  var xhr = new XMLHttpRequest();
  xhr.open("GET", URL + testpath, false);
  xhr.send(null);    
}

const MAX_TESTS = 2;

var listener = {
  _done_onStart: false,
  _done_onData: false,
  _test: 0,

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsIRequestObserver) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  onStartRequest: function(request) {
    switch(this._test) {
      case 0:
        request.suspend();
        syncXHR();
        request.resume();
        break;
      case 1:
        request.suspend();
        syncXHR();
        executeSoon(function() { request.resume(); });
        break;
      case 2:
        executeSoon(function() { request.suspend(); });
        executeSoon(function() { request.resume(); });
        syncXHR();
        break;
    }
    
    this._done_onStart = true;
  },

  onDataAvailable: function(request, stream, offset, count) {
    Assert.ok(this._done_onStart);
    read_stream(stream, count);
    this._done_onData = true;
  },

  onStopRequest: function(request, status) {
    Assert.ok(this._done_onData);
    this._reset();
    if (this._test <= MAX_TESTS)
      next_test();
    else
      httpserver.stop(do_test_finished);      
  },

  _reset: function() {
    this._done_onStart = false;
    this._done_onData = false;
    this._test++;
  }
};

function makeChan(url) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true})
                .QueryInterface(Ci.nsIHttpChannel);
}

function next_test()
{
  var chan = makeChan(URL + testpath);
  chan.QueryInterface(Ci.nsIRequest);
  chan.asyncOpen(listener);
}

function run_test()
{
  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(-1);

  next_test();

  do_test_pending();
}

function serverHandler(metadata, response)
{
  response.setHeader("Content-Type", "text/xml", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
}
