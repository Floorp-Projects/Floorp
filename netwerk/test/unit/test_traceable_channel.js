// Test nsITraceableChannel interface.
// Replace original listener with TracingListener that modifies body of HTTP
// response. Make sure that body received by original channel's listener
// is correctly modified.

do_load_httpd_js();

var httpserver = null;
var pipe = null;
var streamSink = null;

var originalBody = "original http response body";
var gotOnStartRequest = false;

function TracingListener() {}

TracingListener.prototype = {
  onStartRequest: function(request, context) {
    dump("*** tracing listener onStartRequest\n");

    gotOnStartRequest = true;

    // Make sure listener can't be replaced after OnStartRequest was called.
    request.QueryInterface(Components.interfaces.nsITraceableChannel);
    try {
      var newListener = new TracingListener();
      newListener.listener = request.setNewListener(newListener);
    } catch(e) {
      dump("TracingListener.onStartRequest swallowing exception: " + e + "\n");
      return; // OK
    }
    do_throw("replaced channel's listener during onStartRequest.");
  },

  onStopRequest: function(request, context, statusCode) {
    dump("*** tracing listener onStopRequest\n");
    
    do_check_eq(gotOnStartRequest, true);

    try {
      var sin = Components.classes["@mozilla.org/scriptableinputstream;1"].
          createInstance(Ci.nsIScriptableInputStream);

      streamSink.close();
      var input = pipe.inputStream;
      sin.init(input);
      do_check_eq(sin.available(), originalBody.length);
    
      var result = sin.read(originalBody.length);
      do_check_eq(result, originalBody);
    
      input.close();
    } catch (e) {
      dump("TracingListener.onStopRequest swallowing exception: " + e + "\n");
    } finally {
      httpserver.stop(do_test_finished);
    }
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIRequestObserver) ||
        iid.equals(Components.interfaces.nsISupports)
        )
      return this;
    throw Components.results.NS_NOINTERFACE;
  },

  listener: null
}


function HttpResponseExaminer() {}

HttpResponseExaminer.prototype = {
  register: function() {
    Cc["@mozilla.org/observer-service;1"].
      getService(Components.interfaces.nsIObserverService).
      addObserver(this, "http-on-examine-response", true);
    dump("Did HttpResponseExaminer.register\n");
  },

  // Replace channel's listener.
  observe: function(subject, topic, data) {
    dump("In HttpResponseExaminer.observe\n");
    try {
      subject.QueryInterface(Components.interfaces.nsITraceableChannel);
      
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
    if (iid.equals(Components.interfaces.nsIObserver) ||
        iid.equals(Components.interfaces.nsISupportsWeakReference) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  }
}

function test_handler(metadata, response) {
  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.bodyOutputStream.write(originalBody, originalBody.length);
}

function make_channel(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);
  return ios.newChannel(url, null, null).
    QueryInterface(Components.interfaces.nsIHttpChannel);
}

// Check if received body is correctly modified.
function channel_finished(request, input, ctx) {
}

function run_test() {
  var observer = new HttpResponseExaminer();
  observer.register();

  httpserver = new nsHttpServer();
  httpserver.registerPathHandler("/testdir", test_handler);
  httpserver.start(4444);

  var channel = make_channel("http://localhost:4444/testdir");
  channel.asyncOpen(new ChannelListener(channel_finished), null);
  do_test_pending();
}
