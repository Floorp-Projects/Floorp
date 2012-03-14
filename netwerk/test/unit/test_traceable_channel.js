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

    request.QueryInterface(Components.interfaces.nsIHttpChannelInternal);

    var localAddr = "unknown";
    var localPort = "unknown";
    var remoteAddr = "unknown";
    var remotePort = "unknown";
    try {
      localAddr = request.localAddress;
      dump("got local address\n");
    } catch(e) {
      dump("couldn't get local address\n");
    }
    try {
      localPort = request.localPort;
      dump("got local port\n");
    } catch(e) {
      dump("couldn't get local port\n");
    }
    try {
      remoteAddr = request.remoteAddress;
      dump("got remote address\n");
    } catch(e) {
      dump("couldn't get remote address\n");
    }
    try {
      remotePort = request.remotePort;
      dump("got remote port\n");
    } catch(e) {
      dump("couldn't get remote port\n");
    }

    do_check_eq(localAddr, "127.0.0.1");
    do_check_eq(localPort > 0, true);
    do_check_eq(remoteAddr, "127.0.0.1");
    do_check_eq(remotePort, 4444);

    request.QueryInterface(Components.interfaces.nsISupportsPriority);
    request.priority = Ci.nsISupportsPriority.PRIORITY_LOW;

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
    }

    // we're the last OnStopRequest called by the nsIStreamListenerTee
    run_next_test();
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
  // No-op: since the nsIStreamListenerTee calls the observer's OnStopRequest
  // after this, we call run_next_test() there
}

// needs to be global or it'll go out of scope before it observes request
var observer = new HttpResponseExaminer();

var testRuns = 1;  // change this to >1 to run test multiple times 
var iteration = 1;

function run_next_test() {
  if (iteration > testRuns) {
    dump("Shutting down\n");
    httpserver.stop(do_test_finished);
    return;
  }
  if (iteration > 1) {
    dump("^^^ test iteration=" + iteration + "\n");
  }
  var channel = make_channel("http://localhost:4444/testdir");
  channel.asyncOpen(new ChannelListener(channel_finished), null);
  iteration++;
}

function run_test() {
  observer.register();

  httpserver = new nsHttpServer();
  httpserver.registerPathHandler("/testdir", test_handler);
  httpserver.start(4444);

  run_next_test();
  do_test_pending();
}
