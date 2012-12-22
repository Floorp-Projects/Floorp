const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");

var httpserver = null;
var uri = "http://localhost:4444/multipart";

function make_channel(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newChannel(url, "", null);
}

var multipartBody = "--boundary\r\n"+
"Content-type: text/plain\r\n"+
"Content-range: bytes 0-2/10\r\n"+
"\r\n"+
"aaa\r\n"+
"--boundary\r\n"+
"Content-type: text/plain\r\n"+
"Content-range: bytes 3-7/10\r\n"+
"\r\n"+
"bbbbb"+
"\r\n"+
"--boundary\r\n"+
"Content-type: text/plain\r\n"+
"Content-range: bytes  8-9/10\r\n"+
"\r\n"+
"cc"+
"\r\n"+
"--boundary--";

function make_channel(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newChannel(url, "", null);
}

function contentHandler(metadata, response)
{
  response.setHeader("Content-Type", 'multipart/byteranges; boundary="boundary"');
  response.bodyOutputStream.write(multipartBody, multipartBody.length);
}

var numTests = 2;
var testNum = 0;

var testData =
  [
   { data: "aaa", type: "text/plain", isByteRangeRequest: true, startRange: 0, endRange: 2 },
   { data: "bbbbb", type: "text/plain", isByteRangeRequest: true, startRange: 3, endRange: 7 },
   { data: "cc", type: "text/plain", isByteRangeRequest: true, startRange: 8, endRange: 9 }
  ];

function responseHandler(request, buffer)
{
    do_check_eq(buffer, testData[testNum].data);
    do_check_eq(request.QueryInterface(Ci.nsIChannel).contentType,
		testData[testNum].type);
    do_check_eq(request.QueryInterface(Ci.nsIByteRangeRequest).isByteRangeRequest,
		testData[testNum].isByteRangeRequest);
    do_check_eq(request.QueryInterface(Ci.nsIByteRangeRequest).startRange,
		testData[testNum].startRange);
    do_check_eq(request.QueryInterface(Ci.nsIByteRangeRequest).endRange,
		testData[testNum].endRange);
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
  httpserver.start(4444);

  var streamConv = Cc["@mozilla.org/streamConverters;1"]
                     .getService(Ci.nsIStreamConverterService);
  var conv = streamConv.asyncConvertData("multipart/byteranges",
					 "*/*",
					 multipartListener,
					 null);

  var chan = make_channel(uri);
  chan.asyncOpen(conv, null);
  do_test_pending();
}
