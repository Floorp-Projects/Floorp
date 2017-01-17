// Test nsITraceableChannel interface.
//
// The test checks that content-encoded responses are received decoded by a
// tracing listener.

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

var body = [
  0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xcb, 0x2f, 0xca, 0x4c, 0xcf, 0xcc,
  0x4b, 0xcc, 0x51, 0xc8, 0x28, 0x29, 0x29, 0x50, 0x28, 0x4a, 0x2d, 0x2e, 0xc8, 0xcf, 0x2b, 0x4e,
  0x55, 0x48, 0xca, 0x4f, 0xa9, 0x04, 0x00, 0x94, 0xde, 0x94, 0x9c, 0x1b, 0x00, 0x00, 0x00];

var observer;

function TracingListener() {}

TracingListener.prototype = {
  gotOnStartRequest: false,
  pipe: null,
  streamSink: null,
  listener: null,

  // Replace received response body.
  onDataAvailable: function(request, context, inputStream,
                           offset, count) {
    debug("*** tracing listener onDataAvailable\n");
  },

  onStartRequest: function(request, context) {
    debug("*** tracing listener onStartRequest\n");

    this.gotOnStartRequest = true;

    request.QueryInterface(Components.interfaces.nsIHttpChannelInternal);

    do_check_eq(request.localAddress, "127.0.0.1");
    do_check_eq(request.localPort > 0, true);
    do_check_neq(request.localPort, PORT);
    do_check_eq(request.remoteAddress, "127.0.0.1");
    do_check_eq(request.remotePort, PORT);
  },

  onStopRequest: function(request, context, statusCode) {
    debug("*** tracing listener onStopRequest\n");

    var sin = Components.classes["@mozilla.org/scriptableinputstream;1"].
                createInstance(Ci.nsIScriptableInputStream);

    this.streamSink.close();
    var input = this.pipe.inputStream;
    sin.init(input);
    do_check_eq(sin.available(), originalBody.length);

    var result = sin.read(originalBody.length);
    do_check_eq(result, originalBody);

    input.close();

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

      var tee = Cc["@mozilla.org/network/stream-listener-tee;1"].
          createInstance(Ci.nsIStreamListenerTee);
      var newListener = new TracingListener();
      newListener.pipe = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);
      newListener.pipe.init(false, false, 0, 0xffffffff, null);
      newListener.streamSink = newListener.pipe.outputStream;

      var originalListener = subject.setNewListener(tee);
      tee.init(originalListener, newListener.streamSink, newListener);
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

function test_handlerContentEncoded(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Content-Encoding", "gzip", false);
  response.setHeader("Content-Length", "" + body.length, false);

  var bos = Components.classes["@mozilla.org/binaryoutputstream;1"]
    .createInstance(Components.interfaces.nsIBinaryOutputStream);
  bos.setOutputStream(response.bodyOutputStream);

  response.processAsync();
  bos.writeByteArray(body, body.length);
  response.finish();
}

function test_contentEncoded() {
  sendCommand(`makeRequest("http://localhost:${PORT}/testContentEncoded", "${originalBody}");`);
}

function stopServer() {
  observer.unregister();
  httpserver.stop(do_test_finished)
}

function run_test() {

  observer = new HttpResponseExaminer();
  observer.register();

  httpserver.registerPathHandler("/testContentEncoded", test_handlerContentEncoded);
  run_test_in_child("child_tracable_listener.js", test_contentEncoded);
}
