const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

var httpserver = null;

XPCOMUtils.defineLazyGetter(this, "uri", function() {
  return "http://localhost:" + httpserver.identity.primaryPort + "/multipart";
});

function make_channel(url) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
}

var multipartBody = "--boundary\r\n\r\nSome text\r\n--boundary\r\nContent-Type: text/x-test\r\n\r\n<?xml version='1.1'?>\r\n<root/>\r\n--boundary\r\n\r\n<?xml version='1.0'?><root/>\r\n--boundary--";

function contentHandler(metadata, response)
{
  response.setHeader("Content-Type", 'multipart/mixed; boundary="boundary"');
  response.processAsync();

  var body = multipartBody;
  function byteByByte()
  {
    if (!body.length) {
      response.finish();
      return;
    }

    var onebyte = body[0];
    response.bodyOutputStream.write(onebyte, 1);
    body = body.substring(1);
    do_timeout(1, byteByByte);
  }

  do_timeout(1, byteByByte);
}

var numTests = 2;
var testNum = 0;

var testData =
  [
   { data: "Some text", type: "text/plain" },
   { data: "<?xml version='1.1'?>\r\n<root/>", type: "text/x-test" },
   { data: "<?xml version='1.0'?><root/>", type: "text/xml" }
  ];

function responseHandler(request, buffer)
{
  Assert.equal(buffer, testData[testNum].data);
  Assert.equal(request.QueryInterface(Ci.nsIChannel).contentType,
  testData[testNum].type);
  if (++testNum == numTests)
    httpserver.stop(do_test_finished);
}

var multipartListener = {
  _buffer: "",
  _index: 0,

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsIRequestObserver) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  onStartRequest: function(request) {
    this._buffer = "";
  },

  onDataAvailable: function(request, stream, offset, count) {
    try {
      this._buffer = this._buffer.concat(read_stream(stream, count));
      dump("BUFFEEE: " + this._buffer + "\n\n");
    } catch (ex) {
      do_throw("Error in onDataAvailable: " + ex);
    }
  },

  onStopRequest: function(request, status) {
    this._index++;
    // Second part should be last part
    Assert.equal(request.QueryInterface(Ci.nsIMultiPartChannel).isLastPart, this._index == testData.length);
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
  chan.asyncOpen(conv);
  do_test_pending();
}
