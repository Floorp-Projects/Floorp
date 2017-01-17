// Test nsITraceableChannel interface.
//
// The test checks that a tracing listener can modifies the body of a HTTP
// response.
// This test also check that it is not possible to set a tracableLisener after
// onStartRequest is called.

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

const DEBUG = false;

function debug(msg) {
  if (DEBUG) {
    dump(msg);
  }
}

var httpserver = new HttpServer();
httpserver.start(-1);
const PORT = httpserver.identity.primaryPort;

var originalBody = "original http response body";
var replacedBody = "replaced http response body";

var observer;

function TracingListener() {}

TracingListener.prototype = {
  gotOnStartRequest: false,
  pipe: null,
  streamSink: null,
  listener: null,
  onDataAvailableCalled: false,

  // Replace received response body.
  onDataAvailable: function(request, context, inputStream,
                           offset, count) {
    debug("*** tracing listener onDataAvailable\n");

    var binaryInputStream = Cc["@mozilla.org/binaryinputstream;1"].
      createInstance(Components.interfaces.nsIBinaryInputStream);
    binaryInputStream.setInputStream(inputStream);

    var data = binaryInputStream.readBytes(count);
    var origBody = originalBody.substr(offset, count);

    do_check_eq(origBody, data);

    if (!this.onDataAvailableCalled) {
      this.onDataAvailableCalled = true;
      var storageStream = Cc["@mozilla.org/storagestream;1"].
        createInstance(Components.interfaces.nsIStorageStream);
      var binaryOutputStream = Cc["@mozilla.org/binaryoutputstream;1"].
        createInstance(Components.interfaces.nsIBinaryOutputStream);

      storageStream.init(8192, 100, null);
      binaryOutputStream.setOutputStream(storageStream.getOutputStream(0));

      var newBody = replacedBody.substr(offset, count);
      binaryOutputStream.writeBytes(newBody, newBody.length);

      this.listener.onDataAvailable(request, context,
                                    storageStream.newInputStream(0), 0,
                                    replacedBody.length);
    }
  },

  onStartRequest: function(request, context) {
    debug("*** tracing listener onStartRequest\n");

    this.listener.onStartRequest(request, context);

    this.gotOnStartRequest = true;

    request.QueryInterface(Components.interfaces.nsIHttpChannelInternal);

    do_check_eq(request.localAddress, "127.0.0.1");
    do_check_eq(request.localPort > 0, true);
    do_check_neq(request.localPort, PORT);
    do_check_eq(request.remoteAddress, "127.0.0.1");
    do_check_eq(request.remotePort, PORT);

    // Make sure listener can't be replaced after OnStartRequest was called.
    request.QueryInterface(Components.interfaces.nsITraceableChannel);
    try {
      var newListener = new TracingListener();
      newListener.listener = request.setNewListener(newListener);
    } catch(e) {
      do_check_true(true, "TracingListener.onStartRequest swallowing exception: " + e + "\n");
      return; // OK
    }
    do_throw("replaced channel's listener during onStartRequest.");
  },

  onStopRequest: function(request, context, statusCode) {
    debug("*** tracing listener onStopRequest\n");


    this.listener.onStopRequest(request, context, statusCode)

    do_check_eq(this.gotOnStartRequest, true);

    let message = `response`;
    do_await_remote_message(message).then(() => {
      sendCommand("finish();", stopServer);
    });
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIStreamListener) ||
        iid.equals(Components.interfaces.nsIRequestObserver) ||
        iid.equals(Components.interfaces.nsISupports)
        )
      return this;
    throw Components.results.NS_NOINTERFACE;
  }
}

function HttpResponseExaminer() {}

HttpResponseExaminer.prototype = {
  register: function() {
    Cc["@mozilla.org/observer-service;1"].
      getService(Components.interfaces.nsIObserverService).
      addObserver(this, "http-on-examine-response", true);
    debug("Did HttpResponseExaminer.register\n");
  },

  unregister: function() {
    Cc["@mozilla.org/observer-service;1"].
      getService(Components.interfaces.nsIObserverService).
      removeObserver(this, "http-on-examine-response", true);
    debug("Did HttpResponseExaminer.unregister\n");
  },

  // Replace channel's listener.
  observe: function(subject, topic, data) {
    debug("In HttpResponseExaminer.observe\n");

    try {
      subject.QueryInterface(Components.interfaces.nsITraceableChannel);
      var newListener = new TracingListener();
      newListener.listener = subject.setNewListener(newListener);
    } catch(e) {
      do_throw("can't replace listener " + e);
    }

    debug("Did HttpResponseExaminer.observe\n");
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIObserver) ||
        iid.equals(Components.interfaces.nsISupportsWeakReference) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  }
}

function test_handlerSimple(metadata, response) {
  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.bodyOutputStream.write(originalBody, originalBody.length);
}

function test_modify() { 
  sendCommand(`makeRequest("http://localhost:${PORT}/testSimple", "${replacedBody}");`);
}


function stopServer() {
  observer.unregister();
  httpserver.stop(do_test_finished)
}

function run_test() {

  observer = new HttpResponseExaminer();
  observer.register();

  httpserver.registerPathHandler("/testSimple", test_handlerSimple);

  run_test_in_child("child_tracable_listener.js", test_modify);
}
