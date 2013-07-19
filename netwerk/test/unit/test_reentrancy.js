const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = new HttpServer();
var testpath = "/simple";
var httpbody = "<?xml version='1.0' ?><root>0123456789</root>";

function syncXHR()
{
  var xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
            .createInstance(Ci.nsIXMLHttpRequest);
  xhr.open("GET", URL + testpath, false);
  xhr.send(null);    
}

const MAX_TESTS = 2;

var listener = {
  _done_onStart: false,
  _done_onData: false,
  _test: 0,

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIStreamListener) ||
        iid.equals(Components.interfaces.nsIRequestObserver) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  onStartRequest: function(request, ctx) {
    switch(this._test) {
      case 0:
        request.suspend();
        syncXHR();
        request.resume();
        break;
      case 1:
        request.suspend();
        syncXHR();
        do_execute_soon(function() request.resume());
        break;
      case 2:
        do_execute_soon(function() request.suspend());
	do_execute_soon(function() request.resume());
        syncXHR();
        break;
    }
    
    this._done_onStart = true;
  },

  onDataAvailable: function(request, context, stream, offset, count) {
    do_check_true(this._done_onStart);
    read_stream(stream, count);
    this._done_onData = true;
  },

  onStopRequest: function(request, ctx, status) {
    do_check_true(this._done_onData);
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
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var chan = ios.newChannel(url, null, null).QueryInterface(Ci.nsIHttpChannel);
  return chan;
}

function next_test()
{
  var chan = makeChan(URL + testpath);
  chan.QueryInterface(Ci.nsIRequest);
  chan.asyncOpen(listener, null);
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
