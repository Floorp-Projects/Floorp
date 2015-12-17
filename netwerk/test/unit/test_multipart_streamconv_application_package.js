// Tests:
//   test_multipart
//     Loads the multipart file returned by contentHandler()
//     The boundary is ascertained from the first line in the multipart file
//   test_multipart_with_boundary
//     Loads the multipart file returned by contentHandler_with_boundary()
//     The boundary is given in the Content-Type headers, and is also present
//     in the first line of the file.
//   test_multipart_chunked_headers
//     Tests that the headers are properly passed even when they come in multiple
//     chunks (several calls to OnDataAvailable). It first passes the first 60
//     characters, then the rest of the response.

// testData.token - the multipart file's boundary
// Call testData.getData() to get the file contents as a string

// multipartListener
//   - a listener that checks that the multipart file is correctly split into multiple parts

// headerListener
//   - checks that the headers for each part is set correctly

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

var httpserver = null;

XPCOMUtils.defineLazyGetter(this, "uri", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

function make_channel(url) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
}

function contentHandler(metadata, response)
{
  response.setHeader("Content-Type", 'application/package');
  var body = testData.getData();
  response.bodyOutputStream.write(body, body.length);
}

function contentHandler_with_boundary(metadata, response)
{
  response.setHeader("Content-Type", 'application/package; boundary="'+testData.token+'"');
  var body = testData.getData();
  response.bodyOutputStream.write(body, body.length);
}

function contentHandler_chunked_headers(metadata, response)
{
  response.setHeader("Content-Type", 'application/package');
  var body = testData.getData();

  response.bodyOutputStream.write(body.substring(0,60), 60);
  response.processAsync();
  do_timeout(5, function() {
    response.bodyOutputStream.write(body.substring(60), body.length-60);
    response.finish();
  });
}

function contentHandler_type_missing(metadata, response)
{
  response.setHeader("Content-Type", 'text/plain');
  var body = testData.getData();
  response.bodyOutputStream.write(body, body.length);
}

function contentHandler_with_package_header(chunkSize, metadata, response)
{
  response.setHeader("Content-Type", 'application/package');
  var body = testData.packageHeader + testData.getData();

  response.bodyOutputStream.write(body.substring(0,chunkSize), chunkSize);
  response.processAsync();
  do_timeout(5, function() {
    response.bodyOutputStream.write(body.substring(chunkSize), body.length-chunkSize);
    response.finish();
  });
}

var testData = {
  packageHeader: 'manifest-signature: dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk\r\n',
  content: [
   { headers: ["Content-Location: /index.html", "Content-Type: text/html"], data: "<html>\r\n  <head>\r\n    <script src=\"/scripts/app.js\"></script>\r\n    ...\r\n  </head>\r\n  ...\r\n</html>\r\n", type: "text/html" },
   { headers: ["Content-Location: /scripts/app.js", "Content-Type: text/javascript"], data: "module Math from '/scripts/helpers/math.js';\r\n...\r\n", type: "text/javascript" },
   { headers: ["Content-Location: /scripts/helpers/math.js", "Content-Type: text/javascript"], data: "export function sum(nums) { ... }\r\n...\r\n", type: "text/javascript" }
  ],
  token : "gc0pJq0M:08jU534c0p",
  getData: function() {
    var str = "";
    for (var i in this.content) {
      str += "--" + this.token + "\r\n";
      for (var j in this.content[i].headers) {
        str += this.content[i].headers[j] + "\r\n";
      }
      str += "\r\n";
      str += this.content[i].data + "\r\n";
    }

    str += "--" + this.token + "--";
    return str;
  }
}

function multipartListener(test, badContentType, shouldVerifyPackageHeader) {
  this._buffer = "";
  this.testNum = 0;
  this.test = test;
  this.numTests = this.test.content.length;
  // If set to true, that means the package is missing the application/package
  // content type. If so, resources will have their content type set to
  // application/octet-stream
  this.badContentType = badContentType == undefined ? false : badContentType;
  this.shouldVerifyPackageHeader = shouldVerifyPackageHeader;
}

multipartListener.prototype.responseHandler = function(request, buffer) {
    equal(buffer, this.test.content[this.testNum].data);
    equal(request.QueryInterface(Ci.nsIChannel).contentType,
          this.badContentType ? "application/octet-stream" : this.test.content[this.testNum].type);
    if (++this.testNum == this.numTests) {
      run_next_test();
    }
}

multipartListener.prototype.QueryInterface = function(iid) {
  if (iid.equals(Components.interfaces.nsIStreamListener) ||
      iid.equals(Components.interfaces.nsIRequestObserver) ||
      iid.equals(Components.interfaces.nsISupports))
    return this;
  throw Components.results.NS_ERROR_NO_INTERFACE;
}

multipartListener.prototype.onStartRequest = function(request, context) {
  if (this.shouldVerifyPackageHeader) {
    let partChannel = request.QueryInterface(Ci.nsIMultiPartChannel);
    ok(!!partChannel, 'Should be multipart channel');
    equal(partChannel.preamble, this.test.packageHeader);
  }

  this._buffer = "";
  this.headerListener = new headerListener(this.test.content[this.testNum].headers, this.badContentType);
  let headerProvider = request.QueryInterface(Ci.nsIResponseHeadProvider);
  if (headerProvider) {
    headerProvider.visitResponseHeaders(this.headerListener);
  }

  // Verify the original header if the request is a multipart channel.
  let partChannel = request.QueryInterface(Ci.nsIMultiPartChannel);
  if (partChannel) {
    let originalHeader = this.test.content[this.testNum].headers.join("\r\n") + "\r\n\r\n";
    equal(originalHeader, partChannel.originalResponseHeader, "Oringinal header check.");
  }
}

multipartListener.prototype.onDataAvailable = function(request, context, stream, offset, count) {
  try {
    this._buffer = this._buffer.concat(read_stream(stream, count));
  } catch (ex) {
    do_throw("Error in onDataAvailable: " + ex);
  }
}

multipartListener.prototype.onStopRequest = function(request, context, status) {
  try {
    equal(this.headerListener.index, this.test.content[this.testNum].headers.length);
    this.responseHandler(request, this._buffer);
  } catch (ex) {
    do_throw("Error in closure function: " + ex);
  }
}

function headerListener(headers, badContentType) {
  this.expectedHeaders = headers;
  this.badContentType = badContentType;
  this.index = 0;
}

headerListener.prototype.QueryInterface = function(iid) {
  if (iid.equals(Components.interfaces.nsIHttpHeaderVisitor) ||
      iid.equals(Components.interfaces.nsISupports))
    return this;
  throw Components.results.NS_ERROR_NO_INTERFACE;
}

headerListener.prototype.visitHeader = function(header, value) {
  ok(this.index <= this.expectedHeaders.length);
  if (!this.badContentType)
    equal(header + ": " + value, this.expectedHeaders[this.index]);
  this.index++;
}

function test_multipart() {
  var streamConv = Cc["@mozilla.org/streamConverters;1"]
                     .getService(Ci.nsIStreamConverterService);
  var conv = streamConv.asyncConvertData("application/package",
           "*/*",
           new multipartListener(testData),
           null);

  var chan = make_channel(uri + "/multipart");
  chan.asyncOpen2(conv);
}

function test_multipart_with_boundary() {
  var streamConv = Cc["@mozilla.org/streamConverters;1"]
                     .getService(Ci.nsIStreamConverterService);
  var conv = streamConv.asyncConvertData("application/package",
           "*/*",
           new multipartListener(testData),
           null);

  var chan = make_channel(uri + "/multipart2");
  chan.asyncOpen2(conv);
}

function test_multipart_chunked_headers() {
  var streamConv = Cc["@mozilla.org/streamConverters;1"]
                     .getService(Ci.nsIStreamConverterService);
  var conv = streamConv.asyncConvertData("application/package",
           "*/*",
           new multipartListener(testData),
           null);

  var chan = make_channel(uri + "/multipart3");
  chan.asyncOpen2(conv);
}

function test_multipart_content_type_other() {
  var streamConv = Cc["@mozilla.org/streamConverters;1"]
                     .getService(Ci.nsIStreamConverterService);

  var conv = streamConv.asyncConvertData("application/package",
           "*/*",
           new multipartListener(testData, true),
           null);

  var chan = make_channel(uri + "/multipart4");
  chan.asyncOpen2(conv);
}

function test_multipart_package_header(aChunkSize) {
  var streamConv = Cc["@mozilla.org/streamConverters;1"]
                     .getService(Ci.nsIStreamConverterService);

  var conv = streamConv.asyncConvertData("application/package",
           "*/*",
           new multipartListener(testData, false, true),
           null);

  var chan = make_channel(uri + "/multipart5_" + aChunkSize);
  chan.asyncOpen2(conv);
}

// Bug 1212223 - Test multipart with package header and different chunk size.
// Use explict function name to make the test case log more readable.

function test_multipart_package_header_50() {
  return test_multipart_package_header(50);
}

function test_multipart_package_header_100() {
  return test_multipart_package_header(100);
}

function test_multipart_package_header_150() {
  return test_multipart_package_header(150);
}

function test_multipart_package_header_200() {
  return test_multipart_package_header(200);
}

function run_test()
{
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/multipart", contentHandler);
  httpserver.registerPathHandler("/multipart2", contentHandler_with_boundary);
  httpserver.registerPathHandler("/multipart3", contentHandler_chunked_headers);
  httpserver.registerPathHandler("/multipart4", contentHandler_type_missing);

  // Bug 1212223 - Test multipart with package header and different chunk size.
  httpserver.registerPathHandler("/multipart5_50", contentHandler_with_package_header.bind(null, 50));
  httpserver.registerPathHandler("/multipart5_100", contentHandler_with_package_header.bind(null, 100));
  httpserver.registerPathHandler("/multipart5_150", contentHandler_with_package_header.bind(null, 150));
  httpserver.registerPathHandler("/multipart5_200", contentHandler_with_package_header.bind(null, 200));

  httpserver.start(-1);

  run_next_test();
}

add_test(test_multipart);
add_test(test_multipart_with_boundary);
add_test(test_multipart_chunked_headers);
add_test(test_multipart_content_type_other);

// Bug 1212223 - Test multipart with package header and different chunk size.
add_test(test_multipart_package_header_50);
add_test(test_multipart_package_header_100);
add_test(test_multipart_package_header_150);
add_test(test_multipart_package_header_200);
