/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Unit tests for the binary http bindings.
// Tests basic encoding and decoding of requests and responses.

function BinaryHttpRequest(
  method,
  scheme,
  authority,
  path,
  headerNames,
  headerValues,
  content
) {
  this.method = method;
  this.scheme = scheme;
  this.authority = authority;
  this.path = path;
  this.headerNames = headerNames;
  this.headerValues = headerValues;
  this.content = content;
}

BinaryHttpRequest.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIBinaryHttpRequest"]),
};

function test_encode_request() {
  let bhttp = Cc["@mozilla.org/network/binary-http;1"].getService(
    Ci.nsIBinaryHttp
  );
  let request = new BinaryHttpRequest(
    "GET",
    "https",
    "",
    "/hello.txt",
    ["user-agent", "host", "accept-language"],
    [
      "curl/7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l zlib/1.2.3",
      "www.example.com",
      "en, mi",
    ],
    []
  );
  let encoded = bhttp.encodeRequest(request);
  // This example is from RFC 9292.
  let expected = hexStringToBytes(
    "0003474554056874747073000a2f6865" +
      "6c6c6f2e747874406c0a757365722d61" +
      "67656e74346375726c2f372e31362e33" +
      "206c69626375726c2f372e31362e3320" +
      "4f70656e53534c2f302e392e376c207a" +
      "6c69622f312e322e3304686f73740f77" +
      "77772e6578616d706c652e636f6d0f61" +
      "63636570742d6c616e67756167650665" +
      "6e2c206d690000"
  );
  deepEqual(encoded, expected);

  let mismatchedHeaders = new BinaryHttpRequest(
    "GET",
    "https",
    "",
    "",
    ["whoops-only-one-header-name"],
    ["some-header-value", "some-other-header-value"],
    []
  );
  // The implementation uses "NS_ERROR_INVALID_ARG", because that's an
  // appropriate description for the error.  However, that is an alias to
  // "NS_ERROR_ILLEGAL_VALUE", which is what the actual exception uses, so
  // that's what is tested for here.
  Assert.throws(
    () => bhttp.encodeRequest(mismatchedHeaders),
    /NS_ERROR_ILLEGAL_VALUE/
  );
}

function test_decode_request() {
  let bhttp = Cc["@mozilla.org/network/binary-http;1"].getService(
    Ci.nsIBinaryHttp
  );

  // From RFC 9292.
  let encoded = hexStringToBytes(
    "0003474554056874747073000a2f6865" +
      "6c6c6f2e747874406c0a757365722d61" +
      "67656e74346375726c2f372e31362e33" +
      "206c69626375726c2f372e31362e3320" +
      "4f70656e53534c2f302e392e376c207a" +
      "6c69622f312e322e3304686f73740f77" +
      "77772e6578616d706c652e636f6d0f61" +
      "63636570742d6c616e67756167650665" +
      "6e2c206d690000"
  );
  let request = bhttp.decodeRequest(encoded);
  equal(request.method, "GET");
  equal(request.scheme, "https");
  equal(request.authority, "");
  equal(request.path, "/hello.txt");
  let expectedHeaderNames = ["user-agent", "host", "accept-language"];
  deepEqual(request.headerNames, expectedHeaderNames);
  let expectedHeaderValues = [
    "curl/7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l zlib/1.2.3",
    "www.example.com",
    "en, mi",
  ];
  deepEqual(request.headerValues, expectedHeaderValues);
  deepEqual(request.content, []);

  let garbage = hexStringToBytes("115f00ab64c0fa783fe4cb723eaa87fa78900a0b00");
  Assert.throws(() => bhttp.decodeRequest(garbage), /NS_ERROR_UNEXPECTED/);
}

function test_decode_response() {
  let bhttp = Cc["@mozilla.org/network/binary-http;1"].getService(
    Ci.nsIBinaryHttp
  );
  // From RFC 9292.
  let encoded = hexStringToBytes(
    "0340660772756e6e696e670a22736c65" +
      "657020313522004067046c696e6b233c" +
      "2f7374796c652e6373733e3b2072656c" +
      "3d7072656c6f61643b2061733d737479" +
      "6c65046c696e6b243c2f736372697074" +
      "2e6a733e3b2072656c3d7072656c6f61" +
      "643b2061733d7363726970740040c804" +
      "646174651d4d6f6e2c203237204a756c" +
      "20323030392031323a32383a35332047" +
      "4d540673657276657206417061636865" +
      "0d6c6173742d6d6f6469666965641d57" +
      "65642c203232204a756c203230303920" +
      "31393a31353a353620474d5404657461" +
      "671422333461613338372d642d313536" +
      "3865623030220d6163636570742d7261" +
      "6e6765730562797465730e636f6e7465" +
      "6e742d6c656e67746802353104766172" +
      "790f4163636570742d456e636f64696e" +
      "670c636f6e74656e742d747970650a74" +
      "6578742f706c61696e003348656c6c6f" +
      "20576f726c6421204d7920636f6e7465" +
      "6e7420696e636c756465732061207472" +
      "61696c696e672043524c462e0d0a0000"
  );
  let response = bhttp.decodeResponse(encoded);
  equal(response.status, 200);
  deepEqual(
    response.content,
    stringToBytes("Hello World! My content includes a trailing CRLF.\r\n")
  );
  let expectedHeaderNames = [
    "date",
    "server",
    "last-modified",
    "etag",
    "accept-ranges",
    "content-length",
    "vary",
    "content-type",
  ];
  deepEqual(response.headerNames, expectedHeaderNames);
  let expectedHeaderValues = [
    "Mon, 27 Jul 2009 12:28:53 GMT",
    "Apache",
    "Wed, 22 Jul 2009 19:15:56 GMT",
    '"34aa387-d-1568eb00"',
    "bytes",
    "51",
    "Accept-Encoding",
    "text/plain",
  ];
  deepEqual(response.headerValues, expectedHeaderValues);

  let garbage = hexStringToBytes(
    "0367890084cb0ab03115fa0b4c2ea0fa783f7a87fa00"
  );
  Assert.throws(() => bhttp.decodeResponse(garbage), /NS_ERROR_UNEXPECTED/);
}

function test_encode_response() {
  let response = new BinaryHttpResponse(
    418,
    ["content-type"],
    ["text/plain"],
    stringToBytes("I'm a teapot")
  );
  let bhttp = Cc["@mozilla.org/network/binary-http;1"].getService(
    Ci.nsIBinaryHttp
  );
  let encoded = bhttp.encodeResponse(response);
  let expected = hexStringToBytes(
    "0141a2180c636f6e74656e742d747970650a746578742f706c61696e0c49276d206120746561706f7400"
  );
  deepEqual(encoded, expected);

  let mismatchedHeaders = new BinaryHttpResponse(
    500,
    ["some-header", "some-other-header"],
    ["whoops-only-one-header-value"],
    []
  );
  // The implementation uses "NS_ERROR_INVALID_ARG", because that's an
  // appropriate description for the error.  However, that is an alias to
  // "NS_ERROR_ILLEGAL_VALUE", which is what the actual exception uses, so
  // that's what is tested for here.
  Assert.throws(
    () => bhttp.encodeResponse(mismatchedHeaders),
    /NS_ERROR_ILLEGAL_VALUE/
  );
}

function run_test() {
  test_encode_request();
  test_decode_request();
  test_encode_response();
  test_decode_response();
}
