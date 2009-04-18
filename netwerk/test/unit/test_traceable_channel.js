// Test nsITraceableChannel interface.
// Replace original listener with TracingListener that modifies body of HTTP
// response. Make sure that body received by original channel's listener
// is correctly modified.

do_load_httpd_js();

var httpserver = null;
var originalBody = "original http response body";
var replacedBody = "replaced http response body";

function TracingListener() {}

TracingListener.prototype = {

  // Replace received response body.
  onDataAvailable: function(request, context, inputStream,
                           offset, count) {
    dump("*** tracing listener onDataAvailable\n");
    var binaryInputStream = Cc["@mozilla.org/binaryinputstream;1"].
      createInstance(Components.interfaces.nsIBinaryInputStream);
    binaryInputStream.setInputStream(inputStream);

    var data = binaryInputStream.readBytes(count);
    var origBody = originalBody.substr(offset, count);
    do_check_eq(origBody, data);

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
  },

  onStartRequest: function(request, context) {
    this.listener.onStartRequest(request, context);

    // Make sure listener can't be replaced after OnStartRequest was called.
    request.QueryInterface(Components.interfaces.nsITraceableChannel);
    try {
      var newListener = new TracingListener();
      newListener.listener = request.setNewListener(newListener);
    } catch(e) {
      return; // OK
    }
    do_throw("replaced channel's listener during onStartRequest.");
  },

  onStopRequest: function(request, context, statusCode) {
    this.listener.onStopRequest(request, context, statusCode);
    httpserver.stop(do_test_finished);
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIStreamListener) ||
        iid.equals(Components.interfaces.nsIRequestObserver) ||
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
  },

  // Replace channel's listener.
  observe: function(subject, topic, data) {
    try {
      subject.QueryInterface(Components.interfaces.nsITraceableChannel);
      var newListener = new TracingListener();
      newListener.listener = subject.setNewListener(newListener);
    } catch(e) {
      do_throw("can't replace listener" + e);
    }
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
function get_data(request, input, ctx) {
  do_check_eq(replacedBody, input);
}

function run_test() {
  var observer = new HttpResponseExaminer();
  observer.register();

  httpserver = new nsHttpServer();
  httpserver.registerPathHandler("/testdir", test_handler);
  httpserver.start(4444);

  var channel = make_channel("http://localhost:4444/testdir");
  channel.asyncOpen(new ChannelListener(get_data), null);
  do_test_pending();
}
