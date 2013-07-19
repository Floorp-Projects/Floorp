const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");

var httpserver = null;

XPCOMUtils.defineLazyGetter(this, "uri", function() {
  return "http://localhost:" + httpserver.identity.primaryPort + "/multipart";
});

function make_channel(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newChannel(url, "", null);
}

var multipartBody = "--boundary\r\n\r\nSome text\r\n--boundary\r\n\r\n<?xml version='1.0'?><root/>\r\n--boundary--";

function make_channel(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newChannel(url, "", null);
}

function contentHandler(metadata, response)
{
  response.setHeader("Content-Type", 'multipart/mixed; boundary="boundary"');
  response.bodyOutputStream.write(multipartBody, multipartBody.length);
}

var numTests = 2;
var testNum = 0;

var testData =
  [
   { data: "Some text", type: "text/plain" },
   { data: "<?xml version='1.0'?><root/>", type: "text/xml" }
  ];

function responseHandler(request, buffer)
{
    do_check_eq(buffer, testData[testNum].data);
    do_check_eq(request.QueryInterface(Ci.nsIChannel).contentType,
		testData[testNum].type);
    if (++testNum == numTests)
	httpserver.stop(do_test_finished);
}

var multipartListener = {
  _buffer: "",

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIStreamListener) ||
        iid.equals(Components.interfaces.nsIRequestObserver) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  onStartRequest: function(request, context) {
    this._buffer = "";
  },

  onDataAvailable: function(request, context, stream, offset, count) {
    try {
      this._buffer = this._buffer.concat(read_stream(stream, count));
      dump("BUFFEEE: " + this._buffer + "\n\n");
    } catch (ex) {
      do_throw("Error in onDataAvailable: " + ex);
    }
  },

  onStopRequest: function(request, context, status) {
    try {
      responseHandler(request, this._buffer);
    } catch (ex) {
      do_throw("Error in closure function: " + ex);
    }
  }
};

function run_test()
{
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/multipart", contentHandler);
  httpserver.start(-1);

  var streamConv = Cc["@mozilla.org/streamConverters;1"]
                     .getService(Ci.nsIStreamConverterService);
  var conv = streamConv.asyncConvertData("multipart/mixed",
					 "*/*",
					 multipartListener,
					 null);

  var chan = make_channel(uri);
  chan.asyncOpen(conv, null);
  do_test_pending();
}
