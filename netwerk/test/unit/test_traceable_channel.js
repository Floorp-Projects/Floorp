// Test nsITraceableChannel interface.
// Replace original listener with TracingListener that modifies body of HTTP
// response. Make sure that body received by original channel's listener
// is correctly modified.

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

var httpserver = new HttpServer();
httpserver.start(-1);
const PORT = httpserver.identity.primaryPort;

var pipe = null;
var streamSink = null;

var originalBody = "original http response body";
var gotOnStartRequest = false;

function TracingListener() {}

TracingListener.prototype = {
  onStartRequest: function(request) {
    dump("*** tracing listener onStartRequest\n");

    gotOnStartRequest = true;

    request.QueryInterface(Ci.nsIHttpChannelInternal);

// local/remote addresses broken in e10s: disable for now
    Assert.equal(request.localAddress, "127.0.0.1");
    Assert.equal(request.localPort > 0, true);
    Assert.notEqual(request.localPort, PORT);
    Assert.equal(request.remoteAddress, "127.0.0.1");
    Assert.equal(request.remotePort, PORT);

    // Make sure listener can't be replaced after OnStartRequest was called.
    request.QueryInterface(Ci.nsITraceableChannel);
    try {
      var newListener = new TracingListener();
      newListener.listener = request.setNewListener(newListener);
    } catch(e) {
      dump("TracingListener.onStartRequest swallowing exception: " + e + "\n");
      return; // OK
    }
    do_throw("replaced channel's listener during onStartRequest.");
  },

  onStopRequest: function(request, statusCode) {
    dump("*** tracing listener onStopRequest\n");
    
    Assert.equal(gotOnStartRequest, true);

    try {
      var sin = Cc["@mozilla.org/scriptableinputstream;1"].
          createInstance(Ci.nsIScriptableInputStream);

      streamSink.close();
      var input = pipe.inputStream;
      sin.init(input);
      Assert.equal(sin.available(), originalBody.length);
    
      var result = sin.read(originalBody.length);
      Assert.equal(result, originalBody);
    
      input.close();
    } catch (e) {
      dump("TracingListener.onStopRequest swallowing exception: " + e + "\n");
    } finally {
      httpserver.stop(do_test_finished);
    }
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIRequestObserver) ||
        iid.equals(Ci.nsISupports)
        )
      return this;
    throw Cr.NS_NOINTERFACE;
  },

  listener: null
}


function HttpResponseExaminer() {}

HttpResponseExaminer.prototype = {
  register: function() {
    Cc["@mozilla.org/observer-service;1"].
      getService(Ci.nsIObserverService).
      addObserver(this, "http-on-examine-response", true);
    dump("Did HttpResponseExaminer.register\n");
  },

  // Replace channel's listener.
  observe: function(subject, topic, data) {
    dump("In HttpResponseExaminer.observe\n");
    try {
      subject.QueryInterface(Ci.nsITraceableChannel);
      
      var tee = Cc["@mozilla.org/network/stream-listener-tee;1"].
          createInstance(Ci.nsIStreamListenerTee);
      var newListener = new TracingListener();
      pipe = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);
      pipe.init(false, false, 0, 0xffffffff, null);
      streamSink = pipe.outputStream;
      
      var originalListener = subject.setNewListener(tee);
      tee.init(originalListener, streamSink, newListener);
    } catch(e) {
      do_throw("can't replace listener " + e);
    }
    dump("Did HttpResponseExaminer.observe\n");
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIObserver) ||
        iid.equals(Ci.nsISupportsWeakReference) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_NOINTERFACE;
  }
}

function test_handler(metadata, response) {
  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.bodyOutputStream.write(originalBody, originalBody.length);
}

function make_channel(url) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true})
                .QueryInterface(Ci.nsIHttpChannel);
}

// Check if received body is correctly modified.
function channel_finished(request, input, ctx) {
  httpserver.stop(do_test_finished);
}

function run_test() {
  var observer = new HttpResponseExaminer();
  observer.register();

  httpserver.registerPathHandler("/testdir", test_handler);

  var channel = make_channel("http://localhost:" + PORT + "/testdir");
  channel.asyncOpen(new ChannelListener(channel_finished));
  do_test_pending();
}
